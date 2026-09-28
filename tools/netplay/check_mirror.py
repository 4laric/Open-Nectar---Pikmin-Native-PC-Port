"""Netplay M4 lane B1 (issue #885): check a pair's host journals against the
client's mirror-events.txt.

    check_mirror.py --host-run H --join-run J [--session-json F]
                    --root-mirror <root worktree with randomizer/netplay_mirror.py>

Checks (exit 0 only when every one holds):
  1. every line of J/mirror-events.txt parses with the root reference parser
     randomizer.netplay_mirror.parse_mirror_line (imported read-only through
     sys.path), the file ends with a newline, and frames never decrease;
  2. the host has no mirror-events.txt and the join has none of checks.txt,
     deaths.txt, emperor.txt, benefits-used.txt;
  3. host checks.txt lines are unique and equal the slots of the host's
     `[Pikmin Randomizer] CHECK <slot> <name>` log lines;
  4. the CHECKED names equal the host CHECK names plus the host CHECK_APPLIED
     names, each exactly once;
  5. the last DEATHS equals deathsBase (F's pikmin_deaths, else 0) plus the
     host deaths.txt line count, and the last deaths.txt line equals its line
     count (no DEATHS lines when there is no deaths.txt);
  6. the last DEATHLINK (0 when absent) equals the DEATHLINK value of the
     host's final state.txt (when the state line carries one);
  7. EMPEROR is present exactly when host emperor.txt exists;
  8. the RECEIVED lines equal F's received list in index order (none without
     F).

The pure checks live in verify(); tools/netplay/selftest.py drives them with
synthetic run dirs and a local stand-in parser.
"""

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

CHECK_RE = re.compile(r"\[Pikmin Randomizer\] CHECK (\d+) (.+)$")
APPLIED_RE = re.compile(r"\[Pikmin Randomizer\] CHECK_APPLIED (\d+) (.+)$")
JOURNALS = ("checks.txt", "deaths.txt", "emperor.txt", "benefits-used.txt")


def load_root_parser(root):
    root = Path(root).resolve()
    if not (root / "randomizer" / "netplay_mirror.py").exists():
        raise SystemExit(f"check_mirror: {root} has no randomizer/netplay_mirror.py")
    sys.path.insert(0, str(root))
    from randomizer.netplay_mirror import parse_mirror_line  # noqa: E402 (read-only import)
    return parse_mirror_line


def host_log_checks(text):
    """(CHECK names by slot, CHECK_APPLIED names) from a host native log."""
    checks, applied = [], []
    for ln in text.splitlines():
        m = APPLIED_RE.search(ln)
        if m:
            applied.append((int(m.group(1)), m.group(2).rstrip("\r")))
            continue
        m = CHECK_RE.search(ln)
        if m:
            checks.append((int(m.group(1)), m.group(2).rstrip("\r")))
    return checks, applied


def state_deathlink(text):
    """DEATHLINK value of a PIKMIN_STATE line, or None when it has none."""
    words = text.split()
    for i, w in enumerate(words[:-1]):
        if w == "DEATHLINK":
            try:
                return int(words[i + 1])
            except ValueError:
                return None
    return None


def read_text(path):
    try:
        return Path(path).read_text(errors="replace")
    except OSError:
        return None


def verify(host_run, join_run, parse_line, session=None):
    """Returns a list of failure strings (empty when everything matches)."""
    host_run, join_run = Path(host_run), Path(join_run)
    errors = []
    base = 0
    received = []
    if session is not None:
        base = int(session.get("pikmin_deaths", 0))
        received = list(session.get("received", []))

    # 1. grammar + monotonic frames
    events = []
    mpath = join_run / "mirror-events.txt"
    raw = mpath.read_bytes() if mpath.exists() else b""
    if raw and not raw.endswith(b"\n"):
        errors.append("mirror-events.txt does not end with a newline")
    last_frame = -1
    for n, bline in enumerate(raw.split(b"\n")[:-1] if raw else [], 1):
        try:
            line = bline.decode("ascii")
        except UnicodeDecodeError:
            errors.append(f"mirror line {n}: not ASCII")
            continue
        try:
            frame, tag, args = parse_line(line)
        except ValueError as exc:
            errors.append(f"mirror line {n}: {exc}: {line!r}")
            continue
        if frame < last_frame:
            errors.append(f"mirror line {n}: frame {frame} < previous {last_frame}")
        last_frame = frame
        events.append((frame, tag, args))

    # 2. which peer wrote what
    if (host_run / "mirror-events.txt").exists():
        errors.append("host has a mirror-events.txt")
    for name in JOURNALS:
        if (join_run / name).exists():
            errors.append(f"join has {name}")

    # 3. host checks.txt == host CHECK slots, unique
    log = read_text(host_run / "native.log") or ""
    checks, applied = host_log_checks(log)
    ctext = read_text(host_run / "checks.txt")
    clines = [ln.strip() for ln in (ctext or "").splitlines() if ln.strip()]
    if len(clines) != len(set(clines)):
        errors.append(f"host checks.txt has duplicate lines: {clines}")
    slots = sorted(str(s) for s, _ in checks)
    if sorted(clines) != slots:
        errors.append(f"host checks.txt {sorted(clines)} != CHECK log slots {slots}")

    # 4. CHECKED == CHECK + CHECK_APPLIED names, each exactly once
    checked = Counter(a[0] for _f, t, a in events if t == "CHECKED")
    want = Counter([nm for _s, nm in checks] + [nm for _s, nm in applied])
    if checked != want:
        errors.append(f"CHECKED {dict(checked)} != host CHECK+CHECK_APPLIED {dict(want)}")
    if any(v != 1 for v in checked.values()):
        errors.append("a CHECKED name appears more than once")

    # 5. DEATHS
    dtext = read_text(host_run / "deaths.txt")
    dlines = [ln.strip() for ln in (dtext or "").splitlines() if ln.strip()]
    deaths = [a[0] for _f, t, a in events if t == "DEATHS"]
    if dlines:
        if dlines[-1] != str(len(dlines)):
            errors.append(f"deaths.txt last line {dlines[-1]} != line count {len(dlines)}")
        if not deaths or deaths[-1] != base + len(dlines):
            errors.append(f"last DEATHS {deaths[-1] if deaths else None} != base {base} + "
                          f"{len(dlines)} deaths.txt lines")
    elif deaths:
        errors.append(f"DEATHS lines {deaths} without a host deaths.txt")

    # 6. DEATHLINK
    links = [a[0] for _f, t, a in events if t == "DEATHLINK"]
    final_link = state_deathlink(read_text(host_run / "state.txt") or "")
    if final_link is not None:
        got = links[-1] if links else 0
        if got != final_link:
            errors.append(f"last DEATHLINK {got} != host final state.txt DEATHLINK {final_link}")
    elif links:
        errors.append(f"DEATHLINK lines {links} but the host state has no DEATHLINK")

    # 7. EMPEROR
    emperor = sum(1 for _f, t, _a in events if t == "EMPEROR")
    has_file = (host_run / "emperor.txt").exists()
    if (emperor > 0) != has_file or emperor > 1:
        errors.append(f"EMPEROR lines {emperor} vs host emperor.txt exists={has_file}")

    # 8. RECEIVED
    rec = [a for _f, t, a in events if t == "RECEIVED"]
    want_rec = [(i, item) for i, item in enumerate(received)]
    if [tuple(r) for r in rec] != want_rec:
        errors.append(f"RECEIVED {rec} != session received {want_rec}")

    return errors, Counter(t for _f, t, _a in events)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host-run", type=Path, required=True)
    ap.add_argument("--join-run", type=Path, required=True)
    ap.add_argument("--session-json", type=Path, default=None)
    ap.add_argument("--root-mirror", type=Path, required=True)
    a = ap.parse_args(argv)
    parse_line = load_root_parser(a.root_mirror)
    session = json.loads(a.session_json.read_text()) if a.session_json else None
    errors, tags = verify(a.host_run, a.join_run, parse_line, session)
    print(f"check_mirror: mirror events by tag: {dict(sorted(tags.items()))}")
    for e in errors:
        print(f"check_mirror: FAIL: {e}")
    print(f"check_mirror: {'PASS' if not errors else 'FAIL'}")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
