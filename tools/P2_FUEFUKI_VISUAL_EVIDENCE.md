# Fuefuki converted-pose visual — real-GL runtime evidence (#245)

Status: **executed, PASS**. The `P2_FUEFUKI_VISUAL_*` markers below came from an
actual real-GL run of the lane fixture in a room staged with the converted
Fuefuki pose bank (`docs/PIKMIN2_FUEFUKI_ASSETS.md`, `experimental/
pikmin2_fuefuki_stage.py`).

## Scope

Real in this run:

- the frozen P1 host booted with `--experimental-pikmin2-room` on a real
  SDL2/OpenGL window (960×540, centred);
- `pc_p2_hardlanes_setup` parsed the staged `p2-fuefuki-visual.txt`
  (`P2_FUEFUKI_VISUAL_1`) and loaded all 8 converted clips as real `Shape`s
  through `gameflow.loadShape`;
- `pc_p2_hardlanes_draw` drew the active pose through the live camera
  (`P2_FUEFUKI_VISUAL_DRAW`), and the follow-locomotion phases still passed in
  the same run.

Not covered / limitations:

- **Vehicle identity:** the fixture moves a scripted anchor, not a native
  Napkid vehicle. The FSM→clip mapping, pose advance and visual-at-moving-anchor
  transform are exercised; a real vehicle is blocked (see
  `P2_FUEFUKI_VEHICLE_BLOCKER.md`): the arena path stalls before the hardlane
  setup, and a hand-initialised `tekiMgr->newTeki(TEKI_Napkid)` access-violates
  on the next frame.
- `landing`/`landfail` are absent (singular source joint scale); the profile
  lists only the 8 converted clips.
- Baked rigid poses with approximate materials; no skeletal playback or
  key-event execution, no whistle effect ring, no audio and no camera-facing
  billboard orientation.
- No material/visual fidelity acceptance versus retail.

## Provenance

- Native branch `opencode/p2-lane28-fuefuki-follow`, HEAD
  `ca7f9da74eb5e2e38aa4dcd8fd5eb087884c8a3c` (visual module + hardlane draw +
  anchor API + state→clip mapping driven by the fixture).
- Private build `output/lane28-fuefuki-build`, Ninja Release, MinGW-w64 g++
  16.2.0, JAudio ON; `ninja pikmin_pc -n` => no work.
- Fixture `output/p2-lane28-follow-runtime-07/fixture.exe`, SHA-256
  `B2224121314D323C4BD111CF70D680B011A162F180E6BC8C259586F9990FECB2`
  (supersedes `…-05` `4CB86081…`).
- Stage `output/p2-fuefuki-stage-01` (`stage.json`: 8 clips, 31 poses,
  unsupported landing/landfail), overlaid onto the run room
  (`assets/dataDir/courses/pikmin2room/`).

## Marker output (verbatim from run.log)

```
P2_FUEFUKI_VISUAL_READY clips=8
P2_HARDLANES_READY family=Fuefuki visual=1 clips=8
P2_FUEFUKI_VISUAL_DRAW clip=wait pose=0
P2_FUEFUKI_VISUAL_STATE state=7 clip=whisle pose=0
P2_FUEFUKI_VISUAL_TRACK x=0.0 z=-115.0 clip=whisle pose=2
...
PASS FUEFUKI_FOLLOW_RUNTIME
```

The final run adds: FSM state 7 (Whisle) maps to the `whisle` clip
(`P2_FUEFUKI_VISUAL_STATE`); the visual anchor follows the scripted moving
beetle to z=-115.0; and the pose advances within the clip (`pose=2`) because
the clip is only re-selected on a change. The follow-locomotion phases pass in
the same run.

## Next steps

- Stage a moveable vehicle (Napkid proxy) so the FSM→clip mapping and the
  visual-at-vehicle transform are exercised end-to-end.
- Provider 09 material fidelity and the whistle effect ring; audio (#128).
