# Original captain Flick/Koke receiver integration (#1260/#1289)

Private baseline: 0fafbb2867aa6c6136c47a57e6852b4f278ceb1b. Only the owned
receiver module is adapted from capsule c20dddbad; no older Withering FSM,
corpse or held-treasure implementation is imported. Native receiver ID38 stays.

The current state derives captain::State, queries canonical source actor life
and invincibility frames, and delegates genuine Dead preflight/entry to the
captain authority. Missing actor frames stay unsupported. Raw Koke damage is
sent once to canonical addDamage(raw,false); equipment reduction and HP writes
belong there. Refusal has no HP/native-state fallback and no duplicate END retry.

Public key API takes Navi*, expected current NaviState*, actual Self generation,
sourceKey and error. It verifies current owned pointer, internal activation,
canonical actor/world/incarnation, exact typed state and actual SourceBank Self
generation. It does not advance either animator or broadcast P1 animation keys.
The common animator owns both generation guards and clock advancement once.

The source stimulus fence may call pc_p2_source_navi_interaction_dispatch before
legacy actCommon. This TU recognizes only its private final WindInteraction;
recognized instances call guarded actNavi directly, including refusal, while
unknown interaction types remain unhandled. It does not grant source admission.
Component controls use an observable source-fence double and verify that unknown
HP-mutating interactions never execute and legacy actCommon is never called.
The actual Navi stimulus fence and its integration remain the captain owner's
responsibility.

Ordinary Withering wind target eligibility uses the genuine source captain
Creature alive flag (Hanachirashi.cpp780), not native HP. A source-dead captain
with positive native HP is excluded before Wind construction; missing source
lifetime authority is also unavailable. Retail direct InteractWind and
InteractFlick have no alive predicate, so authenticated direct calls retain
that behavior and do not change the source alive flag. Component controls
exercise both cases and the converse source-alive/zero-HP eligibility.
The nearby Flick producer has no alive/HP filter, matching
EnemyFunc::flickNearbyNavi in enemyAction.cpp827; its direct receiver admission
still requires the genuine world, reunion and authenticated source attacker.

Source END1000: Hit enters Fling with JKOKE both listeners None; Fling END is
ignored. Koke enters Timer before addDamage; GetUp restores the genuine saved
source Walk/Follow through captain recover_reaction. JHIT, Koke JKOKE and GETUP
start with Self SourceActor/Bound None. Source assertMotion is exactly the Self
motion identity comparison (FakePiki.cpp109), not an invented completed flag.

Physical Flick bounce/inWater and Fling's current mFakePikiBounceTriangle
producer are missing. Those paths explicitly report missing authority and retain
the existing phase; no native floor/null/no-water guess is used. The nonphysical
source missing-JHIT assertion path can enter Koke with retained flicker. Actual
contact recovery and ordinary gameplay are not qualified by these component
controls. No public invented source contact provider or linker stub is added.
Koke demo Unknown explicitly refuses observation. Actual inactive-world early
Walk currently cannot pass the canonical active-world transition preflight; that
source transition boundary remains an integration gap, not a P1 fallback.

Controls compile the actual receiver TU with observable engine/captain bank and
authority doubles. They test stale state/generation/lifetime/incarnation refusal,
no P1 key delivery, listener changes, re-entry, exact typed phase, authority raw
argument/once ordering, canonical state replacement, recovery and physical
refusal. They do not independently establish damage policy, authored pose bytes,
RNG ordering, angle wrapping or gameplay. The captain owner tests canonical
addDamage separately. Legal assets, logs and binaries remain ignored.

Source feedback delegates only to the genuine selected WalkEnvironment callback;
an absent callback reports missing authority. Presentation/feedback remains open.
The pre-existing Piki whistle/repeated-Blow correction is preserved from the
minimal c20dddbad receiver capsule, not newly qualified Piki source AI here.
