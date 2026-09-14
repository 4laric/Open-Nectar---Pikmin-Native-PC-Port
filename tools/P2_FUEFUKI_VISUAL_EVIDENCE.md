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

- **Clip selection** is wired from the lane FSM (`fuefukiVisualClip`) but this
  fixture has no Napkid vehicle, so the binding does not run and the clip stayed
  `wait`; state-driven clip switching still needs a staged vehicle.
- `landing`/`landfail` are absent (singular source joint scale); the profile
  lists only the 8 converted clips.
- Baked rigid poses with approximate materials; no skeletal playback or
  key-event execution, no whistle effect ring, no audio and no camera-facing
  billboard orientation.
- No material/visual fidelity acceptance versus retail.

## Provenance

- Native branch `opencode/p2-lane28-fuefuki-follow`, HEAD
  `c564660b7258b2baad136d2caa64541e5a0ec582` (visual module + hardlane draw).
- Private build `output/lane28-fuefuki-build`, Ninja Release, MinGW-w64 g++
  16.2.0, JAudio ON; `ninja pikmin_pc -n` => no work.
- Fixture `output/p2-lane28-follow-runtime-05/fixture.exe`, SHA-256
  `4CB86081ED9766398B17F63EDE103A811B293C14ED2C5661B1EF85214B411825`.
- Stage `output/p2-fuefuki-stage-01` (`stage.json`: 8 clips, 31 poses,
  unsupported landing/landfail), overlaid onto the run room
  (`assets/dataDir/courses/pikmin2room/`).

## Marker output (verbatim from run.log)

```
P2_FUEFUKI_VISUAL_READY clips=8
P2_HARDLANES_READY family=Fuefuki visual=1 clips=8
P2_FUEFUKI_VISUAL_DRAW clip=wait pose=0
...
PASS FUEFUKI_FOLLOW_RUNTIME
```

## Next steps

- Stage a moveable vehicle (Napkid proxy) so the FSM→clip mapping and the
  visual-at-vehicle transform are exercised end-to-end.
- Provider 09 material fidelity and the whistle effect ring; audio (#128).
