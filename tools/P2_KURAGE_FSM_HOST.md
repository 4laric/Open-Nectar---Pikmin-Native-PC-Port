# Kurage flight-FSM host and autonomous Attack suction (lane 29, #243)

Lane 29 / native worktree `output/native-lane29` (branch
`opencode/p2-lane29-jellyfloat`) based on the approved native baseline
`f14c6851473ac1161be56c8b98f4f905232f3635`. This slice moves the transcribed
source Kurage flight lifecycle (`pc_port/pc_p2_kurage_fsm.h`) onto the bounded
real host and connects the ordinary **Attack** state to the suction admission
scan, which the lane issue recorded as the primary remaining natural gate
("normal gameplay does not initiate it").  The player-visible actor is still the
private bounded host, not an ordinary generated `TEKI_Frog`; that replacement is
tracked separately.

## What changed

`pc_port/pc_p2_kurage_arena.{h,cpp}`:

- Opt-in FSM authority: `pc_p2_kurage_arena_fsm_enable/enabled/state/altitude`.
  When enabled, the host's vertical motion and state come from
  `p2kurage::Fsm` (Wait/Move/Chase/Attack/...) instead of the preview sine.
- `getSearchedTarget` host approximation `findSuctionTarget()`: the first live
  Pikmin inside the source vertical suction window (`inSuctionWindow`) and
  `mAttackRadius` whose sticker is not the owner.  View-angle/sight rejection
  stays with the receiver.
- The source Attack state starts the retail `attack.bca` clock
  (`pc_p2_retail_player.h`) on entry and consumes its KeyEvent 2/1 as the
  `p2kurage::In::keyEvent`; during the open suction interval the host calls
  `pc_p2_kurage_receiver_scan_admit`, so ordinary Attack itself initiates
  capture.  `pc_p2_retail::Player` advances in animation frames (30 fps), not
  seconds.
- `pc_p2_kurage_arena_set_owner_facts(hasHealth, bittered)` supplies the source
  bitter/zero-health pause gates to the receiver; owner death
  (`pc_p2_kurage_arena_update(delta, false)`) releases through the existing
  captured-scale-restoring receiver path.
- The bounded animation-END stand-in (`kFsmMotionFrames`) is clearly a
  placeholder: the converted MOD is a static pose and real clip completion
  belongs to the #431 motion-event bridge.

`tools/p2_kurage_runtime.cpp`:

- `--flight-fsm-admission`: FSM-driven flight -> source Attack -> autonomous
  suction admission -> stomach attachment.
- `--flight-fsm-death`: admission, then owner death mid-digestion -> release
  with restored scale.

`tools/run_kurage_flight_fsm.py`: prepares a fresh room session from a base
session and runs one scenario, recording a JSON provenance row and log.

## Evidence

Private build `output/native-lane29-build` (Ninja Release/MinGW gcc 16.2.0,
JAudio ON, test hooks OFF), `ninja -n pikmin_pc`: no work to do.
`bin/nectar.exe` SHA-256
`BD37CFC5E609EDA57D8885A1267423A452C413649F3AC47E4210EE2516449BBB`.

Fixture built with `scripts/build_pikmin2_fixture.py` into
`output/p2-lane29-fsm-fixture-final` (provenance `status=built`,
`expected_native_head=f14c6851…`); `fixture.exe` SHA-256
`EC80A586853452DC5A8278E0079536EA1C809E1E35877385F6C2E4924FDCBBBB`.  Both runs
use `PIKMIN_P2_ROOM_WINDOW=960x540` and a live squad.

Admission (`output/p2-lane29-final-admission`, 20 reds, window centred
`373,263`):

```
P2_KURAGE_FSM state=1 motion=6 altitude=150.000 vy=-79.966 ticks=0
P2_KURAGE_FSM state=4 motion=10 altitude=110.483 vy=-67.306 ticks=29
P2_KURAGE_FSM_ADMISSION_PASS state=4 auto=1 attach=1 stomach=1 altitude=74.6
PASS KURAGE_RUNTIME flight_fsm_admission
```

Interrupted release through death (`output/p2-lane29-final-death`):

```
P2_KURAGE_FSM_DEATH_PASS released=1 alive=1 scale_restored=1 state=4
PASS KURAGE_RUNTIME flight_fsm_interrupt
```

Regressions on the same fixture: `basic`, `--receiver-kill`,
`--receiver-transfer`, `--receiver-stageexit` and `--receiver-ingestion` PASS.
`--receiver-admission` remains the previously recorded flaky real-frame timing
scene (one FAIL, one PASS, one access violation across three runs); it is not
changed by this slice and is not used as an acceptance row here.

Standalone policy gates (warning-clean `-std=gnu++17 -Wall -Wextra -Werror`):
`p2_kurage_flight_policy_test PASS checks=35`, `p2_kurage_fsm_test PASS
checks=35`.

## Admission gates

| Gate | Status | Note |
|---|---|---|
| A Identity/content | PARTIAL | Kurage (id 57) visuals/receiver bound; ordinary generated-actor binding unchanged and not re-run here. |
| B Source behavior | PARTIAL | Wait->Attack vertical life is FSM-driven; animation playback is the static converted pose, motion-END is a bounded stand-in. |
| C Combat/receivers | PASS (bounded host) | Ordinary Attack suction autonomously admits and attaches a live Pikmin. |
| D Death/drop/transport | BLOCKED | No corpse/pellet/Onion transport in this slice. |
| E Lifetime | PARTIAL | Owner-death release restores scale; late birth/recycled address not exercised here. |
| F Persistence | UNTESTED | No restart/save path in this slice. |
| G Product/mixed scene | UNTESTED | Private opt-in host, not a generated-session launch. |

## Remaining

Moving suction joint (converted MOD omits JNT1), real animation/event playback
(#431), ordinary `TEKI_Frog` -> Jellyfloat spawn replacement, Greater captain
capture and Drop, materials/opacity, corpse/reward, restart and generated-seed
admission all remain open.
