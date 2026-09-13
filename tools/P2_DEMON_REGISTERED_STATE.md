# Dedicated Demon drop state prototype (#242)

Base2c08d6b8. PC-only appended ID36/Count37; retail0-35 and non-PCCount36 preserved.
New module and four narrow shared file changes are isolated in this worktree.
Exact hook-only diff: output/demon-registered-hooks.patch. Not applied to shared native.

Physics: entry rejects rope/stick/flying/ignore-gravity/disabled movement, clears
current+previous triangle and stale platform, resets fixed/on-ground status and
sets current fixed reference. Assigns actualY=-400, full target=(0,-speed,0),
volatile zero. Dry static floor only. Post-Navi Creature update zeroes actual,
target and volatile only while the same dedicated state owns grounded recovery.
Reset invalidates ownership and clears owned velocity/contact before native reset;
it does not teleport and gravity may resume under the next state at altitude.

Damage: JKoke END commits Lay before real InteractAttack. RAII guard permits
same-stack resume; restart preserves phase. Rejected damage is consumed once,
without retry, and undamaged recovery proceeds. Lethal result leaves the Dead
transition to native outer finishDamage, preventing duplicate Dead init. External
InteractAttack resume explicitly quenches drop velocity before moving to Walk.

Animation issuance: each owned motion gets a distinct retained deque listener,
with captain generation and issuance serial. It validates ownership before calling
Navi::animationKeyUpdated, preserving native finishDamage behavior. MsgAnim is
accepted only within that validated callback scope and matching motion. Existing
Damage animation callbacks through Navi have no issuance scope and cannot deliver
another drop hit. Tokens are retained, not recycled, up to a4096 lifetime cap;
admission reserves room for three motions and then refuses safely. This is a
bounded prototype lifetime, not an unlimited production allocation strategy.
Reset revokes ownership but retains listener storage. State/manager destruction
is NOT wired: callback-domain teardown before token memory disposal remains open.

Unresolved generic interruption ownership:
Generic cleanup cancels only; it intentionally does not overwrite impulses supplied
by an incoming Flick/Geyser/etc. Direct fall->Walk through an arbitrary external
transit can retain falling velocity. Known reset and external attack paths have
explicit dispositions; unknown transitions are NOT production-safe by implication.
Do not enable natural Demon capture until root resolves those callsites. No generic
StateMachine, Creature physics or damage implementation was edited to hide this.

Compatibility: normal P1 InteractAttack is not P2 KokeDamage fidelity; actual capture
binding/owner actor is absent. Scene lifetime, allocator reuse, water, moving
platforms, slope/ledge behavior and arbitrary asynchronous event delivery remain
unverified. No declaration of full enemy completion follows from this prototype.
