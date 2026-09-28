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

With --acceptance it also requires a `HEAL captain=2`, an ANCHOR kind whose
captains alternate, and a `DEATHLINK killed=3 p1=0` line.

All run_pair.py options pass through unchanged (see run_pair.py --help).
"""

import argparse
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


def alternating_anchor(lines):
    """First ANCHOR kind with a 1 -> 2 or 2 -> 1 hand-over between consecutive
    grants of that kind (a failed placement legitimately repeats a captain,
    so the whole sequence need not alternate). Returns (kind, sequences)."""
    per_kind = {}
    for ln in lines:
        m = re.search(r"ANCHOR kind=(\S+) captain=(\d)", ln)
        if m:
            per_kind.setdefault(m.group(1), []).append(int(m.group(2)))
    for kind, caps in per_kind.items():
        if any(a != b for a, b in zip(caps, caps[1:])):
            return kind, per_kind
    return None, per_kind


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
    if mine.acceptance:
        heal = [ln for ln in lines["host"] if ln.startswith("[coop-policy] HEAL captain=2 ")]
        kind, caps = alternating_anchor(lines["host"])
        dl = [ln for ln in lines["host"] if re.match(r"\[coop-policy\] DEATHLINK killed=3 p1=0 ", ln)]
        print(f"coop_policy_pair: acceptance heal_captain2={len(heal)} alternating_anchor={kind}:{caps} "
              f"deathlink_killed3_p1down={len(dl)}")
        if not heal or not kind or not dl:
            print("coop_policy_pair: FAIL: acceptance lines missing")
            ok = False
    print(f"coop_policy_pair: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
