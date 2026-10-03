# Original Catfish source FSM (#1256)

Codex owns this family implementation through GitHub account 4laric. It is
original source26 only. The integration owner connects its API to the original
provider; the existing AP Catfish adapter is a separate caller.

The authoritative behavior is `plugProjectNishimuraU/Catfish.cpp`,
`plugProjectYamashitaU/KochappyBase.cpp` and `kochappyState.cpp`,
`enemyAction.cpp`, `include/Game/EnemyBase.h`, and `sysGCU/sysShape.cpp` in
the read-only Pikmin2 research checkout. Retail Catfish parameter member SHA
is `c251933cb91034fca63e90dbcc3f75819bb8ee9601b5c7918d5ff772deb69182`.
Legal BMD/BCA/parameter files remain private.

The retail values include health200, speed60, turn ratio.1 capped10 degrees
per update, territory280, home80, private70, sight/search200, view/search
angles180, attack/hit range50 and angles25, damage10. Flick has knockback80,
damage1, radius25, chance1. The four blow thresholds are1/1/2/2 with stuck
bands1/2/3; damage counters are rounded then narrowed to u8 as in source.

The indexed enemyanimmgr has nine registrations. Wait6 and Turn7 both use
the actual wait1 BCA with loop keys0/29; its first registration4 has no keys.
The name-only resource bank deliberately does not replace that indexed
source timing. All seven BCA durations/events/sample endpoints are strictly
validated, and the resident Catfish rows must remain unchanged. Source
Animator keys fire only when `key.frame < int(timer)`: bite17 is observed at
pose18, swallow75 at76, flick25 at26 and restore47 at48. Unfinished loops
reset to their start and discard overshoot; finishMotion disables that loop
until END. END clamps the drawn frame to duration-1 and fires once.
The authored body/mouth joints follow before event capture and again after
any state/animation or facing change, matching that frame's visible pose.
Birth's WaitArg 'rand' consumes the source random draw; later Wait transitions
pass nullptr and consume none. Frame0 makes that initial timer zero.
Alive pressed Pikmin remain
searchable; only the host's separate sprout population states are excluded.

Wait searches before finishing its loop; END turns toward the stored target
and enters Walk or Turn. Turn/Walk apply source alert, angles and motion END
decisions; Walk enforces territory and Home states return to the authored
nest. Attack's KEY2 attacks all captains using strict 3D radius and hit angle,
captures through actual two-joint mouths, and flicks body stickers. Zero
captures switches to actual waitact2. KEY3 consumes only held mouths. Flick
returns to the requested state or source previous state at END. Dead entry
spawns original drops once and releases mouths; Dead END invokes the native
kill/corpse funnel corresponding to Demo entry.

`pc_p2_catfish_source_set_flick_receiver` is mandatory before resource
admission. Its callback receives the logical source angle before EnemyFunc's
PI offset. It must perform actual source Pikmin/Navi vector and damage
reactions. There is no borrowed P1 flick fallback. The mouth module is a
separate family dependency. Source press adds stored damage and one enabled
flick count; its retail callback returns false and does not enter Kochappy's
fatal Press state. The native helper reports true for an owned actor so the
shared caller can suppress the borrowed P1 Pressed event.

Remaining integration/acceptance: connect the actual flick receiver and
provider hooks, production compilation/link, typed corpse carry/yield, and
human gameplay/save-resume. Bitter/spray state, purple stun/earthquake,
damage squash animation, effects/sound fidelity and Demo's global captain
unlock timer are not implemented by this FSM and require their owning
systems. This code does not claim those reactions as completed.
Catfish's actual setEnemyNonStone enables NoInterrupt after Flick KEY2;
KEY3, Flick init and Flick cleanup reset it. The family now tracks that flag
and a monotonic clear serial for each actual true-to-false reset. That reset
requests source down/bounce effects; effects and actual Stone remain open.
Retail carcass Carry5 uses type5 duration40 and loop10/29. The
source corpse accessor exposes that clip and phase separately. Pre-carry
starts paused at frame0, source carry-start resumes the clock, optional
restart starts Carcass again, and carry-finish allows the loop to exit at
frame39. Carry-stop pauses without changing frame; start resumes it.
Lifecycle hooks also own the pending escaped corpse during Pellet::init,
before becomePellet assigns its pointer and dieSoon sets dead-state2. The
corpse accessor and ticking still require the actual retained pellet.
BTeki's family update runs before its dead-state gate, so the
retained actor-backed corpse advances without replaying death/drop events.
The shared integration owner must connect actual pellet view callbacks and
the corpse draw override; transport gameplay remains an acceptance gate.

The source lifecycle owner can query `pc_p2_catfish_source_gate` for the
complete registry activation identity/token, alive/dead, health, distinct
invulnerability, literal Catfish bitter immunity false, NoInterrupt, FSM
state/indexed animation/source frame and nonStoneClearSerial. The query
refuses missing/mismatched original registrations. The identity-checked
do_start_stone family callback only zeros target velocity, matching
KochappyBase.cpp129-140 and EnemyBase.cpp1632-1644; do_finish_stone is empty.
These callbacks do not create, accept or track a substitute Stone state.
The actual Stone owner must pause animation/FSM and own event backup,
Bittered/BitterQueued and restoration. No P1 pressed/bitter flags are read.

Queue contract: EnemyBase.cpp1611-1625 queues when NoInterrupt or real Stone
start failure prevents entry. LivingState::updateAlways in enemyBase.cpp
475-482 retries queued start.
Stone init clears BitterQueued (enemyBase.cpp704-705); Stone cleanup clears
it again (745), clears Bittered and resumes motion before the empty finish
callback. PAL LivingState also clears queued on health<=0 (478). Resetting
Catfish NoInterrupt does not itself clear BitterQueued. Family clear serial
is a notification of source reset/down-effect, never queue consumption.

For direct gameplay use exactly20 Pikmin, the current original start overlay,
a centered960x540 native window and private arena/save/logs. Observe Wait
notice and Turn/Walk, return from beyond territory, two actual mouth holds,
captain bite, white poison, retail flick thresholds and direction, pressure
damage without instant crush death, drops at death entry, corpse transport,
unload/reentry and save/quit/resume. Record exact commit/executable/assets;
portable tests and a build are separate from that gameplay gate.
