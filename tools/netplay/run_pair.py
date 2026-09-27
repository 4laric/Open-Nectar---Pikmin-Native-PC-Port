"""Two-peer lockstep launcher for netplay M3 (issue #880).

Launches a host and a joiner as two hidden, private, bounded instances over
loopback UDP, each with its own scripted local input file (two different
gen_inputs.py seeds), the same bootstrap content, and the lossy impairment
env on both. Both peers run unthrottled (as fast as the session allows) for
tests and stop via PIKMIN_NETPLAY_EXIT_AFTER_TICKS.

Modelled on run_replay.py: junctioned read-only assets, a state.txt
refresher thread per run dir, the game run with the run dir as cwd
(settings are read relative to cwd), NECTAR_SAVE_DIR pointed at a private
dir, PIKMIN_RANDOMIZER_TEST_BACKGROUND=1 and SDL_AUDIODRIVER=dummy. Only
the spawned PIDs are ever signalled.

After both peers exit it compares the two hash logs with compare_hashes.py,
greps both stdout logs for desync / handshake-refused / disconnected lines,
and prints a summary.

Expectations (--expect):
  sync        both exit 0, identical hash logs, 0 desync lines (default)
  refuse      both exit 4 with a '[netplay] handshake refused:' line
  disconnect  joiner is killed mid-session; host exits 0 with a
              '[netplay] disconnected:' line
"""

import argparse
import os
import shutil
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

try:
    import _winapi
except ImportError:  # non-Windows: junctions unavailable; caller must symlink
    _winapi = None

DEFAULT_ASSETS = Path("C:/Users/alari/bbft/dist/cohesion/pikmin/assets")
HERE = Path(__file__).resolve().parent
GEN = HERE / "gen_inputs.py"
CMP = HERE / "compare_hashes.py"


def parse_kv(items, what):
    out = {}
    for item in items or []:
        if "=" not in item:
            raise SystemExit(f"--{what} needs key=value, got {item!r}")
        key, value = item.split("=", 1)
        out[key.strip()] = value.strip()
    return out


def write_bootstrap(path, token, profile):
    path.write_text(
        f"PIKMIN_RANDOMIZER 5\nSESSION {token}\nFINGERPRINT {token}\n"
        f"PROFILE {profile}\nCATALOG gameplay-checks-v5\nPLACEMENT identity-v1\n"
        f"GOAL 25\nDAYS repeat-day29-v1\nCOLOR red\nSTARTING_FLARLIC 10\nEND\n"
    )


def link_assets(run, assets):
    link = run / "assets"
    if not link.exists():
        if _winapi is not None:
            _winapi.CreateJunction(str(assets.resolve()), str(link))
        else:
            os.symlink(str(assets.resolve()), str(link), target_is_directory=True)


def gen_inputs(ticks, seed, out):
    r = subprocess.run(
        [sys.executable, str(GEN), "--ticks", str(ticks), "--seed", str(seed),
         "--out", str(out)],
        capture_output=True, text=True,
    )
    if r.returncode != 0:
        raise SystemExit(f"gen_inputs failed: {r.stderr[-2000:]}")


SCRUB_KEYS = (
    "PIKMIN_INPUT_RECORD",
    "PIKMIN_INPUT_REPLAY",
    "PIKMIN_STATE_HASH_LOG",
    "PIKMIN_NETPLAY_EXIT_AFTER_TICKS",
    "PIKMIN_NETPLAY_TEST_PREROLL_RAND",
    "PIKMIN_NETPLAY_DETERMINISTIC",
    "PIKMIN_NETPLAY_UNTHROTTLED",
    "PIKMIN_NETPLAY_TEST_CAMERA_WOBBLE",
    "PIKMIN_NETPLAY_DEBUG_NAVI_POS",
    "PIKMIN_NETPLAY_HOST",
    "PIKMIN_NETPLAY_JOIN",
    "PIKMIN_NETPLAY_DELAY",
    "PIKMIN_NETPLAY_SEED",
    "PIKMIN_NETPLAY_LOCAL_INPUT_FILE",
    "PIKMIN_NETPLAY_TEST_LATENCY_MS",
    "PIKMIN_NETPLAY_TEST_JITTER_MS",
    "PIKMIN_NETPLAY_TEST_LOSS_PCT",
    "PIKMIN_NETPLAY_TEST_SEED",
    "PIKMIN_NETPLAY_TEST_REORDER_PCT",
    "PIKMIN_NETPLAY_TEST_REORDER_MS",
    "PIKMIN_NETPLAY_HANDSHAKE_TIMEOUT_MS",
)


def launch(exe, run, boot, extra_args, env_extra, stdout_log, unthrottled=True):
    cmd = [str(exe.resolve()), "--randomizer-seed", str(boot)] + list(extra_args)
    env = dict(os.environ)
    for key in SCRUB_KEYS:
        env.pop(key, None)
    env.update(
        PIKMIN_RANDOMIZER_TEST_BACKGROUND="1",
        SDL_AUDIODRIVER="dummy",
        NECTAR_SAVE_DIR=str((run / "save").resolve()),
    )
    # M5: throttled runs (the path humans use: 30 Hz pacing + the
    # frames-ahead throttle) set PIKMIN_NETPLAY_UNTHROTTLED=0 explicitly.
    env["PIKMIN_NETPLAY_UNTHROTTLED"] = "1" if unthrottled else "0"
    env.update(env_extra)
    env.pop("BBFT_PORT", None)
    startup = None
    if sys.platform == "win32":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    child_out = open(stdout_log, "w")
    proc = subprocess.Popen(
        cmd, cwd=str(run), env=env, startupinfo=startup,
        stdout=child_out, stderr=subprocess.STDOUT,
    )
    return proc, child_out


def count_lines(path):
    try:
        with open(path, "r", errors="replace") as f:
            return sum(1 for line in f if line.strip())
    except OSError:
        return 0


def grep(path, needle):
    try:
        text = Path(path).read_text(errors="replace")
    except OSError:
        return []
    return [ln for ln in text.splitlines() if needle in ln]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe", type=Path, required=True, help="netplay game executable (host)")
    p.add_argument("--exe-b", type=Path, default=None, help="joiner executable override (exe negative test)")
    p.add_argument("--ticks", type=int, required=True)
    p.add_argument("--out", type=Path, required=True, help="private output dir (host/ + join/ below)")
    p.add_argument("--assets", type=Path, default=DEFAULT_ASSETS)
    p.add_argument("--profile", default="foh-day2")
    p.add_argument("--host-port", type=int, default=5077)
    p.add_argument("--seed-a", type=int, default=101, help="host local-input gen seed")
    p.add_argument("--seed-b", type=int, default=202, help="joiner local-input gen seed")
    p.add_argument("--netplay-seed", type=int, default=1, help="PIKMIN_NETPLAY_SEED for both")
    p.add_argument("--delay", type=int, default=2, help="PIKMIN_NETPLAY_DELAY for both")
    p.add_argument("--latency-ms", type=float, default=0.0)
    p.add_argument("--jitter-ms", type=float, default=0.0)
    p.add_argument("--loss-pct", type=float, default=0.0)
    p.add_argument("--impair-seed", type=int, default=7)
    p.add_argument("--timeout", type=float, default=900)
    p.add_argument("--handshake-timeout-ms", type=int, default=30000)
    p.add_argument("--env", nargs="*", default=[], metavar="K=V", help="extra env for both peers")
    p.add_argument("--env-host", nargs="*", default=[], metavar="K=V")
    p.add_argument("--env-join", nargs="*", default=[], metavar="K=V")
    p.add_argument("--config-overrides", nargs="*", default=[], metavar="key=value",
                   help="private pikmin_settings.conf for both peers")
    p.add_argument("--config-overrides-join", nargs="*", default=[], metavar="key=value",
                   help="extra private settings for the joiner only (settings negative test)")
    p.add_argument("--bootstrap-b", type=Path, default=None,
                   help="joiner bootstrap override file (bootstrap negative test)")
    p.add_argument("--throttled", action="store_true",
                   help="run the real-time 30 Hz path (no UNTHROTTLED); M5 evidence")
    p.add_argument("--no-restamp-session", action="store_true",
                   help="keep --bootstrap-b bytes verbatim (m8 SESSION-strip positive test)")
    p.add_argument("--session-only-difference", action="store_true",
                   help="m8 positive test: join bootstrap differs from the host only in "
                        "SESSION (same FINGERPRINT); the handshake must succeed")
    p.add_argument("--exe-args", nargs="*", default=[])
    p.add_argument("--expect", choices=("sync", "refuse", "disconnect"), default="sync")
    p.add_argument("--kill-joiner-after", type=float, default=20.0,
                   help="disconnect test: seconds after start to kill the joiner")
    a = p.parse_args(argv)

    out = a.out.resolve()
    host_run = out / "host"
    join_run = out / "join"
    host_run.mkdir(parents=True, exist_ok=True)
    join_run.mkdir(parents=True, exist_ok=True)

    token = uuid.uuid4().hex * 2
    token_join = token
    host_boot = host_run / "bootstrap.txt"
    write_bootstrap(host_boot, token, a.profile)
    join_boot = join_run / "bootstrap.txt"
    if a.bootstrap_b is not None:
        shutil.copyfile(str(a.bootstrap_b.resolve()), str(join_boot))
        if not a.no_restamp_session:
            # Re-stamp the per-run SESSION token to this run's token (the
            # handshake hash strips SESSION lines by design, so the manifest
            # difference under test is preserved while state.txt admission,
            # which compares the session token, keeps working).
            lines = join_boot.read_text().splitlines()
            lines = [f"SESSION {token}" if ln.startswith("SESSION ") else ln for ln in lines]
            join_boot.write_text("\n".join(lines) + "\n")
        else:
            # m8: keep --bootstrap-b bytes verbatim AND stamp the joiner's
            # state.txt with the join bootstrap's own SESSION, so local
            # admission passes on both sides while the handshake must still
            # succeed on the stripped hash (SESSION differs, all else same).
            for ln in join_boot.read_text().splitlines():
                if ln.startswith("SESSION "):
                    token_join = ln[len("SESSION "):].strip()
                    break
    else:
        write_bootstrap(join_boot, token, a.profile)
    if a.session_only_difference:
        # m8 positive test: same manifest, different per-run SESSION token.
        # The joiner's state.txt (below) uses token_join so local admission
        # passes; the handshake strips SESSION, so it must succeed.
        token_join = uuid.uuid4().hex * 2
        assert token_join != token
        join_boot.write_text(
            f"PIKMIN_RANDOMIZER 5\nSESSION {token_join}\nFINGERPRINT {token}\n"
            f"PROFILE {a.profile}\nCATALOG gameplay-checks-v5\nPLACEMENT identity-v1\n"
            f"GOAL 25\nDAYS repeat-day29-v1\nCOLOR red\nSTARTING_FLARLIC 10\nEND\n"
        )

    link_assets(host_run, a.assets)
    link_assets(join_run, a.assets)

    overrides = parse_kv(a.config_overrides, "config-overrides")
    if overrides:
        for run in (host_run, join_run):
            with open(run / "pikmin_settings.conf", "w") as f:
                for key, value in sorted(overrides.items()):
                    f.write(f"{key} = {value}\n")
    join_extra_cfg = parse_kv(a.config_overrides_join, "config-overrides-join")
    if join_extra_cfg:
        with open(join_run / "pikmin_settings.conf", "a") as f:
            for key, value in sorted(join_extra_cfg.items()):
                f.write(f"{key} = {value}\n")

    for run in (host_run, join_run):
        (run / "save").mkdir(exist_ok=True)

    # Per-peer scripted local inputs (brief: two different seeds).
    host_inputs = out / "host_inputs.pkni"
    join_inputs = out / "join_inputs.pkni"
    gen_inputs(a.ticks + 50, a.seed_a, host_inputs)
    gen_inputs(a.ticks + 50, a.seed_b, join_inputs)

    host_hash = host_run / "hashes.txt"
    join_hash = join_run / "hashes.txt"
    host_log = host_run / "native.log"
    join_log = join_run / "native.log"

    impair = {
        "PIKMIN_NETPLAY_TEST_LATENCY_MS": str(a.latency_ms),
        "PIKMIN_NETPLAY_TEST_JITTER_MS": str(a.jitter_ms),
        "PIKMIN_NETPLAY_TEST_LOSS_PCT": str(a.loss_pct),
        "PIKMIN_NETPLAY_TEST_SEED": str(a.impair_seed),
    }
    base_extra = {
        "PIKMIN_NETPLAY_SEED": str(a.netplay_seed),
        "PIKMIN_NETPLAY_DELAY": str(a.delay),
        "PIKMIN_NETPLAY_EXIT_AFTER_TICKS": str(a.ticks),
        "PIKMIN_NETPLAY_HANDSHAKE_TIMEOUT_MS": str(a.handshake_timeout_ms),
        "PIKMIN_STATE_HASH_LOG": "",  # replaced per peer below
    }
    base_extra.update(impair)
    base_extra.update(parse_kv(a.env, "env"))

    host_extra = dict(base_extra)
    host_extra["PIKMIN_STATE_HASH_LOG"] = str(host_hash)
    host_extra["PIKMIN_NETPLAY_LOCAL_INPUT_FILE"] = str(host_inputs.resolve())
    host_extra.update(parse_kv(a.env_host, "env-host"))
    join_extra = dict(base_extra)
    join_extra["PIKMIN_STATE_HASH_LOG"] = str(join_hash)
    join_extra["PIKMIN_NETPLAY_LOCAL_INPUT_FILE"] = str(join_inputs.resolve())
    join_extra.update(parse_kv(a.env_join, "env-join"))

    join_exe = a.exe_b.resolve() if a.exe_b is not None else a.exe.resolve()
    host_args = ["--netplay-host", str(a.host_port)] + list(a.exe_args)
    join_args = ["--netplay-join", f"127.0.0.1:{a.host_port}"] + list(a.exe_args)

    stop = threading.Event()

    def refresh(run, tok):
        while not stop.is_set():
            pending = run / "state.tmp"
            try:
                pending.write_text(f"PIKMIN_STATE 5 {tok} 1 0 127 0 0 END\n")
                os.replace(pending, run / "state.txt")
            except OSError:
                pass
            stop.wait(0.1)

    threads = [threading.Thread(target=refresh, args=(host_run, token)),
               threading.Thread(target=refresh, args=(join_run, token_join))]
    for t in threads:
        t.start()

    rc_host, rc_join = 1, 1
    host_proc = join_proc = None
    host_out = join_out = None
    start = time.time()
    try:
        host_proc, host_out = launch(a.exe, host_run, host_boot, host_args, host_extra, host_log,
                                       unthrottled=not a.throttled)
        # Stagger the joiner slightly so the host's socket is bound first.
        time.sleep(1.0)
        join_proc, join_out = launch(join_exe, join_run, join_boot, join_args, join_extra, join_log,
                                     unthrottled=not a.throttled)
        if a.expect == "disconnect":
            time.sleep(a.kill_joiner_after)
            if join_proc.poll() is None:
                print(f"run_pair: killing joiner pid {join_proc.pid} for disconnect test")
                join_proc.kill()
            try:
                rc_join = join_proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                rc_join = 124
            try:
                rc_host = host_proc.wait(timeout=a.timeout)
            except subprocess.TimeoutExpired:
                host_proc.kill()
                rc_host = 124
        else:
            try:
                rc_host = host_proc.wait(timeout=a.timeout)
            except subprocess.TimeoutExpired:
                host_proc.kill()
                try:
                    rc_host = host_proc.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    rc_host = 124
                print(f"run_pair: host timeout after {a.timeout}s, killed pid {host_proc.pid}")
            try:
                rc_join = join_proc.wait(timeout=60)
            except subprocess.TimeoutExpired:
                join_proc.kill()
                try:
                    rc_join = join_proc.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    rc_join = 124
                print("run_pair: joiner timeout, killed pid "
                      f"{join_proc.pid if join_proc else '?'}")
    finally:
        stop.set()
        for t in threads:
            t.join()
        for f in (host_out, join_out):
            try:
                if f is not None:
                    f.close()
            except OSError:
                pass
        for proc in (host_proc, join_proc):
            try:
                if proc is not None and proc.poll() is None:
                    proc.kill()
            except OSError:
                pass
    secs = time.time() - start

    n_host = count_lines(host_hash)
    n_join = count_lines(join_hash)
    cmp_rc, cmp_tail = None, ""
    if a.expect == "sync":
        r = subprocess.run([sys.executable, str(CMP), str(host_hash), str(join_hash)],
                           capture_output=True, text=True)
        cmp_rc = r.returncode
        cmp_tail = (r.stdout + r.stderr).strip().splitlines()
        cmp_tail = cmp_tail[-1] if cmp_tail else ""
    des_host = grep(host_log, "desync detected")
    des_join = grep(join_log, "desync detected")
    ref_host = grep(host_log, "handshake refused")
    ref_join = grep(join_log, "handshake refused")
    dis_host = grep(host_log, "disconnected:")

    print(f"run_pair: expect={a.expect} ticks={a.ticks} time={secs:.1f}s")
    print(f"run_pair: host exit={rc_host} hashes={n_host} log={host_log}")
    print(f"run_pair: join exit={rc_join} hashes={n_join} log={join_log}")
    if a.expect == "sync":
        print(f"run_pair: compare exit={cmp_rc}: {cmp_tail}")
    print(f"run_pair: desync lines host={len(des_host)} join={len(des_join)}")
    print(f"run_pair: refused lines host={len(ref_host)} join={len(ref_join)}")
    print(f"run_pair: disconnected lines host={len(dis_host)}")

    ok = True
    if a.expect == "sync":
        if rc_host != 0 or rc_join != 0:
            ok = False
        if cmp_rc != 0:
            ok = False
        if des_host or des_join:
            ok = False
        if n_host != a.ticks or n_join != a.ticks:
            print(f"run_pair: FAIL: hash lines {n_host}/{n_join} != requested {a.ticks}")
            ok = False
    elif a.expect == "refuse":
        if rc_host != 4 or rc_join != 4:
            print(f"run_pair: FAIL: expected both exit 4, got {rc_host}/{rc_join}")
            ok = False
        if not ref_host or not ref_join:
            print("run_pair: FAIL: expected refused lines on both peers")
            ok = False
    elif a.expect == "disconnect":
        if rc_host != 0:
            print(f"run_pair: FAIL: expected host exit 0, got {rc_host}")
            ok = False
        if not dis_host:
            print("run_pair: FAIL: expected a disconnected line on the host")
            ok = False
    print(f"run_pair: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
