"""Directed scripted input for the netplay determinism harness (#982 refresh).

gen_inputs.py plays a random walk; this writes a PKNI v2 script that walks the
captain toward a world target and then fights there, so a pair can reach
something 700+ units from the start (an Empress, a Groink) in a bounded run.

Control yaw is held at 0 (the sim builds its movement basis from the input
yaw, never the camera), so stick (sx, sy) moves the captain along world
direction (sx, -sy) in x/z: pass the x/z vector from the captain start to the
target and the script does the conversion. Phases (ticks at the session rate):
  0 .. --whistle-ticks   B held (whistle): gather the Pikmin
  .. --walk-ticks        full-deflection walk toward the target
  .. end                 slow approach with A taps every --throw-period ticks
                         (throws), B whistle pulses every --whistle-period
Start, Y and X are never pressed.
"""
import argparse
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_inputs as gi  # noqa: E402


def stick_for(dx, dz, mag):
    n = math.hypot(dx, dz) or 1.0
    return int(round(mag * dx / n)), int(round(-mag * dz / n))


def ticks(total, dx, dz, whistle_ticks, walk_ticks, throw_period, whistle_period, wobble, redirects=()):
    base = (dx, dz)
    for t in range(total):
        # --redirect TICK,DX,DZ: from TICK on, head along (DX, DZ) instead.
        dx, dz = base
        for at, rx, rz in redirects:
            if t >= at:
                dx, dz = rx, rz
        full = stick_for(dx, dz, 100)
        buttons, sx, sy = 0, 0, 0
        if t < whistle_ticks:
            buttons |= gi.BTN_B
        elif t < whistle_ticks + walk_ticks:
            sx, sy = full
        else:
            k = t - whistle_ticks - walk_ticks
            # Slow approach; a small sideways sway keeps the facing alive and
            # varies the throw line without leaving the heading.
            ang = 0.35 * math.sin(2 * math.pi * k / max(1, wobble))
            c, s = math.cos(ang), math.sin(ang)
            sx, sy = stick_for(dx * c - dz * s, dx * s + dz * c, 45)
            if k % throw_period < 3:
                buttons |= gi.BTN_A
            elif whistle_period and k % whistle_period > whistle_period - 12:
                buttons |= gi.BTN_B
        yield ((buttons, sx, sy, 0, 0, 0, 0), None, [0, 0, 0, 0])


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--ticks", type=int, required=True)
    p.add_argument("--dx", type=float, required=True, help="x from the captain start to the target")
    p.add_argument("--dz", type=float, required=True, help="z from the captain start to the target")
    p.add_argument("--whistle-ticks", type=int, default=90)
    p.add_argument("--walk-ticks", type=int, default=600)
    p.add_argument("--throw-period", type=int, default=24)
    p.add_argument("--whistle-period", type=int, default=240)
    p.add_argument("--wobble", type=int, default=180)
    p.add_argument("--redirect", action="append", default=[], metavar="TICK,DX,DZ",
                   help="from TICK on, head along (DX, DZ); repeatable (a captain that was displaced mid-run)")
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args(argv)
    redirects = sorted((int(r.split(",")[0]), float(r.split(",")[1]), float(r.split(",")[2])) for r in a.redirect)
    gi.write_file(a.out, ticks(a.ticks, a.dx, a.dz, a.whistle_ticks, a.walk_ticks, a.throw_period,
                               a.whistle_period, a.wobble, redirects))
    print(f"gen_directed: wrote {a.ticks} ticks toward ({a.dx}, {a.dz}) to {a.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
