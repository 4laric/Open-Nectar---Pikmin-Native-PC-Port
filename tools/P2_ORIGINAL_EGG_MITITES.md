# Original Egg Mitites (#1286)

This family producer accepts an authenticated original Egg outcome from #1252.
`pc_p2_tamago_egg.h` is the integration API. The caller converts its group
outcome to `EggGroupRequest`: full parent catalog fingerprint, generator UID,
ordinal, scene epoch and activation; group slot0; Egg origin, already drawn
facing, and source velocity(0,200,0). The parent may be a standalone Egg37 or
an authentic Egg carried by Qurione16. The caller validates that provenance.
Every nested member is explicitly source68, with member0..9 beneath group
slot0. No parent catalog row, AP source binding or placement is fabricated.

Install a genuine typed Honey provider before preparing the original scene.
Its preflight must validate actual ItemHoney resources. `Born` means an actual
typed Honey was born and initialized; `PoolEmpty` means actual manager
allocation failed; `ResourceError` is a broken resource contract and stops the
private process rather than pretending allocation was null. Context belongs
to the selected scene/manager and must outlive the Egg parent and all children.
Honey has nested emitting-member ancestry and reward slot0. The Honey owner
reports actual terminal retirement through `original_honey_retired`.
First absorb instead calls `original_honey_consumed`, which records consumption
without claiming physical retirement. Call `original_honey_retired` only on
actual body cleanup/terminal Dead reconciliation; it preserves any earlier
consumption. Provider replacement remains refused while that Honey body lives.
Retire Honey and report its terminal events before resetting the family.
The original scene owner must retain actual Honey services/context and both
journals until HoneyManager cleanup succeeds after real Piki/Navi forget.
Then release/reset the owned Mitites and clear this ledger. Whole-graph save
must reconcile Honey Dead/first-consumption with member frontiers before
serialization. Scene unload cannot synthesize consumption or reward births.
This teardown/persistence integration belongs to #1252 and remains required.

Prepare after physical Teki resources and the private Tamago pose bank are
staged, before the first outcome. The producer needs no AP actors sidecar.
Preparation requires a valid Chappy physical model, animation manager,
parameters and strategy, although child birth deliberately does not start
Chappy AI. Preparation does not reset existing family actors. Reinstalling the
same provider/context is idempotent; replacement is refused while children or
Honey remain owned. The currently supported original context is surface,
manager limit10. Actual cave30 needs a future explicit scene contract and
fixture, and is refused here.

`original_manager_available` reports prepared resources independently of pool
capacity. Birth first gates both source manager free slots and actual global
allocated Teki slots at10. Falling, dead and corpse-phase bodies occupy slots
until actual forget. Member0 allocation determines source success; follower
allocation failure leaves a partial successful group. An attempted group,
including a null leader, cannot dispatch again until scene reset. This lets
the Egg owner apply its genuine manager-null fallback exactly once.

Birth performs source onInit seven draws per allocated member, the mandatory
discarded manager radius draw, source leader/yaw/offset ordering and follower
Ball inheritance. Original landing retains the initial goal, draws only the
bounce velocity at contact, then draws Walk time on the next update. The
existing family movement and borrowed P1 physics/rendering remain port
limitations; object compilation does not establish retail gameplay fidelity.

Parent teardown does not purge the group. Member0 remains the sole leader
authorization after its retirement; followers clear its pointer without
promotion or an extra panic. Original children leave no carcass and never
birth a P1 Water reward. Child query returns a session-unique token and typed
member identity; group frontier retains born, terminal, retired and Honey
attempt/born/fault/retired/consumed state until reset. Forget removes the live
query before pooled address reuse. `original_egg_scene_release` must run
outside a Teki update; family reset also retires these owned physical children
before clearing registration. Scene release leaves the frontier available
until reset. It does not purge unrelated actors.
The engine kill path is `Creature::kill` -> `BTeki::doKill`, whose native
`pc_p2_forget_teki` call precedes `tekiMgr->kill` (tekibteki.cpp985). Manager
`newTeki` likewise forgets pooled identity before init (tekimgr.cpp329).

Focused validation is `p2_tamago_egg_policy_test` (assertions enabled even in
Release), existing `p2_tamago_policy_test`, and compilation of
`CMakeFiles/pikmin_pc.dir/pc_port/pc_p2_tamago.cpp.obj`. These are source and
contract checks using mock allocation/RNG/provider IO, not production linking
or gameplay acceptance. They exercise the production-used policy and frontier
helpers; they do not execute an engine Teki reset, actual pool address reuse,
real provider resource loading, or physical scene teardown. Those checks remain
pending integration and the human scene test below.

Human acceptance after the owners integrate the genuine Egg/Honey adapters:

1. Use a new private original surface session with the established 20-Pikmin
   baseline. Verify 20 live Pikmin and the centered960x540 campaign window.
2. Break an ordinary Egg through gameplay until its natural source RNG chooses
   Mitites. Observe ten source68 members fall, bounce, astonish and wander.
   Do not inject HP, call birth from a console or stage ten fake placements.
3. Confirm removal of the Egg leaves its live children. Break a second Egg
   selecting Mitites while the first ten bodies remain allocated; observe the
   Egg's authentic allocation-failure Honey fallback rather than another group.
4. Kill a member naturally. Observe one genuine typed Honey, no carcass/AP
   reward, and ordinary nectar consumption. Confirm only member0 panics at
   group landing, including when member0 later retires before its fellows.
5. Allow natural Hide expiry; verify actual member retirement frees source
   manager capacity. Repeat an ordinary Egg encounter after capacity returns.
6. Exit and re-enter the scene. Verify owned children/Honey retire, old live
   queries disappear, retained frontier is observed before reset, and recycled
   addresses acquire only their new identity. Save/resume remains pending the
   original-session persistence owner; the frontier API does not claim restore.
