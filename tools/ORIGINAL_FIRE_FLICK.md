# Original FireChappy flick correction (#1253)

Only a registered original source33 actor takes this path. AP/P1 and other
Chappy sources retain their previous flick implementation.

Retail ChappyBase.cpp329 and enemyAction.cpp768–850 order sticker Pikmin,
nearby Pikmin, then captains. Sticker chance RNG occurs even at chance1 and
before mouth rejection. Only sticker angles round face+PI. Nearby selection
uses strict squared3D distance below40 and excludes current stickers on this
actor; the prior pass can detach a sticker and allow another nearby acceptance.
Knockback120 and captain damage1 come from the private audited GPVE01 parameter
SHA recorded in pc_p2_fire_flick.h. Pikmin ignore flick damage in the source
receiver. Receivers consume launch RNG only after acceptance; captain damage
occurs at Koke END. Existing Blow receivers can accept another flick.

The separately owned receiver capsule supplies real state factories and
registrations. Range controls alone do not verify receiver behavior or gameplay.

## Direct gameplay acceptance

Use the ordinary source33 encounter with20 Pikmin and a centered960x540 window.
Record a real flick event with body-attached Pikmin, nearby Pikmin, and a captain.
Verify sticker release precedes nearby/captain reactions; keep mouth-held Pikmin
in the mouth. Compare targets just inside40, exactly40/outside, and with vertical
separation greater than40. Pikmin attached to another creature remain eligible
for the nearby pass. Check launch direction follows enemy facing, captain damage
appears at recovery animation completion, and receivers recover to ordinary
control. Record repeated flick acceptance after a body sticker detaches into
the nearby sphere; Flick/Panic, pressed, swallowed and dead targets must refuse.
Do not use position/state writes as ordinary gameplay acceptance. Save/resume,
collider and visual fidelity remain separately owned acceptance gates.
