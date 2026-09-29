#!/usr/bin/env python3
"""Report for the netplay M6b production snapshot (issue #896).

usage: snapshot_report.py RUN_DIR [RUN_DIR ...] [--exe EXE] [--label NAME]
                          [--block 300] [--boot 400] [--sites 30]

Reads, per run dir, snapshot.csv (per tick, written with
PIKMIN_NETPLAY_SNAPSHOT_MEASURE=1), snapshot_synctest.csv (per test) and
snapshot_report.txt (the exit report), and prints markdown.

Rules (M6b item 0; decision section 1b; review MV2-3/6/7):
  * ONE filter rule, stated at the top and applied to every table, next to
    the unfiltered rows. n is printed on every row;
  * every p95 carries a bootstrap 95% CI: a block bootstrap (blocks of
    --block consecutive rows) for per-tick series, an iid bootstrap for
    per-test data (restores);
  * restores come from ONE source per report: the production-path
    rollbacks of the synctest driver (pc_snapshot_restore through the ring),
    i.e. snapshot_synctest.csv. There is no measure-mode restore in the
    production snapshot, so nothing here is a lower bound;
  * machine state columns: busy_other (system busy minus this process),
    the share of ticks whose main thread ran on the highest efficiency class
    (P-core), and the calibration spin (a fixed QPC-timed loop per tick).

Standard library only (nm comes from the MinGW toolchain, for --exe).
"""

import argparse
import bisect
import csv
import os
import random
import shutil
import subprocess
import sys
from pathlib import Path

IMAGE_BASE = 0x140000000  # the snapshot exe links with --disable-dynamicbase

FILTER_RULE = ("steady = live play (live=1, phase=0: no day end or title), no barrier tick, "
               "no re-simulated tick (resim=0), no open synctest window (sync_phase=0) and dirty <= 3000 pages "
               "(stage-load ticks excluded). For restores: the test's anchor tick is steady. "
               "Unfiltered = every saved tick / every test.")


def pct(values, p):
    s = sorted(values)
    if not s:
        return float("nan")
    i = (len(s) - 1) * p / 100.0
    lo = int(i)
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (i - lo)


def load_csv(path):
    rows = []
    if not path.exists():
        return rows
    with open(path, newline="", errors="replace") as fh:
        rd = csv.reader(line for line in fh if not line.startswith("#"))
        try:
            hdr = next(rd)
        except StopIteration:
            return rows
        for rec in rd:
            if len(rec) != len(hdr):
                continue
            d = {}
            for k, v in zip(hdr, rec):
                try:
                    d[k] = float(v)
                except ValueError:
                    d[k] = v
            rows.append(d)
    return rows


def steady(r):
    return (r.get("live", 0) >= 1 and r.get("phase", 0) == 0 and r.get("barrier", 0) == 0
            and r.get("resim", 0) == 0 and r.get("sync_phase", 0) == 0 and r.get("dirty", 0) <= 3000)


def block_ci(xs, p, block, reps, rng):
    n = len(xs)
    if n < 2:
        return float("nan"), float("nan")
    blocks = [xs[i:i + block] for i in range(0, n, block)]
    est = []
    for _ in range(reps):
        s = []
        for _ in range(len(blocks)):
            s.extend(rng.choice(blocks))
        est.append(pct(s, p))
    return pct(est, 2.5), pct(est, 97.5)


def iid_ci(xs, p, reps, rng):
    if len(xs) < 2:
        return float("nan"), float("nan")
    est = [pct([rng.choice(xs) for _ in xs], p) for _ in range(reps)]
    return pct(est, 2.5), pct(est, 97.5)


def f2(v):
    return "-" if v != v else f"{v:.2f}"


def f3(v):
    return "-" if v != v else f"{v:.3f}"


class Symbols:
    def __init__(self, exe):
        self.addrs, self.names = [], []
        nm = shutil.which("nm") or "C:/msys64/mingw64/bin/nm.exe"
        if not exe or not os.path.exists(exe):
            return
        try:
            out = subprocess.run([nm, "-C", "--defined-only", exe], capture_output=True, text=True,
                                 errors="replace", check=False).stdout
        except OSError:
            return
        pairs = []
        for line in out.splitlines():
            parts = line.split(" ", 2)
            if len(parts) == 3 and parts[1] in ("T", "t"):
                try:
                    pairs.append((int(parts[0], 16), parts[2]))
                except ValueError:
                    pass
        pairs.sort()
        self.addrs = [a for a, _ in pairs]
        self.names = [n for _, n in pairs]

    def name(self, rva):
        i = bisect.bisect_right(self.addrs, IMAGE_BASE + rva) - 1
        if i < 0:
            return "?"
        n = self.names[i]
        return n if len(n) < 100 else n[:97] + "..."


def machine_cols(rows):
    busy = [r["busy_other"] for r in rows if r.get("busy_other", -1) >= 0]
    busy_all = [r["busy_all"] for r in rows if r.get("busy_all", -1) >= 0]
    cls = [r["eff_class"] for r in rows if r.get("eff_class", -1) >= 0]
    top = max(cls) if cls else -1
    pshare = (100.0 * sum(1 for c in cls if c == top) / len(cls)) if cls else float("nan")
    calib = [r["calib_us"] for r in rows if r.get("calib_us", 0) > 0]
    return {
        "busy_other_p50": pct(busy, 50), "busy_all_p50": pct(busy_all, 50), "pcore_pct": pshare,
        "calib_p50": pct(calib, 50), "calib_p95": pct(calib, 95),
    }


def save_rows(rows):
    # every tick that saved into the ring (the first barrier starts the ring)
    out = []
    started = False
    for r in rows:
        if r.get("barrier", 0) >= 1:
            started = True
        if started:
            out.append(r)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("runs", nargs="+", type=Path)
    ap.add_argument("--exe", default=None)
    ap.add_argument("--label", default=None)
    ap.add_argument("--block", type=int, default=300)
    ap.add_argument("--boot", type=int, default=400)
    ap.add_argument("--sites", type=int, default=30)
    ap.add_argument("--save-budget", type=float, default=2.0)
    ap.add_argument("--restore-budget", type=float, default=4.0)
    a = ap.parse_args(argv)
    rng = random.Random(896)

    print(f"# Snapshot report{': ' + a.label if a.label else ''}\n")
    print(f"**Filter rule (every table):** {FILTER_RULE}\n")
    print(f"**CIs:** bootstrap 95%, block bootstrap with {a.block}-row blocks for per-tick series, iid for per-test "
          f"data, {a.boot} resamples.\n")
    print("**Restore source (one per report):** production-path rollbacks of the in-process synctest "
          "(`snapshot_synctest.csv`: pc_snapshot_restore = collect + ring restore + globals + ResetWriteWatch).\n")

    pooled = {"save_f": [], "save_u": [], "rest_f": {}, "rest_u": {}}
    save_lines, rest_lines, comp_lines, mach_lines, mem_lines = [], [], [], [], []
    for run in a.runs:
        name = run.parent.parent.name if run.name == "run" else run.name
        rows = load_csv(run / "snapshot.csv")
        srows = save_rows(rows)
        filt = [r for r in srows if steady(r)]
        mc = machine_cols(srows)
        mcf = machine_cols(filt)
        for label, sel, key in (("filtered", filt, "save_f"), ("unfiltered", srows, "save_u")):
            xs = [r["save_ms"] for r in sel]
            lo, hi = block_ci(xs, 95, a.block, a.boot, rng)
            p95 = pct(xs, 95)
            verdict = ("PASS" if hi <= a.save_budget else "MARGINAL" if p95 <= a.save_budget else "FAIL") if xs else "-"
            m = mcf if label == "filtered" else mc
            save_lines.append(f"| {name} | {label} | {len(xs)} | {f2(pct(xs, 50))} | {f2(p95)} [{f2(lo)}, {f2(hi)}] | "
                              f"{f2(max(xs) if xs else float('nan'))} | {f2(m['busy_other_p50'])} | {f2(m['pcore_pct'])} | "
                              f"{f2(m['calib_p50'])} | {verdict} |")
            pooled[key].extend(xs)
        for c in ("ww_ms", "copy_ms", "gcmp_ms", "gsave_ms", "dirty", "gd", "auth_ms", "frame_ms", "guard_ms"):
            xs = [r[c] for r in filt if c in r]
            comp_lines.append(f"| {name} | {c} | {len(xs)} | {f3(pct(xs, 50))} | {f3(pct(xs, 95))} | {f3(max(xs) if xs else float('nan'))} |")
        pk = [r["pikis"] for r in filt]
        mach_lines.append(f"| {name} | {len(srows)} | {f2(mc['busy_all_p50'])} | {f2(mc['busy_other_p50'])} | "
                          f"{f2(mc['pcore_pct'])} | {f2(mc['calib_p50'])} / {f2(mc['calib_p95'])} | "
                          f"{f2(pct(pk, 50))} / {f2(pct(pk, 95))} |")
        scan = [r["scan_mb"] for r in rows if "scan_mb" in r]
        ring = [r["ring_mb"] for r in rows if "ring_mb" in r]
        heap = [r["heap_mb"] for r in rows if "heap_mb" in r]
        tch = [r["touched_mb"] for r in rows if r.get("touched_mb", -1) >= 0]
        bars = sum(1 for r in rows if r.get("barrier", 0) >= 1)
        reb = [r["rebase_ms"] for r in rows if r.get("barrier", 0) >= 1]
        mem_lines.append(f"| {name} | {f2(max(scan) if scan else float('nan'))} | {f2(max(heap) if heap else float('nan'))} | "
                         f"{f2(max(tch) if tch else float('nan'))} | {f2(max(ring) if ring else float('nan'))} | {bars} | "
                         f"{f2(pct(reb, 50))} / {f2(max(reb) if reb else float('nan'))} |")
        # restores
        tests = load_csv(run / "snapshot_synctest.csv")
        tickrow = {int(r["tick"]): r for r in rows}
        for label, key in (("filtered", "rest_f"), ("unfiltered", "rest_u")):
            sel = tests if label == "unfiltered" else [t for t in tests if steady(tickrow.get(int(t["anchor"]), {}))]
            ks = sorted(set(int(t["k"]) for t in sel))
            for k in ks + ["all"]:
                xs = [t["restore_ms"] for t in sel if k == "all" or int(t["k"]) == k]
                if not xs:
                    continue
                lo, hi = iid_ci(xs, 95, a.boot, rng)
                p95 = pct(xs, 95)
                verdict = "PASS" if hi <= a.restore_budget else "MARGINAL" if p95 <= a.restore_budget else "FAIL"
                bad = sum(1 for t in sel if (k == "all" or int(t["k"]) == k) and t["first_bad"] > 0)
                un = [t["union_pages"] for t in sel if k == "all" or int(t["k"]) == k]
                rest_lines.append(f"| {name} | {label} | {k} | {len(xs)} | {f2(pct(xs, 50))} | {f2(p95)} [{f2(lo)}, {f2(hi)}] | "
                                  f"{f2(max(xs))} | {sum(1 for x in xs if x > a.restore_budget)} | {f2(pct(un, 50))} | "
                                  f"{bad} | {verdict} |")
                pooled[key].setdefault(k, []).extend(xs)

    print("## Save (ms per tick: GetWriteWatch + ring copy + globals compare + globals undo)\n")
    print("| run | rows | n | p50 | p95 [95% CI] | max | busy_other p50 | P-core % | calib us p50 | vs 2 ms |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for line in save_lines:
        print(line)
    for label, key in (("filtered", "save_f"), ("unfiltered", "save_u")):
        xs = pooled[key]
        if xs:
            lo, hi = block_ci(xs, 95, a.block, a.boot, rng)
            print(f"| **pooled** | {label} | {len(xs)} | {f2(pct(xs, 50))} | {f2(pct(xs, 95))} [{f2(lo)}, {f2(hi)}] | "
                  f"{f2(max(xs))} | | | | |")
    print("\n## Save components (filtered rows)\n")
    print("| run | column | n | p50 | p95 | max |")
    print("|---|---|---|---|---|---|")
    for line in comp_lines:
        print(line)
    print("\n## Restore (ms, production path, per k)\n")
    print("| run | rows | k | n | p50 | p95 [95% CI] | max | over budget | union pages p50 | hash mismatches | vs 4 ms |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for line in rest_lines:
        print(line)
    for label, key in (("filtered", "rest_f"), ("unfiltered", "rest_u")):
        for k in sorted((k for k in pooled[key] if k != "all")) + (["all"] if "all" in pooled[key] else []):
            xs = pooled[key][k]
            lo, hi = iid_ci(xs, 95, a.boot, rng)
            print(f"| **pooled** | {label} | {k} | {len(xs)} | {f2(pct(xs, 50))} | {f2(pct(xs, 95))} [{f2(lo)}, {f2(hi)}] | "
                  f"{f2(max(xs))} | {sum(1 for x in xs if x > a.restore_budget)} | | | |")
    print("\n## Machine state (all saved ticks)\n")
    print("| run | ticks | busy_all p50 | busy_other p50 | P-core % | calib us p50 / p95 | field Pikmin p50 / p95 (filtered) |")
    print("|---|---|---|---|---|---|---|")
    for line in mach_lines:
        print(line)
    print("\n## Memory\n")
    print("| run | scanned MB max | heap used MB max | touched MB max | ring committed MB max | barriers | rebaseline ms p50 / max |")
    print("|---|---|---|---|---|---|---|")
    for line in mem_lines:
        print(line)

    sym = Symbols(a.exe)
    for run in a.runs:
        rep = run / "snapshot_report.txt"
        if not rep.exists():
            continue
        name = run.parent.parent.name if run.name == "run" else run.name
        print(f"\n## Exit report: {name}\n")
        sites = {"outside_sim": [], "region": []}
        for line in rep.read_text(errors="replace").splitlines():
            if line.startswith("[snapshot] site "):
                parts = dict(p.split("=", 1) for p in line.split()[3:] if "=" in p)
                label = line.split()[2]
                sites.setdefault(label, []).append(parts)
            elif line.startswith("[snapshot]"):
                print("    " + line)
        for label in ("outside_sim", "region"):
            rowsl = sites.get(label, [])[: a.sites]
            if not rowsl:
                continue
            print(f"\nTop {len(rowsl)} {label} operator new call sites (return address):\n")
            print("| category | function | count | bytes |")
            print("|---|---|---|---|")
            for s in rowsl:
                rva = int(s.get("rva", "0"), 16)
                fn = sym.name(rva) if s.get("module", "").lower().startswith("nectar") else s.get("module", "?")
                print(f"| {s.get('cat')} | `{fn}` | {s.get('count')} | {s.get('bytes')} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
