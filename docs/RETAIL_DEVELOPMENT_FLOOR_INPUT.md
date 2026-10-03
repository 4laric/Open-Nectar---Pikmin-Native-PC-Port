# Retail development-floor selected inputs (#930)

Implementation owner: Codex through shared account 4laric.

The actual retained OriginalSession selects `p2-original/development-floor.p2d`.
This role is optional only after valid OriginalSession initialization. A present
role that cannot be read or authenticated is a fatal/refused selection, never a
fallback to the surface course. Membership does not authenticate file bytes.

The version-one format is whitespace-delimited, in this exact field order:

```
P2_RETAIL_DEVELOPMENT_FLOOR_1 tutorial_1 1
plan <full SHA-256 of floor.p2f>
geometry <full SHA-256 of geometry.mod>
routes <full SHA-256 of routes.ini>
start <full SHA-256 of start.json>
pool <full SHA-256 of unit-pool.txt>
layout <full SHA-256 of start-layout.txt>
```

Only tutorial_1 floors 1 and 2 are admitted by this version. Digests are 64
lowercase hexadecimal characters. Extra fields, aliases and caller paths are
rejected. The selected descriptor must retain the selection role and all six
inputs for the selected floor. To support both floors, retain both sets.

Canonical source-start roles for SAVE1229 (exact final04 bytes, no rewriting):

| Floor | Companion | Raw unit pool | Raw selected layout |
| --- | --- | --- | --- |
| 1 | `p2-original/retail-caves/tutorial_1/floor1/start.json` | `p2-original/retail-caves/tutorial_1/floor1/unit-pool.txt` | `p2-original/retail-caves/tutorial_1/floor1/start-layout.txt` |
| 2 | `p2-original/retail-caves/tutorial_1/floor2/start.json` | `p2-original/retail-caves/tutorial_1/floor2/unit-pool.txt` | `p2-original/retail-caves/tutorial_1/floor2/start-layout.txt` |

For each floor, the other three roles use the same directory with `floor.p2f`,
`geometry.mod`, and `routes.ini`. Map the existing unmodified
`p2-retail-floor.txt` to `floor.p2f`, `p2-retail-start.json` to `start.json`,
`p2-retail-unit-pool.txt` to `unit-pool.txt`, and
`p2-retail-start-layout.txt` to `start-layout.txt`. The geometry and route roles
contain the source-validated converted MOD/INI, not JSON intermediates.

The reader verifies actual campaign/session/revision before and after every
input, retains the authenticated buffers and binds plan cave/floor and MOD/INI
digests to the selection. This is input integrity only. Start JSON/source pool
and layout semantics, native MOD/INI admission, actual MapMgr ground queries,
StageInfo installation, independent birth census, captain rig binding and the
canonical SourceWorld lifecycle remain required before physical commitment or
activity. No selection or parser result grants gameplay authority.
