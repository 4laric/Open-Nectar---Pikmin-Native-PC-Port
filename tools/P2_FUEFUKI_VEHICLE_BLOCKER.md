# Fuefuki native vehicle (Napkid 11) — staging blocker (#245)

Status: **BLOCKED**. Recorded 2026-09-14. The lane visual and follow policy run
against a scripted anchor (see `P2_FUEFUKI_VISUAL_EVIDENCE.md`); anchoring them on
a *native moveable vehicle* needs the shared arena/generator path, which does
not currently complete.

## What was tried

1. **Arena path (`experimental.pikmin2_fuefuki_arena.py`).** Built a run dir on
   the original P1 practice stage with a Napkid 11 generator + Chappy control,
   overlaid the converted pose bank and `p2-fuefuki-visual.txt`, and ran
   `nectar.exe --experimental-pikmin2-room`. The run loaded 26 generators /
   24 creatures, then stopped at
   `P2 preview: treasure generator missing` and never reached
   `P2_HARDLANES_READY`; the process ended before the room-preview hardlane
   setup. The P2 room-preview path expects the converted P2 room's treasure
   generator, which the P1 practice arena does not provide.

2. **Direct `tekiMgr` birth in the working converted room.** In
   `p2_fuefuki_follow_runtime.cpp`, `tekiMgr->hasType(TEKI_Napkid)` returned
   **1** and `tekiMgr->newTeki(TEKI_Napkid)` returned a **type-11** `Teki`
   object. The fixture then applied the visible part of
   `GenObjectTeki::birth` (`mPersonality->mPosition/mNestPosition`, `reset()`,
   `startAI(0)`) and reached `P2_FUEFUKI_FOLLOW_RT_READY`. On the next engine
   frame the process access-violated (`0xC0000005`): the hand-initialised
   Teki is missing the generator's remaining setup.

The probe was reverted; the runtime fixture is green again (`7411332e`).

## What is proven / not proven

- Proven: the Napkid type is loaded and constructible in the converted room
  (`has_type=1`, `newTeki` non-null, `mTekiType==TEKI_Napkid`).
- Not proven: a stable, moveable native vehicle. Neither the arena generator
  path nor a hand-rolled `Teki` init produces a frame-stable actor.

## Next steps (owner: integration/arena #186 + lane 28)

- Fix the P2 room-preview setup so the P1 practice arena loads (treasure
  generator requirement), then let `GenObjectTeki::birth` create the Napkid and
  let `pc_p2_hardlanes_setup` bind it.
- Or complete the `GenObjectTeki` init sequence for a direct birth (strategy
  table / brain / motion / nest), verified against a crash-free frame loop.
- Once stable, drive the lane visual/follow from the live vehicle transform and
  assert state-driven clip switching there.
