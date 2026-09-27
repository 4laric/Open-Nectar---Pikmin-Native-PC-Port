"""FP determinism gate for netplay builds (issue #879, lane m2d).

Usage: fp_gate.py <exe>

Runs objdump over the linked game executable and FAILS if it finds:
  - an x87 transcendental instruction (fsin/fcos/fsincos/fpatan/fptan/
    f2xm1/fyl2x/fyl2xp1) -- mingw-w64 libm implements its transcendentals
    with these, and their microcode results can differ between CPU vendors;
  - an FMA instruction (vfmadd*/vfmsub*/vfnmadd*) -- the netplay build uses
    -ffp-contract=off, so none may appear (the x86-64 baseline has no FMA);
  - an import of tan/sin/cos/atan/exp/log/pow (any float/double/long-double
    or atan2/sincos spelling) from msvcrt/ucrt -- e.g. tan resolved to the
    system DLL, which varies with the Windows version.

Allowed x87 exceptions (explicit symbol + reason; empty today: after the
software libm there are no transcendental hits left):
  ALLOW = {}  # (symbol, insn) -> reason

Exit 0 when the exe is clean, 1 with diagnostics otherwise. Each hit is
attributed to its symbol via `nm` so the report names the libm function.
"""

import bisect
import re
import shutil
import subprocess
import sys
from pathlib import Path

X87 = {"fsin", "fcos", "fsincos", "fpatan", "fptan", "f2xm1", "fyl2x", "fyl2xp1"}

# (symbol, insn) -> reason. Documents the exceptional hits the gate lets
# through, e.g. CRT startup helpers. Empty: no exception is needed.
ALLOW = {}

FMA_RE = re.compile(r"\bvfmadd\w*|\bvfmsub\w*|\bvfnmadd\w*|\bvfnmsub\w*", re.IGNORECASE)

MATH_IMPORT_STEMS = {
    "tan", "tanf", "sin", "sinf", "cos", "cosf", "sincos", "sincosf",
    "atan", "atanf", "atan2", "atan2f", "asin", "asinf", "acos", "acosf",
    "exp", "expf", "log", "logf", "pow", "powf",
}

CRT_DLLS = ("msvcrt", "ucrt", "api-ms-win-crt")


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    if p.returncode != 0:
        raise SystemExit(f"fp_gate: command failed: {' '.join(cmd)}\n{p.stderr[:2000]}")
    return p.stdout


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    exe = Path(argv[1])
    if not exe.is_file():
        print(f"fp_gate: FAIL: no such file: {exe}")
        return 1
    objdump = shutil.which("objdump")
    if not objdump:
        print("fp_gate: FAIL: objdump not on PATH (msys2 mingw64 provides it)")
        return 1

    dis = run([objdump, "-d", str(exe)])
    syms = []
    try:
        nm_out = run([shutil.which("nm") or "nm", str(exe)])
        for line in nm_out.splitlines():
            parts = line.split()
            if len(parts) == 3 and parts[1] in "TtWwRrDd":
                try:
                    syms.append((int(parts[0], 16), parts[2]))
                except ValueError:
                    pass
        syms.sort()
    except SystemExit:
        syms = []
    addrs = [a for a, _ in syms]
    names = [n for _, n in syms]

    def owner(addr):
        if not addrs:
            return "?"
        i = bisect.bisect_right(addrs, addr) - 1
        return names[i] if i >= 0 else "?"

    fails = []
    counts = {}
    addr_re = re.compile(r"^\s*([0-9a-fA-F]+):\s+(?:[0-9a-fA-F]{2} )+\s*(\S+)")
    for line in dis.splitlines():
        m = addr_re.match(line)
        if not m:
            continue
        insn = m.group(2).lower()
        if insn in X87 or FMA_RE.match(insn):
            addr = int(m.group(1), 16)
            sym = owner(addr)
            if (sym, insn) in ALLOW:
                continue
            kind = "x87-transcendental" if insn in X87 else "fma"
            fails.append(f"{kind}: {insn} at {m.group(1)} in <{sym}>")
            counts[insn] = counts.get(insn, 0) + 1

    imports = run([objdump, "-p", str(exe)])
    cur_dll = ""
    for line in imports.splitlines():
        dm = re.match(r"\s*DLL Name:\s*(\S+)", line)
        if dm:
            cur_dll = dm.group(1).lower()
            continue
        parts = line.split()
        # objdump -p import rows: vma, ordinal/hint, type?, name
        if len(parts) >= 4 and cur_dll:
            name = parts[-1].strip().lower().lstrip("_")
            # strip stdcall decorations (@N) and C++ mangling guard
            name = re.sub(r"@\d+$", "", name)
            is_crt = cur_dll.startswith(CRT_DLLS)
            if is_crt and name in MATH_IMPORT_STEMS:
                fails.append(f"msvcrt-import: {parts[-1].strip()} from {cur_dll}")

    if fails:
        print(f"fp_gate: FAIL: {exe}")
        print(f"fp_gate: {len(fails)} forbidden reference(s):")
        for f in fails[:50]:
            print(f"  {f}")
        if len(fails) > 50:
            print(f"  ... and {len(fails) - 50} more")
        if counts:
            print("fp_gate: insn counts: " + ", ".join(f"{k}={v}" for k, v in sorted(counts.items())))
        return 1
    print(f"fp_gate: PASS: {exe} (no x87 transcendentals, no FMA, no msvcrt math imports)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
