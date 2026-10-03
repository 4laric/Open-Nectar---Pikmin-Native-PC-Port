# Original cave Pod (#1280)

This receiver is original P2 `Game::Onyon`, `ONYON_TYPE_POD=3`,
`ONYON_OBJECT_POD=1`. It is separate from RGB Onions and the surface ship (4).
Source: projectPiki/pikmin2 `src/plugProjectKandoU/onyonMgr.cpp`, revision
`29bc5478edffa2c963c88fdd261d876d055aa2b0`, especially `setType`,
`getGoalPos`, `getSuckPos`, `onSetPosition` and `InteractSuckDone::actOnyon`.
The port reuses the native `Suckable`, route and pellet suction chassis. Only
the native completed-suction boundary can issue callback-scoped receipt
authority. It never creates an economy ledger, seeds, or P1 ship repairs.

## Resource and presentation boundary

Preflight verifies exact source archive, raw model, collision text and converted
model bytes against the fixed SHA-256 pins in `pc_p2_original_pod.h`. Resources
stay in private output; no game bytes are distributed in source. The known
conversion is the existing `pod.mod` generated from `user/Kando/pod/arc.szs`
`pot.bmd`. The four spheres are original `user/Kando/pod/texts.szs` `coll.txt`,
transformed by the model's original bind-pose joint matrices 2/3. Their radii
are 31, 22, 10 and 13. Physics follows that static model pose. The intake is the
original Pod's position plus `(0,95,0)`; original source placement is preserved.
Native route arrival uses the original bounding radius (31). This is a native
route adaptation, not the original P2 route manager.

The conversion has 367 vertices, 594 triangles and six textures. Model animation,
original TEV, texture matrices, effects, sound mapping and radar presentation
are unfinished. Original wait/pmotion/suction resources exist but are not played.
The static pose and geometry cannot qualify original animation/effect behavior.

## Floor and cargo integration

The floor owner supplies an authenticated `Config.floor` and `ContextProvider`
checking original campaign, selected session/SAVE fingerprint, source cave/floor,
source/catalog hashes and the full seed/visit/layout/serial scene incarnation.
`Config.births` borrows the independent floor/SAVE identity authority; binding
checks exact row/ordinal/epoch/instance/activation against its issued origin.
The provider is available during prepared installation and after commit. A
descriptor by itself is never authentication. Preflight resolves resources and
the native nearest route point before an actor is born. Birth attaches a
dedicated `OBJTYPE_P2Pod` to the existing melting-pot creature manager, which
provides ordinary rendering, search and collision participation.

Prepare/birth the receiver, bind actual registered cargo with its full original
`BirthIdentity`, then commit only after the entire floor installs successfully.
Before commit, goal/context/receipt authority remains unavailable. On a partial
failure, the cargo owner retires its actors and the floor calls
`abort_prepared`; that operation is forbidden after commit or any suction.
The Pod owns only its recipient bindings and receiver, never cargo actor lifetime.

The cargo owner's completed callback verifies `completed(pellet,receiver,scene)`
and exact `context`, then commits the canonical ledger. It must not retire or
rebind cargo during the callback. The native pellet state retires cargo after
the callback succeeds. Failed canonical persistence aborts the process rather
than silently destroying uncredited cargo. Completion checks the actual current
goal state, finished progress, finished wait and live exact receiver. Consumer
code cannot invoke the private native completion seam or forge an event token.

Plain release refuses all unfinished cargo. Active carry, suction, a lost
binding or an outstanding receipt blocks every release. Uncollected ground
treasures are distinct: `release_uncollected` requires an explicit quiescent
boundary callback from the floor/cargo/SAVE owner to retain their actual native
graph, without marking them consumed or changing the ledger. Its absence or
failure leaves the receiver/bindings unchanged. The callback must not destroy
or rebind cargo; the floor owner retires actors after accepted release. This
permits original leave-behind semantics once the owning retention is qualified.
A read-only, versioned pending snapshot preserves every unfinished cargo's
full source/floor/scene/birth identity, including quiescent ground cargo.
Unexpected kills revoke pool-address ownership immediately and preserve only
typed source/scene tombstones. Reused native pool addresses are not Pod cargo.
It grants no SAVE authority. Pending cargo must continue to refuse card writes
until the SAVE owner can atomically compose cargo, Pod and the canonical ledger.

## Ordinary gameplay acceptance script (30–90 seconds)

Use the floor owner's composed Emergence floor 2 launcher with original geometry,
actual source cargo and canonical ledger, 20 Pikmin, a centered 960x540 window,
and a fresh private save/log directory. The source BaseGen7 slot is
`(-680,25.5,595)`, yaw 135 degrees (2.35619449 radians). Boot must contain the
dedicated actual Pod body, not a Red Onion or engineering receiver.

1. Record canonical Pokos/seen before touching the source treasure. Approach
   and throw/command enough of the live starting squad using ordinary controls.
2. Observe the actual carry along the source floor to the Pod. During travel and
   ascent, Pokos/seen must remain unchanged; pending unload/SAVE must be refused.
3. Observe completed ascent and `P2_ORIGINAL_POD_SUCK_DONE` for the exact floor
   instance/epoch. The canonical value and seen entry must change exactly once,
   the cargo must disappear, and P1 repairs/Onion population must not change.
4. Revisit the receiver after completion and verify no repeat award. Save/resume
   needs the owning SAVE transaction; do not substitute a callback invocation,
   forced transport action, staged cargo position or synthetic consumed ledger.

This document is a test script, not evidence that gameplay passed. A syntax or
production build pass does not establish haul, collision or save acceptance.
