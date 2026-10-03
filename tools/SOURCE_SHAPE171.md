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
