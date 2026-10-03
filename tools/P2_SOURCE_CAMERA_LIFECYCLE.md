# Selected source camera prerequisite (#1228)

Implementation owner: Codex through shared account `4laric`. This change supplies
bounded source-semantic camera math and a process-static culling flag. It does
not install a source PlayCamera, CameraMgr, Graphics viewport list or grant GL
acceptance. Resource #1252 must retain `P2_PIKI_SOURCE_VIEWPORT_UNAVAILABLE` until
the owned port below exists. The existing source Body/Scene bootstrap remains
the authority for selection and physical admission.

## What is actually retained today

`src/plugPikiKando/gameCoreSection.cpp` constructs P1 `PcamCameraManager` at
`2521`, a second native `Camera`/Pcam manager at `2526–2532`, and selects those
through `getViewCamera` (`5322`). `beginView` (`5491`) updates the P1 camera with
window aspect, P1 FOV, first-person/ordinary near distances and caller far clip.
`draw` (`5535`) multiplies that camera's perspective and look-at matrices.
`NativeViewScope` at `5539` authenticates the draw's Graphics pointer and slot;
it contains no camera, projection, source Viewport or ParticleMgr list.

There is no retained P2 `PlayCamera` / `CameraMgr` here. Reinterpreting those P1
fields or copying P1 frustum planes would supply a different controller and
projection. The source port must own the render transform as well as culling.

## Original hooks and required owned port

All original paths below are relative to the read-only
`native/pikmin2-research` checkout.

| Lifecycle | Original implementation | Required native hook |
| --- | --- | --- |
| Construct | `src/plugProjectKandoU/baseGameSection.cpp:2198`: two PlayCameras retaining actual source Navi objects, then CameraMgr/loadResource and BlendCamera | Selected Scene owner retains two source PlayCameras and CameraMgr; tie ownership to canonical context, selection revision, native serial and descriptor, never an ambient Graphics camera. |
| Load parameters | `src/plugProjectNishimuraU/CameraMgr.cpp:27,421`: two CameraParms plus VibrationParms read from caveCameraParms.txt for cave / groundCameraParms.txt otherwise | Extend the selected source-input publisher with the genuine raw camera parameter role and original Stream/parser semantics. Do not substitute constructor values for missing resources. |
| Bind/reset | `src/plugProjectKandoU/baseGameSection.cpp:554,2352`: splitter off, two retained Graphics Viewports setCamera, CameraMgr setViewport/init(Olimar), then ParticleMgr setViewport/start | Extend the selected `initStage` admission chain (`gameCoreSection.cpp:2219`) after both real source captain resets and Body/Plate construction, before activation. Allocate the actual source viewport array once; retain it through effect cleanup. |
| Initial controller state | `src/plugProjectNishimuraU/playCamera.cpp:14,82`: constructor defaults; init requires actual Navi and both parameter owners, selects Mid/Behind, derives source target/Plate weighted position and faceDir+PI, copies FOV/near/far, builds look matrix | Source targets must borrow the actual two source captain objects and their ordered Plate bodies. No P1 squad count, fixed target, fixture-camera values or guessed camera distances. |
| Advance | `src/plugProjectKandoU/baseGameSection.cpp:378` updates CameraMgr in doDraw, including paused ControllerLockBoth path | Once per actual source section draw boundary before viewport/model/effect draws. Do not advance once per viewport or use the P1 updateCoopCameras loop. Respect actual source frozen state and controller lock. |
| Select/copy | `src/plugProjectNishimuraU/CameraMgr.cpp:115,362`: Both updates both; active Navi updates one then copies look matrix/CameraData to the other and calls noUpdate; no active Navi updates both | Preserve the original mode and active source Navi identity. A shared camera does not collapse two referenced viewport identities. |
| Matrix/planes | `src/plugProjectNishimuraU/playCamera.cpp:187,222,243`; `src/sysCommonU/camera.cpp:341`: save previous projection/view, source doUpdate/updateMatrix or JST, screen constants, then six planes | Retain current/previous source transforms and projection. Build the source Rx/Ry/Rz/T matrices and copy the owned source projection and view to native rendering, suppressing P1 beginView recomputation for this selected source path. |
| Viewport refresh | `src/sysGCU/graphics.cpp:169,190,208`: source flags/bounds2 determine viewability; refresh updates aspect | Preserve original splitter and viewport bounds/flags lifecycle; use the centred 960x540 fixture as outer window only. Projection aspect comes from the retained source bounds2, not global window aspect. |
| Draw | `src/plugProjectKandoU/baseGameSectionDraw.cpp`: newdraw_drawAll sets source J3D view/current view matrix, viewport and projection, followed by source model/particle draw | Under the authentic NativeViewScope, verify the corresponding retained source viewport and transform are the ones copied to Graphics; the culling predicate still visits all referenced source viewports. |
| Retire | `src/plugProjectKandoU/baseGameSection.cpp:1919`: ParticleMgr killAll/reset precedes camera del and cameraMgr del, then Navi/Piki reset | Revoke draw/scene borrows first, retain viewport/camera storage through effect cleanup, then release cameras before source Navi/Body destruction. Hook canonical scene release (`gameCoreSection.cpp:1052`), preserving current refusal preflight. |

The controller port also needs the literal `PlayCamera::setTargetParms` (`457`),
`changeTargetTheta` (`572`), `changeTargetAtPosition` (`589`), `updateParms` (`623`),
zoom/input/demo transitions and vibration update. Target position includes the
ordered source CPlate creature positions weighted by the loaded detached-weight
parameter. Camera collision and source GameSystem frozen state must come from
their real owners. Supplying a constant camera while these inputs are absent
does not implement this port. Codex retains this prerequisite scope under #1228;
Scene #930 supplies actual captain/Plate/map borrows, SAVE #1229 supplies the
actual source GameSystem state, and Resource #1252 consumes the retained list.

## Math leaf and process flag

`pc_p2_source_camera_math` ports Camera/CullFrustum plane order 0..5, the source
double tan/atan with float casts, inverse-translation position (or genuine
running-JST position), Plane's `dot(normal,position)-offset < -radius` rejection,
Viewport flags/bounds2/viewable and zero-dimension aspect rule. Matrixf's unusual
struct names map side/up/view to **storage rows**; inverse translation uses
columns. ParticleMgr count is the original exclusive assertion `0 <= count < 4`.
The math cull loops all referenced viewports and preserves the old output on
missing-camera/nonfinite/bounds refusal. Input structs alone grant no provenance.

`ParticleMgr::disableCulling` is a process-static volatile bool, zero initialized
to false. The available original `.cpp/.h/.s` corpus contains its declaration
and three reads, with no named source writes. Constructor/reset/start/setViewport
do not change it. `processDisableCulling()` ports that lifetime; no new setter,
selection reset or guessed named event is invented. The eventual owned resource
adapter must read this function rather than use its own default or environment
toggle. The math API's explicit bool exists for engineering controls only.

Actual halo BEM userWork `0x10` gives source small radius 10; flags `0x20` and
`0x30` give 30 and 100, no radius bits bypass culling. Resource #1252 owns decoding
the genuine selected BEM flag. Current draw slot and active viewport count never
replace ParticleMgr's retained referenced array.

The SDK axis rotation shape uses host sqrt/sin/cos. It does not emulate PPC
`frsqrte`, fused/paired-single rounding or the original libm bit-for-bit. This is
source-semantic host math, with explicit tolerance in analytic plane controls,
not a PPC floating-point equivalence or gameplay result.

## Bounded validation

Build the standalone control in an ignored private directory:

```powershell
& C:/msys64/mingw64/bin/g++.exe -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Werror -I pc_port pc_port/pc_p2_source_camera_math.cpp tools/test_p2_source_camera_math.cpp -o ../course-startup-148/source-camera-math-control01.exe
& ../course-startup-148/source-camera-math-control01.exe
```

36 controls cover plane order/sign, aspect, rotated/translated inverse position,
JST override, exact near/far sphere boundaries, four side exclusions, source
process default, second/third-viewport visibility, flags/dimensions, empty viewport
list, exclusive count bound, missing camera despite earlier visible or disabled
culling, and refusal output preservation. These controls establish no canonical
Scene/Body, actual camera update, particle installation, GL draw or gameplay.

After the owned controller port, acceptance requires a genuine selected startup,
camera motion with both native slots, a sphere invisible in the draw slot but
visible through another referenced viewport, live source viewable transitions,
selection/retirement refusal and actual halo effect GL/gameplay. Keep that
acceptance blocked until the source-owned render transform and viewport list are
retained and matched at the draw boundary.
