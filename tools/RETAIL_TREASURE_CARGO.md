# Original loose treasure cargo (#1232)

The source floor owner supplies `p2retailcargo::Config`: its independently
authenticated prepared floor snapshot/context, selected-ledger birth authority,
and an authored-placement lookup. Placement yaw is in radians; the retail plan
stores degrees. No default authority or engineering placement exists.

The floor owner's `p2retail::cargoPlacement(NativeFloor&)` adapter, published at
`3a24500edd928e0bcae39048903c5562f97e3069`, has this exact callback signature.
It obtains the row from authenticated floor-plan bytes while SceneOps owns the
prepared scene, then converts authored degrees to radians. The borrowed
NativeFloor must outlive all callbacks, including partial release. This adapter
does not supply Config.context or the independent birth authority.

Call preflight after selecting the original session and binding the master
treasury SOURCE, before native cargo allocation. It verifies the complete
aggregate through authenticated input buffers and loads converted models from
those same buffers. Call birth only for the expected literal loose source.
It allocates a native Pellet directly with a private PelletConfig and PelletView,
original radius/height/carry radius and catalogue minimum/maximum. It does not
look up a numbered pellet or author a P1 generator. The actual Pod must already
be prepared with the same independent floor/birth authorities.

The provider binds each cargo to the Pod's private completed-suction callback.
It verifies the source/body/profile again there before canonical credit and
equipment reconciliation. The Pod kills the actor after the callback. Source
records revoke actor pointers on kill; original incarnations remain represented.
Consumed source rows require a canonical receipt and an absent typed binding.

Prepared abort coordinates Pod rollback before retiring provisional cargo.
Collected release coordinates Pod release before retiring collected cargo; do
not repeat the receiver transition separately. Unfinished cargo still requires
an actual retained graph and restore provider. These functions do not implement
native SAVE or unfinished-ground persistence.

When SceneOps owns a PodFloorLifecycle, pass its synchronous release operation
to the PodTeardown overload. Cargo checks the receiver phase before calling it
and actual native receiver absence afterward, then retires cargo. The selected
prepared-context provider must survive through this operation; a revoked live
FloorSession snapshot cannot provide teardown authority.

Before revoking floor readers, SceneOps.canRelease calls the read-only
`pc_p2_retail_treasure_cargo_can_release_collected` predicate. It checks the
selected context, every owned canonical receipt, and the matching committed
Pod's actual unfinished cargo and transaction state. Refusal leaves the floor
active. The mutating release repeats these checks; readiness is not a token or
permission to retain unfinished actors.

Native PC `operator new` uses the C heap; App-heap reset does not reclaim the
parsed model graph. Cargo retains one bounded model bank for the actual System
lifetime, keyed by the full authenticated master and model hashes. All nested
graphics registration occurs under SYSHEAP_Sys. Models, partial owners and
their registry nodes remain owned across floor release; every preflight still
authenticates selected bytes, and reuse refuses a changed System/bank/hash or
missing registry/texture ownership. An incomplete parsed entry is retained and
refused rather than repeatedly allocating another graph. Changing this bank or
resetting its System registry requires a fresh process; no root-only Shape
delete is used.

PelletConfig and View live inside stable cargo records. Native kill revokes the
live view association; record disposal after kill detaches a matching dead
pellet's borrowed profile, while preserving a recycled slot's replacement.
The native pool must remain alive until this disposal finishes. Exhausted pool
births erase the provisional record. PC Parm initialization directly constructs
its values, avoiding overwritten default String allocations on repeated births.
The read-only resource_usage API reports retained/complete models, registry
nodes, records, live actors and remaining native profile borrowers.

Before repeated-entry gameplay admission, the actual runner composition must
record these counts and native allocator statistics across failed preparation,
pool-exhaustion retries, kill/reuse and repeated floor release. The model count
must plateau at the selected bank's bound; completed release must leave zero
cargo records, live actors and native borrowers. Pure ownership controls and
syntax checks do not replace that native lifecycle run.

Constructor allocation control: 1,000 real Parm<String> constructions with the
previous WIN32 header made 4,000 overwritten default-buffer allocations; the
PC initializer fix makes zero. The non-WIN32 PC branch also makes zero. Test
link seams cover unused serialization/editor methods only. Shared storage and
borrow controls exercise bounded partial ownership, repeated reuse, changed
hash/System/bank refusal, killed-profile detachment and recycled-slot safety;
they allocate no native actor and issue no source or gameplay authority.

The authenticated bank retains the original Pod archive, pot.bmd, collision and
text sources. Its qualified receiver model uses the distinct
`assets/dataDir/courses/pikmin2retailpod/pod.mod` role. The historical held Pod
dependency remains hashed separately and is not drawn as the receiver.

This is a native carry/body adapter. Original dynamic LOD switching, inertia,
particle collision fidelity, animation and effects remain unqualified. Parsed
source physics metadata does not prove that the native solver reproduces P2.
Syntax and source controls do not establish ordinary gameplay acceptance.

Ordinary validation: start the actual selected Emergence floor 2, obtain the
source-authentic Purple carrying strength, carry Atlas with its unchanged 101
minimum to the original Pod, and observe completed suction before the single
200-Poko canonical receipt/map unlock. Test unfinished/lost refusal separately.
Only after graph admission exists, perform native SAVE and fresh-process resume.
