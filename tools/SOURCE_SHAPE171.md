# Genuine selected model constructor diagnostic (#148)

`pikmin_ci_fixture_source_shape171` clones the real root production source graph
with a replacement main. It uses actual `System`, native `Shape`/`BaseShape`,
production endian `Stream`, native texture registration, and the reviewed White37
allocation arena and scoped renderer cleanup (`c0ec489dfe4978f2183d28bc26a02fe7623b2b2c`).
There is no Shape/Scene double, Body bank, Services installation or gameplay World.
The dependency-only harness base is the qualified `b6ff17d617d2a59d4b8260f13d193e642b7faa54`.

The input tool verifies all 364 role hashes in the actual red starting-bank
contract against its existing 1327-role development packet receipt and the RGB04
producer directory. It emits a private TSV containing exactly 171 selected MOD
buffers. The executable rechecks role uniqueness, count, extent and SHA256 before
any native construction. Those checks attest exact constructor inputs; selected
SDK/Stage admission remains separate. Neither tool copies assets into source.

Build only the explicit diagnostic target in a private graph. Run locally with
the generated private TSV as its sole argument. CI build/package does not need
the game assets. Keep the manifest, model bytes and runtime logs under ignored
output. This console diagnostic does not launch gameplay or a window; any later
gameplay fixture still requires 20 Pikmin/two source captains and centered 960x540.

The diagnostic constructs a real System and real bounded App heap for reader
bookkeeping. Model roots, nested allocations and aligned arrays belong to each
creating-thread native allocation arena. Owner C++ bookkeeping stays outside
that capture. It performs native read, external-resource refusal, texture-name
resolution, initialise, initIni(false) and optimize for every selected model.

A separate genuine model remains live while three full 171-model cycles load
and dispose. Each cycle observes real texture/graphics registrations, rejects
wrong-thread and nested capture/disposal before mutation, then removes only its
owned graphics references and releases arena storage. The retained foreign model
must stay unchanged. Half-buffer failures at indices 0, 85 and 170 test genuine
partially parsed graphs and their cleanup. No corrupted bytes acquire admission.

Results must report actual constructor counts, allocation observations and the
graphics plateau. These checks do not establish GL device upload/rendering,
source actor allocation, species/FSM/animation, selected Scene/World ownership,
the six strong Body lifecycle APIs, ordinary source20 mechanics or physical SAVE.
Synthetic callback/census/state flags cannot fill those gaps.

## Purple/White source-bank diagnostic

`import_p2_piki_source_bank.py` authenticates GPVE01 archive/model/parameter bytes
and imports the same fourteen-motion, 171-model baseline for Purple and White.
Their ordinary bud/flower assets are genuine `bud.bmd`/`flower.bmd`; Red assets
remain unchanged. The bank has 365 roles: the original 364-role counterpart plus
`joint-anchors.json`, containing eleven sampled source joint matrices per pose.
These anchors do not implement live source bone ownership or animation clocks.

`source_shape171_species_inputs.py --bank <private-bank> --species purple|white
--output <fresh-private.tsv>` verifies the complete bank and emits the distinct
`SHAPE171_SOURCE_BANK_INPUTS` schema. The constructor reader accepts its exact
Purple/White prefix and source receipt identity; it never relabels models as Red
or manufactures a selected-packet fingerprint. Existing Red schema and tests
remain supported. Run the actual native fixture with the TSV as its sole argument.
This tests constructors and cleanup only. Full source FSM motion coverage,
selected Body/Scene adoption, effects, impact/poison gameplay and SAVE remain open.


### Separate full registered motion source successor

`import_p2_piki_motion_sources.py` retains all 67 registered genuine BCA files,
the authenticated species BMD, Piki parameters and original `animmgr.txt`.
Its bounded scan permits genuine authored zero scale, while checking framing,
skeleton, track offsets/lengths and finite values. This raw source closure has
71 roles and grants no native animator, normal history or Scene authority.

`import_p2_piki_source_bank.py --coverage registered` converts 65 clips with the
unchanged strict decoder, producing 770 sampled MODs per species. It retains
raw IDs19 GrowUp2 and63 Suwareru with an explicit unconverted reason. Purple's
untextured body uses the genuine TEV register0 base color; materials still use
the established simplified pipeline. The default fourteen-clip/171-model bank
and its constructor input schema remain unchanged.

GrowUp2 frame12 has three triangles affected by a singular direct draw matrix
that retain nonzero area. Omitting these triangles or inventing normals is
invalid. Retail J3D swaps normal destinations per actual native viewport draw;
its exact-zero determinant inverse transpose leaves the destination unchanged.
The actual Body animator must own those two buffers through each actor lifetime,
including motion transitions, before claiming live support for both zero-scale
clips. Offline sampled geometry cannot establish current-camera lighting,
complete animation/FSM, gameplay or SAVE acceptance.


`import_p2_piki_zero_sources.py --source-bank <raw67-bank> --species purple|white
--output <fresh-private-output>` exports the exact105 raw transform frames for
SHA-pinned19/63 plus original unbaked geometry, draw entries, authored joint
parents and remapped bind transforms. `piki_source_geometry.py` is a literal
snapshot adapter of the established converter parser with only the final bake
omitted; it pins the genuine models before parsing. All work completes before
fresh output creation. The shared converter and its singular-normal rejection
remain unchanged. Exported raw scale/rotation/translation values retain exact
source samples and hold-last track indexing. No normal matrices are generated.
The actual owner must consume source draw/vertex/normal indices and maintain
its own view-space destination buffers; these data files do not grant that
lifecycle, a native animation clock, or a selected Scene.
