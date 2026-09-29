#!/usr/bin/env python3
"""Build a crude globals preserve list for the M6a synctest (issue #896).

The spike's synctest restores the exe's .data/.bss bracket. Infrastructure
statics (the GL backend, the audio facade and its mixer state, the Dolphin OS
stubs, timing/presentation, window and settings) must not be rolled back:
their counterparts (GL objects, the audio thread, OS threads) are not.

With LTO the link map no longer says which translation unit owns a symbol, so
this classifies by name instead: a data/bss symbol is preserved when its
identifier (or, for a function-local static, its function's name) appears as
a token in an infrastructure source file and in no simulation source file.
Crude on purpose (the spike measures what breaks; M6b replaces this with a
library split and a linker script).

Output: one "rva_lo rva_hi name" line per preserved symbol (hex), for
PIKMIN_NETPLAY_SNAPSHOT_PRESERVE.
"""

import argparse
import re
import shutil
import subprocess
from pathlib import Path

IMAGE_BASE = 0x140000000

INFRA_GLOBS = [
    "pc_port/gl/*.cpp", "pc_port/gl/*.inc", "pc_port/audio/*.cpp", "pc_port/dolphin_stubs/*.cpp",
    "pc_port/timing/*.cpp", "pc_port/settings/*.cpp", "pc_port/launcher/*.cpp", "pc_port/pc_window*.cpp",
    "pc_port/pc_main.cpp", "pc_port/pc_gpu_preference.cpp", "pc_port/pc_icon.cpp", "pc_port/pc_file_dialog.cpp",
    "pc_port/pc_gyro.cpp", "pc_port/netplay/pc_netplay_session.cpp", "pc_port/netplay/pc_netplay_udp.cpp",
    "pc_port/netplay/pc_netplay_ice.cpp", "pc_port/netplay/pc_netplay_launch*.cpp",
    "pc_port/netplay/pc_netplay_present.cpp", "pc_port/netplay/pc_snapshot_spike.cpp",
    # M6b production snapshot (#896): its own statics are infrastructure (the
    # crowd bootstrap in pc_snapshot_game.cpp is sim and stays out).
    "pc_port/netplay/pc_snapshot.cpp", "pc_port/netplay/pc_snapshot_region.cpp",
    "pc_port/netplay/pc_snapshot_ring.cpp",
    "src/sysDolphin/sysNew.cpp",
]
SIM_GLOBS = ["src/**/*.cpp", "pc_port/*.cpp", "pc_port/netplay/*.cpp", "pc_port/mods/**/*.cpp", "include/**/*.h"]

# M6b (#896): audio facade statics that mirror sim state and must roll back
# with it (decision R-A / C2-2). SeSystem (sim, in the region) keeps Jac event
# handles and branches on Jac_CheckFreeEvents(); with the facade's event table
# preserved, a rollback left the two disagreeing and SeSystem::destroyEvent hit
# its fatal "free events did not grow" check (M6b runs/g1A-foh-a, tick 7030).
# These names are never preserved, whatever file defines them.
SIM_KEEP = {
    "sEvents", "sFreeEvents", "sEventClock",
    "sMenuOrPauseActive", "sMenuActive", "sPauseActive", "sDVDPauseActive", "sEventResumeFrames",
    "sDemoEventPaused", "sCountdownSounds", "sCurrentDemo", "sDemoTimedEvent", "sDemoPartsId",
    "sDemoOnyonCount", "sDemoPartsCount", "sKeepDemoStreamOnFinish", "sDemoWasSkipped", "sDemoJamActive",
    "sPartsFindDemoActive", "sTextDemoActive", "sVoiceRandom",
}

TOKEN = re.compile(r"[A-Za-z_][A-Za-z0-9_]{2,}")


def tokens(files):
    out = set()
    for f in files:
        try:
            out.update(TOKEN.findall(f.read_text(errors="replace")))
        except OSError:
            pass
    return out


def base_names(sym):
    """Identifiers a data symbol can be matched by."""
    names = []
    m = re.match(r"_ZL\d+(\w+?)(\.lto_priv\.\d+)?$", sym)
    if m:
        names.append(m.group(1))
    m = re.match(r"_ZN12_GLOBAL__N_1L?\d+(\w+?)E(\.lto_priv\.\d+)?$", sym)
    if m:
        names.append(m.group(1))
    s = sym.replace("(anonymous namespace)::", "")
    s = re.sub(r"\.lto_priv\.\d+$", "", s)
    s = s.replace("guard variable for ", "").replace("variable for ", "")
    if "::" in s:
        head = s.split("(")[0]
        parts = [p for p in re.split(r"::", head) if p]
        names.extend(parts)
    else:
        names.append(s.split("(")[0])
    return [n for n in names if TOKEN.fullmatch(n)]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True)
    ap.add_argument("--src", required=True, help="native worktree root")
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    root = Path(a.src)
    infra_files = sorted({p for g in INFRA_GLOBS for p in root.glob(g)})
    infra_set = set(infra_files)
    sim_files = sorted({p for g in SIM_GLOBS for p in root.glob(g)} - infra_set)
    infra_tok = tokens(infra_files)
    sim_tok = tokens(sim_files)
    only_infra = infra_tok - sim_tok

    nm = shutil.which("nm") or "C:/msys64/mingw64/bin/nm.exe"
    # PE nm prints no sizes: a symbol's extent is up to the next data symbol.
    syms = {}
    for flags in (["-C"], []):
        text = subprocess.run([nm] + flags + ["--defined-only", a.exe], capture_output=True, text=True,
                              errors="replace").stdout
        for line in text.splitlines():
            parts = line.split(" ", 2)
            if len(parts) != 3 or parts[1] not in ("b", "B", "d", "D"):
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            if addr < IMAGE_BASE:
                continue
            syms.setdefault(addr, []).append(parts[2].strip())
    addrs = sorted(syms)
    lines = []
    total = 0
    for i, addr in enumerate(addrs):
        end = addrs[i + 1] if i + 1 < len(addrs) else addr + 8
        if end - addr > (1 << 20):
            end = addr + 8  # a gap this large is the end of a section, not one object
        names = syms[addr]
        if any(n.startswith("sArenaMemory") for n in names):
            continue  # the static arena is outside the bracket when the spike runs
        if any(bn in SIM_KEEP for n in names for bn in base_names(n)):
            continue
        if any(bn in only_infra for n in names for bn in base_names(n)):
            lines.append((addr - IMAGE_BASE, end - IMAGE_BASE, names[0]))
    uniq = sorted(set(lines))
    with open(a.out, "w") as f:
        f.write(f"# preserve list for {a.exe}: {len(uniq)} symbols, infra files {len(infra_files)}, sim files {len(sim_files)}\n")
        for lo, hi, sym in uniq:
            f.write(f"{lo:x} {hi:x} {sym}\n")
            total += hi - lo
    print(f"{len(uniq)} symbols, {total} bytes -> {a.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
