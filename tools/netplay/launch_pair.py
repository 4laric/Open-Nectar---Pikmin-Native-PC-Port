"""Launcher pair test for the netplay one-command lane (issue #887).

Runs a host and a joiner through the NEW one-command switches only
(--netplay-host-ice / --netplay-join-ice, --netplay-test-hidden,
--netplay-test-ticks, --netplay-input), moving the codes only through the
files/stdin the humans would use (offer/answer .txt files). No netplay env
vars are set on either peer beyond the hidden-run plumbing
(PIKMIN_RANDOMIZER_TEST_BACKGROUND, SDL_AUDIODRIVER) and the lab network
(PIKMIN_NETPLAY_STUN=none, ICE ports, impairment env): in particular the
joiner is given NO bootstrap or settings file in variant (a), and the host
is never given --bootstrap (default new-game bootstrap path).

Variants (all: 3000 ticks unless --ticks says otherwise, DELAY=auto default):
  a  joiner has no bootstrap and no settings file (pure bundle adoption).
  b  joiner's settings file differs only in presentation keys (window size,
     gamma, a keybind) with identical sim values: must stay identical.
  c  joiner's settings file differs in one sim key (chainActions=1): the
     session must use the host's value, stay identical, and leave the
     joiner's file byte-identical on disk (SHA-256 before/after).

Hidden, private, bounded: run dirs as cwd, NECTAR_SAVE_DIR under the run
dir (the launcher also scopes its own save dir), only spawned PIDs signalled.
"""

import argparse
import hashlib
import os
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import run_pair as rp

LAUNCH_SCRUB_KEYS = (
    "PIKMIN_NETPLAY_HOST",
    "PIKMIN_NETPLAY_JOIN",
    "PIKMIN_NETPLAY_DELAY",
    "PIKMIN_NETPLAY_SEED",
    "PIKMIN_NETPLAY_LOCAL_INPUT_FILE",
    "PIKMIN_NETPLAY_EXIT_AFTER_TICKS",
    "PIKMIN_NETPLAY_ICE_HOST",
    "PIKMIN_NETPLAY_ICE_JOIN",
    "PIKMIN_NETPLAY_ICE_CODE_OUT",
    "PIKMIN_NETPLAY_ICE_ANSWER_IN",
    "PIKMIN_NETPLAY_STUN",
    "PIKMIN_NETPLAY_TURN",
    "PIKMIN_NETPLAY_ICE_TURN_ONLY",
    "PIKMIN_NETPLAY_ICE_PORT_BEGIN",
    "PIKMIN_NETPLAY_ICE_PORT_END",
    "PIKMIN_NETPLAY_ICE_TIMEOUT_MS",
    "PIKMIN_NETPLAY_ICE_GATHER_TIMEOUT_MS",
    "PIKMIN_NETPLAY_ICE_BIND",
    "PIKMIN_NETPLAY_INPUT",
    "PIKMIN_NETPLAY_TEST_LATENCY_MS",
    "PIKMIN_NETPLAY_TEST_JITTER_MS",
    "PIKMIN_NETPLAY_TEST_LOSS_PCT",
    "PIKMIN_NETPLAY_TEST_SEED",
    "PIKMIN_NETPLAY_TEST_REORDER_PCT",
    "PIKMIN_NETPLAY_TEST_REORDER_MS",
    "PIKMIN_NETPLAY_HANDSHAKE_TIMEOUT_MS",
    "PIKMIN_NETPLAY_TEST_DROP_HS_FIRST_N",
    "PIKMIN_NETPLAY_TEST_DROP_FINAL_ACK",
    "PIKMIN_NETPLAY_DISCONNECT_MS",
    "PIKMIN_NETPLAY_TEST_LOAD_DELAY_MS",
    "PIKMIN_NETPLAY_TEST_SCRIPT_VIA_ACCUM",
    "PIKMIN_NETPLAY_TEST_SESSION_TOKEN",
    "PIKMIN_NETPLAY_RANDSTATE_STREAM",
    "PIKMIN_INPUT_RECORD",
    "PIKMIN_INPUT_REPLAY",
    "PIKMIN_STATE_HASH_LOG",
    "PIKMIN_NETPLAY_TEST_PREROLL_RAND",
    "PIKMIN_NETPLAY_DETERMINISTIC",
    "PIKMIN_NETPLAY_UNTHROTTLED",
    "PIKMIN_NETPLAY_TEST_CAMERA_WOBBLE",
    "PIKMIN_NETPLAY_DEBUG_NAVI_POS",
)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def wait_file(path, timeout, what):
    start = time.time()
    while time.time() - start < timeout:
        try:
            text = path.read_text(errors="replace").strip()
        except OSError:
            text = ""
        if text:
            return text
        time.sleep(0.5)
    raise SystemExit(f"launch_pair: timeout waiting for {what} at {path}")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe", type=Path, required=True)
    p.add_argument("--ticks", type=int, default=3000)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--assets", type=Path, default=rp.DEFAULT_ASSETS)
    p.add_argument("--variant", choices=("a", "b", "c"), default="a")
    p.add_argument("--seed-a", type=int, default=101)
    p.add_argument("--seed-b", type=int, default=202)
    p.add_argument("--timeout", type=float, default=1200)
    p.add_argument("--code-timeout", type=float, default=300)
    p.add_argument("--handshake-timeout-ms", type=int, default=30000)
    p.add_argument("--ice-timeout-ms", type=int, default=120000)
    p.add_argument("--input-host", default="keyboard")
    p.add_argument("--input-join", default="gamepad:0")
    p.add_argument("--bootstrap-host", type=Path, default=None,
                   help="host --bootstrap file (code-length probe with a real seed file)")
    a = p.parse_args(argv)

    for key in LAUNCH_SCRUB_KEYS:
        os.environ.pop(key, None)

    out = a.out.resolve()
    host_run = out / "host"
    join_run = out / "join"
    host_run.mkdir(parents=True, exist_ok=True)
    join_run.mkdir(parents=True, exist_ok=True)

    # Variant (a): no bootstrap, no settings file on the joiner (and no
    # settings file on the host either: pure defaults + bundle adoption).
    # Variants (b)/(c): the joiner gets its own settings file.
    join_conf = join_run / "pikmin_settings.conf"
    try:
        join_conf.unlink()
    except OSError:
        pass
    join_conf_before = None
    if a.variant == "b":
        join_conf.write_text(
            "windowWidth = 800\nwindowHeight = 600\ngamma = 1.5\nvsync = 0\nkey_5 = 44\n")
        join_conf_before = sha256_file(join_conf)
    elif a.variant == "c":
        join_conf.write_text("chainActions = 1\n")
        join_conf_before = sha256_file(join_conf)

    rp.link_assets(host_run, a.assets)
    rp.link_assets(join_run, a.assets)
    for run in (host_run, join_run):
        (run / "save").mkdir(exist_ok=True)

    host_inputs = out / "host_inputs.pkni"
    join_inputs = out / "join_inputs.pkni"
    rp.gen_inputs(a.ticks + 50, a.seed_a, host_inputs)
    rp.gen_inputs(a.ticks + 50, a.seed_b, join_inputs)

    host_hash = host_run / "hashes.txt"
    join_hash = join_run / "hashes.txt"
    host_log = host_run / "native.log"
    join_log = join_run / "native.log"
    host_offer = out / "host_offer.txt"
    host_answer_in = out / "host_answer_in.txt"
    join_answer = out / "join_answer.txt"
    for f in (host_offer, host_answer_in, join_answer):
        try:
            f.unlink()
        except OSError:
            pass

    token = uuid.uuid4().hex * 2
    base_extra = {
        "PIKMIN_RANDOMIZER_TEST_BACKGROUND": "1",
        "SDL_AUDIODRIVER": "dummy",
        "PIKMIN_NETPLAY_STUN": "none",
        "PIKMIN_NETPLAY_ICE_TIMEOUT_MS": str(a.ice_timeout_ms),
        "PIKMIN_NETPLAY_HANDSHAKE_TIMEOUT_MS": str(a.handshake_timeout_ms),
        "PIKMIN_STATE_HASH_LOG": "",
    }
    host_extra = dict(base_extra)
    host_extra["PIKMIN_STATE_HASH_LOG"] = str(host_hash)
    host_extra["PIKMIN_NETPLAY_LOCAL_INPUT_FILE"] = str(host_inputs.resolve())
    host_extra["PIKMIN_NETPLAY_ICE_PORT_BEGIN"] = "48100"
    host_extra["PIKMIN_NETPLAY_ICE_PORT_END"] = "48109"
    join_extra = dict(base_extra)
    join_extra["PIKMIN_STATE_HASH_LOG"] = str(join_hash)
    join_extra["PIKMIN_NETPLAY_LOCAL_INPUT_FILE"] = str(join_inputs.resolve())
    join_extra["PIKMIN_NETPLAY_ICE_PORT_BEGIN"] = "48110"
    join_extra["PIKMIN_NETPLAY_ICE_PORT_END"] = "48119"

    host_args = ["--netplay-host-ice",
                 "--netplay-code-out", str(host_offer),
                 "--netplay-answer-in", str(host_answer_in),
                 "--netplay-test-hidden", "--netplay-test-ticks", str(a.ticks),
                 "--netplay-input", a.input_host]
    if a.bootstrap_host is not None:
        host_args += ["--bootstrap", str(a.bootstrap_host.resolve())]
    join_args = ["--netplay-join-ice", f"@{host_offer}",
                 "--netplay-code-out", str(join_answer),
                 "--netplay-test-hidden", "--netplay-test-ticks", str(a.ticks),
                 "--netplay-input", a.input_join]

    stop = threading.Event()
    t0 = time.time()

    def refresh(run, tok):
        while not stop.is_set():
            try:
                pending = run / "state.tmp"
                pending.write_text(f"PIKMIN_STATE 5 {tok} 1 0 127 0 0 END\n")
                os.replace(pending, run / "state.txt")
            except OSError:
                pass
            stop.wait(0.1)

    # PIKMIN_NETPLAY_TEST_SESSION_TOKEN pins the SESSION token on both
    # peers (host default bootstrap + joiner bundle re-stamp), so the
    # state.txt refreshers below match exactly as in run_pair.py.
    base_extra["PIKMIN_NETPLAY_TEST_SESSION_TOKEN"] = token
    host_extra["PIKMIN_NETPLAY_TEST_SESSION_TOKEN"] = token
    join_extra["PIKMIN_NETPLAY_TEST_SESSION_TOKEN"] = token
    threads = [threading.Thread(target=refresh, args=(host_run, token)),
               threading.Thread(target=refresh, args=(join_run, token))]
    for t in threads:
        t.start()

    # NOTE: no --randomizer-seed is passed: the host writes its default
    # bootstrap into its private run dir, and the joiner adopts the bundle.
    # run_pair.launch() always appends --randomizer-seed, so launch inline.
    def launch_new(exe, run, extra_args, env_extra, stdout_log):
        cmd = [str(exe.resolve())] + list(extra_args)
        env = dict(os.environ)
        for key in rp.SCRUB_KEYS:
            env.pop(key, None)
        for key in LAUNCH_SCRUB_KEYS:
            env.pop(key, None)
        env.update(
            PIKMIN_RANDOMIZER_TEST_BACKGROUND="1",
            SDL_AUDIODRIVER="dummy",
            NECTAR_SAVE_DIR=str((run / "save").resolve()),
        )
        env["PIKMIN_NETPLAY_UNTHROTTLED"] = "1"
        env.update(env_extra)
        env.pop("BBFT_PORT", None)
        startup = None
        if sys.platform == "win32":
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        child_out = open(stdout_log, "w")
        proc = subprocess.Popen(cmd, cwd=str(run), env=env,
                                startupinfo=startup, stdout=child_out,
                                stderr=subprocess.STDOUT)
        return proc, child_out

    rc_host, rc_join = 1, 1
    host_proc = join_proc = None
    host_out = join_out = None
    start = time.time()
    try:
        host_proc, host_out = launch_new(a.exe, host_run, host_args, host_extra, host_log)
        offer = wait_file(host_offer, a.code_timeout, "host offer")
        print(f"launch_pair: offer ({len(offer)} chars) host -> join")
        join_proc, join_out = launch_new(a.exe, join_run, join_args, join_extra, join_log)
        answer = wait_file(join_answer, a.code_timeout, "join answer")
        print(f"launch_pair: answer ({len(answer)} chars) join -> host")
        host_answer_in.write_text(answer + "\n")
        try:
            rc_host = host_proc.wait(timeout=a.timeout)
        except subprocess.TimeoutExpired:
            host_proc.kill()
            try:
                rc_host = host_proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                rc_host = 124
            print(f"launch_pair: host timeout after {a.timeout}s, killed pid {host_proc.pid}")
        try:
            rc_join = join_proc.wait(timeout=60)
        except subprocess.TimeoutExpired:
            join_proc.kill()
            try:
                rc_join = join_proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                rc_join = 124
            print("launch_pair: joiner timeout, killed pid "
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

    n_host = rp.count_lines(host_hash)
    n_join = rp.count_lines(join_hash)
    r = subprocess.run([sys.executable, str(rp.CMP), str(host_hash), str(join_hash)],
                       capture_output=True, text=True)
    cmp_rc = r.returncode
    cmp_tail = (r.stdout + r.stderr).strip().splitlines()
    cmp_tail = cmp_tail[-1] if cmp_tail else ""
    des_host = rp.grep(host_log, "desync detected")
    des_join = rp.grep(join_log, "desync detected")
    ref_host = rp.grep(host_log, "handshake refused")
    ref_join = rp.grep(join_log, "handshake refused")
    bundle_host = rp.grep(host_log, "ice offer bundle")
    bundle_join = rp.grep(join_log, "adopted the host session bundle")
    auto_host = rp.grep(host_log, "auto delay:")
    auto_join = rp.grep(join_log, "auto delay:")
    cfg_host = rp.grep(host_log, "[netplay] config=")
    cfg_join = rp.grep(join_log, "[netplay] config=")
    boot_host = rp.grep(host_log, "[netplay] bootstrap=")
    boot_join = rp.grep(join_log, "[netplay] bootstrap=")

    print(f"launch_pair: variant={a.variant} ticks={a.ticks} time={secs:.1f}s")
    print(f"launch_pair: host exit={rc_host} hashes={n_host} log={host_log}")
    print(f"launch_pair: join exit={rc_join} hashes={n_join} log={join_log}")
    print(f"launch_pair: compare exit={cmp_rc}: {cmp_tail}")
    print(f"launch_pair: desync lines host={len(des_host)} join={len(des_join)}")
    print(f"launch_pair: refused lines host={len(ref_host)} join={len(ref_join)}")
    for ln in bundle_host:
        print(f"launch_pair: host {ln.strip()}")
    for ln in bundle_join:
        print(f"launch_pair: join {ln.strip()}")
    for ln in auto_host:
        print(f"launch_pair: host {ln.strip()}")
    for ln in auto_join:
        print(f"launch_pair: join {ln.strip()}")
    for ln in cfg_host:
        print(f"launch_pair: host {ln.strip()}")
    for ln in cfg_join:
        print(f"launch_pair: join {ln.strip()}")
    for ln in boot_host:
        print(f"launch_pair: host {ln.strip()}")
    for ln in boot_join:
        print(f"launch_pair: join {ln.strip()}")

    ok = True
    if rc_host != 0 or rc_join != 0:
        ok = False
    if cmp_rc != 0:
        ok = False
    if des_host or des_join:
        ok = False
    if n_host != a.ticks or n_join != a.ticks:
        print(f"launch_pair: FAIL: hash lines {n_host}/{n_join} != requested {a.ticks}")
        ok = False
    if not bundle_host or not bundle_join:
        print("launch_pair: FAIL: missing bundle offer/adopt lines")
        ok = False
    # The adopted session must hash identically on both peers (seed,
    # sim settings, bootstrap): the config=/bootstrap= lines prove it.
    if [ln.strip() for ln in cfg_host] != [ln.strip() for ln in cfg_join]:
        print("launch_pair: FAIL: config hash lines differ between peers")
        ok = False
    if [ln.strip() for ln in boot_host] != [ln.strip() for ln in boot_join]:
        print("launch_pair: FAIL: bootstrap hash lines differ between peers")
        ok = False
    if a.variant in ("b", "c"):
        if not join_conf.exists():
            print("launch_pair: FAIL: joiner settings file missing after the run")
            ok = False
        else:
            after = sha256_file(join_conf)
            print(f"launch_pair: joiner settings sha256 before={join_conf_before} after={after}")
            if after != join_conf_before:
                print("launch_pair: FAIL: joiner settings file changed on disk")
                ok = False
    print(f"launch_pair: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
