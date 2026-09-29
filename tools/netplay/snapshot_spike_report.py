#!/usr/bin/env python3
"""Summarise an M6a snapshot-spike run (issue #896).

Reads <run>/snapshot_spike.csv (one line per tick, written by
pc_port/netplay/pc_snapshot_spike.cpp), optionally
<run>/snapshot_spike_sites.txt (call-site tables written at exit),
<run>/snapshot_synctest.csv, <run>/snapshot_spike_audit.txt,
<run>/snapshot_spike_ptrscan.txt and <run>/snapshot_spike_gdirty.txt, and
prints:

  * machine load over the run (sys_busy) and the clean-tick filter;
  * p50/p95/p99/max of every per-tick metric: all ticks, live gameplay, and
    split by phase (steady play, day end, loads);
  * histograms, per-slice view and the worst ticks;
  * the #896 budget check (fix round 1):
      - save for the current layout and, with the hot/cold split, for a
        compact region (GetWriteWatch over the touched extents only);
      - restore from real synctest rollbacks per k (--sync), with the
        measurement mode's content-identical one-tick copy-back labelled a
        single-copy lower bound;
      - a window table for N = 1..7 resim ticks: real k-tick restore (drawn
        from the synctest distribution for k = N) + N consecutive resim
        ticks (auth + parseMessages, + save each) + the presented frame and
        its save;
      - the M8 per-frame metric under a rollback-depth model for 150 ms RTT;
      - a least-squares fit of GetWriteWatch cost against calls, MB scanned
        and dirty pages;
  * call sites (region, off-region, unknown frees, malloc audit) symbolised
    with nm against --exe, the pointer scan, the coverage audit, the globals
    dirty-byte classification (--preserve) and the synctest per k.

Engine-free, standard library only (nm comes from the MinGW toolchain).
"""

import argparse
import bisect
import csv
import math
import os
import random
import re
import shutil
import subprocess
from pathlib import Path

IMAGE_BASE = 0x140000000
FRAME_BUDGET = 33.3


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


def col(rows, c):
    return [r[c] for r in rows if isinstance(r.get(c), float) and r[c] >= 0]


def stats_table(rows, cols, title):
    print(f"\n### {title} (n={len(rows)})\n")
    if not rows:
        return
    print("| metric | p50 | p95 | p99 | max | mean |")
    print("|---|---|---|---|---|---|")
    for c in cols:
        vals = col(rows, c)
        if not vals:
            continue
        mean = sum(vals) / len(vals)
        print(f"| {c} | {fmt(pct(vals, 50))} | {fmt(pct(vals, 95))} | {fmt(pct(vals, 99))} | "
              f"{fmt(max(vals))} | {fmt(mean)} |")


def histogram(rows, c, edges, title):
    vals = col(rows, c)
    if not vals:
        return
    print(f"\n{title} ({c}, n={len(vals)}):\n")
    print("| bucket | ticks | share |")
    print("|---|---|---|")
    prev = None
    for e in edges + [float("inf")]:
        lo = prev if prev is not None else float("-inf")
        n = sum(1 for v in vals if lo < v <= e) if prev is not None else sum(1 for v in vals if v <= e)
        label = f"<= {e:g}" if prev is None else (f"{prev:g} .. {e:g}" if e != float("inf") else f"> {prev:g}")
        print(f"| {label} | {n} | {100.0 * n / len(vals):.2f}% |")
        prev = e


def over_day(rows, c, buckets=12):
    if not rows or c not in rows[0]:
        return
    n = len(rows)
    size = max(1, n // buckets)
    print(f"\n{c} over the run ({buckets} slices):\n")
    print("| ticks | live share | p50 | p95 | max |")
    print("|---|---|---|---|---|")
    for i in range(0, n, size):
        part = rows[i:i + size]
        vals = [r[c] for r in part if r[c] >= 0]
        live = sum(1 for r in part if r["live"] >= 1) / len(part)
        if not vals:
            continue
        print(f"| {int(part[0]['tick'])}-{int(part[-1]['tick'])} | {live:.2f} | {fmt(pct(vals, 50))} | "
              f"{fmt(pct(vals, 95))} | {fmt(max(vals))} |")


def worst(rows, c, k=10):
    if not rows:
        return
    print(f"\nWorst {k} ticks by {c}:\n")
    print("| tick | live | phase | " + c + " | frame_ms | auth_ms | rd_total | gd | save_ms | alloc_bytes | sys_busy |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for r in sorted(rows, key=lambda r: -r[c])[:k]:
        print(f"| {int(r['tick'])} | {int(r['live'])} | {int(r.get('phase', -1))} | {fmt(r[c])} | {fmt(r['frame_ms'])} | "
              f"{fmt(r['auth_ms'])} | {int(r['rd_total'])} | {int(r['gd'])} | {fmt(r['save_ms'])} | "
              f"{int(r['alloc_bytes'])} | {fmt(r.get('sys_busy', -1))} |")


def is_load(r):
    return r["rd_total"] > 3000 or r["alloc_bytes"] > 4e6 or r.get("barrier", 0) >= 1


def phase_of(r):
    if r["live"] < 1:
        return "non_live"
    if is_load(r):
        return "load"
    if r.get("phase", 0) >= 1:
        return "day_end"
    return "steady"


# ---------------------------------------------------------------------------
# Budget
# ---------------------------------------------------------------------------
def save_current(r):
    return r["ww_ms"] + r["save_ms"] + r["gcmp_ms"] + r["gsave_ms"]


def save_compact(r):
    return r["ww_hot_ms"] + r["save_ms"] + r["gcmp_ms"] + r["gsave_ms"]


def resim_cost(r, with_done=False):
    c = r["auth_ms"] + r.get("parse_ms", 0.0)
    if with_done:
        c += r.get("done_ms", 0.0)
    return c


def load_sync(paths):
    """Real rollback restores per k from one or more snapshot_synctest.csv."""
    per_k = {}
    rows_all = []
    for p in paths:
        p = Path(p)
        if p.is_dir():
            p = p / "snapshot_synctest.csv"
        if not p.exists():
            continue
        header = None
        with open(p) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if header is None:
                    header = line.split(",")
                    continue
                r = dict(zip(header, line.split(",")))
                r["_src"] = str(p)
                rows_all.append(r)
                per_k.setdefault(int(r["k"]), []).append(float(r["restore_ms"]))
    return per_k, rows_all


def consecutive_windows(rows, n):
    """Yield (i) indices whose rows i-n..i are consecutive ticks."""
    for i in range(n, len(rows)):
        if rows[i]["tick"] - rows[i - n]["tick"] == n:
            yield i


def window_table(rows, restore_k, save_fn, label, with_done=False, seed=896):
    rnd = random.Random(seed)
    print(f"\nWindow model ({label}; resim = auth + parseMessages{' + doneRender' if with_done else ''}; "
          f"restore drawn from the real synctest restores for k = N):\n")
    print("| N | restore p50 / p95 | windows | p50 | p95 | p99 | max | over 33.3 ms |")
    print("|---|---|---|---|---|---|---|---|")
    out = {}
    for n in range(1, 8):
        rk = restore_k.get(n)
        if not rk:
            continue
        ws = []
        for i in consecutive_windows(rows, n):
            resim = sum(resim_cost(rows[j], with_done) + save_fn(rows[j]) for j in range(i - n, i))
            w = rnd.choice(rk) + resim + rows[i]["frame_ms"] + save_fn(rows[i])
            ws.append(w)
        if not ws:
            continue
        over = sum(1 for w in ws if w > FRAME_BUDGET)
        out[n] = ws
        print(f"| {n} | {fmt(pct(rk, 50))} / {fmt(pct(rk, 95))} | {len(ws)} | {fmt(pct(ws, 50))} | {fmt(pct(ws, 95))} | "
              f"{fmt(pct(ws, 99))} | {fmt(max(ws))} | {100.0 * over / len(ws):.2f}% |")
    return out


def depth_model(rows, restore_k, save_fn, label, seed=897):
    """M8 per-frame metric at 150 ms RTT.

    One-way latency L = 75 ms plus up to one tick of send granularity and
    jitter, uniform in [0, 33.3) ms. With local input delay d, the remote
    input for tick T arrives ceil(L / 33.3 - d) ticks late, which is the
    rollback depth when the prediction was wrong. Worst case: every frame
    mispredicts (analog input changes every tick). The frame then costs
    restore_D + D x (resim + save) + frame + save.
    """
    rnd = random.Random(seed)
    print(f"\nM8 per-frame metric, 150 ms RTT ({label}; every frame mispredicts, the worst case):\n")
    print("| input delay d | depth mix | frames | p50 | p95 | p99 | frames over 33.3 ms |")
    print("|---|---|---|---|---|---|---|")
    for d in (1, 2):
        ws = []
        depths = {}
        for i in consecutive_windows(rows, 7):
            lat = 75.0 + rnd.uniform(0.0, 33.3)
            depth = max(0, math.ceil(lat / 33.3 - d))
            depths[depth] = depths.get(depth, 0) + 1
            rk = restore_k.get(depth)
            if depth > 0 and not rk:
                continue
            w = (rnd.choice(rk) if depth > 0 else 0.0)
            w += sum(resim_cost(rows[j]) + save_fn(rows[j]) for j in range(i - depth, i))
            w += rows[i]["frame_ms"] + save_fn(rows[i])
            ws.append(w)
        if not ws:
            continue
        mix = ", ".join(f"D={k}: {100.0 * v / sum(depths.values()):.0f}%" for k, v in sorted(depths.items()))
        over = sum(1 for w in ws if w > FRAME_BUDGET)
        print(f"| {d} | {mix} | {len(ws)} | {fmt(pct(ws, 50))} | {fmt(pct(ws, 95))} | {fmt(pct(ws, 99))} | "
              f"{100.0 * over / len(ws):.2f}% |")


def lstsq3(xs, ys):
    """Least squares y = a*x0 + b*x1 + c*x2 (normal equations, 3x3)."""
    m = [[0.0] * 3 for _ in range(3)]
    v = [0.0] * 3
    for x, y in zip(xs, ys):
        for i in range(3):
            v[i] += x[i] * y
            for j in range(3):
                m[i][j] += x[i] * x[j]
    # Gaussian elimination
    a = [row[:] + [v[i]] for i, row in enumerate(m)]
    for i in range(3):
        piv = max(range(i, 3), key=lambda r: abs(a[r][i]))
        if abs(a[piv][i]) < 1e-12:
            return None
        a[i], a[piv] = a[piv], a[i]
        for r in range(3):
            if r != i:
                f = a[r][i] / a[i][i]
                for c in range(i, 4):
                    a[r][c] -= f * a[i][c]
    return [a[i][3] / a[i][i] for i in range(3)]


def ww_fit(rows):
    if not rows or rows[0].get("ww_hot_calls") is None:
        return
    split = [r for r in rows if r.get("ww_hot_calls", 0) > 0]
    if not split:
        return
    xs, ys = [], []
    for r in split:
        xs.append([r["ww_hot_calls"], r["hot_mb"], r["rd_hot"]])
        ys.append(r["ww_hot_ms"])
        xs.append([r["ww_cold_calls"], r["cold_mb"], r["rd_cold"]])
        ys.append(r["ww_cold_ms"])
    fit = lstsq3(xs, ys)
    print("\n### GetWriteWatch cost fit (MV-2): ms = a x calls + b x MB scanned + c x dirty pages reported\n")
    print("Rows: per tick, the hot pass (touched extents, what a compact region scans) and the cold pass (the rest).\n")
    for name, key in (("hot", "hot"), ("cold", "cold")):
        calls = col(split, f"ww_{key}_calls")
        mb = col(split, f"{key}_mb")
        rd = col(split, f"rd_{key}")
        ms = col(split, f"ww_{key}_ms")
        print(f"- {name}: calls p50 {fmt(pct(calls, 50))}, MB p50 {fmt(pct(mb, 50))}, dirty p50 {fmt(pct(rd, 50))}, "
              f"ms p50 {fmt(pct(ms, 50))} / p95 {fmt(pct(ms, 95))}")
    if fit:
        a, b, c = fit
        print(f"- fit: a = {a * 1000:.2f} us per call, b = {b * 1000:.3f} us per MB, c = {c * 1000:.3f} us per dirty page")
        pred = [a * x[0] + b * x[1] + c * x[2] for x in xs]
        ss_res = sum((y - p) ** 2 for y, p in zip(ys, pred))
        mean = sum(ys) / len(ys)
        ss_tot = sum((y - mean) ** 2 for y in ys) or 1.0
        print(f"- R^2 = {1.0 - ss_res / ss_tot:.3f} over {len(ys)} rows")


def budget(rows, title, restore_k, sync_note):
    if not rows:
        return
    have_split = rows[0].get("ww_hot_ms") is not None and any(r.get("ww_hot_calls", 0) > 0 for r in rows)
    print(f"\n### Budget check (#896): {title} (n={len(rows)})\n")
    print("- save (current layout) = GetWriteWatch over the three zones + undo/shadow copies + globals compare + globals save")
    if have_split:
        print("- save (compact, measured) = GetWriteWatch over the touched extents only (hot pass) + the same copies and globals work")
    saves = [("current", save_current)] + ([("compact", save_compact)] if have_split else [])
    for name, fn in saves:
        v = [fn(r) for r in rows]
        p95 = pct(v, 95)
        print(f"- save {name}: p50 {fmt(pct(v, 50))} / p95 {fmt(p95)} / p99 {fmt(pct(v, 99))} / max {fmt(max(v))} ms "
              f"-> p95 <= 2 ms: {'PASS' if p95 <= 2.0 else 'FAIL'}")
    syn = [r["restore_ms"] + r["restore_ww_ms"] + r.get("grestore_ms", 0.0) for r in rows if r["restore_ms"] > 0]
    if syn:
        print(f"- single-copy lower bound (content-identical one-tick copy-back, measurement mode; NOT a rollback): "
              f"p50 {fmt(pct(syn, 50))} / p95 {fmt(pct(syn, 95))} ms")
    if restore_k:
        print(f"- real rollback restore (synctest{sync_note}), per k:\n")
        print("| k | tests | p50 | p95 | p99 | max | over 4 ms | verdict p95 <= 4 ms |")
        print("|---|---|---|---|---|---|---|---|")
        for k in sorted(restore_k):
            v = restore_k[k]
            over = sum(1 for x in v if x > 4.0)
            print(f"| {k} | {len(v)} | {fmt(pct(v, 50))} | {fmt(pct(v, 95))} | {fmt(pct(v, 99))} | {fmt(max(v))} | "
                  f"{over} ({100.0 * over / len(v):.1f}%) | {'PASS' if pct(v, 95) <= 4.0 else 'FAIL'} |")
    else:
        print("- real rollback restore: no synctest given (--sync); restore verdict undetermined")
    auth = col(rows, "auth_ms")
    parse = col(rows, "parse_ms")
    done = col(rows, "done_ms")
    frame = col(rows, "frame_ms")
    print(f"\n- auth p50 {fmt(pct(auth, 50))} / p95 {fmt(pct(auth, 95))}; parseMessages p50 {fmt(pct(parse, 50))} / "
          f"p95 {fmt(pct(parse, 95))}; doneRender p50 {fmt(pct(done, 50))} / p95 {fmt(pct(done, 95))}; "
          f"frame p50 {fmt(pct(frame, 50))} / p95 {fmt(pct(frame, 95))} ms")
    for n in (7,):
        sums = [sum(resim_cost(rows[j]) for j in range(i - n + 1, i + 1)) for i in consecutive_windows(rows, n - 1)]
        if sums:
            print(f"- measured sum of {n} consecutive resim ticks (auth + parse): p50 {fmt(pct(sums, 50))} / "
                  f"p95 {fmt(pct(sums, 95))} ms")
    if restore_k:
        for name, fn in saves:
            window_table(rows, restore_k, fn, f"save {name}")
        if have_split:
            window_table(rows, restore_k, save_compact, "save compact, conservative", with_done=True)
        for name, fn in saves:
            depth_model(rows, restore_k, fn, f"save {name}")


# ---------------------------------------------------------------------------
# Symbols
# ---------------------------------------------------------------------------
class Symbols:
    def __init__(self, exe, kinds=("T", "t")):
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
            if len(parts) == 3 and parts[1] in kinds:
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

    def name_off(self, rva):
        va = IMAGE_BASE + rva
        i = bisect.bisect_right(self.addrs, va) - 1
        if i < 0:
            return "?"
        return f"{self.name(rva)}+0x{va - self.addrs[i]:x}"


def site_name(sym, rva):
    if rva <= 1:
        return "arena (sys heap, not attributed)" if rva == 1 else "?"
    return sym.name(rva)


def sites(path, sym, top):
    if not path.exists():
        return
    lines = path.read_text(errors="replace").splitlines()
    print("\n### Exit report\n")
    for line in lines:
        if not line.startswith("[m6a] site"):
            print("    " + line)
    site_re = re.compile(r"\[m6a\] site (\w+) cat=(\S+) module=(\S+) rva=0x([0-9a-f]+) count=(\d+) bytes=(\d+)")
    for label in ("off", "unknown", "region", "malloc"):
        entries = []
        for line in lines:
            m = site_re.match(line)
            if m and m.group(1) == label:
                entries.append((m.group(2), m.group(3), int(m.group(4), 16), int(m.group(5)), int(m.group(6))))
        if not entries:
            continue
        print(f"\nTop {top} {label} call sites (return address of the allocation call):\n")
        print("| category | module | function | count | bytes |")
        print("|---|---|---|---|---|")
        for cat, mod, rva, count, nbytes in entries[:top]:
            fn = sym.name(rva) if mod.lower().endswith(".exe") else f"+0x{rva:x}"
            print(f"| {cat} | {mod} | `{fn}` | {count} | {nbytes} |")
        if label in ("off", "malloc"):
            agg = {}
            for cat, mod, rva, count, nbytes in entries:
                fn = sym.name(rva) if mod.lower().endswith(".exe") else mod
                key = (cat, fn)
                c, b = agg.get(key, (0, 0))
                agg[key] = (c + count, b + nbytes)
            print(f"\n{label} by category and function (top {top} by count):\n")
            print("| category | function | count | bytes |")
            print("|---|---|---|---|")
            for (cat, fn), (c, b) in sorted(agg.items(), key=lambda kv: -kv[1][0])[:top]:
                print(f"| {cat} | `{fn}` | {c} | {b} |")
            if label == "malloc":
                sim = [(fn, c, b) for (cat, fn), (c, b) in agg.items() if cat == "main_sim"]
                direct = [(fn, c, b) for fn, c, b in sim if "piki_pc_alloc" not in fn]
                tot_c = sum(c for _, c, _ in direct)
                tot_b = sum(b for _, _, b in direct)
                print(f"\nDirect malloc/calloc/realloc in the SIM domain (main thread, inside idle, no infra scope; "
                      f"piki_pc_alloc's own call excluded): {tot_c} calls, {tot_b} bytes")


def ptrscan(path, sym, dsym, top=25):
    if not path.exists():
        return
    scans = []
    pairs = {}
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("scan "):
            scans.append(line)
        elif line.startswith("pair "):
            kv = dict(p.split("=", 1) for p in line.split()[1:])
            key = (kv["kind"], int(kv["src"], 16), int(kv["dst"], 16))
            pairs[key] = max(pairs.get(key, 0), int(kv["count"]))
    print("\n### Pointer scan (MV-4)\n")
    for s in scans:
        print("    " + s)
    for kind, srcname, dstname in (("region_to_off", "region block (alloc site)", "off-region block (alloc site)"),
                                   ("globals_to_off", "global (symbol)", "off-region block (alloc site)"),
                                   ("off_to_region", "off-region block (alloc site)", "region block (alloc site)")):
        agg = {}
        for (k, src, dst), c in pairs.items():
            if k != kind:
                continue
            s = dsym.name(src) if kind == "globals_to_off" else site_name(sym, src)
            d = site_name(sym, dst)
            agg[(s, d)] = max(agg.get((s, d), 0), c)
        if not agg:
            continue
        print(f"\n{kind}: {srcname} -> {dstname} (max count over scans, top {top}):\n")
        print("| source | target | pointers |")
        print("|---|---|---|")
        for (s, d), c in sorted(agg.items(), key=lambda kv: -kv[1])[:top]:
            print(f"| `{s}` | `{d}` | {c} |")


def audit(path, sym):
    if not path.exists():
        return
    audits = []
    missed = []
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("audit "):
            audits.append(dict(p.split("=", 1) for p in line.split()[1:]))
        elif line.startswith("#missed"):
            missed.append(dict(p.split("=", 1) for p in line.split()[1:]))
    print("\n### Coverage audit (MV-3)\n")
    if not audits:
        print("no audits")
        return
    pages = [int(a["pages"]) for a in audits]
    changed = [int(a["changed"]) for a in audits]
    miss = [int(a["missed"]) for a in audits]
    print(f"- {len(audits)} audits, committed pages hashed p50 {pct(pages, 50):.0f}; pages changed between audits "
          f"p50 {pct(changed, 50):.0f} / max {max(changed)}; changed without a write-watch report: total {sum(miss)}, "
          f"max {max(miss)} in one interval")
    if missed:
        agg = {}
        for m in missed:
            name = site_name(sym, int(m["site_rva"], 16))
            agg[name] = agg.get(name, 0) + 1
        print("- missed pages by owning region block (first 32 per audit):")
        for name, c in sorted(agg.items(), key=lambda kv: -kv[1])[:20]:
            print(f"    {c} `{name}`")


def gdirty(path, dsym, preserve):
    if not path.exists():
        return
    pres = []
    if preserve and Path(preserve).exists():
        for line in Path(preserve).read_text().splitlines():
            if line.startswith("#"):
                continue
            p = line.split()
            if len(p) >= 2:
                pres.append((int(p[0], 16), int(p[1], 16)))
    pres.sort()
    pres_lo = [a for a, _ in pres]

    def preserved(rva):
        i = bisect.bisect_right(pres_lo, rva) - 1
        return i >= 0 and rva < pres[i][1]

    ticks = {}
    for line in path.read_text().splitlines():
        p = line.split()
        if len(p) != 3:
            continue
        ticks.setdefault(int(p[0]), []).append((int(p[1], 16), int(p[2], 16)))
    if not ticks:
        return
    print(f"\n### Dirty globals by symbol (MV-11; {len(ticks)} sample ticks; infra = on the preserve list)\n")
    sym_count = {}
    sim_bytes = infra_bytes = 0
    per_tick = []
    for t, runs in sorted(ticks.items()):
        pages_sim, pages_infra = set(), set()
        for lo, hi in runs:
            name = dsym.name(lo)
            infra = preserved(lo)
            key = (name, "infra" if infra else "sim")
            sym_count[key] = sym_count.get(key, 0) + 1
            if infra:
                infra_bytes += hi - lo
                pages_infra.add(lo >> 12)
            else:
                sim_bytes += hi - lo
                pages_sim.add(lo >> 12)
        per_tick.append((t, len(pages_sim), len(pages_infra), len(pages_sim & pages_infra)))
    print("| tick | pages with sim bytes | pages with infra bytes | pages with both |")
    print("|---|---|---|---|")
    for t, s, i, b in per_tick[:12]:
        print(f"| {t} | {s} | {i} | {b} |")
    print(f"\nChanged bytes over the samples: sim {sim_bytes}, infra {infra_bytes}. Most often changed symbols:\n")
    print("| symbol | class | sample ticks changed |")
    print("|---|---|---|")
    for (name, cls), c in sorted(sym_count.items(), key=lambda kv: -kv[1])[:40]:
        print(f"| `{name}` | {cls} | {c} |")


def synctest(rows, notes, sym, dsym):
    if not rows:
        return
    print(f"\n### Synctest ({len(rows)} tests)\n")
    ks = sorted({int(r["k"]) for r in rows})
    has_new = "audio_bad" in rows[0]
    print("| k | tests | curated match | audio match | region diff (any) | region diff unreported | globals diff | "
          "preserved-byte diff | union region p50 | union globals p50 | restore p50 / p95 ms | region / shadow / globals / ww p50 ms |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for k in ks:
        rk = [r for r in rows if int(r["k"]) == k]
        match = sum(1 for r in rk if r["first_bad"] == "0")
        amatch = sum(1 for r in rk if r.get("audio_bad", "0") == "0")
        rd = [int(r["region_diff_pages"]) for r in rk]
        ru = [int(r.get("region_unreported", 0)) for r in rk]
        gd = [int(r["global_diff_pages"]) for r in rk]
        pd = [int(r.get("global_pres_diff", 0)) for r in rk]
        un = [int(r.get("region_union", 0)) for r in rk]
        gu = [int(r.get("global_union", 0)) for r in rk]
        rms = [float(r["restore_ms"]) for r in rk]
        parts = ""
        if has_new:
            parts = " / ".join(fmt(pct([float(r[c]) for r in rk], 50)) for c in
                               ("restore_region_ms", "restore_shadow_ms", "restore_glob_ms", "restore_ww_ms"))
        print(f"| {k} | {len(rk)} | {match} | {amatch} | p50 {pct(rd, 50):.0f} max {max(rd)} | max {max(ru)} | "
              f"p50 {pct(gd, 50):.0f} max {max(gd)} | p50 {pct(pd, 50):.0f} | {pct(un, 50):.0f} | {pct(gu, 50):.0f} | "
              f"{fmt(pct(rms, 50))} / {fmt(pct(rms, 95))} | {parts} |")
    # residual diffs, symbolised
    rsites, gsyms, psyms = {}, {}, {}
    for n in notes:
        kv = dict(p.split("=", 1) for p in n.split()[1:] if "=" in p)
        if n.startswith("#rdiff") and "site_rva" in kv:
            name = site_name(sym, int(kv["site_rva"], 16))
            bo = kv.get("blk_off", "?")
            if bo != "?" and int(bo, 16) >= 16:
                bo = f"payload+0x{int(bo, 16) - 16:x}"
            key = (name, bo, kv.get("reported", "?"))
            rsites[key] = rsites.get(key, 0) + 1
        elif n.startswith("#gdiff"):
            name = dsym.name_off(int(kv["rva"], 16))
            gsyms[name] = gsyms.get(name, 0) + 1
        elif n.startswith("#pdiff"):
            name = dsym.name_off(int(kv["rva"], 16))
            psyms[name] = psyms.get(name, 0) + 1
    if rsites:
        print("\nRegion pages still differing after re-advance (first 16 per test), by owning block:\n")
        print("| allocation site | offset (payload = after the 16-byte header) | reported by write watch | tests |")
        print("|---|---|---|---|")
        for (name, bo, rep), c in sorted(rsites.items(), key=lambda kv: -kv[1])[:20]:
            print(f"| `{name}` | {bo} | {rep} | {c} |")
    if gsyms:
        print("\nGlobals still differing (non-preserved bytes), by symbol:\n")
        for name, c in sorted(gsyms.items(), key=lambda kv: -kv[1])[:20]:
            print(f"- `{name}`: {c}")
    if psyms:
        print("\nPreserved globals that change during play (not rolled back; review for sim state), by symbol:\n")
        for name, c in sorted(psyms.items(), key=lambda kv: -kv[1])[:60]:
            print(f"- `{name}`: {c}")


def spread(runs, filt):
    """p50/p95 of the key metrics per run, and the spread across runs (MV-6)."""
    keys = [("save current", save_current), ("save compact", save_compact),
            ("auth", lambda r: r["auth_ms"]), ("resim (auth+parse)", resim_cost),
            ("frame", lambda r: r["frame_ms"]), ("ww", lambda r: r["ww_ms"]), ("ww hot", lambda r: r["ww_hot_ms"])]
    print(f"\n### Spread across runs (MV-6; steady live ticks{', sys_busy filter' if filt else ''})\n")
    print("| metric | " + " | ".join(Path(r).parent.parent.name if Path(r).name == 'run' else Path(r).name for r, _ in runs) +
          " | p95 min..max |")
    print("|---|" + "---|" * len(runs) + "---|")
    for name, fn in keys:
        cells, p95s = [], []
        for _, rows in runs:
            try:
                v = [fn(r) for r in rows]
            except (KeyError, TypeError):
                v = []
            if not v:
                cells.append("-")
                continue
            p95s.append(pct(v, 95))
            cells.append(f"{fmt(pct(v, 50))} / {fmt(pct(v, 95))}")
        rng = f"{fmt(min(p95s))}..{fmt(max(p95s))}" if p95s else "-"
        print(f"| {name} | " + " | ".join(cells) + f" | {rng} |")


def steady_rows(path, skip, busy_max):
    rows = load_csv(Path(path) / "snapshot_spike.csv")
    rows = [r for r in rows if r["tick"] > skip and r["live"] >= 1 and r["resim"] < 1]
    rows = [r for r in rows if phase_of(r) == "steady" and r.get("sync_phase", 0) == 0]
    if busy_max is not None:
        rows = [r for r in rows if r.get("sys_busy", -1) < 0 or r["sys_busy"] <= busy_max]
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run", type=Path, help="run dir (holds snapshot_spike.csv)")
    ap.add_argument("--exe", default=None, help="nectar.exe used for the run (symbolises call sites)")
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--skip", type=int, default=0, help="ignore the first N ticks (boot)")
    ap.add_argument("--sync", nargs="*", default=[], help="synctest run dirs or CSVs for the real restore per k")
    ap.add_argument("--also", nargs="*", default=[], help="more run dirs of the same configuration (spread table)")
    ap.add_argument("--busy-max", type=float, default=None,
                    help="drop ticks whose machine busy %% (sys_busy) exceeds this (MV-6)")
    ap.add_argument("--preserve", default=None, help="preserve list (classifies dirty globals as infra)")
    a = ap.parse_args(argv)

    rows = load_csv(a.run / "snapshot_spike.csv")
    rows = [r for r in rows if r["tick"] > a.skip]
    live = [r for r in rows if r["live"] >= 1 and r["resim"] < 1 and r.get("sync_phase", 0) == 0]
    print(f"# M6a snapshot spike report: {a.run}\n")
    print(f"ticks logged {len(rows)}, live gameplay ticks {len(live)} (synctest passes excluded)")
    busy = [r["sys_busy"] for r in rows if r.get("sys_busy", -1) >= 0]
    if busy:
        print(f"\nMachine load (whole-system busy %, sampled every 30 ticks): p50 {pct(busy, 50):.1f} / p95 "
              f"{pct(busy, 95):.1f} / max {max(busy):.1f}")
    if a.busy_max is not None:
        before = len(live)
        live = [r for r in live if r.get("sys_busy", -1) < 0 or r["sys_busy"] <= a.busy_max]
        print(f"busy filter <= {a.busy_max}%: kept {len(live)} of {before} live ticks")
    phases = {}
    for r in live:
        phases.setdefault(phase_of(r), []).append(r)
    print("phases: " + ", ".join(f"{k} {len(v)}" for k, v in sorted(phases.items())))
    cols = ["auth_ms", "idle_ms", "frame_ms", "present_ms", "done_ms", "parse_ms", "retrace_ms", "rd_auth", "rd_post",
            "rd_total", "rd_meta", "rd_arena", "rd_small", "rd_large", "gd", "pikis", "ww_ms", "ww_hot_ms",
            "ww_cold_ms", "ww_hot_calls", "ww_cold_calls", "hot_mb", "cold_mb", "rd_hot", "rd_cold", "save_ms",
            "restore_ms", "restore_ww_ms", "grestore_ms", "gcmp_ms", "gsave_ms", "full_ms", "full_mb", "gfull_ms",
            "ww_late", "conc_r", "conc_g", "allocs", "alloc_bytes", "frees", "free_bytes", "live_blocks", "live_mb",
            "small_hw_mb", "large_hw_mb", "touched_mb", "committed_mb", "off_allocs", "off_bytes",
            "region_frees_off_main", "unknown_frees", "sys_busy"]
    stats_table(rows, cols, "All ticks")
    stats_table(live, cols, "Live gameplay ticks")
    for ph in ("steady", "day_end", "load"):
        stats_table(phases.get(ph, []), cols, f"Live, phase {ph}")
    steady = phases.get("steady", [])
    histogram(steady, "rd_total", [8, 16, 32, 64, 128, 256, 512, 1024, 4096], "Region dirty pages per tick, steady")
    histogram(steady, "gd", [4, 8, 16, 32, 64, 128], "Globals dirty pages per tick, steady")
    histogram(steady, "frame_ms", [2, 4, 8, 16, 33.3], "Whole frame, steady")
    histogram(steady, "auth_ms", [0.5, 1, 2, 4, 8, 16], "Authoritative pass, steady")
    if any(r.get("pikis", 0) > 0 for r in live):
        print("\nDirty pages and auth by field Pikmin count (live, steady):\n")
        print("| pikis | ticks | rd_total p50 | rd_total p95 | auth p50 | auth p95 |")
        print("|---|---|---|---|---|---|")
        buckets = {}
        for r in steady:
            b = int(r.get("pikis", 0)) // 10 * 10
            buckets.setdefault(b, []).append(r)
        for b in sorted(buckets):
            rs = buckets[b]
            if len(rs) < 30:
                continue
            print(f"| {b}-{b + 9} | {len(rs)} | {fmt(pct(col(rs, 'rd_total'), 50))} | {fmt(pct(col(rs, 'rd_total'), 95))} | "
                  f"{fmt(pct(col(rs, 'auth_ms'), 50))} | {fmt(pct(col(rs, 'auth_ms'), 95))} |")
    over_day(rows, "rd_total")
    over_day(rows, "frame_ms")
    over_day(rows, "auth_ms")
    worst(rows, "frame_ms")
    worst(rows, "rd_total")
    worst(live, "save_ms")
    worst(live, "auth_ms")
    ww_fit(steady)
    sync_paths = list(a.sync)
    if (a.run / "snapshot_synctest.csv").exists():
        sync_paths.append(str(a.run / "snapshot_synctest.csv"))
    restore_k, sync_rows = load_sync(sync_paths)
    note = f", {len(sync_rows)} tests from {len(set(r['_src'] for r in sync_rows))} run(s)" if sync_rows else ""
    budget(steady, "steady live gameplay ticks", restore_k, note)
    budget(live, "all live gameplay ticks", restore_k, note)
    sym = Symbols(a.exe)
    dsym = Symbols(a.exe, kinds=("b", "B", "d", "D"))
    sites(a.run / "snapshot_spike_sites.txt", sym, a.top)
    audit(a.run / "snapshot_spike_audit.txt", sym)
    ptrscan(a.run / "snapshot_spike_ptrscan.txt", sym, dsym)
    gdirty(a.run / "snapshot_spike_gdirty.txt", dsym, a.preserve)
    own_sync = a.run / "snapshot_synctest.csv"
    if own_sync.exists():
        rows_s, notes = [], []
        header = None
        for line in own_sync.read_text().splitlines():
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                notes.append(line)
                continue
            if header is None:
                header = line.split(",")
                continue
            rows_s.append(dict(zip(header, line.split(","))))
        synctest(rows_s, notes, sym, dsym)
    if a.also:
        runs = [(str(a.run), steady_rows(a.run, a.skip, a.busy_max))]
        for extra in a.also:
            runs.append((extra, steady_rows(extra, a.skip, a.busy_max)))
        spread(runs, a.busy_max is not None)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
