# Kurage/OniKurage flight-FSM host and autonomous Attack suction (lane 29, #243)

Lane 29 / native worktree `output/native-lane29` (branch
`opencode/p2-lane29-jellyfloat`) based on the approved native baseline
`f14c6851473ac1161be56c8b98f4f905232f3635`, then `4edb7bad` for the Lesser
slice. This document covers two slices:

1. **Lesser** — move the transcribed source Kurage flight lifecycle onto the
   bounded real host and connect the ordinary **Attack** state to the suction
   admission scan (the lane's primary remaining natural gate).
2. **Greater** — add the OniKurage (id 72) variant and its source `Drop` state.

The player-visible actor is still the private bounded host, not an ordinary
generated `TEKI_Frog`; that replacement is tracked separately.

## Lesser slice

`pc_port/pc_p2_kurage_arena.{h,cpp}`:

- Opt-in FSM authority: `pc_p2_kurage_arena_fsm_enable/enabled/state/altitude`.
  When enabled, the host's vertical motion and state come from
  `p2kurage::Fsm` (Wait/Move/Chase/Attack/...) instead of the preview sine.
- `getSearchedTarget` host approximation `findSuctionTarget()`: the first live
  Pikmin inside the source vertical suction window (`inSuctionWindow`) and
  `mAttackRadius` whose sticker is not the owner.
- The source Attack state starts the retail `attack.bca` clock
  (`pc_p2_retail_player.h`) on entry and consumes its KeyEvent 2/1 as the
  `p2kurage::In::keyEvent`; during the open suction interval the host calls
  `pc_p2_kurage_receiver_scan_admit`, so ordinary Attack itself initiates
  capture.  `p2retail::Player` advances in animation frames (30 fps), not
  seconds.
- `pc_p2_kurage_arena_set_owner_facts(hasHealth, bittered)` supplies the source
  bitter/zero-health pause gates; owner death
  (`pc_p2_kurage_arena_update(delta, false)`) releases through the existing
  captured-scale-restoring receiver path.
- `tools/p2_kurage_runtime.cpp`: `--flight-fsm-admission` and
  `--flight-fsm-death`.
- `tools/run_kurage_flight_fsm.py`: prepares a fresh room session from a base
  session and runs one scenario, recording a JSON provenance row and log.

## Greater (OniKurage) slice

- `pc_p2_kurage_arena_set_greater(true)` reconstructs the shared `Fsm` with
  `Variant::Greater`, which switches the pitch numerics and registers the
  OniKurage-only `Drop` state (`pc_p2_onikurage_fsm.h`).  `Pikmin` suction is
  OniKurage's Kurage-verbatim loop on the shared receiver, so it needs no new
  path.
- `pc_p2_kurage_arena_set_captain_held(bool)` is the labelled host seam for the
  Attack-END -> `Drop` route.  Real captain capture (`InteractSarai`, two mouth
  slots) is lane 12 provider work and stays **BLOCKED**; the seam exists only so
  the source `Drop` fall can be exercised.
- While in `State::Drop` the host integrates gravity (`kDropGravity`) and feeds
  `velocityY`/`dropShouldFinish` facts to the FSM.
- `tools/p2_kurage_runtime.cpp`: `--flight-fsm-greater` and
  `--flight-fsm-greater-drop`.

## Fixture stabilization

The private fixture birthed a Piki with only `init()` + a direct `mMode`
assignment.  Drawn free before capture (as the admission paths do), it could
fault in `ViewPiki::refresh -> CollInfo::updateInfo -> Vector3f::multMatrix`.
The fixture now mirrors the known-good init (`initColor`/`setFlower`), which
removed the crash across every scenario (0 crashes in 6/6 runs each of
admission/death/greater/greater-drop/ingestion/kill/transfer/stageexit).

## Evidence

Private build `output/native-lane29-build` (Ninja Release/MinGW gcc 16.2.0,
JAudio ON, test hooks OFF), `ninja -n pikmin_pc`: no work to do.
`bin/nectar.exe` SHA-256
`A0CBEFA91334C7BC4166BB065E39151A58F8FF7DD01AB84EAF2F4C2B2D3F9AAB`.

Fixture `output/p2-lane29-greater-fixture-04` (provenance `status=built`);
`fixture.exe` SHA-256
`56C0553F3EED7834F58ACBA7BDDC2DBCC83DB827350FA33573437D4EB8EA1230`.  All runs
use `PIKMIN_P2_ROOM_WINDOW=960x540` (centred `373,263`) and a 20-red squad.

```
P2_KURAGE_FSM_ADMISSION_PASS variant=57 state=4 auto=1 attach=1 stomach=1 altitude=74.6
PASS KURAGE_RUNTIME flight_fsm_admission

P2_KURAGE_FSM_DEATH_PASS released=1 alive=1 scale_restored=1 state=4
PASS KURAGE_RUNTIME flight_fsm_interrupt

P2_KURAGE_ARENA_VARIANT variant=Greater id=72
P2_KURAGE_FSM_ADMISSION_PASS variant=72 state=4 auto=1 attach=1 stomach=1 altitude=67.7
PASS KURAGE_RUNTIME flight_fsm_admission

P2_KURAGE_ARENA_VARIANT variant=Greater id=72
P2_KURAGE_FSM_DROP_PASS variant=72 drop_seen=1 landed_state=6 altitude=75.7
PASS KURAGE_RUNTIME flight_fsm_greater_drop
```

Standalone policy gates (warning-clean `-std=gnu++17 -Wall -Wextra -Werror`):
`p2_kurage_flight_policy_test PASS checks=35`, `p2_kurage_fsm_test PASS
checks=35`.

## Admission gates

| Gate | Status | Note |
|---|---|---|
| A Identity/content | PARTIAL | Kurage (57) and OniKurage (72) variants run; ordinary generated-actor binding unchanged. |
| B Source behavior | PARTIAL | FSM flight for both variants; static converted pose, motion-END is a bounded stand-in. |
| C Combat/receivers | PASS (bounded host) | Ordinary Attack suction autonomously admits and attaches a live Pikmin. |
| D Death/drop/transport | BLOCKED | No corpse/pellet/Onion transport; OniKurage `Drop` is not the Pikmin cargo path. |
| E Lifetime | PARTIAL | Owner-death release restores scale; late birth/recycled address not exercised. |
| F Persistence | UNTESTED | No restart/save path in this slice. |
| G Product/mixed scene | UNTESTED | Private opt-in host, not a generated-session launch. |

## Remaining

Moving suction joint (converted MOD omits JNT1), real animation/event playback
(#431), ordinary `TEKI_Frog` -> Jellyfloat spawn replacement, Greater captain
capture/Drop with a real Navi (lane 12), materials/opacity, corpse/reward,
restart and generated-seed admission all remain open.
