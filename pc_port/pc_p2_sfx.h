// Engine glue for pc_p2_sfx_policy.h: plays the P1 approximation of a P2
// species event through seSystem->playSoundDirect (JACEVENT_Battle) at the
// actor's position, rate limited per (source_id, token). Output-only: no sim
// state, no sim RNG, wall-clock rate limiting (netplay lockstep safe).
#ifndef PC_P2_SFX_H
#define PC_P2_SFX_H

#include "pc_p2_sfx_policy.h"
#include "Vector.h"

// Request one event for the actor identified by (sourceId, token). Returns
// the SE id sent to JAudio, or p2sfx::kNone when unmapped / rate limited /
// the sound system is unavailable.
int pc_p2_sfx(unsigned sourceId, unsigned token, p2sfx::Event event, const Vector3f& position);

// Feed the actor's XZ position every tick while it is walking; fires Step
// through pc_p2_sfx once per strideLength units travelled.
void pc_p2_sfx_stride(unsigned sourceId, unsigned token, const Vector3f& position, float strideLength);

// Forget per-actor gates (species reset / actor forget). Passing token 0
// forgets every actor of that species.
void pc_p2_sfx_forget(unsigned sourceId, unsigned token);

#endif // PC_P2_SFX_H
