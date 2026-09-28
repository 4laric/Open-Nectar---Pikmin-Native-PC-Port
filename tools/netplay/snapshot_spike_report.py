#!/usr/bin/env python3
"""Summarise an M6a snapshot-spike run (issue #896).

Reads <run>/snapshot_spike.csv (one line per tick, written by
pc_port/netplay/pc_snapshot_spike.cpp), optionally
<run>/snapshot_spike_sites.txt (call-site tables written at exit) and
<run>/snapshot_synctest.csv, and prints:

  * p50/p95/p99/max of every per-tick metric, over all ticks and over live
    gameplay ticks only (naviMgr present);
  * a coarse histogram of the key metrics over the run;
  * the worst ticks by frame cost, dirty pages and save cost;
  * the #896 budget check (save p95 <= 2 ms, restore p95 <= 4 ms,
    7 resim ticks + 1 presented frame <= 33.3 ms), both from percentiles
    and from a sliding 7-tick window;
  * the call-site tables symbolised with nm against --exe;
  * synctest results when present.

Engine-free, standard library only (nm comes from the MinGW toolchain).
"""

import argparse
import bisect
import csv
import math
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

IMAGE_BASE = 0x140000000


def pct(values, p):
    if not values:
        return float("nan")
    s = sorted(values)
    k = (len(s) - 1) * p / 100.0
    lo = math.floor(k)
    hi = math.ceil(k)
    if lo == hi:
        return s[int(k)]
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def load_csv(path):
    rows = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            out = {}
            for k, v in row.items():
                try:
                    out[k] = float(v)
                except (TypeError, ValueError):
                    out[k] = v
            rows.append(out)
    return rows


def fmt(v):
    if isinstance(v, float) and math.isnan(v):
        return "nan"
    if abs(v) >= 1000:
        return f"{v:.0f}"
    if abs(v) >= 10:
        return f"{v:.1f}"
    return f"{v:.3f}"


def stats_table(rows, cols, title):
    print(f"\n### {title} (n={len(rows)})\n")
    print("| metric | p50 | p95 | p99 | max | mean |")
    print("|---|---|---|---|---|---|")
    for c in cols:
        vals = [r[c] for r in rows if isinstance(r.get(c), float) and r[c] >= 0]
        if not vals:
            continue
        mean = sum(vals) / len(vals)
        print(f"| {c} | {fmt(pct(vals, 50))} | {fmt(pct(vals, 95))} | {fmt(pct(vals, 99))} | "
              f"{fmt(max(vals))} | {fmt(mean)} |")


def histogram(rows, col, edges, title):
    vals = [r[col] for r in rows if isinstance(r.get(col), float) and r[col] >= 0]
    if not vals:
        return
    print(f"\n{title} ({col}, n={len(vals)}):\n")
    print("| bucket | ticks | share |")
    print("|---|---|---|")
    prev = None
    for e in edges + [float("inf")]:
        lo = prev if prev is not None else float("-inf")
        n = sum(1 for v in vals if lo < v <= e) if prev is not None else sum(1 for v in vals if v <= e)
        label = f"<= {e:g}" if prev is None else (f"{prev:g} .. {e:g}" if e != float("inf") else f"> {prev:g}")
        print(f"| {label} | {n} | {100.0 * n / len(vals):.2f}% |")
        prev = e


def over_day(rows, col, buckets=12):
    """Percentiles of col per slice of the run, to show how it moves over the day."""
    if not rows:
        return
    n = len(rows)
    size = max(1, n // buckets)
    print(f"\n{col} over the run ({buckets} slices):\n")
    print("| ticks | live share | p50 | p95 | max |")
    print("|---|---|---|---|---|")
    for i in range(0, n, size):
        part = rows[i:i + size]
        vals = [r[col] for r in part if r[col] >= 0]
        live = sum(1 for r in part if r["live"] >= 1) / len(part)
        if not vals:
            continue
        print(f"| {int(part[0]['tick'])}-{int(part[-1]['tick'])} | {live:.2f} | {fmt(pct(vals, 50))} | "
              f"{fmt(pct(vals, 95))} | {fmt(max(vals))} |")


def worst(rows, col, k=10):
    print(f"\nWorst {k} ticks by {col}:\n")
    print("| tick | live | " + col + " | frame_ms | auth_ms | rd_total | gd | save_ms | alloc_bytes | free_bytes | small_hw_mb | large_hw_mb |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for r in sorted(rows, key=lambda r: -r[col])[:k]:
        print(f"| {int(r['tick'])} | {int(r['live'])} | {fmt(r[col])} | {fmt(r['frame_ms'])} | {fmt(r['auth_ms'])} | "
              f"{int(r['rd_total'])} | {int(r['gd'])} | {fmt(r['save_ms'])} | {int(r['alloc_bytes'])} | "
              f"{int(r['free_bytes'])} | {fmt(r['small_hw_mb'])} | {fmt(r['large_hw_mb'])} |")


def budget(rows, title):
    if not rows:
        return
    save = [r["ww_ms"] + r["save_ms"] + r["gcmp_ms"] + r["gsave_ms"] for r in rows]
    restore = [r["restore_ms"] + r["restore_ww_ms"] + r["gsave_ms"] for r in rows]
    auth = [r["auth_ms"] for r in rows]
    frame = [r["frame_ms"] for r in rows]
    print(f"\n### Budget check (#896): {title} (n={len(rows)})\n")
    print("save = GetWriteWatch (both calls) + region undo/shadow copy + globals compare + globals save")
    print("restore = region copy-back + write-watch reset + globals copy-back (estimated as the globals save cost)")
    print("resim tick = authoritative pass + save; presented frame = whole frame (frame_ms)\n")
    sp95, rp95 = pct(save, 95), pct(restore, 95)
    print(f"- save p50 {fmt(pct(save, 50))} / p95 {fmt(sp95)} / p99 {fmt(pct(save, 99))} / max {fmt(max(save))} ms "
          f"-> budget p95 <= 2 ms: {'PASS' if sp95 <= 2.0 else 'FAIL'}")
    print(f"- restore p50 {fmt(pct(restore, 50))} / p95 {fmt(rp95)} / p99 {fmt(pct(restore, 99))} / max {fmt(max(restore))} ms "
          f"-> budget p95 <= 4 ms: {'PASS' if rp95 <= 4.0 else 'FAIL'}")
    resim = [a + s for a, s in zip(auth, save)]
    pctl = rp95 + 7 * pct(resim, 95) + pct(frame, 95)
    print(f"- percentile model: restore p95 + 7 x (auth + save) p95 + frame p95 = {fmt(rp95)} + 7 x {fmt(pct(resim, 95))} "
          f"+ {fmt(pct(frame, 95))} = {fmt(pctl)} ms -> <= 33.3 ms: {'PASS' if pctl <= 33.3 else 'FAIL'}")
    window = []
    for i in range(6, len(rows)):
        w = restore[i] + sum(resim[i - 6:i + 1]) + frame[i]
        window.append(w)
    if window:
        over = sum(1 for w in window if w > 33.3)
        print(f"- sliding 7-tick window (restore + 7 real consecutive resim ticks + that frame): p50 {fmt(pct(window, 50))} / "
              f"p95 {fmt(pct(window, 95))} / p99 {fmt(pct(window, 99))} / max {fmt(max(window))} ms; "
              f"{over} of {len(window)} windows ({100.0 * over / len(window):.2f}%) exceed 33.3 ms")


class Symbols:
    def __init__(self, exe):
        self.addrs = []
        self.names = []
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
        va = IMAGE_BASE + rva
        i = bisect.bisect_right(self.addrs, va) - 1
        if i < 0:
            return "?"
        n = self.names[i]
        return n if len(n) < 110 else n[:107] + "..."


def sites(path, exe, top):
    if not path.exists():
        return
    sym = Symbols(exe)
    lines = path.read_text(errors="replace").splitlines()
    print("\n### Exit report\n")
    for line in lines:
        if not line.startswith("[m6a] site"):
            print("    " + line)
    site_re = re.compile(r"\[m6a\] site (\w+) cat=(\S+) module=(\S+) rva=0x([0-9a-f]+) count=(\d+) bytes=(\d+)")
    for label in ("off", "unknown", "region"):
        entries = []
        for line in lines:
            m = site_re.match(line)
            if m and m.group(1) == label:
                entries.append((m.group(2), m.group(3), int(m.group(4), 16), int(m.group(5)), int(m.group(6))))
        if not entries:
            continue
        print(f"\nTop {top} {label}-region allocation call sites (return address of operator new):\n")
        print("| category | module | function | count | bytes |")
        print("|---|---|---|---|---|")
        for cat, mod, rva, count, nbytes in entries[:top]:
            fn = sym.name(rva) if mod.lower().endswith(".exe") else f"+0x{rva:x}"
            print(f"| {cat} | {mod} | `{fn}` | {count} | {nbytes} |")
        # aggregate by category x function for the off-region table
        if label == "off":
            agg = {}
            for cat, mod, rva, count, nbytes in entries:
                fn = sym.name(rva) if mod.lower().endswith(".exe") else mod
                key = (cat, fn)
                c, b = agg.get(key, (0, 0))
                agg[key] = (c + count, b + nbytes)
            print(f"\nOff-region by category and function (top {top} by count):\n")
            print("| category | function | count | bytes |")
            print("|---|---|---|---|")
            for (cat, fn), (c, b) in sorted(agg.items(), key=lambda kv: -kv[1][0])[:top]:
                print(f"| {cat} | `{fn}` | {c} | {b} |")


def synctest(path):
    if not path.exists():
        return
    rows = []
    notes = []
    with open(path) as f:
        header = None
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                notes.append(line)
                continue
            if header is None:
                header = line.split(",")
                continue
            rows.append(dict(zip(header, line.split(","))))
    print(f"\n### Synctest ({path.name})\n")
    if not rows:
        print("no completed tests")
        return
    n = len(rows)
    bad = [r for r in rows if r["first_bad"] != "0"]
    ks = sorted({r["k"] for r in rows})
    print(f"- k={','.join(ks)}: {n} tests, {n - len(bad)} matched, {len(bad)} mismatched "
          f"({100.0 * len(bad) / n:.1f}% mismatch)")
    names = ["total", "navi", "piki", "teki", "item", "world", "rng", "rand"]
    cols = {}
    for r in bad:
        m = int(r["bad_mask"], 16)
        key = "+".join(names[i] for i in range(8) if m & (1 << i))
        cols[key] = cols.get(key, 0) + 1
    if cols:
        print("- first mismatching columns: " + ", ".join(f"{k}: {v}" for k, v in sorted(cols.items(), key=lambda kv: -kv[1])))
    fb = {}
    for r in bad:
        fb[r["first_bad"]] = fb.get(r["first_bad"], 0) + 1
    if fb:
        print("- first mismatching step: " + ", ".join(f"+{k}: {v}" for k, v in sorted(fb.items(), key=lambda kv: int(kv[0]))))
    rd = [int(r["region_diff_pages"]) for r in rows]
    gd = [int(r["global_diff_pages"]) for r in rows]
    rms = [float(r["restore_ms"]) for r in rows]
    rp = [int(r["restored_pages"]) for r in rows]
    print(f"- region pages differing after re-advance: p50 {pct(rd, 50):.0f} p95 {pct(rd, 95):.0f} max {max(rd)}; "
          f"globals pages: p50 {pct(gd, 50):.0f} p95 {pct(gd, 95):.0f} max {max(gd)}")
    print(f"- rollback restore of k ticks: p50 {pct(rms, 50):.3f} ms p95 {pct(rms, 95):.3f} ms max {max(rms):.3f} ms; "
          f"pages p50 {pct(rp, 50):.0f} p95 {pct(rp, 95):.0f} max {max(rp)}")
    if notes:
        print(f"- first diff notes ({len(notes)} total):")
        for line in notes[:20]:
            print("    " + line)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run", type=Path, help="run dir (holds snapshot_spike.csv)")
    ap.add_argument("--exe", default=None, help="nectar.exe used for the run (symbolises call sites)")
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--skip", type=int, default=0, help="ignore the first N ticks (boot)")
    a = ap.parse_args(argv)

    rows = load_csv(a.run / "snapshot_spike.csv")
    rows = [r for r in rows if r["tick"] > a.skip]
    live = [r for r in rows if r["live"] >= 1 and r["resim"] < 1]
    print(f"# M6a snapshot spike report: {a.run}\n")
    print(f"ticks logged {len(rows)}, live gameplay ticks {len(live)}")
    cols = ["auth_ms", "idle_ms", "frame_ms", "rd_auth", "rd_post", "rd_total", "rd_meta", "rd_arena", "rd_small",
            "rd_large", "gd", "ww_ms", "save_ms", "restore_ms", "restore_ww_ms", "gcmp_ms", "gsave_ms", "full_ms",
            "full_mb", "gfull_ms", "allocs", "alloc_bytes", "frees", "free_bytes", "live_blocks", "live_mb",
            "small_hw_mb", "large_hw_mb", "touched_mb", "off_allocs", "off_bytes", "region_frees_off_main",
            "unknown_frees"]
    stats_table(rows, cols, "All ticks")
    stats_table(live, cols, "Live gameplay ticks")
    histogram(live, "rd_total", [8, 16, 32, 64, 128, 256, 512, 1024, 4096], "Region dirty pages per tick, live")
    histogram(live, "gd", [4, 8, 16, 32, 64, 128], "Globals dirty pages per tick, live")
    histogram(live, "frame_ms", [2, 4, 8, 16, 33.3], "Whole frame, live")
    histogram(live, "auth_ms", [0.5, 1, 2, 4, 8, 16], "Authoritative pass, live")
    over_day(rows, "rd_total")
    over_day(rows, "frame_ms")
    over_day(rows, "auth_ms")
    worst(rows, "frame_ms")
    worst(rows, "rd_total")
    worst(live, "save_ms")
    worst(live, "auth_ms")
    budget(live, "live gameplay ticks")
    budget(rows, "all ticks")
    sites(a.run / "snapshot_spike_sites.txt", a.exe, a.top)
    synctest(a.run / "snapshot_synctest.csv")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
