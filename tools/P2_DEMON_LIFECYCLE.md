# Demon lifecycle continuation under current family ownership (#242)

Dependent worktree from frozen36184162; prior fixture04 remains immutable.
The new workflow52cfbb3 authorizes complete private native candidates. This change
addresses known captain semantics with focused peer review, not generic physics.

A PC-only NaviStateMachine::transit override invokes a family hook before the
existing base implementation, only affecting a currently owned Demon drop.
Named audited destinations Walk, Dead, Flick, Geyzer, Bury, Pressed and DemonDrop
quench old actual/target/volatile velocity before next-state initialization.
Flick/Bomb/Bubble/Fire inputs use mFlickIntensity, retained untouched. Geyzer's
launch destination is assigned after transit; its new velocity is generated in
exec. Bury/Pressed carry no incoming vector. Unknown destinations preserve inputs
and emit an unaudited marker: they remain outside this candidate's guarantees.
Valid ground/contact is preserved on ordinary state handoff, unlike reset.

GameCoreSection::exitStage now revokes every active registered drop state before
nulling naviMgr. This cancels pending damage, clears captain/expected/dispatch,
and retires admission permanently for that state instance. No animation, physics,
or state transition occurs at scene exit. NaviMgr iteration uses its public
Iterator interface. Empty manager is safe. This follows the actual quit path in
plugPikiColin/newPikiGame.cpp, before subsequent soft reset/App-heap disposal.
Navi::doKill is not relied upon: it does not cover this scene path.

Lifetime boundary: native state/animator memory is later discarded by the App heap;
no pointer to its deque listeners may be called after disposal. Retiring protects
the interval before disposal and prevents re-admission of that instance. This is
not process-lifetime listener storage or a proof of arbitrary external async
callback safety. Direct GameExit/App-reset routes still require the production
invariant that an active course has run exitStage first. No generic heap-reset
hook dereferences a possibly stale global manager to paper over that invariant.

New runtime modes supplement all nine prior registered tests: direct falling
handoff to Walk; real Flick and Geyzer receivers with intact payloads; Bury and
Pressed handoffs; actual live GameCoreSection::exitStage. The exit mode probes an
obsolete listener only BEFORE heap disposal, verifies retired admission, then
terminates the private process. It does not claim scene reload/allocator reuse.
Drop setup remains explicit Walk/position injection, without natural enemy capture.
