# Staged actor allocation ownership

`pc_midday::ActorAllocationGraph` owns an actor root and all constructor-owned
FSM/action/collision/path/formation/controller/effect allocations. Its returned
Navi/ViewPiki pointers are borrows. Retain the graph owner for their lifetime.
Construction and destruction both require the same actual `ConstructorFence` to
remain valid and held. There is no release or live-world transfer operation.

## Free slots

Use `prepareFreeNavi(NaviProp&, slot, NaviAncillaryConfig, fence, error, failAt)`
or `prepareFreeViewPiki(PikiProp&, PikiAncillaryConfig, fence, error, failAt)`.
These APIs do not accept ActorBytes or a logical resolver. They do not read an
existing actor, fabricate serialized fields, consult managers, or infer co-op
settings. Borrowed properties must have the exact compiled NaviProp/PikiProp
type and remain alive with the staged scene.

Navi uses explicit slot 0 or 1, collider capacity 5, plate capacity 1..4096,
controller port 1..4, and finite plate parameters (start offset 0..100, length
limit 10..1000, maximum position size 1..50). ViewPiki uses collider capacity 4
and route buffer capacity 0..32767. A zero path capacity owns no path array.
All nested allocations use the same concrete owner as occupied actors.

Defaults are deliberately inert: null current states and resource bindings,
empty references, no selected action, and no state-entry callbacks. In addition
to the restore-only constructor defaults, free Piki uses its ordinary constructor
radius 8 and `_68=1`; free Navi uses size 20, health from the supplied NaviProp,
lower-motion cooldown 4, zero neutral/throw/seed counters, and light FOV 20.
No RNG-selected color, size or position is presented as recovered state.

**Allocated free storage is not yet birth-ready.** Native
`PikiMgr::createObject()` normally calls `ViewPiki::init()` before making a slot
free; `MonoObjectMgr::birth()` later returns the slot without initializing it.
Before exposing the staged slot, the scene owner must therefore complete and
validate canonical shape/animation/motion-table and leaf bindings, collider
parts, controller/captain references, update registrations, extension identity
reset and explicit initial state/appearance policy. It must also establish a
live-world ownership/fence transfer protocol. Neither allocation success nor a
free pool status proves those requirements.

## Failure and validation

An occupied output is rejected without replacing it. Failed preparation of an
empty output disposes every partial allocation and leaves it empty. `failAt` is
a zero-based ledger allocation boundary; `allocationAttempts()` records the
attempts even on failure. Object entries count once; arrays count their metadata
and backing allocation separately. STL internals remain RAII-owned but are not
individually injected. Cleanup first detaches recursively owning links, then
runs concrete destructors in reverse allocation order, with the root last.

The original `--owned-graphs` engine fixture is pinned independently at 6a97f1a6.
The follow-up `--free-owned-graphs` fixture exercises these explicit free APIs.
Linux syntax/component checks do not substitute for either actual engine run,
resource binding, reusable pool acceptance or complete paused world restoration.
