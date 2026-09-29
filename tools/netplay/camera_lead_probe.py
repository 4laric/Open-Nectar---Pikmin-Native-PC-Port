"""Responsiveness probe for the netplay M5c lane A lead camera (issue #887).

For each requested input delay it runs two scripted lockstep pairs through
run_pair.py with PIKMIN_NETPLAY_CAMERA_TRACE=1: one with the lead camera on
(default) and one with PIKMIN_NETPLAY_CAMERA_LEAD=0 (the opt-out, which
presents the sim camera exactly as before M5c). Both peers play probe scripts
from gen_camera_inputs.py: hands-off pads with a camera turn, a zoom, an angle
change and an attention click at known record indices (the joiner's 100
records after the host's). Record i is a peer's i-th submitted input; it
lands on GekkoNet frame i + delay.

From the per-frame `[netplay] camlead` trace lines it reports, per event and
peer, the first presented frame whose view (yaw, distance, pitch, fov)
differs from the resting view just before the event, as frames after the
event's record index (0 = the frame the input is submitted, i.e. the next
presented frame). A fifth event, a mouse free-camera drag injected with
PIKMIN_NETPLAY_TEST_CAMERA_DRAG at a known frame, checks the drag routing:
the drag is immediate in both modes and turns each peer's own view (the
routing fix is session-wide; the joiner's drag used to land on P1's camera,
which the joiner never shows). It also compares the two runs' hash logs (the
lead camera must not change the simulation) and prints the gameplay proof
(START_STAGE on both peers, distinct navi/piki/teki/item tuples).

Everything runs hidden and private through run_pair.py (loopback binds,
private run dirs, bounded). Only run_pair's own spawned PIDs are touched.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GEN = HERE / "gen_camera_inputs.py"
RUN_PAIR = HERE / "run_pair.py"
COMPARE = HERE / "compare_hashes.py"

TRACE = re.compile(
    r"\[netplay\] camlead f=(\d+) lead=(\d) upd=(\d) steps=(-?\d+)(?: start_cut)? sim_yaw=(\d+) view_yaw=(\d+) "
    r"sim_dist=([-\d.]+) view_dist=([-\d.]+) sim_pitch=([-\d.]+) view_pitch=([-\d.]+) "
    r"sim_fov=([-\d.]+) view_fov=([-\d.]+) corr=([-\d.]+)")


def parse_trace(log: Path):
    rows = {}
    for line in log.read_text(errors="replace").splitlines():
        m = TRACE.search(line)
        if not m:
            continue
        f = int(m.group(1))
        rows[f] = {
            "lead": int(m.group(2)),
            "sim": (int(m.group(5)), m.group(7), m.group(9), m.group(11)),
            "view": (int(m.group(6)), m.group(8), m.group(10), m.group(12)),
        }
    return rows


SUBMIT = re.compile(r"\[netplay\] camlead submit f=(\d+) yaw=(\d+)")


def parse_submits(log: Path):
    out = []
    for line in log.read_text(errors="replace").splitlines():
        m = SUBMIT.search(line)
        if m:
            out.append((int(m.group(1)), int(m.group(2))))
    return out


def yaw_follow(rows, submits, delay, turn_record):
    """Live-yaw check: each submitted yaw (landing frame L = record + delay)
    against the view yaw of the frame presented just before it was sampled
    (L - delay - 1). Returns (checked, mismatches, first record >= the turn
    whose submitted yaw differs from the one before, relative to the turn)."""
    checked = mismatches = 0
    first = None
    prev = None
    for landing, yaw in submits:
        shown = rows.get(landing - delay - 1)
        if shown is not None:
            checked += 1
            if shown["view"][0] != yaw:
                mismatches += 1
        record = landing - delay
        if first is None and prev is not None and record >= turn_record and yaw != prev:
            first = record - turn_record
        prev = yaw
    return checked, mismatches, first


def first_change(rows, event, column, window=60):
    """First frame >= event - 2 whose `column` tuple differs from the rest
    value at event - 1. Returns (frame, rest_ok)."""
    rest = rows.get(event - 1)
    if rest is None:
        return None, False
    rest_ok = all(rows.get(event - k, {}).get(column) == rest[column] for k in (1, 2, 3))
    for f in range(event - 2, event + window):
        r = rows.get(f)
        if r is not None and r[column] != rest[column]:
            return f, rest_ok
    return None, rest_ok


def peer_log(out: Path, peer: str) -> Path:
    return out / "host" / "run" / "native.log" if peer == "host" else out / "join" / "peer" / "run" / "native.log"


def gen_probe(path: Path, ticks: int, base: int):
    subprocess.run([sys.executable, str(GEN), "probe", "--ticks", str(ticks), "--out", str(path),
                    "--turn-at", str(base), "--zoom-at", str(base + 300), "--angle-at", str(base + 500),
                    "--attention-at", str(base + 700)], check=True)
    return {"turn": base, "zoom": base + 300, "angle": base + 500, "attention": base + 700, "drag": base + 800}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True, help="private output dir")
    p.add_argument("--delays", default="3,4")
    p.add_argument("--port", type=int, required=True, help="first host UDP port (one per run)")
    p.add_argument("--ticks", type=int, default=2100)
    p.add_argument("--host-base", type=int, default=900, help="host turn record index")
    p.add_argument("--shot-frames", default="", help="PIKMIN_NETPLAY_CAMERA_SHOT frames for the host (a,b,c)")
    p.add_argument("--extra-env", nargs="*", default=[])
    p.add_argument("--live-yaw", action="store_true",
                   help="PIKMIN_NETPLAY_TEST_SCRIPT_LIVE_YAW=1 on both peers: scripted pads, live control yaw. "
                        "Checks that every submitted yaw is the view yaw the peer presented when it was sampled; "
                        "lead and opt-out then submit different yaws, so their hash logs differ by design and only "
                        "host vs joiner is compared")
    a = p.parse_args(argv)

    a.out.mkdir(parents=True, exist_ok=True)
    inputs = a.out / "inputs"
    inputs.mkdir(exist_ok=True)
    ev_host = gen_probe(inputs / "probe-host.pkni", a.ticks + 50, a.host_base)
    ev_join = gen_probe(inputs / "probe-join.pkni", a.ticks + 50, a.host_base + 100)

    port = a.port
    failures = 0
    table = []
    for delay in [int(x) for x in a.delays.split(",") if x]:
        runs = {}
        for mode in ("lead", "optout"):
            out = a.out / f"d{delay}-{mode}"
            env = ["PIKMIN_NETPLAY_CAMERA_TRACE=1"] + list(a.extra_env)
            if a.live_yaw:
                env.append("PIKMIN_NETPLAY_TEST_SCRIPT_LIVE_YAW=1")
            if mode == "optout":
                env.append("PIKMIN_NETPLAY_CAMERA_LEAD=0")
            env_host = [f"PIKMIN_NETPLAY_LOCAL_INPUT_FILE={(inputs / 'probe-host.pkni').resolve()}",
                        f"PIKMIN_NETPLAY_TEST_CAMERA_DRAG={ev_host['drag']}:0.2"]
            if a.shot_frames:
                shots = a.out / "shots"
                shots.mkdir(exist_ok=True)
                env_host.append(f"PIKMIN_NETPLAY_CAMERA_SHOT={shots.resolve()}/d{delay}:{a.shot_frames}")
                (shots / f"d{delay}").mkdir(exist_ok=True)
            cmd = [sys.executable, str(RUN_PAIR), "--exe", str(a.exe), "--ticks", str(a.ticks), "--out", str(out),
                   "--host-port", str(port), "--delay", str(delay), "--env", *env, "--env-host", *env_host,
                   "--env-join", f"PIKMIN_NETPLAY_LOCAL_INPUT_FILE={(inputs / 'probe-join.pkni').resolve()}",
                   f"PIKMIN_NETPLAY_TEST_CAMERA_DRAG={ev_join['drag']}:0.2"]
            port += 1
            log = a.out / f"d{delay}-{mode}.log"
            with open(log, "w") as fh:
                fh.write("CMD: " + " ".join(cmd) + "\n")
                fh.flush()
                rc = subprocess.run(cmd, stdout=fh, stderr=subprocess.STDOUT).returncode
                fh.write(f"EXIT: {rc}\n")
            print(f"camera_lead_probe: delay {delay} {mode}: run_pair exit {rc} ({log})")
            if rc != 0:
                failures += 1
            runs[mode] = out
        # Gameplay proof per run and peer.
        for mode, out in runs.items():
            for peer, rel in (("host", "host/run"), ("join", "join/peer/run")):
                log = (out / rel / "native.log").read_text(errors="replace")
                starts = log.count("START_STAGE")
                tuples = set()
                for ln in (out / rel / "hashes.txt").read_text(errors="replace").splitlines():
                    f = ln.split()
                    if len(f) >= 6:
                        tuples.add(tuple(f[2:6]))
                summary = [ln for ln in log.splitlines() if "camera lead summary" in ln]
                print(f"camera_lead_probe: delay {delay} {mode} {peer}: START_STAGE={starts} tuples={len(tuples)} "
                      f"{summary[-1].split('] ', 1)[-1] if summary else 'no summary'}")
                if starts < 1:
                    failures += 1
        # Hash logs: lead vs opt-out, per peer (the same inputs); in live-yaw
        # mode the two runs submit different yaws, so each run's host and
        # joiner logs are compared instead.
        if a.live_yaw:
            for mode, out in runs.items():
                r = subprocess.run([sys.executable, str(COMPARE), str(out / "host/run/hashes.txt"),
                                    str(out / "join/peer/run/hashes.txt")], capture_output=True, text=True)
                print(f"camera_lead_probe: delay {delay} {mode} hashes host vs joiner: {r.stdout.strip()} (exit {r.returncode})")
                if r.returncode != 0:
                    failures += 1
            r = subprocess.run([sys.executable, str(COMPARE), str(runs["lead"] / "host/run/hashes.txt"),
                                str(runs["optout"] / "host/run/hashes.txt")], capture_output=True, text=True)
            print(f"camera_lead_probe: delay {delay} hashes lead vs opt-out (expected to differ): {r.stdout.strip()}")
            for mode, out in runs.items():
                for peer, turn in (("host", ev_host["turn"]), ("join", ev_join["turn"])):
                    checked, bad, first = yaw_follow(parse_trace(peer_log(out, peer)), parse_submits(peer_log(out, peer)),
                                                     delay, turn)
                    print(f"camera_lead_probe: delay {delay} {mode} {peer}: submitted yaw == presented view yaw for "
                          f"{checked - bad}/{checked} submits; first yaw change {first} records after the turn")
                    want = 1 if mode == "lead" else delay + 2
                    if bad != 0 or checked == 0 or first != want:
                        failures += 1
            continue
        for peer, rel in (("host", "host/run/hashes.txt"), ("join", "join/peer/run/hashes.txt")):
            r = subprocess.run([sys.executable, str(COMPARE), str(runs["lead"] / rel), str(runs["optout"] / rel)],
                               capture_output=True, text=True)
            print(f"camera_lead_probe: delay {delay} {peer} hashes lead vs opt-out: {r.stdout.strip()} (exit {r.returncode})")
            if r.returncode != 0:
                failures += 1
        for peer, events in (("host", ev_host), ("join", ev_join)):
            lead_rows = parse_trace(peer_log(runs["lead"], peer))
            opt_rows = parse_trace(peer_log(runs["optout"], peer))
            for kind, ev in events.items():
                fl, okl = first_change(lead_rows, ev, "view")
                fo, oko = first_change(opt_rows, ev, "view")
                lat_l = None if fl is None else fl - ev
                lat_o = None if fo is None else fo - ev
                table.append((delay, peer, kind, ev, lat_l, lat_o, okl and oko))
                if kind == "drag":
                    # Immediate in both modes, on both peers (the drag goes
                    # straight into the sim camera, no input delay).
                    if lat_l != 0 or lat_o != 0 or not (okl and oko):
                        failures += 1
                elif lat_l != 0 or lat_o != delay + 1 or not (okl and oko):
                    failures += 1

    print()
    print("delay peer kind       record  lead_latency  optout_latency  camera_at_rest")
    for delay, peer, kind, ev, ll, lo, rest in table:
        print(f"{delay:5d} {peer:4s} {kind:10s} {ev:6d}  {str(ll):>12s}  {str(lo):>14s}  {'yes' if rest else 'NO'}")
    print()
    print(f"camera_lead_probe: {'PASS' if failures == 0 else 'FAIL'} ({failures} problem(s))")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
