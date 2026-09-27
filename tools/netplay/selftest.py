"""Tool-only self test for the netplay harness scripts (ctest target
netplay_replay_selftest). No game is launched.

1. Generates a small PKNI file with gen_inputs.py, checks the header, the
   tick count and that Start/Y are never pressed.
2. Writes two identical synthetic hash logs, compares them (expect exit 0).
3. Mutates one sub-hash column, compares (expect exit 1 naming the column).
4. Truncates one log, compares (expect exit 1).
"""

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
GEN = HERE / "gen_inputs.py"
CMP = HERE / "compare_hashes.py"
PY = sys.executable

FORBIDDEN = 0x0800 | 0x1000  # Y | Start


def run_gen(ticks, seed, out, extra=()):
    r = subprocess.run(
        [PY, str(GEN), "--ticks", str(ticks), "--seed", str(seed), "--out", str(out), *extra],
        capture_output=True, text=True,
    )
    assert r.returncode == 0, f"gen_inputs failed: {r.stderr}"


def read_pkni(path, expect_version=2, expect_rec=56, expect_pad=14):
    with open(path, "rb") as f:
        blob = f.read()
    assert blob[:4] == b"PKNI", f"bad magic: {blob[:4]!r}"
    version, pads, rec = struct.unpack_from("<HHH", blob, 4)
    assert (version, pads, rec) == (expect_version, 4, expect_rec), (version, pads, rec)
    body = blob[10:]
    assert len(body) % expect_rec == 0, len(body)
    nticks = len(body) // expect_rec
    for i in range(nticks):
        for p in range(4):
            off = i * expect_rec + p * expect_pad
            (buttons,) = struct.unpack_from("<H", body, off)
            assert not buttons & FORBIDDEN, f"tick {i} pad {p}: menu button {buttons:#x}"
            if expect_version == 2:
                (flags,) = struct.unpack_from("<B", body, off + 13)
                assert flags == 0, f"tick {i} pad {p}: flags must be 0, got {flags}"
    return nticks


def run_cmp(a, b):
    return subprocess.run([PY, str(CMP), str(a), str(b)], capture_output=True, text=True)


def main():
    failures = 0

    def check(cond, what):
        nonlocal failures
        print(("PASS " if cond else "FAIL ") + what)
        if not cond:
            failures += 1

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        gen_file = tmp / "inputs.pkni"
        run_gen(200, 7, gen_file)
        check(read_pkni(gen_file) == 200, "gen_inputs writes 200 parseable v2 ticks, no Start/Y")

        # Deterministic: same seed regenerates byte-identical output.
        gen_file2 = tmp / "inputs2.pkni"
        run_gen(200, 7, gen_file2)
        check(gen_file.read_bytes() == gen_file2.read_bytes(), "gen_inputs is seed-deterministic")

        # v2 yaw varies slowly and pads disagree; --v1 keeps the M1 format.
        with open(gen_file, "rb") as f:
            blob = f.read()
        yaws0 = [struct.unpack_from("<H", blob, 10 + p * 14 + 11)[0] for p in range(4)]
        check(any(y != 0 for y in yaws0), "v2 carries nonzero yaw")
        gen_v1 = tmp / "inputs_v1.pkni"
        run_gen(200, 7, gen_v1, extra=("--v1",))
        check(read_pkni(gen_v1, expect_version=1, expect_rec=44, expect_pad=11) == 200,
              "--v1 writes 200 parseable v1 ticks")

        base = []
        for t in range(1, 51):
            base.append(
                f"{t} {'ab' * 8} {'11' * 8} {'22' * 8} {'33' * 8} "
                f"{'44' * 8} {'55' * 8} {'66' * 8}"
            )
        ha = tmp / "a.hash"
        hb = tmp / "b.hash"
        ha.write_text("\n".join(base) + "\n")
        hb.write_text("\n".join(base) + "\n")
        r = run_cmp(ha, hb)
        check(r.returncode == 0 and "identical: 50 ticks" in r.stdout, "compare identical logs exits 0")

        mutated = list(base)
        row = mutated[29].split()
        row[3] = "99" * 8  # piki column at tick 30
        mutated[29] = " ".join(row)
        hc = tmp / "c.hash"
        hc.write_text("\n".join(mutated) + "\n")
        r = run_cmp(ha, hc)
        check(
            r.returncode == 1 and "tick 30" in r.stdout and "piki" in r.stdout,
            "compare reports first divergent tick and column",
        )

        hd = tmp / "d.hash"
        hd.write_text("\n".join(base[:40]) + "\n")
        r = run_cmp(ha, hd)
        check(r.returncode == 1 and "mismatch" in r.stdout, "compare reports length mismatch")

    if failures:
        print(f"selftest: {failures} failure(s)")
        return 1
    print("selftest: all checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
