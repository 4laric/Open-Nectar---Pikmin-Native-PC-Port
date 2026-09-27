"""Seeded scripted input generator for the netplay determinism harness.

Writes the PKNI binary format (see pc_port/netplay/pc_input_log.h):
10-byte header (magic, version, pad count, record size) followed by one
record per tick, 4 pads each.

v2 (default): 56-byte records, 4 pads of 14 bytes each (11 PADStatus bytes
+ control-yaw u16 LE + flags u8). Each pad carries a plausible slowly
varying control yaw, so replays exercise the yaw-as-input path.
v1 (--v1): 44-byte records, 4 pads of 11 bytes (M1 format, no yaw).

The generator plays a plausible field session on pad 0: smooth main-stick
walk patterns, periodic A (throw/pluck), B holds (whistle), rare X
(disband), C-stick nudges and trigger presses. It never presses Start or Y
and never opens menus. Pads 1-3 are neutral (no controller).
"""

import argparse
import math
import random
import struct
import sys
from pathlib import Path

MAGIC = b"PKNI"
VERSION_V2 = 2
VERSION_V1 = 1
PAD_COUNT = 4
RECORD_SIZE_V2 = 56
RECORD_SIZE_V1 = 44

# PAD buttons (include/Dolphin/pad.h). Start and Y are never emitted.
BTN_LEFT = 0x0001
BTN_RIGHT = 0x0002
BTN_DOWN = 0x0004
BTN_UP = 0x0008
TRIG_Z = 0x0010
TRIG_R = 0x0020
TRIG_L = 0x0040
BTN_A = 0x0100
BTN_B = 0x0200
BTN_X = 0x0400

STICK_MAX = 96  # comfortable deflection, inside the s8 range


def gen_ticks(nticks: int, seed: int):
    rng = random.Random(seed)
    # Walk state: current stick target + ticks remaining on this leg.
    wx, wy = 0, 0
    leg_left = 0
    # Timed button events: list of (button, ticks_remaining).
    holds = {}
    # Cooldowns before the next scheduled press.
    next_a = rng.randint(20, 60)
    next_b = rng.randint(80, 200)
    next_x = rng.randint(300, 700)
    next_c = rng.randint(30, 120)
    next_t = rng.randint(100, 300)
    cx, cy = 0, 0
    c_left = 0
    trig_l, trig_r = 0, 0
    t_left = 0
    # Slowly varying control yaw per pad (turns): a base drift plus a
    # gentle sinusoid, so consecutive ticks differ by a fraction of a
    # degree like a real camera, plus per-pad offsets so pads disagree.
    yaw_base = [rng.uniform(0.0, 1.0) for _ in range(4)]
    yaw_rate = [rng.uniform(-0.0006, 0.0006) for _ in range(4)]
    yaw_amp = [rng.uniform(0.0, 0.004) for _ in range(4)]
    yaw_period = [rng.uniform(300.0, 1200.0) for _ in range(4)]

    for tick in range(nticks):
        if leg_left <= 0:
            # New walk leg: mostly mid deflections, sometimes idle.
            if rng.random() < 0.15:
                wx, wy = 0, 0
            else:
                ang = rng.uniform(0, 2 * 3.141592653589793)
                mag = rng.randint(40, STICK_MAX)
                wx = int(round(mag * math.cos(ang)))
                wy = int(round(mag * math.sin(ang)))
            leg_left = rng.randint(20, 90)

        # Jitter the stick a little so consecutive ticks are not identical.
        jx = max(-127, min(127, wx + rng.randint(-6, 6)))
        jy = max(-127, min(127, wy + rng.randint(-6, 6)))
        leg_left -= 1

        buttons = 0
        # A presses: short 2-3 tick taps.
        next_a -= 1
        if next_a <= 0:
            holds[BTN_A] = rng.randint(2, 3)
            next_a = rng.randint(40, 120)
        # B holds: whistle, 10-30 ticks.
        next_b -= 1
        if next_b <= 0:
            holds[BTN_B] = rng.randint(10, 30)
            next_b = rng.randint(120, 300)
        # X: rare single-tick disband.
        next_x -= 1
        if next_x <= 0:
            holds[BTN_X] = 1
            next_x = rng.randint(400, 900)
        for btn, left in list(holds.items()):
            if left > 0:
                buttons |= btn
                holds[btn] = left - 1

        # C-stick nudges.
        next_c -= 1
        if next_c <= 0:
            cx = rng.randint(-80, 80)
            cy = rng.randint(-80, 80)
            c_left = rng.randint(3, 10)
            next_c = rng.randint(50, 200)
        if c_left > 0:
            c_left -= 1
        else:
            cx, cy = 0, 0

        # Triggers: occasional analog squeezes plus digital bit.
        next_t -= 1
        if next_t <= 0:
            trig_l = rng.randint(60, 200)
            trig_r = rng.randint(60, 200)
            t_left = rng.randint(4, 12)
            next_t = rng.randint(150, 400)
        if t_left > 0:
            t_left -= 1
            if trig_l > 120:
                buttons |= TRIG_L
            if trig_r > 120:
                buttons |= TRIG_R
        else:
            trig_l, trig_r = 0, 0

        assert not buttons & 0x1800, "Start/Y must never be pressed"
        yaws = []
        for p in range(4):
            turns = (
                yaw_base[p]
                + yaw_rate[p] * tick
                + yaw_amp[p] * math.sin(2 * math.pi * tick / yaw_period[p])
            )
            yaws.append(int(turns * 65536.0 + 0.5) & 0xFFFF)
        yield (buttons, jx, jy, cx, cy, trig_l, trig_r, yaws)


def write_file(path: Path, ticks, v1: bool = False):
    version = VERSION_V1 if v1 else VERSION_V2
    record_size = RECORD_SIZE_V1 if v1 else RECORD_SIZE_V2
    with open(path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<HHH", version, PAD_COUNT, record_size))
        for buttons, sx, sy, cx, cy, tl, tr, yaws in ticks:
            # Pad 0: live input, connected.
            if v1:
                f.write(
                    struct.pack(
                        "<HbbbbBBBBb", buttons, sx, sy, cx, cy, tl, tr, 0, 0, 0
                    )
                )
            else:
                f.write(
                    struct.pack(
                        "<HbbbbBBBBbHb", buttons, sx, sy, cx, cy, tl, tr, 0, 0, 0,
                        yaws[0], 0,
                    )
                )
            # Pads 1-3: neutral, no controller (yaw still varies for realism).
            for p in range(1, 4):
                if v1:
                    f.write(struct.pack("<HbbbbBBBBb", 0, 0, 0, 0, 0, 0, 0, 0, 0, -1))
                else:
                    f.write(
                        struct.pack(
                            "<HbbbbBBBBbHb", 0, 0, 0, 0, 0, 0, 0, 0, 0, -1,
                            yaws[p], 0,
                        )
                    )


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--ticks", type=int, required=True)
    p.add_argument("--seed", type=int, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--v1", action="store_true",
                   help="write the M1 v1 format (no yaw); default is v2 with yaw")
    a = p.parse_args(argv)
    if a.ticks <= 0:
        print("gen_inputs: --ticks must be positive", file=sys.stderr)
        return 2
    write_file(a.out, gen_ticks(a.ticks, a.seed), v1=a.v1)
    print(f"gen_inputs: wrote {a.ticks} ticks (seed {a.seed}) v{'1' if a.v1 else '2'} to {a.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
