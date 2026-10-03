# Minimal source Flick receiver capsule (#1260)

This branch carries the full source55 implementation. The same receiver module and focused controls are independently available as the minimal PR184 capsule on frozen 5c67884e1.

Public family APIs:

```
bool pc_p2_source_flick_piki(BTeki*,Piki*,float knockback,float angle);
bool pc_p2_source_flick_navi(BTeki*,Navi*,float knockback,float damage,float angle);
```

Retail interactPiki.cpp610-635 and interactNavi.cpp113+ specify direction draws
only after eligibility. Magnitude is knockback*(1+.1*rand), Y100+50*rand,
XZ negative sin/cos. Angle below -10 uses receiver facing. Pikmin ignore damage;
source Blow applies one extra vertical random draw, flags1 and10% leaf chance
at bounce. Retail Flick rejects legacy Flick/Panic while permitting the Blow
state entered by previous acceptance. Captains store supplied damage until
Koke END; life gauge updates there once. Existing source55 wrappers remain.

The actual-TU controls cover RNG/repeated Blow/guards, vectors and sentinel,
Pikmin health/leaf, Purple wind rejection, whistle recovery, and Navi Koke END
damage timing. These are component controls, not gameplay acceptance.

## Admission limits

Native PikiState has no retail invincible()/dead() virtual interfaces. Current
mapped guards refuse Pressed/Swallowed/Dying/Dead, plus legacy Flick/Panic for
flicking and actual isAlive/mouth constraints. Retail DemoWait/Fountainon/Holein
invincible states have no qualified native mapping; this capsule does not claim
admission parity for those transitions. Future original state owners must expose
actual immunity/dead data; no blanket guessed guard or fake immunity bit is added.
Retail InteractFlick/InteractWind Navi do not invoke the native invincible API.
Captain demo/upgrade gates, including Repugnant Appendage for wind, remain open.

Actual native contact, animation interruption, ordinary gameplay and save/resume
still require the family owner's20-Pikmin centered960x540 fixture qualification.

Recovery correction: retail PikiBlowState onFlute records recovery before bounce;
KokeDamage receives flag0x8000 with a one-second timer. A flute notification after
bounce sets the Koke timer to zero. The native pending flag is consumed into a
local recovery flag so a later notification is observable. Navi Fling JKoke END
alone does not restart Koke or apply damage; bounce enters Koke, whose END applies
ordinary damage once and enters Dead immediately below1HP (exact1 remains alive).

The current ordinary captain HP path does not implement retail Navi::addDamage
JusticeAlloy, active-game/demo, state/actor invincibility or damage effects gates.
Native animation-assert interruption paths also remain unqualified. The engine
doubles use a constant random value and identity roundAng: controls assert draw
counts and caller arithmetic, not independent RNG draw ordering or angle wrapping.
counts and caller arithmetic, not independent RNG draw ordering or angle wrapping.

Read-only captain dispatch gate: pc_p2_source_navi_reaction_gate only succeeds
when the active owned WindNaviState pointer matches the captain current state.
Matching a numeric native ID alone is insufficient. Hit/Fling report source Flick;
Koke/Timer/GetUp report source KokeDamage. Each init receives a distinct activation
stamp. Both literal source states inherit NaviState invincible=false. Unsupported
queries return false and kindUnsupported, not a native-state immunity judgment.
Controller identity/alive/actor invincibility/demo/game-active gates belong to the
original captain damage owner (#1289). This query does not install a damage seam.
