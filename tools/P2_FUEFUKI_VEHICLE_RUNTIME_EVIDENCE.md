# Fuefuki real-vehicle natural claim — runtime evidence (#245)

Status: **executed, PASS**. `tools/p2_fuefuki_vehicle_runtime.cpp` ran on a real
SDL2/OpenGL window inside the cargo-free practice arena, where the Napkid 11
placement vehicle births through the real generator and `pc_p2_hardlanes` binds
it. This is the first run where the lane FSM reached `Whisle`, claimed real
Pikmin, and the follow locomotion moved them — on a native actor, not the
fixture's own binding.

## Provenance

- Native branch `opencode/p2-lane28-fuefuki-follow`, HEAD
  `c10e663e982ad1abfd41408cebfd7962db9dd404`.
- Private build `output/lane28-fuefuki-build`, Ninja Release, MinGW-w64 g++
  16.2.0, JAudio ON; `ninja pikmin_pc -n` => no work.
- Fixture `output/p2-lane28-vehicle-runtime-03/fixture.exe`, SHA-256
  `170FAA3DB011C364E88F0951A2E9873D587B0B881F4F7C719EC7B929DB4340F4`.
- Run dir `output/p2-fuefuki-arena-real3/556113df…` (arena `p2-cargo-free.txt`,
  `p2-fuefuki-teki.txt`, overlaid pose bank + motion table).

## Marker output (verbatim)

```
P2_FUEFUKI_VEHICLE_RT_WINDOW size=960x540 centered=1
P2_HARDLANES_READY family=Fuefuki vehicle=Napkid gen=245001 type=11 follow_locomotion=actteki_volatile_approx
P2_HARDLANES_READY family=Fuefuki visual=1 clips=8
P2_HARDLANES_READY family=Fuefuki motion=1 clips=10
P2_FUEFUKI_VEHICLE_RT_READY vehicle=-150.0,38.2,1849.9 state=2
P2_FUEFUKI_VEHICLE_RT_STAGE frames=30 state=2 held=0
...
P2_FUEFUKI_VEHICLE_RT_STAGE frames=300 state=4 held=0
P2_FUEFUKI_VEHICLE_RT_STAGE frames=330 state=7 held=0
P2_FUEFUKI_VEHICLE_RT_CLAIM state=7 held=2 frames=335
P2_FUEFUKI_VEHICLE_RT_MOVE held=2 moved=190.4 frames=3 state=7
PASS FUEFUKI_VEHICLE_RUNTIME
```

Phase detail:

- The fixture moves the captain 400+ units away and places six staged Pikmin in
  the 60..130 unit annulus around the vehicle, so the hardlane probe reports no
  intruder and the FSM can leave `Land`.
- FSM progression: `Land(2) -> Jump(3) -> Stay(1) -> Land(2) -> Wait(4) ->
  Whisle(7)`. Reaching `Wait` needed the separate motion-state clip mapping
  (below); before it the Land state was fed the looping `wait.bca` and stalled.
- At frame 335 (state 7, casting) the real whistle claimed **2** Pikmin
  (`held=2`).
- The follow locomotion then moved the claimed Pikmin 190.4 units (flick +
  volatile-velocity drive) — real motion on the real vehicle.

## Fix that unblocked it

`pc_p2_fuefuki_visual_clip_for_state` maps `Land -> wait` because `landing` has
no converted geometry. The motion feed must instead use the real `landing.bca`,
so `p2_fuefuki_motion_clip_for_state` was added and the hardlane driver uses it;
the visual keeps the wait pose. Without this, `Land` was fed a looping clip and
never delivered its END, so the FSM never advanced past `Land`.

## Honest limits

- The vehicle is **Napkid 11**, not enemy 41 (`native_identity` BLOCKED).
- The FSM is fed by the converted motion event table, not source skeletal
  Beetle animation (#128).
- `moved=190.4` combines the beetle's Jump flick with the labeled
  `mVolatileVelocity` follow drive; follower count was small (2). No combat,
  death, reward or persistence in this run.
- Whistle effect ring/audio and retail material fidelity remain open.
