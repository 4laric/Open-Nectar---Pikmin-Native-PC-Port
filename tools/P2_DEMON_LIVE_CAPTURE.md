# Demon live capture milestone (#215)

This private successor starts from lifecycle `418d4f26344814ffcd9be80c6b4815ab111936aa`.

It adds a PC-only `NaviState` ID 37 for P2 `NaviSaraiExitState`: a successful
escape detaches the real P1 mouth-stick link, starts `FALL`, has no invented
FallMeck velocity, suppresses `Navi::isAtari`, and returns to Walk on a floor
or bounce. The state-based collision suppression restores itself on every
transition.

`pc_demon_capture` accepts an already-selected owner mouth `CollPart`, creates
P1's live `startStickMouth` relationship, then requires the owner and part to
remain exact matches. P1 has no `CollPart::isMouth` metadata, so the enemy host
must select and validate its source mouth slot; the bridge limits the supplied
part to the existing sphere/collision attachment shapes. Native Creature stick
updates own the captive's following transform. Cached visual matrices are not
used for production capture.

The production `Navi::doAI` gate samples D-pad click edges once per AI update
and forwards them to the source-derived six-input escape window. This maps the
P2 analog directional edge to P1's available digital direction edges. The
random sampler is `gsys->getRand(1.0f)` and the policy consumes it only after
six inputs, matching the source's conditional samples.

Forced releases remain distinct: `pc_demon_forced_release(captain, damage,
speed)` detaches before admitting the registered FallMeck policy with the
caller-provided nonnegative damage. Its generation is receiver-global and
strictly monotonic across captors. If admission refuses, the captain stays
detached in Walk. It is never used for voluntary Sarai escape.

`pc_demon_owner_lost` and `pc_demon_scene_exit` revoke bridge authority and
call `endStickMouth` while the captain, owner and collision part still live.
The owner host must call `pc_demon_owner_lost` before its own disposal; no
claim is made after heap disposal.

Validation so far: changed MinGW translation units compile in
`output/demon-live-build-01`; `tools/p2_demon_escape_test.cpp` passes from
`output/demon-live-tests-01`. A live owner/collision runtime fixture remains
required before integration.
