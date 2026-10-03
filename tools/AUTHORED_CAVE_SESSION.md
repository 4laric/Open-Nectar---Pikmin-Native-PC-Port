The optional authored provider selects `AUTHORED_CAVE_ROUTE 1` immediately after
the existing `CAVE_CHECKS` bootstrap block. The remaining fields are seed,
32-hex cave token, exact route sidecar SHA-256, surface and floor descriptors
(stage ID, stage index, native filename, INI SHA-256, landing XYZ), then exit XYZ.
The normal bootstrap ends with `END`. Floor filename is
`stages/generated-forest.ini`; both realms use the same native stage ID/index.
Surface landing is the entrance; floor landing is the spawn; exit is the geyser.
Captured party surfaceHomes remain the actual return captain positions.

The selected run contains `p2-authored-cave-route.txt` (1 to 65536 bytes).
Its canonical owner format is `P2_AUTHORED_CAVE_ROUTE 1`, seed, token, the same
surface/floor descriptors and exit XYZ, then `END`, without trailing tokens.
It omits its own SHA-256. The randomizer verifies its bytes; the strong linked
provider `pc_p2_authored_cave_route_validate(route, runRoot, error)` validates
its fields and the actual two INI inputs before checkpoint loading.
`PIKMIN_P2_AUTHORED_CAVE_PROVIDER` is defined only for a target linking that
actual provider. A marker in a target without the provider refuses startup.

Selected authored cards use `PIKMIN_CAMPAIGN_GENERATED_CAVE_3`. After existing
bud budgets and cache banks, they always include the exact selected route and
`AUTHORED_CAVE_SESSION 1` with a 0/1 presence flag. Living records contain route,
day, both tagged cache identities, active native-cache identity and full Party3.
Empty day-boundary records still authenticate the selected route. Version 3
cannot fall back to SurfaceSession or original campaign state. Unselected
generated campaigns retain version 2 serialization and parsing.

`pc_randomizer_authored_cave_checkpoint_set(session,banks)` validates prospective
values and copies both before replacing either. Failure leaves both unchanged.
`session_set` uses current banks. Hashes use routeSha + '/' + realm + '/' + bytes;
empty banks use '-'. Cold restoration must verify `activeCacheMatches` against
the actual selected native card's cache before creating a party. This codec
does not perform native UI, destination preflight, physical card writing,
rollback or cold allocation. The actual provider owns these operations and
both survivor dispatchers, with one cold-resume owner.

Tests exercise prospective decoding, 20 body identities/growth/health/owner,
two captains, route mismatches, tagged bank swaps, bad cache bytes, dead captain,
invalid presence flags, truncation and empty descriptors. They are component
controls, not gameplay SAVE/resume evidence.
