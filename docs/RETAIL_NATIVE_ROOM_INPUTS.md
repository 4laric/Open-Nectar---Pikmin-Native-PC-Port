# Native selected room and water inputs (#930)

`P2_RETAIL_DEVELOPMENT_FLOOR_2 tutorial_1 <1|2>` requires eight ordered SHA
lines: `plan`, `geometry`, `routes`, `start`, `pool`, `layout`, `room-census`,
`water-census`. The first six roles retain the version-1 meanings. The two new
roles are `p2-original/retail-caves/tutorial_1/floor<N>/room-census.json` (1 MiB)
and `water-census.json` (64 KiB). Missing, changed or malformed inputs refuse.
Version 1 remains supported and provides neither new owner getter. No old
packet is rewritten. These are the exact reviewed authored profiles from root
b903531e and c3d387d; independent whole-buffer pins authenticate their original
archive/member provenance rather than trusting hashes described in the JSON.

`P2_RETAIL_DEVELOPMENT_FLOOR_3` preserves the eight version-2 digests and adds
the mandatory ninth `floor-parameters` digest. Its role is
`p2-original/retail-caves/tutorial_1/floor<N>/floor-parameters.json`, at most
64 KiB. Versions 1 and 2 are unchanged and have no floor-parameter getter.
The supported root 0b21d409 profiles retain the authenticated original caveinfo
member. Native decoding re-reads its literal parameter lines, header count,
unique explicit f000/f001 ranges and all selected values; f013 must be explicit
0 or 1. `hasHiddenCollision` is exactly `f013 == TRUE(1)`. A missing flag or role
is unavailable and cannot become false. Actual SceneRuntime owns the parsed
floor parameters under the same serial/revision/thread/session lifetime guards.
The getter is a construction view, not live trace/contact admission.

`P2_RETAIL_DEVELOPMENT_FLOOR_4` preserves version 3 and requires a tenth
`source-routes` digest for `floor<N>/source-routes.json` (256 KiB). Independent
whole-buffer pins qualify the two root c7c28d7d profiles. Native decoding
re-admits room/water/floor bindings, checks the retained pool member against
the exact selected pool, and re-decodes raw original route.txt ordinals,
decimal tokens, binary32 position/radius, and ordered links. The independently
pinned construction recipe preserves first-created radius, shared waypoint
membership and room mappings; ordered full-eight links are reconstructed from
literal local prefixes. Counts include declared -1 holes, only unused tails
are padded -1. No links are sorted or deduplicated. This bounded implementation
does not independently implement the general original unit-pool door parser;
whole qualified profile pins authenticate those definitions and construction.

SceneRuntime owns immutable `SourceRouteInputs`; the strong construction-view
getter requires the same current context/thread/session/serial/revision and
refuses Releasing. These are not positioned live WayPoints. Actual source
MapMgr.getMinY, map.linkable/inverse links, room flags/visited mutation and the
ordered FakePiki room callback remain prerequisites. Never use routes.ini or
native P1 openness for them. Earlier versions and existing packets are intact.
The source exit audit also disproves a zero-platform shortcut: successful
Hole.onSetPosition creates collision-enabled futa/side; BigFountain creates
foun. Actual source PlatMgr, geometry and lifetime producers remain required.

`parseSourceRoomCensus` verifies selected buffer/plan bindings and re-decodes
retained original binary32 bits, A/B/C order, mapcodes, serialized unit bounds,
vertex bounds and divider headers from the raw members. Signed zero is retained.
The actual SceneRuntime adopts ordered immutable unit/room records and retains
the original bytes. The declared iteration/index and makeOneRoom arguments are
engineered development authoring, not original random RoomMgr generation.

`parseSourceWaterInputs` re-admits room bindings and verifies original 31-byte
version-0/count-0 members. Missing is not zero. These are authenticated raw
inputs, not an installed source SeaMgr or a current cached actor-water state.

`pc_p2_retail_scene_rooms` and `pc_p2_retail_scene_water_inputs` require the
actual context plus the borrower's captured native serial and selection
revision. They refuse old versions, replaced/releasing owners and wrong native
threads. Returned const views are borrowed: the scene must outlive borrowers,
and consumers must recheck the owner before access. Scene ownership records a
monotonic creating-thread token, not a reused OS thread ID or bank-first caller.

The source-geometry successor calls Numeric1261's makeTR/PSMTXMultVec helper
from dd5b91f2e, with explicit f32/FMA boundaries and paired-lane order. The owner
retains actual matrices, transformed source vertices, offset A/B/C, mapcodes,
triangle-to-room indices and expanded vertex bounds. Portable quarter-LUT
arithmetic controls pass; original PPC/libm runtime bit identity is unobserved.
The room FMA backend is superseded by Numeric411a985a: exact binary32 products,
binary64 TwoSum residual and midpoint correction avoid the observed host
MinGW fmaf error, including signed finite-MAX overflow boundaries. Scene930
reruns both actual floor vertex/matrix/triangle-provenance controls with that
backend. Earlier dd5/3866 receipts remain engineering evidence. Original
hardware/FPSCR comparison remains unobserved.
A native quantized quarter-turn matrix is not substituted. Source combined
planes/spheres/grid, hiddenCollision and active Plat lifecycle remain absent.

SceneRuntime registers each source-empty SeaMgr list against the adopted room
matrix and native serial. `pc_p2_retail_scene_find_water` requires the exact
current context, captured serial/revision, owner thread, actual map and retained
registered room/matrix census. It returns KnownDry only for the authenticated
version-0/count-0 profile; unavailable input/map/owner is a refusal, never dry.
The result defaults to Unavailable, outputs stay unchanged on refusal, and a
successful result pins the actual source geometry/water views. Queries revoke
at Releasing before body teardown; storage remains through body retirement,
then checked map release retires registration. No nonempty WaterBox or drain
object is fabricated. Nonempty profiles remain unsupported.

Captain/body must perform source checkWater at the actual FakePiki doAnimation
phase and update its lifetime-bound cached water with the original callback
ordering. Receiver callbacks read that cache; they must not recompute position.
The scene query grants no actor phase, movement/contact, World activity,
original RNG or SAVE authority. Actual native query/retirement runtime
acceptance and full production linking remain pending.
