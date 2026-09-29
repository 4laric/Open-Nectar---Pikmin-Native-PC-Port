"""Tool-only self test for the netplay harness scripts (ctest target
netplay_replay_selftest). No game is launched.

1. Generates a small PKNI file with gen_inputs.py, checks the header, the
   tick count and that Start/Y are never pressed.
2. Writes two identical synthetic hash logs, compares them (expect exit 0).
3. Mutates one sub-hash column, compares (expect exit 1 naming the column).
4. Truncates one log, compares (expect exit 1).
5. M4 lane B1: check_mirror.verify() on synthetic host/join run dirs with a
   local stand-in for the root parse_mirror_line (the real check imports the
   root reference parser; the selftest must not depend on a root worktree).
6. Gapfix C (issue #885): run_replay's refresher survives PermissionError
   on os.replace; run_pair's sim-frame keyed state scripts (parser, cursor,
   hash-log tick reader); coop_policy_pair's --acceptance defaults and its
   wall-clock DeathLink guard; b1_pairs' DeathLink apply-frame reader.
"""

import hashlib
import struct
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import check_mirror  # noqa: E402  (M4 lane B1 pure checks)
import run_pair  # noqa: E402  (gapfix C: frame-keyed state scripts)
import run_replay  # noqa: E402  (gapfix C: refresher retry)
import coop_policy_pair  # noqa: E402  (gapfix C: acceptance defaults)
import b1_pairs  # noqa: E402  (gapfix C: DeathLink apply frames)
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


def stand_in_parse(line):
    """Minimal stand-in for root randomizer.netplay_mirror.parse_mirror_line:
    the same line rules (printable ASCII, single spaces, canonical numbers,
    known tags and arities). Raises ValueError like the reference."""
    if not line or len(line) > 256 or any(ord(c) < 0x20 or ord(c) > 0x7E for c in line):
        raise ValueError("bad line")
    parts = line.split(" ")
    if any(x == "" for x in parts) or len(parts) < 3 or parts[0] != "FRAME":
        raise ValueError("bad separators")

    def num(t):
        if not t.isdigit() or (len(t) > 1 and t[0] == "0"):
            raise ValueError("non-canonical number")
        return int(t)

    frame, tag, rest = num(parts[1]), parts[2], parts[3:]
    if tag == "EMPEROR" and not rest:
        return frame, tag, ()
    if tag == "CHECKED" and rest:
        return frame, tag, (" ".join(rest),)
    if tag in ("DEATHS", "DEATHLINK") and len(rest) == 1:
        return frame, tag, (num(rest[0]),)
    if tag == "RECEIVED" and len(rest) == 2:
        return frame, tag, (num(rest[0]), num(rest[1]))
    raise ValueError("unknown tag or arity")


def mirror_case(tmp, name, host_files, join_files):
    host = tmp / name / "host" / "run"
    join = tmp / name / "join" / "peer" / "run"
    host.mkdir(parents=True)
    join.mkdir(parents=True)
    for d, files in ((host, host_files), (join, join_files)):
        for fname, data in files.items():
            (d / fname).write_bytes(data.encode("ascii") if isinstance(data, str) else data)
    return host, join


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
        # M2c review M4: --v1 must reproduce the M1 input stream, not just the
        # format. Golden SHA-256 of the M1 generator's output for
        # --ticks 200 --seed 7 (base 7b21c90d2 tools/netplay/gen_inputs.py).
        M1_GOLDEN_200_S7 = "8796ad2372d53ddd3236105cd09b02a8bdb52f3d10039e787394fd7788bf0cfe"
        check(hashlib.sha256(gen_v1.read_bytes()).hexdigest() == M1_GOLDEN_200_S7,
              "--v1 output is byte-identical to the M1 stream (M4)")

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

        # m6: 9-column (with rand) logs compare, and a mid-file width change
        # is an error rather than a misaligned compare.
        base9 = [ln + f" {'77' * 8}" for ln in base]
        h9a = tmp / "e.hash"
        h9b = tmp / "f.hash"
        h9a.write_text("\n".join(base9) + "\n")
        h9b.write_text("\n".join(base9) + "\n")
        r = run_cmp(h9a, h9b)
        check(r.returncode == 0 and "identical: 50 ticks" in r.stdout,
              "compare identical 9-column logs exits 0")
        mut9 = list(base9)
        row9 = mut9[10].split()
        row9[8] = "88" * 8  # rand column at tick 11
        mut9[10] = " ".join(row9)
        h9c = tmp / "g.hash"
        h9c.write_text("\n".join(mut9) + "\n")
        r = run_cmp(h9a, h9c)
        check(r.returncode == 1 and "tick 11" in r.stdout and "rand" in r.stdout,
              "compare reports divergent rand column")
        ragged = list(base9)
        ragged[20] = " ".join(ragged[20].split()[:8])  # 8-col row mid-file
        h9d = tmp / "h.hash"
        h9d.write_text("\n".join(ragged) + "\n")
        r = run_cmp(h9a, h9d)
        check(r.returncode == 1 and "expected 9 columns" in r.stdout,
              "compare rejects mid-file width change")

        # M4 lane B1: check_mirror pure checks.
        good_log = (
            "[Pikmin Randomizer] START_STAGE 1 day=2\n"
            "[Pikmin Randomizer] CHECK 47 Pikmin: Forest of Hope Landing\n"
            "[Pikmin Randomizer] CHECK_APPLIED 57 Some Streamed Check\n"
            "[Pikmin Randomizer] DEATHLINK_TOTAL 2\n"
            "[Pikmin Randomizer] CHECK 30 Population: 20 total Pikmin\n"
        )
        good_mirror = (
            "FRAME 40 RECEIVED 0 8\nFRAME 40 RECEIVED 1 1\n"
            "FRAME 120 CHECKED Pikmin: Forest of Hope Landing\n"
            "FRAME 300 CHECKED Some Streamed Check\n"
            "FRAME 300 DEATHLINK 2\nFRAME 310 DEATHS 3\n"
            "FRAME 400 CHECKED Population: 20 total Pikmin\nFRAME 500 DEATHS 4\n"
        )
        session = {"received": [8, 1], "pikmin_deaths": 2}
        host_files = {
            "native.log": good_log,
            "checks.txt": "47\n30\n",
            "deaths.txt": "1\n2\n",
            "state.txt": "PIKMIN_STATE 9 tok 1 0 127 0 CHECKS 0 DEATHLINK 2 END\n",
        }
        h, j = mirror_case(tmp, "m_ok", host_files, {"mirror-events.txt": good_mirror})
        errs, tags = check_mirror.verify(h, j, stand_in_parse, session)
        check(errs == [] and tags["CHECKED"] == 3 and tags["RECEIVED"] == 2,
              f"check_mirror accepts a consistent pair ({errs})")

        def bad_case(name, what, host_over=None, join_over=None, sess=session):
            hf = dict(host_files)
            hf.update(host_over or {})
            jf = {"mirror-events.txt": good_mirror}
            jf.update(join_over or {})
            for k in [k for k, v in hf.items() if v is None]:
                del hf[k]
            for k in [k for k, v in jf.items() if v is None]:
                del jf[k]
            hh, jj = mirror_case(tmp, name, hf, jf)
            e, _t = check_mirror.verify(hh, jj, stand_in_parse, sess)
            check(bool(e), f"check_mirror rejects: {what}")

        bad_case("m_crlf", "CRLF line (the parser rejects a carriage return)",
                 join_over={"mirror-events.txt": good_mirror.replace("\n", "\r\n")})
        bad_case("m_frames", "decreasing frames",
                 join_over={"mirror-events.txt": good_mirror.replace("FRAME 500", "FRAME 5")})
        bad_case("m_hostmirror", "a host mirror-events.txt",
                 host_over={"mirror-events.txt": "FRAME 1 EMPEROR\n"})
        bad_case("m_joinchecks", "a join checks.txt", join_over={"checks.txt": "47\n"})
        bad_case("m_dupcheck", "duplicate checks.txt lines", host_over={"checks.txt": "47\n30\n47\n"})
        bad_case("m_missingcheck", "checks.txt missing a CHECK slot", host_over={"checks.txt": "47\n"})
        bad_case("m_dupchecked", "a CHECKED name twice",
                 join_over={"mirror-events.txt": good_mirror + "FRAME 600 CHECKED Some Streamed Check\n"})
        bad_case("m_deaths", "DEATHS not base + deaths.txt lines", host_over={"deaths.txt": "1\n"})
        bad_case("m_deathlink", "DEATHLINK != the host's applied DEATHLINK_TOTAL",
                 host_over={"native.log": good_log.replace("DEATHLINK_TOTAL 2", "DEATHLINK_TOTAL 3")})
        bad_case("m_deathlink_none", "DEATHLINK lines with no applied total on the host",
                 host_over={"native.log": good_log.replace("[Pikmin Randomizer] DEATHLINK_TOTAL 2\n", "")})
        bad_case("m_deaths_retract", "a DEATHS total that decreases (fatal for the M4c ingest)",
                 join_over={"mirror-events.txt": good_mirror.replace("FRAME 310 DEATHS 3", "FRAME 310 DEATHS 5")})
        bad_case("m_deathlink_retract", "a DEATHLINK total that decreases",
                 host_over={"native.log": good_log.replace("DEATHLINK_TOTAL 2\n",
                                                           "DEATHLINK_TOTAL 2\n[Pikmin Randomizer] DEATHLINK_TOTAL 1\n")},
                 join_over={"mirror-events.txt": good_mirror.replace("FRAME 300 DEATHLINK 2\n",
                                                                     "FRAME 300 DEATHLINK 2\nFRAME 305 DEATHLINK 1\n")})
        bad_case("m_received_gap", "RECEIVED indices out of order",
                 join_over={"mirror-events.txt": good_mirror.replace("FRAME 40 RECEIVED 0 8\nFRAME 40 RECEIVED 1 1\n",
                                                                     "FRAME 40 RECEIVED 1 1\nFRAME 40 RECEIVED 0 8\n")})
        bad_case("m_emperor", "emperor.txt without EMPEROR", host_over={"emperor.txt": "EMPEROR_DEFEATED a b\n"})
        bad_case("m_received", "RECEIVED not the session list", sess={"received": [8, 2], "pikmin_deaths": 2})
        bad_case("m_nosession", "RECEIVED lines without a session.json", sess=None)

        # Gapfix C: run_replay's refresher never dies on PermissionError.
        rr = tmp / "refresh"
        rr.mkdir()
        fails = {"left": 3}

        def flaky_replace(src, dst):
            if fails["left"] > 0:
                fails["left"] -= 1
                raise PermissionError(13, "Access is denied (simulated WinError 5)")
            Path(src).replace(dst)

        done = threading.Event()
        errs = {"n": 0, "last": ""}
        th = threading.Thread(target=run_replay.refresh_loop, args=(rr, "STATE\n", done, errs, flaky_replace, 0.01))
        th.start()
        deadline = time.time() + 5
        while time.time() < deadline and not (rr / "state.txt").exists():
            time.sleep(0.01)
        alive = th.is_alive()
        done.set()
        th.join(5)
        check(alive and errs["n"] == 3 and "PermissionError" in errs["last"]
              and (rr / "state.txt").read_text() == "STATE\n",
              f"run_replay refresher retries PermissionError and keeps state.txt fresh ({errs})")

        # Gapfix C: run_pair sim-frame keyed state scripts.
        ss = tmp / "states-frames.txt"
        ss.write_text("# comment\n0 PIKMIN_STATE a {TOKEN} END\nf600 PIKMIN_STATE b END\n"
                      "30 PIKMIN_STATE c END\nf1200 PIKMIN_STATE d END\n")
        sched = run_pair.load_state_script(ss, "tok")
        check([(k, v) for k, v, _l in sched] == [("t", 0.0), ("f", 600), ("t", 30.0), ("f", 1200)]
              and sched[0][2] == "PIKMIN_STATE a tok END\n",
              "run_pair loads f<tick> keys in file order")
        step = run_pair.sched_step
        check(step(sched, 0, 100.0, None) == 0, "frame key waits while the hash log is empty")
        check(step(sched, 0, 100.0, 599) == 0, "frame key waits below its tick, however long")
        check(step(sched, 0, 10.0, 600) == 1, "frame key fires at its tick; the next time key still waits")
        check(step(sched, 0, 31.0, 600) == 2 and step(sched, 0, 31.0, 5000) == 3,
              "entries apply in file order once each key is reached")
        check(step(sched, 0, 1000.0, 300) == 0, "a later time key never jumps a pending frame key")
        legacy = tmp / "states-legacy.txt"
        legacy.write_text("20 PIKMIN_STATE b END\n0 PIKMIN_STATE a END\n5 PIKMIN_STATE c END\n")
        ls = run_pair.load_state_script(legacy, "tok")
        check([v for _k, v, _l in ls] == [0.0, 5.0, 20.0] and step(ls, 0, 6.0, None) == 1
              and step(ls, 0, 25.0, None) == 2,
              "time-only scripts are sorted and step as before")
        for name, text in (("bad-order", "0 PIKMIN_STATE a END\nf900 PIKMIN_STATE b END\nf600 PIKMIN_STATE c END\n"),
                           ("bad-key", "0 PIKMIN_STATE a END\nfx PIKMIN_STATE b END\n"),
                           ("bad-first", "f0 PIKMIN_STATE a END\n")):
            bad = tmp / f"states-{name}.txt"
            bad.write_text(text)
            try:
                run_pair.load_state_script(bad, "tok")
                rejected = False
            except SystemExit:
                rejected = True
            check(rejected, f"run_pair rejects a {name} frame-keyed script")
        hl = tmp / "hashes-tail.txt"
        check(run_pair.hash_log_tick(hl) is None, "hash_log_tick: missing log -> None")
        rows = "".join(f"{i} {'ab' * 8} {'11' * 8}\n" for i in range(1, 301))
        hl.write_text(rows + "301 abab")  # a torn last line is ignored
        cache = {}
        check(run_pair.hash_log_tick(hl, cache) == 300 and run_pair.hash_log_tick(hl, cache) == 300,
              "hash_log_tick reads the last complete line")

        # Gapfix C: coop_policy_pair --acceptance defaults and guard.
        acc = tmp / "acc"
        argv = coop_policy_pair.acceptance_argv(["--exe", "x", "--env", "A=1"], acc, ["A=1"])
        st = acc / "acceptance-states.txt"
        ev = acc / "acceptance-events.txt"
        check(argv[argv.index("--host-state-script") + 1] == str(st)
              and argv[argv.index("--join-state-script") + 1] == str(st)
              and argv[argv.index("--env") + 1] == f"PIKMIN_NETPLAY_TEST_COOP_EVENTS={ev}"
              and "A=1" in argv and ev.read_text().splitlines()[1:] == ["40 HP 2 0.5", "1000 DOWN 1"]
              and argv[argv.index("--profile") + 1] == "impact-day2",
              "coop_policy_pair --acceptance passes the built-in schedule, events and profile")
        argv2 = coop_policy_pair.acceptance_argv(["--profile", "navel-day2"], tmp / "acc2", [])
        check(argv2.count("--profile") == 1 and argv2[1] == "navel-day2",
              "coop_policy_pair --acceptance keeps an explicit --profile")
        check(coop_policy_pair.wall_clock_deathlink_steps(st) == [],
              "the built-in acceptance schedule keys every DeathLink rise to a frame")
        wall = tmp / "states-wall.txt"
        wall.write_text("0 PIKMIN_STATE 9 t 1 0 127 0 CHECKS 0 DEATHLINK 0 END\n"
                        "40 PIKMIN_STATE 9 t 1 0 127 0 CHECKS 0 DEATHLINK 1 END\n")
        check(coop_policy_pair.wall_clock_deathlink_steps(wall) == [(1, "40s", 0, 1)],
              "coop_policy_pair flags a wall-clock DeathLink step")
        mixed = tmp / "states-mixed.txt"
        mixed.write_text("0 PIKMIN_STATE 9 t 1 0 127 0 CHECKS 0 BENEFITS 0 0 0 0 0 0 0 0 0 DEATHLINK 0 END\n"
                         "5 PIKMIN_STATE 9 t 1 0 127 0 CHECKS 0 BENEFITS 0 1 0 0 0 0 0 0 0 DEATHLINK 0 END\n"
                         "f1200 PIKMIN_STATE 9 t 1 0 127 0 CHECKS 0 BENEFITS 0 1 0 0 0 0 0 0 0 DEATHLINK 1 END\n")
        check(coop_policy_pair.wall_clock_deathlink_steps(mixed) == [],
              "wall-clock grants are allowed when the DeathLink rise is frame keyed")

        # Gapfix C: b1_pairs pairs each DEATHLINK_TOTAL with the next apply frame.
        dlog = tmp / "dl-native.log"
        dlog.write_text("[netplay] randstate gen=1 applied at frame=32\n"
                        "[Pikmin Randomizer] DEATHLINK_TOTAL 2\n"
                        "[netplay] randstate gen=2 applied at frame=640\n"
                        "[netplay] randstate gen=3 applied at frame=700\n"
                        "[Pikmin Randomizer] DEATHLINK_TOTAL 3\n"
                        "[netplay] randstate gen=4 applied at frame=1230\n")
        check(b1_pairs.deathlink_apply_frames(dlog) == [640, 1230],
              "b1_pairs reads the DeathLink apply frames")

        # Gapfix C: coop_policy_pair finds grants inside a HOLD window.
        hlog = tmp / "hold-native.log"
        body = ("[coop-policy] ANCHOR kind=FLOWERS captain=1 next=2 live=11\n"
                "[netplay] hold at frame=100 freeze-after=111\n"
                "[Pikmin Randomizer] FLOWER_SHOWER nectar=5\n"
                "[netplay] held at frame=111\n"
                "{during}"
                "[netplay] resume at frame=112 gen=2 held_ms=22000\n"
                "[coop-policy] ANCHOR kind=FLOWERS captain=2 next=1 live=11\n")
        hlog.write_text(body.format(during=""))
        w = coop_policy_pair.hold_windows(hlog)
        check(len(w) == 1 and len(w[0][2]) == 2 and w[0][3] == [] and len(w[0][4]) == 1,
              "coop_policy_pair: grants before the freeze and after the resume are not in the hold")
        hlog.write_text(body.format(during="[coop-policy] HEAL captain=2 reason=lowest hp=50.0->100.0\n"))
        w = coop_policy_pair.hold_windows(hlog)
        check(len(w) == 1 and len(w[0][3]) == 1, "coop_policy_pair flags a grant between held at and resume")

    if failures:
        print(f"selftest: {failures} failure(s)")
        return 1
    print("selftest: all checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
