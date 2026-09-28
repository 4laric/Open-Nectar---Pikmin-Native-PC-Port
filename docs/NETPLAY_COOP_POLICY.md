# Co-op randomizer policy (netplay M4 D-policy, issue #885)

Applies in co-op only: `pc_coop_active()` with a second captain
(`GameCoreSection::mNavi2`). Every netplay session is co-op, and so is local
`--coop` / `PIKMIN_COOP=1`. Single-captain play is unchanged (the co-op code
is an early branch in `GameCoreSection::updateAI`; the single-captain
statements sit untouched in its `else`).

## Owner rules (final)

| Rule | Co-op behaviour |
|---|---|
| 1. Heal | Goes to the *triggering* captain: the one live, hurt captain whose HP dropped since the previous co-op tick. With none or several, the live hurt captain with the lowest HP (ties to P1). A downed captain is never healed. One heal consumed per grant. |
| 2. Anchors | Bomb trap, Progg, prerelease and Flower Shower each keep a cursor (all start on P1). An attempt tries the cursor's captain if live, else (or if its placement fails) the other live captain in the same tick; after a success the cursor points past the captain used. Cooldowns stay one per kind. |
| 3. Any captain | Benefits, the prerelease tick and DeathLink run while any captain lives; the prerelease trap ends on day end or when no captain lives. |
| 4. DeathLink | One unit of Pikmin from the combined field pool (whichever captain they follow), one consume per link. |

Live captain = `hp > 1 && state != NAVISTATE_Dead` (the `pcIsLastNaviStanding`
predicate). Captain 1 = `mNavi`, 2 = `mNavi2`.

Interpretations for the owner to confirm:

* "Triggering captain" is the captain whose damage consumes the pending heal:
  the native side cannot tell which captain's check produced an Archipelago
  heal.
* Bombs-at-Onion and the +10 Pikmin delivery are Onion-anchored and are not
  rotated; "delivery anchors" is read as the captain-anchored Flower Shower.
* Exploration (Land checks) stays anchored to P1 and needs P1 live.
* Cursors, the HP samples and the event tick reset when the stage or day
  changes.

The decisions are pure functions in `pc_port/pc_coop_policy.{h,cpp}`
(`pc_coop_policy_test`). All state is sim state (no RNG, no wall clock), so
lockstep peers agree. Co-op-only log lines start with `[coop-policy]`.

## Test hooks

* `PIKMIN_NETPLAY_TEST_COOP_EVENTS=<file>` (inert when unset): up to 64 lines
  `<tick> HP <1|2> <fraction>` or `<tick> DOWN <1|2>`, `<tick>` = co-op
  randomizer `updateAI` calls since the stage started. `DOWN` knocks a
  captain down like `Navi::finishDamage` and refuses the last one standing.
  Pass the same file to both peers; it is not in the handshake config hash.
* `PIKMIN_RANDOMIZER_TEST_SCRIPT=coop-policy` + `PIKMIN_COOP_POLICY_CASE`
  (TEST_HOOKS builds only): `tools/netplay/coop_policy_native.py --case
  <heal-p2-only|heal-lowest|heal-trigger|heal-p1-down|anchors|any-alive|deathlink-p1-down>`.
  The prerelease trap needs Candypops/Geysers, which Forest of Hope lacks: run
  `anchors` with `--profile navel-day2` to include it.
* `tools/netplay/coop_policy_pair.py` = `run_pair.py` with the schema-9
  bootstrap plus identical-`[coop-policy]`-lines checks on both peers.
