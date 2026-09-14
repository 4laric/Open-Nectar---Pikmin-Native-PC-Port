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

## Ordinary generated-actor slice

`pc_port/pc_p2_kurage_teki.{h,cpp}` — the real consumer (a generated
`TEKI_Frog` bound by the `p2-kurage-teki.txt` sidecar through
`GameCoreSection::finalSetup`):

- `pc_p2_kurage_teki_fsm_enable(true)` is opt-in; the default binding-only path
  (draw + receiver) is unchanged.
- When enabled, `pc_p2_kurage_teki_tick` runs the same source flight lifecycle
  and Attack-suction scan as the arena host: it drives the actor's `mSRT.t.y`
  from `p2kurage::Fsm`, starts the retail `attack.bca` clock on Attack entry
  and consumes KeyEvent 2/1, and admits nearby Pikmin while the suction window
  is open.
- Probes: `pc_p2_kurage_teki_fsm_enabled/state/auto_admissions/fsm_ticks`.
- Three source-faithfulness fixes found by this path: `attackPlaying` is set on
  a successful player start; only stomach-attached Pikmin count toward the
  source fall/flick threshold (mouth-travel Pikmin do not); the isolated
  preview leaves the day/UI overlay active, so the fixture clears it before
  arming (the arena scenarios already ran with it clear).
- `tools/p2_kurage_runtime.cpp` gains `--receiver-auto-fsm`;
  `tools/run_kurage_automatic_binding.py` gains `--scenario binding|auto-fsm`.

## Greater captain capture (lane 12 consumer)

`pc_port/pc_p2_kurage_arena.{h,cpp}` now composes the live captain with lane 12's
`P2CaptainPolicy` (`pc_p2_captain_policy.h`, #130) and the OniKurage
`MouthSlots` policy (`pc_p2_onikurage_mouth.h`):

- `pc_p2_kurage_arena_set_captain_target(P2CaptainPolicy*, captain, Navi*)`.
  The policy owns identity/ownership; the host owns the family-local capture
  during the Attack suction window and the bounded attach while held.
- Capture uses the source `getSearchedTarget`/`naviSearchAdmit` window; the
  slot policy (`capture`, `advanceDefaultOffset`, `isNaviSuck`) drives the FSM
  `naviSucked`/`naviSuckFinished`, so a real capture (not the earlier seam)
  routes Attack END -> `Drop`.
- Release on leaving `Drop` or on owner death calls
  `releaseCaptured` + `onDeath`, so the captain is never lost or duplicated.
- Probes: `pc_p2_kurage_arena_captain_occupied/captured`.
- The isolated room has one real `Navi` mapped to captain A; captain B is a
  nominal present slot so the source zero-control guard is satisfied.  This is
  a labelled lane-29 bounded adapter, not lane-12 live-adapter acceptance (the
  lane-12 doc records the live `Navi`/`NaviMgr` adapter as its next slice).
- `tools/p2_kurage_runtime.cpp`: `--flight-fsm-greater-captain`.

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
`0639B13E76245D83787FEE4A8EB8086A42A21B55E6B91F48B483BE3E4F8F672F`.

Fixture `output/p2-lane29-captain-fixture-01` (provenance `status=built`);
`fixture.exe` SHA-256
`679B033D4E1D2AB786BBB0716EE4DAEE450AD6300CF19DBB8A0D11600196191E`.  All runs
use `PIKMIN_P2_ROOM_WINDOW=960x540` (centred `373,263`) and a 20-red squad.

Greater captain capture (lane 12 consumer):

```
P2_KURAGE_CAPTAIN_CAPTURED captain=0 epoch=1
P2_KURAGE_CAPTAIN_CAPTURED_PHASE captain=A state=Captured
P2_KURAGE_CAPTAIN_RELEASED captain=0 state=6
P2_KURAGE_GREATER_CAPTAIN_PASS captured=1 drop=1 released=1 occupied=0
PASS KURAGE_RUNTIME flight_fsm_greater_captain
```

Ordinary generated actor (frog profile, sidecar `P2_KURAGE_TEKI_1 1 201001 0`):

```
P2_KURAGE_AUTO_BIND_PASS generator=201001 type=0 source=GameCoreSection::finalSetup ...
P2_KURAGE_AUTO_FSM_ARMED ordinary_actor=1 enabled=1
P2_KURAGE_AUTO_FSM_ADMISSION_PASS state=4 auto=1 attach=1 stomach=1
PASS KURAGE_RUNTIME ordinary_actor_fsm_admission
```

Arena variants:

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
| A Identity/content | PARTIAL | Kurage (57) and OniKurage (72) variants run; the generated `TEKI_Frog` ordinary actor runs the Kurage FSM when the sidecar opts in. Visuals remain the private adapter. |
| B Source behavior | PARTIAL | FSM flight for both variants, on the arena host and the ordinary actor; static converted pose, motion-END is a bounded stand-in. |
| C Combat/receivers | PASS (bounded host + ordinary actor) | Ordinary Attack suction autonomously admits and attaches a live Pikmin in both hosts; Greater captures and releases a live captain through lane 12's policy. |
| D Death/drop/transport | BLOCKED | No corpse/pellet/Onion transport; OniKurage `Drop` is not the Pikmin cargo path. |
| E Lifetime | PARTIAL | Owner-death release restores scale; late birth/recycled address not exercised. |
| F Persistence | UNTESTED | No restart/save path in this slice. |
| G Product/mixed scene | UNTESTED | Private opt-in host, not a generated-session launch. |

## Remaining

Moving suction joint (converted MOD omits JNT1), real animation/event playback
(#431), replacing the underlying P1 Frog proxy behavior/motion with the full
Jellyfloat host (the FSM currently drives vertical motion while the P1 proxy
still animates), materials/opacity, corpse/reward, restart and generated-seed
admission all remain open.  The Greater captain capture uses a lane-29 bounded
Navi adapter; lane 12's live `Navi`/`NaviMgr` host adapter is still the provider
gate for captain health/switch/knockout fidelity.
