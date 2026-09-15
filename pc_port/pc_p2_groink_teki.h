#pragma once
class BTeki;
// Family sidecar binding the parked P2GroinkCarcass policy to a live generated
// Groink host actor (MiniHoudai 78 / FminiHoudai 97).  Binding is read from
// p2-groink-teki.txt at finalSetup; the carcass lifecycle is then driven once
// per frame from the actor's own update and the KillPellet / RequestBirth /
// ActivateGauge / DeactivateGauge host commands act on the real pellet and
// life gauge.  All emissions are logged as P2_GROINK_CARCASS_* markers.
void pc_p2_groink_teki_setup();
void pc_p2_groink_teki_reset();
void pc_p2_groink_teki_forget(BTeki*);
void pc_p2_groink_teki_tick(BTeki*);
bool pc_p2_groink_teki_is_bound(const BTeki*);
// Read-only probes for the runtime fixture / root validator.  timer() and
// health() surface the policy's own regeneration timeline; the requested-birth
// count confirms the carcass-birth marker was produced, not injected.
float pc_p2_groink_teki_timer(const BTeki*);
float pc_p2_groink_teki_health(const BTeki*);
int pc_p2_groink_teki_births(const BTeki*);
