"""Co-op policy under lockstep (netplay M4 D-policy, issue #885).

Thin wrapper over run_pair.py (imported from this folder, not edited): it
swaps run_pair.write_bootstrap for the schema-9 M4d template (CHECKSET 6,
BENEFITS 30, DEATHLINK 3), runs run_pair.main(argv), then checks the policy
decisions on both peers:

* both peers' `[coop-policy]` lines are identical, in order (they are sim
  decisions, so a difference is a desync the hash may not see yet);
* both peers logged identical `[coop-policy] TEST event` lines when
  PIKMIN_NETPLAY_TEST_COOP_EVENTS is passed (the knob is not in the
  handshake config hash yet, so this is the guard);
* gameplay proof: START_STAGE and `[Pikmin Randomizer]` lines on both peers,
  and the distinct (navi, piki, teki, item) hash tuples.

Always: the ANCHOR lines replay the round-robin cursor strictly (see
check_anchors); any grant off the cursor without a logged ANCHOR_SKIP fails.
With --acceptance it also requires a `HEAL captain=2`, at least one
round-robin hand-over between two grants of one kind logged while both
captains were live (`live=11`, no fallback), and a `DEATHLINK killed=3 p1=0`
line.

All run_pair.py options pass through unchanged (see run_pair.py --help).
"""

import argparse
import hashlib
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import run_pair  # noqa: E402

TEMPLATE = (
    "PIKMIN_RANDOMIZER 9\nSESSION {TOKEN}\nFINGERPRINT {TOKEN}\nPROFILE {PROFILE}\n"
    "CATALOG gameplay-checks-v9\nPLACEMENT identity-v1\nGOAL 25\nDAYS repeat-day29-v1\n"
    "COLOR red\nCHECKSET 6\nENEMIES 0\nSTARTING_FLARLIC 2\nBENEFITS 30\nDEATHLINK 3\nEND\n"
)


def write_bootstrap_m4d(path, token, profile, flarlic=10):
    # STARTING_FLARLIC is fixed at 2 by the template (flarlic is ignored).
    Path(path).write_text(TEMPLATE.replace("{TOKEN}", token).replace("{PROFILE}", profile))


def policy_lines(log):
    try:
        text = Path(log).read_text(errors="replace")
    except OSError:
        return []
    return [ln[ln.index("[coop-policy]"):].strip() for ln in text.splitlines() if "[coop-policy]" in ln]


def grep_count(log, needle):
    try:
        return sum(1 for ln in Path(log).read_text(errors="replace").splitlines() if needle in ln)
    except OSError:
        return 0


def distinct_tuples(hashes):
    seen = set()
    try:
        for ln in Path(hashes).read_text(errors="replace").splitlines():
            cols = ln.split()
            if len(cols) >= 6:
                seen.add(tuple(cols[2:6]))
    except OSError:
        pass
    return len(seen)


ANCHOR_RE = re.compile(r"\[coop-policy\] ANCHOR kind=(\S+) captain=(\d) next=(\d) live=([01])([01])")
SKIP_RE = re.compile(r"\[coop-policy\] ANCHOR_SKIP kind=(\S+) captain=(\d) reason=(\S+)")


def check_anchors(lines):
    """Strict round-robin check over one peer's [coop-policy] lines.

    Replays the cursor: every kind starts on captain 1 (and again after a
    `RESET` line). Each ANCHOR grant must land on the cursor captain, unless
    an ANCHOR_SKIP for that captain was logged right before it (then it is a
    fallback, marked `*`). The skip reason must agree with the grant's live
    flags, the grant must go to a live captain, and `next=` must be the other
    captain. A round-robin hand-over is two consecutive grants of one kind,
    both logged with `live=11`, the second on the cursor (not a fallback).

    Returns (errors, sequences per kind, hand-overs per kind)."""
    expected, skips, last = {}, {}, {}
    errors, seqs, handovers = [], {}, {}
    for ln in lines:
        if ln.startswith("[coop-policy] RESET "):
            expected.clear()
            skips.clear()
            last.clear()
            for kind in seqs:
                seqs[kind].append("|")
            continue
        m = SKIP_RE.match(ln)
        if m:
            skips.setdefault(m.group(1), []).append((int(m.group(2)), m.group(3)))
            continue
        m = ANCHOR_RE.match(ln)
        if not m:
            if "] ANCHOR " in ln:
                errors.append(f"unparsed ANCHOR line: {ln}")
            continue
        kind, cap, nxt = m.group(1), int(m.group(2)), int(m.group(3))
        live = (m.group(4) == "1", m.group(5) == "1")
        cursor = expected.get(kind, 1)
        skipped = skips.pop(kind, [])
        fallback = cap != cursor
        if fallback and cursor not in [c for c, _ in skipped]:
            errors.append(f"{kind}: grant on captain {cap} but the cursor was {cursor} and no ANCHOR_SKIP for {cursor}: {ln}")
        if not fallback and skipped:
            errors.append(f"{kind}: ANCHOR_SKIP logged for a grant on the cursor captain: {ln}")
        for c, reason in skipped:
            if c == cap:
                errors.append(f"{kind}: captain {c} skipped and granted in one attempt: {ln}")
            if reason == "not-live" and live[c - 1]:
                errors.append(f"{kind}: captain {c} skipped as not-live but live={m.group(4)}{m.group(5)}: {ln}")
            if reason == "placement" and not live[c - 1]:
                errors.append(f"{kind}: captain {c} placement skip while not live: {ln}")
        if not live[cap - 1]:
            errors.append(f"{kind}: grant on a captain that is not live: {ln}")
        if nxt != 3 - cap:
            errors.append(f"{kind}: next={nxt} is not the captain after {cap}: {ln}")
        prev = last.get(kind)
        if prev and prev[1] == (True, True) and live == (True, True) and not fallback and prev[0] != cap:
            handovers[kind] = handovers.get(kind, 0) + 1
        last[kind] = (cap, live)
        expected[kind] = nxt
        seqs.setdefault(kind, []).append(f"{cap}{'*' if fallback else ''}")
    return errors, {k: ",".join(v) for k, v in seqs.items()}, handovers


def exe_identity(argv):
    """`path sha256` of the --exe argument, so the log ties the run to a build."""
    pre = argparse.ArgumentParser(add_help=False)
    pre.add_argument("--exe", type=Path)
    got, _ = pre.parse_known_args(argv)
    if got.exe is None or not got.exe.is_file():
        return "exe=? sha256=?"
    return f"exe={got.exe.resolve()} sha256={hashlib.sha256(got.exe.read_bytes()).hexdigest()}"


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    pre = argparse.ArgumentParser(add_help=False)
    pre.add_argument("--acceptance", action="store_true")
    pre.add_argument("--out", type=Path)
    pre.add_argument("--env", nargs="*", default=[])
    mine, _ = pre.parse_known_args(argv)
    pass_argv = [x for x in argv if x != "--acceptance"]
    if "-h" in argv or "--help" in argv:
        print(__doc__)
    knob = any(item.startswith("PIKMIN_NETPLAY_TEST_COOP_EVENTS=") for item in mine.env)

    run_pair.write_bootstrap = write_bootstrap_m4d
    identity = exe_identity(pass_argv)
    print(f"coop_policy_pair: {identity}")
    rc = run_pair.main(pass_argv)
    if mine.out is None:
        return rc

    out = mine.out.resolve()
    peers = {"host": out / "host" / "run", "join": out / "join" / "peer" / "run"}
    lines = {name: policy_lines(run / "native.log") for name, run in peers.items()}
    ok = rc == 0
    print(f"coop_policy_pair: run_pair exit={rc}")
    for name, run in peers.items():
        log = run / "native.log"
        stage = grep_count(log, "START_STAGE")
        rand = grep_count(log, "[Pikmin Randomizer]")
        tuples = distinct_tuples(run / "hashes.txt")
        print(f"coop_policy_pair: {name} START_STAGE={stage} randomizer_lines={rand} "
              f"distinct_navi_piki_teki_item={tuples} coop_policy_lines={len(lines[name])}")
        if stage < 1 or rand < 1 or tuples < 2:
            print(f"coop_policy_pair: FAIL: {name} shows no gameplay")
            ok = False
    if lines["host"] != lines["join"]:
        print("coop_policy_pair: FAIL: [coop-policy] lines differ between peers")
        for i, (h, j) in enumerate(zip(lines["host"], lines["join"])):
            if h != j:
                print(f"  first difference at #{i}: host={h!r} join={j!r}")
                break
        else:
            print(f"  lengths host={len(lines['host'])} join={len(lines['join'])}")
        ok = False
    events = {name: [ln for ln in ls if ln.startswith("[coop-policy] TEST ")] for name, ls in lines.items()}
    if knob and (not events["host"] or events["host"] != events["join"]):
        print("coop_policy_pair: FAIL: TEST event lines missing or different between peers")
        ok = False
    for ln in lines["host"]:
        print(f"coop_policy_pair: host {ln}")
    errors, seqs, handovers = check_anchors(lines["host"])
    print(f"coop_policy_pair: anchors {seqs} (* = logged fallback) round_robin_handovers_both_live={handovers}")
    for err in errors:
        print(f"coop_policy_pair: FAIL: anchor {err}")
    if errors:
        ok = False
    if mine.acceptance:
        heal = [ln for ln in lines["host"] if ln.startswith("[coop-policy] HEAL captain=2 ")]
        dl = [ln for ln in lines["host"] if re.match(r"\[coop-policy\] DEATHLINK killed=3 p1=0 ", ln)]
        rr = sum(handovers.values())
        print(f"coop_policy_pair: acceptance heal_captain2={len(heal)} round_robin_handovers_both_live={rr} "
              f"deathlink_killed3_p1down={len(dl)}")
        if not heal or not rr or not dl:
            print("coop_policy_pair: FAIL: acceptance lines missing")
            ok = False
    print(f"coop_policy_pair: {identity}")
    print(f"coop_policy_pair: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
