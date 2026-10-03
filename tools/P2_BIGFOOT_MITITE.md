# BigFoot natural Mitite death drop (#1231)

Implementation source: native 51e25ed622ecda443805c81e8730e58b69381496. Source/compiled tests are bounded evidence; this manual encounter has not yet passed natural gameplay acceptance.

Create a NEW private smoke package with the paired #1231 root content dependency change. Do not refresh the owner's old save/session in place. From that root worktree:

```powershell
py -3.12 scripts/p2_smoke_seed.py --area foh --p1-bulborb-slots --slots 1 --species 69 --seed bigfoot-mitite-1231 --out C:/Users/alari/pikmin-randomizer/output/bigfoot-mitite-natural-1231 --content-cache C:/Users/alari/pikmin-randomizer/output/p2-content-dense --iso 'C:/Users/alari/Downloads/PIKMIN2 for GAMECUBE.iso' --exe <exact-reviewed-private-nectar.exe> --no-verify
$env:PIKMIN_P2_ROOM_WINDOW = '960x540'
& C:/Users/alari/pikmin-randomizer/output/bigfoot-mitite-natural-1231/play.ps1 -Exe <exact-reviewed-private-nectar.exe>
```

Before natural acceptance, verify the fresh package's 20-Pikmin baseline and observed centered960x540 startup. Keep native stdout/session logs under this new output directory. Do not enable autoplay, probes, health editing, boosted damage or forced death. The source boss health remains10000.

1. Approach the closest replacement bulborb slot (about720units from Forest of Hope captain start). Observe the normal drop-in and press attacks; throw Pikmin onto the body and defeat it through ordinary combat.
2. During the actual death clip at source key2, observe one cluster of30 Mitites falling from the body. Natural terrain collision should trigger bounce/scatter/panic. Keep the boss on camera through the death clip.
3. Observe the boss vanish with no carcass and the Mitites remain independently alive. Whistle the scattered squad, attack a Mitite, and observe its real death and nectar drop. Survivors should eventually dive and leave rather than follow a dead boss.
4. Check logs: one `P2_LONG_LEGS_BIRTH species=BigFoot ... count=30 requested=30`, one `P2_BIGFOOT_MITITE_GROUP ... count=30`,30 distinct member births, actual LAND events, later boss ESCAPE. No child campaign binding/extraAP checks. The boss's existing no-carcass kill check remains once.
5. Repeat with an actual held ship-part slot using the established #901 held-part transfer mechanism: the ship part drops through the generic death funnel and no Mitite group spawns. AP reward receipt alone should still allow the Mitite group.

Record executable SHA256, exact native/root commits, package/session paths, window/squad observation, encounter video/log and result. Fewer births under a full80-Teki pool are tolerated source allocation failures and logged honestly; they do not qualify the full30-member encounter.

Known port limits retained: the Mitite behavior remains the existing P1-backed family host; full source tumbling rotation while falling is not rendered. Natural combat/drop, held-part encounter and full campaign save/resume remain open until observed.
