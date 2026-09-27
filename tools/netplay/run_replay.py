"""Hidden, private, bounded replay launcher for the netplay harness.

Runs one fresh game process that replays a PKNI input file while writing a
per-tick state-hash log, then exits via PIKMIN_NETPLAY_EXIT_AFTER_TICKS.

Bootstrap follows tools/run_campaign_resume.py's non-campaign path:
junctioned read-only assets, a state.txt refresher thread, the game run with
the run dir as cwd (settings are read relative to cwd, so the user's
settings are never touched), NECTAR_SAVE_DIR pointed at a private dir,
PIKMIN_RANDOMIZER_TEST_BACKGROUND=1 and SDL_AUDIODRIVER=dummy.

Only the spawned PID is ever signalled. Nothing outside the run dir and the
private save dir is written.
"""

import argparse
import os
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


def parse_kv(items, what):
    out = {}
    for item in items or []:
        if "=" not in item:
            raise SystemExit(f"--{what} needs key=value, got {item!r}")
        key, value = item.split("=", 1)
        out[key.strip()] = value.strip()
    return out


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe", type=Path, required=True, help="game executable")
    p.add_argument("--replay", type=Path, required=True, help="PKNI input file")
    p.add_argument("--ticks", type=int, required=True)
    p.add_argument("--out", type=Path, required=True, help="private run dir")
    p.add_argument("--assets", type=Path, default=DEFAULT_ASSETS)
    p.add_argument(
        "--config-overrides",
        nargs="*",
        default=[],
        metavar="key=value",
        help="written into a private pikmin_settings.conf in the run dir",
    )
    p.add_argument("--preroll-rand", type=int, default=0)
    p.add_argument("--timeout", type=float, default=600)
    p.add_argument("--env", nargs="*", default=[], metavar="K=V")
    p.add_argument("--profile", default="foh-day2")
    p.add_argument("--exe-args", nargs="*", default=[])
    a = p.parse_args(argv)

    run = a.out.resolve()
    run.mkdir(parents=True, exist_ok=True)

    token = uuid.uuid4().hex * 2
    boot = run / "bootstrap.txt"
    boot.write_text(
        f"PIKMIN_RANDOMIZER 5\nSESSION {token}\nFINGERPRINT {token}\n"
        f"PROFILE {a.profile}\nCATALOG gameplay-checks-v5\nPLACEMENT identity-v1\n"
        f"GOAL 25\nDAYS repeat-day29-v1\nCOLOR red\nSTARTING_FLARLIC 10\nEND\n"
    )

    assets_link = run / "assets"
    if not assets_link.exists():
        if _winapi is not None:
            _winapi.CreateJunction(str(a.assets.resolve()), str(assets_link))
        else:
            os.symlink(str(a.assets.resolve()), str(assets_link), target_is_directory=True)

    overrides = parse_kv(a.config_overrides, "config-overrides")
    if overrides:
        with open(run / "pikmin_settings.conf", "w") as f:
            for key, value in sorted(overrides.items()):
                f.write(f"{key} = {value}\n")

    save_dir = run / "save"
    save_dir.mkdir(exist_ok=True)

    hash_log = run / "hashes.txt"
    stdout_log = run / "native.log"

    done = threading.Event()

    def refresh():
        while not done.is_set():
            pending = run / "state.tmp"
            pending.write_text(f"PIKMIN_STATE 5 {token} 1 0 127 0 0 END\n")
            os.replace(pending, run / "state.txt")
            done.wait(0.1)

    thread = threading.Thread(target=refresh)
    thread.start()

    env = dict(
        os.environ,
        PIKMIN_RANDOMIZER_TEST_BACKGROUND="1",
        SDL_AUDIODRIVER="dummy",
        PIKMIN_NETPLAY_DETERMINISTIC="1",
        PIKMIN_NETPLAY_UNTHROTTLED="1",
        PIKMIN_STATE_HASH_LOG=str(hash_log),
        PIKMIN_INPUT_REPLAY=str(a.replay.resolve()),
        PIKMIN_NETPLAY_EXIT_AFTER_TICKS=str(a.ticks),
        NECTAR_SAVE_DIR=str(save_dir),
    )
    if a.preroll_rand:
        env["PIKMIN_NETPLAY_TEST_PREROLL_RAND"] = str(a.preroll_rand)
    env.update(parse_kv(a.env, "env"))
    env.pop("BBFT_PORT", None)

    cmd = [str(a.exe.resolve()), "--randomizer-seed", str(boot)] + list(a.exe_args)
    startup = None
    if sys.platform == "win32":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0

    start = time.time()
    proc = subprocess.Popen(
        cmd, cwd=str(run), env=env, startupinfo=startup,
        stdout=open(stdout_log, "w"), stderr=subprocess.STDOUT,
    )
    try:
        rc = proc.wait(timeout=a.timeout)
    except subprocess.TimeoutExpired:
        # Only our own child PID is ever signalled.
        proc.kill()
        try:
            rc = proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            rc = 124
        print(f"run_replay: timeout after {a.timeout}s, killed pid {proc.pid}")
    finally:
        done.set()
        thread.join()
    secs = time.time() - start

    nlines = 0
    if hash_log.exists():
        with open(hash_log, "r", errors="replace") as f:
            nlines = sum(1 for line in f if line.strip())
    tps = (nlines / secs) if secs > 0 else 0.0
    print(f"run_replay: exit={rc} ticks={nlines}/{a.ticks} time={secs:.1f}s tps={tps:.1f}")
    print(f"STDOUT_LOG={stdout_log}")
    print(f"HASH_LOG={hash_log}")
    return rc if isinstance(rc, int) else 1


if __name__ == "__main__":
    raise SystemExit(main())
