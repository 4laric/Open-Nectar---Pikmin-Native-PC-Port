# Original Fiery Bulblax and Burrowing Snagret (#1253)

Implementation owner: Codex through shared GitHub account 4laric.

Source ID 33 is FireChappy (Fiery Bulblax), real TEKI_Swallow 4 body with the
imported ChappyBase/FireChappy FSM. Source ID 34 is SnakeCrow (Burrowing Snagret),
real TEKI_Chappy 3 body with the imported SnakeJoint burrow/peck/swallow FSM.
These IDs belong to the original catalog; no AP source roster is required.
Retail genEnemy.cpp chooses EnemyGeneratorBase for both: literal ???? version,
empty tail and null initArg. Common placement, UID, count/deathCount, facing,
drop settings and lifecycle remain in the original catalog and GroupCourse.
Unsupported tails/birthType/treasure providers refuse whole admission.

Provider: p2original::bulblax_snagret::Native::provider(), a GroupProvider.
Central native forget must call pc_p2_original_bulblax_snagret_forget(actor)
before registry retirement/address reuse (Snagret additionally does so in its
family forget). Resource dependencies: Chappy original API including source 33;
batch3 original resources/birth APIs for 34. The course consumer must prepare
the complete Chappy source union (e.g. 2/33/43) once BEFORE per-provider preflight.
Real enemy corpses use PelletView config plus their physical dead bank;
ordinary number drops separately preload their Pellet shapes.

Compile tools/test_p2_original_bulblax_snagret.cpp with provider,
pc_p2_original_spawn_plan.cpp, pc_p2_original_drop.cpp and
pc_p2_original_catalog.cpp. Pass an actual private p2-snagret-bank.txt to test
strict 13-clip timing checks. This controlled-engine guard establishes admission,
reservation, attempted ordinal permanence, null/partial births and failed-bind
cleanup; it does not establish real combat, transport or persistence.

## Direct human gameplay (pending)

Stage a fresh private imported tutorial day 5 with the original literal rows,
20 live Pikmin and a centered 960x540 window. Preserve legal assets/saves/logs
under ignored output. Use the actual course consumer and source positions;
never substitute an AP slot or generated proxy encounter.

1. Confirm 20 Pikmin and the window baseline. Approach the Fiery Bulblax with
   reds using ordinary controls. Observe wake/chase, fire aura, bite/swallow,
   damage and flick. Verify vulnerable colors catch fire and reds resist it.
2. Defeat it through ordinary attacks, wait through the actual death clip,
   and verify one natural corpse plus literal probability/count number drops.
   Carry its real corpse to an Onion and observe population and removal.
3. Approach the Burrowing Snagret. Observe the buried state, authored fast/slow
   emergence, directional peck, held Pikmin capture and swallow, dive/flick,
   and return to the buried state. Attack while buried and emerged to observe
   the actual invulnerability gate. Defeat and carry its corpse normally.
4. Leave/reenter through the real course path: verify cleanup and no stale
   actors/mouth occupants/private pose registrations; observe original respawn
   timing. Save through the actual campaign save UI, exit, relaunch, and verify
   generator deaths/drop state and subsequent respawn timing persist.

Record native commit/executable SHA256/no-work build result and fresh logs.
Stop captain-down/lockout observations; kill only processes owned by this test.
Build and synthetic tests cannot satisfy this human mechanic acceptance.


The real-engine fixture pikmin_ci_fixture_original_snagret uses ONLY the literal
nonloop/5-29.txt#2 SnakeCrow row (UID1389661387), checks real family birth/health,
20-Pikmin and960x540 startup, then observes180 engine updates and cleans/reenters
with a fresh activation. It is partial source admission, never whole-course or
natural attack/save acceptance. Use canonical bounded run_pikmin2_fixture.py
with --experimental-pikmin2-surface tutorial arguments, a fresh staged private
run directory and PASS P2_ORIGINAL_SNAGRET_RUNTIME marker. --manual-encounter
leaves that unmodified literal Snagret running for ordinary human controls.
Actual source33 initgen.txt#17 carries treasure841 and remains explicitly
refused until genuine original treasure resource/birth/delivery/persistence
integration exists. Never remove that treasure to manufacture an accepted row.

The fixture vendors scripts/p2_fixture_captain_guard.h exactly as the owned
p2_original_snagret_captain_guard.h. It checks dead state, dead flag,
nonfinite/low HP and disappearance of an initialized captain immediately
AFTER engine idle and BEFORE movie/pause/readiness/observation/PASS gates.
--guard-negative-test exercises that guard before engine boot: require raw
exit86, P2_FIXTURE_CAPTAIN_DOWN and no PASS. The standalone guard test checks
the same truth table. Diagnostic mode parks the captain on actual course
terrain at least700XZ units from the literal encounter; it does not modify
health. --manual-encounter bypasses diagnostic parking for ordinary controls.
