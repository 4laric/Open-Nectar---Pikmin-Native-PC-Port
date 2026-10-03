# Original Sentinel plants (#1284)

Implementation owner: Codex through shared account `4laric`.

The source producer is `pc_p2_original_shijimi_group.{h,cpp}`. It has no
generator aliases or AP identities. Its adapter must use the independent
original parent identity and the child discriminator `(emission=0, member=0..4)`.
Member 0 is the hidden Red/Purple leader; members 1..4 are Yellow followers.
Every source emission consumes the plant sentinel before manager lookup.
An absent manager or a genuine null allocation is terminal, including partial
follower groups. Infrastructure errors invoke whole-emission cleanup and leave
an unpublishable pending record, rather than becoming fabricated null births.

Source ordering comes from retail GPVE01 revision 0 and research files:
`plants.cpp`, `shijimiChouMgr.cpp`, `shijimiChou.cpp`, `shijimiChouState.cpp`,
and `Entities/ShijimiChou.h` in the private read-only P2 research checkout.
The leader performs raw birth, onInit, leaderInit, unused rand(), then its
50% color roll. Each follower performs Y scatter, raw birth (including scale
RNG), color roll, appearance effect, forced Yellow color, and onInit. Failed
raw births omit color/init draws. The source group count is the last successful
follower loop index; it is not a general population counter.

Plant-origin genItem short-circuits the nectar-rate draw. Red/Purple require
their respective first-spray-made flag. Honey must be the real source factory
and execute init(nullptr), including its RNG, before assigning color. Source
velocity uses sin(face)*50 for both X and Z, with Y=200 and position Y+2.
The ledger records failed Honey births and retains ownership after the
Spectralid or its original plant retires. It never synthesizes an Egg journal.

The pure producer test checks the source call trace, RNG interleaving, every
single-child allocation failure, one-shot manager absence, spray suppression,
Honey null birth, cleanup after a half-init failure, and atomic saved-journal
validation. These controls do not qualify a native actor, real appearance
effects, ordinary Pikmin attachment, scene teardown, or campaign resume.

The foliage converter now accepts literal source50 Ooinu_l and source87
Magaret. Source50 is a 60-frame BCA with raw loop attribute2 and no registered
loop keys; source87 is a 74-frame BCA with attribute0 and no keys. Both use
normal-layer Plants behavior. Their fp27 child birth heights are 85 and 70.
Conversion alone does not admit either sentinel into gameplay.

The canonical Honey adapter is `pc_p2_original_shijimi_honey.{h,cpp}` and
`pc_p2_original_shijimi_honey_native.cpp`. It preserves the full plant root
identity and appends `(EmitterKind::PlantSpectralid, emission=0, member=0..4)`
with Honey slot0. It calls the real owner-pinned `honey::Manager::birth` with
the same source RNG. Successful init(nullptr) consumes its original draw;
capacity failure is true/null and consumes none. The actual scene owner's
Honey consumed callback must dispatch this ancestry to `PlantGroups::consume`.
No Egg contents record is created. Spectralid facing is not copied onto Honey:
retail genItem sets only position and velocity after initialization.

`pc_p2_original_shijimi_state.{h,cpp}` carries the plant-origin Wait/Fly/Fall/
Dead/Leave transitions and separate simulation, culling and leader-cluster
phases. Its source controls check attachment-only Fall, the authored END
boundary for drops, exact source counters and the absence of timed Leave
cleanup. It is an unbound mechanical component, not a native gameplay test.
Native services still must provide genuine attachment, floor triangles,
source animation keys, transforms, appearance and scene-owned retirement.

`tools/p2_shijimi_effect_resources.py` extracts verified genuine PID21/22/23
and `IP2_stardust1_i` from GPVE01 game.jpc into private output. This does not
reuse the Watage PID484 renderer. The native continuous chase emitter and
billboard rendering remain unqualified.

The source77 geometry importer `tools/p2_original_shijimi_resources.py`
authenticates all three original archives, literal three-joint rig, collider
and exact carry/dead/move BCA identities before writing the native bank.
The native `SourceBank` consumes 28 sampled render poses and all109 original
joint0 frames. Mechanical collision uses the integer BCA sample and actual
source key clock independently of visibility. Native bodies must own distinct
flattened geometry. SourceBank compilation does not prove body ownership or
Pikmin attachment; both remain native acceptance requirements.

The private composition entry is `tools/original_sentinel_build/`. It consumes
the real resource owner's build harness and adds Sentinel Honey/state sources
and strict controls. Only the coordinated GitHub runner may qualify the heavy
native composition. Syntax-only compilation is not a linked executable proof.

## Direct gameplay smoke script

Use a fresh private original Forest arena/card with 20 starting Pikmin and a
960x540 centered window. Stop on captain down or startup extinction.

1. Walk the captain into the actual source50 forest/plantsgen.txt#12. Observe
   its touch animation and four visible Yellow followers. Verify the fifth
   object is the hidden leader using the typed source journal, not a visible
   replacement actor. Walk through it again after animation END: no new group.
2. Throw a Pikmin onto a follower. Observe real attachment, Fall, Dead END,
   the actual Honey drop, and normal nectar drinking/maturity change. Ordinary
   damage and a synthetic health write are not substitutes for attachment.
3. Repeat at actual source87 forest/plantsgen.txt#28. Its child origin uses
   source fp27=70. Confirm independent parent/emission ownership.
4. Leave the scene with a surviving follower and a loose Honey. Re-enter and
   inspect the actual scene lifetime behavior. Save/resume through the ordinary
   source card UI when that path is admitted. No duplicate child/drop event or
   stale leader reference may survive; saved child state must use no-init
   allocation and must not replay birth/init RNG.

Required native gates remain unqualified until the real dynamic source77
adapter, native receiver/animation clocks, shared typed Honey ancestry, genuine
TChouDown effects, and source checkpoint provider execute together. Preserve
failed runs and label source controls separately from direct gameplay.
