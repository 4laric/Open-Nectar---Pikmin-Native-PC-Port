# Minimal source Flick receiver capsule (#1260)

Base is frozen5c67884e1. This capsule contains no source55 provider/FSM or
held-treasure ancestry. It adds the actual shared vector receiver files,
append-only Piki37/Navi38 IDs and factory registrations, its production TU,
and focused controls compiled against the actual TU with small engine doubles.

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
