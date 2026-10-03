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

Current views grant no source transform matrix, combined planes/spheres/grid,
hiddenCollision, Plat lifecycle, water query, movement/contact, World activity,
original RNG or SAVE authority. The genuine source numeric transform is a
separate producer; a native quantized quarter-turn matrix is not substituted.
