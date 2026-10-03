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
Each original unit also retains the exact sixteen serialized binary32 plane
fields per triangle (triangle plane and three edge planes). Source local
getCurrTri/height/insideXZ query uses these original TriangleTable.readObject
fields after actual inverse-room transformation; it must not substitute the
recomputed planes of the combined movement mesh. Original divider queries and
inverse transform arithmetic are still separate pending implementation.
The original unit grid retains serialized counts/scales and every cell's
ordered triangle-index list, including duplicates, in z+x*maxZ storage. Decode
checks every count/index and exact file consumption. Actual GridDivider.read
overwrites scales with abs(serializedBoundsMax-Min)/count; that source numeric
step and clamped radius-zero query still belong to the actual query backend.
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

Source height adoption uses the frozen Numeric bb4d7fe1 helpers (independently
reviewed pure arithmetic), the actual original four-plane/cell fields, source
PSMTXInverse, transformed serialized unit bounding box and its Y-zeroed _190
sphere. SceneRuntime privately owns the derived ordered query grids/rooms and
guards every query borrow with its actual installed map, context, native thread,
selected campaign/session/revision/serial and non-retiring phase. Original local
triangle identity points into the owned raw plane record. The fresh highest-floor
query preserves a genuine no-hit as null triangle/heights0; unavailable ownership
is an atomic refusal. Hidden-floor true is unsupported until its real sentinel
exists. On version3/4 source starts and captain offsets use this original query,
rather than P1 ground. Earlier inputs provide no source-height owner.

`pc_p2_retail_scene_prebirth_support` is declared in `pc_p2_retail_height.h`.
It takes the genuine SceneContext, captured native serial/revision, absolute
authored position, a SourcePrebirthSupport output and error. It requires actual
Prepared Stage ownership, a non-null original supporting triangle at/below the
authored Y and registered source-empty KnownDry. The result retains height owner,
raw census, serial/revision, original triangle/index/room, min/maxY/normal and the
typed actual SourceWaterResult. Missing water/support never becomes dry, and the
position is not relocated. Body factory must query every authored slot before
its first Pool birth; an unborn actor needs no BodyBorrowerGuard. Runtime bodies
still require their own lifetime/phase contracts. Queries revoke before body
teardown; original storage survives until checked map release. The real Stage
query path is implemented and compiles, but has not been executed in a full
native Stage fixture. Both authentic inputs prove all20 authored positions with
a pure fixture input owner, not a fake SceneContext/runtime admission.

Version4 now also builds the actual Scene-owned source route state during map
installation. First-created local points use their actual source room matrix;
door Y is0 and other points query the owned original height provider.
During first-created point construction the actual provider uses the original
room birth-prefix roster, then restores the complete owned roster before
makeInvertLinks. Non-door Y remains source-transformed until getMinY replaces
it; only doors force Y0. Qualified profiles also had bit-identical prefix/full
heights, but that narrow observation is not used as a general equivalence rule.
Radius and memberships remain first-created/source ordered. makeInvertLinks performs
real source linkable height samples before reverse-From lookup, retains full
eight slots/counts and refuses To overflow. Fresh flags0 are then setCloseAll
to Unvisited0x80; fresh room visited flags are false. Failed construction
publishes no graph and keeps public height/support queries unregistered.

The current const source graph getter requires genuine context/serial/revision,
owned selected route inputs and registered height/map state. It is distinct
from native routes.ini/P1 openness. The Scene room writer accepts the actual
Navi/room only during Root's private NativeTrace room phase, authenticated by
strong pc_p2_original_captain_room_visit_current; no accepting stub exists.
It rejects reentry with a private scoped mutation guard, stages openRoom flags,
revalidates Root/Scene before publishing, preserves original graph addresses,
clears only0x80 for matching RoomList memberships and then marks that room
visited. Closed0x01 is retained. This happens in the caller's source terrain-
then-room-before-bounce/wall/Plat phase. Genuine Root phase composition and
actual runtime acceptance remain open; pure bit/graph controls alone grant
neither a native room callback nor platforms/SAVE.
