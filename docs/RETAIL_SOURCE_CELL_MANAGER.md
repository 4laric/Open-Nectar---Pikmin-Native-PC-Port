# Actual source CellMgr adapter scope (#930)

Implementation owner: Codex through shared account 4laric. Scene930 owns the
actual shared manager; Body/Piki, Captain and enemy/item owners provide their
real source objects. This is a concrete implementation contract, not a working
manager or contact/LOD admission. Original source pins and line audits are in
`output/retail-caves-1274-tests01/source-cell-audit21/` and Numeric's read-only
audit of source 632af93787b9c95b63f0c13be32b161375ce3a96.

The scene derives actual map bounds by the source transformation of each
serialized unit bound box, unions adopted rooms, then expands X/Z by 320.
These bounds differ from transformed collision-vertex bounds expanded by 10.
Actor CellMgr uses scale108; Plat and room managers use128 and170 respectively.
Scene initialization owns the persistent cell pyramid, actual constructor
policy fields and explicit passID initialization. It must not rebuild or clear
the graph every frame: source clear retains leg pointers/Piki-Navi counters.

Each actual body provider must supply an incarnation-bound object kind, raw
source getPosition, getCellRadius, getBoundingSphere and current source
collision tree/parts, with birth insertion order and checked entry/exit. These
are separate from a native P1 range/CollInfo and from source map-trace radius.
For the Piki contract supplied by the body owner, bounding sphere radius4 is
centered at actual body position, LOD sphere radius15 and map-trace radius8.5.
The single contact part is attribute0/idroot/offset0/joint0/baseRadius4; its
current contact center comes from the authenticated animated joint0 world
translation, not merely SRT translation. Other families must supply their
own real facts. Missing producers are unavailable, never absent-body defaults.

Source entry removes existing four legs, inserts at most four in inclusive
X-outer/Z-inner order, and inserts new legs after the oldest head. Pyramid
counts propagate to parents. Creation uses source reciprocal/ceil arithmetic,
the one-time scale-by1.5 adjustment for dimensions above200, and source layer
calculations. Conversion/libm edges need explicit qualification. Actor kill
exits the correct owned cell manager before onKill/resource retirement.

Source FakePiki animation calls updateCell before updateLOD (far.01/close.009).
Frame collision resolution follows animation/entry and precedes simulation.
Movement does not implicitly update membership; only actual source entry
phases do, including specific stick/target paths. Collision-disabled source
state skips updateCell. Body providers must expose the actual phase and
runtime flags rather than allowing a caller to stamp readiness.

Queries traverse leg links, Z-inner/X-outer rectangles, then ascending layers.
PassID wraps at0x4000000 and is assigned only after condition and overlap
acceptance. The overlap uses current raw X/Z positions and candidate bounding
radius, ignores sphere center/Y, and uses rounded dz-squared plus source
FMA(dx,dx,dz-squared). Optimise skips that overlap filter. Piki grasp-situation
fast uses the actual source radius300/optimise=true policy. Accepted-only
marking must preserve a rejected candidate's later eligibility in repeated
cells.

AILOD_PikiInCell comes from stored rectangle local/descendant Piki-or-Navi leg
counts, not a proximity/contact scan. A Navi alone can enable it. Body, pose,
membership and viewport/visibility lifetimes remain separate. The actor's
source LOD phase consumes the real manager counts and actual source camera
facts; it cannot inherit native P1 flags.

Acceptance requires actual two-floor map-bound derivation, correct entry/head
order and four-leg removal, ancestor counts, source query repeat/passID/overlap
controls, Navi-only/coarse-cell LOD controls, real current body/tree providers
and birth/kill/reentry lifecycle traces. Every query/contact callback must
revalidate current scene/campaign/session/serial/revision, actual body lifetime
and tree/pose lease before access and after external callbacks. Retiring
revokes queries before body teardown, retains owned membership storage through
checked exits, and frees it only after all body consumers retire. Full source
collision resolution needs actual policy/contact/effect owners; a correct
iterator alone does not grant gameplay or SAVE acceptance.
