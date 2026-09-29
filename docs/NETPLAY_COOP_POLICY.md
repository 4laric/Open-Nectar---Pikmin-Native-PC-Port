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
* Cursors, the HP samples and the event tick reset on every stage entry:
  a stage or day change, or the time of day going backwards. The last case
  covers a repeated day 29 on the same stage (`pc_randomizer_next_day` pins
  the day at 29) and a same-day reload, where stage and day stay the same.
  Each reset logs `[coop-policy] RESET stage=<s> day=<d> reason=<start|stage|day|clock>`.
* "Delivery anchors" may instead mean BOMB_DELIVERY / PIKMIN_DELIVERY. If
  so, the 3-bomb ring would get its own cursor on the captains with the
  Onion as the fallback; not done pending the owner's answer.

## Anchor log lines

* `[coop-policy] ANCHOR kind=<k> captain=<n> next=<m> live=<p1><p2>` for
  every grant (`live` = which captains were live for that attempt).
* `[coop-policy] ANCHOR_SKIP kind=<k> captain=<n> reason=not-live` or
  `reason=placement why=<ring-no-ground|ring-water|ring-height|no-onion-near|no-spot|not-walking> ... x= z=`
  right before an ANCHOR that landed on a captain other than the cursor's,
  one per captain passed over. An attempt where nobody could be placed logs
  nothing (it retries next tick).
* `coop_policy_pair.py` replays the cursor from these lines and fails on any
  grant off the cursor without a matching ANCHOR_SKIP.

The attempt loop is `pc_coop_anchor_try` (engine-free, unit-tested), which
`coopAnchored` in `gameCoreSection.cpp` calls.

## Drift guard

The co-op branch carries copies of the single-captain BOMBS, DELIVERY,
DeathLink loop and observation code so that single-captain play stays
byte-identical. Mirror every change to one side on the other; netplay and
local co-op always run the co-op side. Factoring the shared pieces into
helpers is an integration item, to be proven with the M1 replay comparison.

The decisions are pure functions in `pc_port/pc_coop_policy.{h,cpp}`
(`pc_coop_policy_test`). All state is sim state (no RNG, no wall clock), so
lockstep peers agree. Co-op-only log lines start with `[coop-policy]`.

## Test hooks

* `PIKMIN_NETPLAY_TEST_COOP_EVENTS=<file>` (inert when unset): up to 64 lines
  `<tick> HP <1|2> <fraction>`, `<tick> DOWN <1|2>`, `<tick> SQUAD <1|2>
  <1..200>`, `<tick> DISMISS <1|2>`, `<tick> HOME <1|2>` or `<tick> SUNSET`
  (`#` starts a comment), `<tick>` = co-op randomizer `updateAI` calls since
  the stage started, 1-based (tick 0 is rejected). `DOWN` knocks a captain
  down like `Navi::finishDamage` and refuses the last one standing.
  * Day-end kinds (gapfix K): `SQUAD` moves up to N Pikmin from the other
    captain's squad into this one's (the squad action is abandoned before
    the owner changes, so both formation plates stay legal); `DISMISS` is the
    captain's own dismiss (`Navi::releasePikis`); `HOME` stands the captain's
    free Pikmin 60 units from the Onion of their colour (or the ship);
    `SUNSET` jumps the clock to the day's end hour and logs what the day-end
    enter paths will see.
    * `DISMISS` only works once the captain is in control: the dismiss walks
      the formation plate, which the captain's control update refreshes, and
      that does not run during the stage-start movie (co-op ticks 1 to ~280
      on `foh-day2`). It then logs `released=0 kept=N`.
    * `SUNSET` is refused while that movie runs or a captain is still in
      `NAVISTATE_Starting` (`reason=stage-start`). A day end started there
      never gives the captains control, and every squad is left behind.
    * The day end sets the clock back and re-arms the schedule, so all four
      kinds are refused inside the day-end sequence (`reason=day-end`). They
      fire again on the next stage/day.
  * Committed fixtures for the day-end halt (REG 4, `karl caught a cold !`).
    Both are pairs on the netplay exe, with the file passed to both peers:
    `py -3.12 tools/netplay/run_pair.py --exe <np nectar.exe> --ticks 6000
    --delay 1 --netplay-seed 0 --host-port <port> --out <private dir> --env
    PIKMIN_NETPLAY_TEST_COOP_EVENTS=<absolute path to the fixture>`.
    * `tools/netplay/fixtures/coop_dayend_home.events` covers
      `enterFreePikmins`: captain 2's free Pikmin stand by the Onions.
    * `coop_dayend_squad.events` covers `Navi::enterAllPikis`: captain 2
      still has a squad at the sunset whistle.
    * Expected: `run_pair: PASS` and no `[PANIC]`. With the default seeds the
      day-2 end runs to map select and the day-3 reseed (tick 3734 on seed
      0). The re-armed schedule then runs a second co-op day end on day 3.
    * A build without the fix halts both peers with `karl caught a cold !`
      at the first day end, near tick 700 (home) or 900 (squad).
  * Compiled only into the netplay build (`PIKI_NETPLAY_BUILD`) and honoured
    only with `PIKMIN_RANDOMIZER_TEST_BACKGROUND=1`; the default build never
    reads it.
  * Parsed once per process; a file over 16384 bytes is rejected whole. The
    tick restarts at every stage entry, so the schedule re-arms each
    stage/day (`[coop-policy] TEST events armed stage= day= count=`).
  * Pass the same file to both peers; it is not in the handshake config hash
    yet (integration item for B1).
* `PIKMIN_RANDOMIZER_TEST_SCRIPT=coop-policy` + `PIKMIN_COOP_POLICY_CASE`
  (TEST_HOOKS builds only): `tools/netplay/coop_policy_native.py --case
  <heal-p2-only|heal-lowest|heal-trigger|heal-p1-down|anchors|any-alive|deathlink-p1-down>`.
  The prerelease trap needs Candypops/Geysers, which Forest of Hope lacks: run
  `anchors` with `--profile navel-day2` to include it.
* `tools/netplay/coop_policy_pair.py` = `run_pair.py` with the schema-9
  bootstrap plus identical-`[coop-policy]`-lines checks on both peers.
