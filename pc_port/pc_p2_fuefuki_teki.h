#pragma once
// Campaign OWN port of the Antenna Beetle (P2 Fuefuki, source 41), #245.
//
// A seed-bound 41 rides a suppressed TEKI_Chappy placement vehicle (hostType
// 41 -> 3). Every live tick is decided by the engine-free source FSM
// (pc_p2_fuefuki_fsm.h), keyed on the actor's own campaign token; the host
// only integrates the FSM's velocity against its map collision. The retail
// key events (landing/landfail/jump/whisle/...) come from the staged
// P2_RETAIL_EVENTS_1 table through the verified retail player, the staged
// retail enemyparm.txt supplies every general/proper parameter, and the staged
// pose bank draws the P2 model (the P1 host model never draws while bound).
//
// Whistle theft is the source ActTeki contract: claimed Pikmin leave the party
// and walk the beetle's footmark trail (pc_p2_fuefuki_follow.h); they cannot be
// whistled back while the beetle lives, end with the emote -> Free exit when the
// beetle takes off (Jump KEYEVENT_3, EB_Untargetable), and fall into a
// non-lethal astonished Panic when it dies, from which a captain whistle
// reclaims them. None of the room-preview hardlanes concessions (engage slide,
// captain park, forced FreeMode recruits, carry-min=1) exist on this path.
class BTeki;
class Piki;
class Navi;
class Creature;
class Graphics;
class Matrix4f;

void pc_p2_fuefuki_teki_setup();
void pc_p2_fuefuki_teki_reset();
void pc_p2_fuefuki_teki_forget(BTeki*);
void pc_p2_fuefuki_teki_tick(BTeki*);
bool pc_p2_fuefuki_teki_is_bound(const BTeki*);
int pc_p2_fuefuki_teki_bound_count();
// Host suppression: true while a bound beetle is alive or playing its dead clip.
bool pc_p2_fuefuki_teki_suppress_ai(const BTeki*);
// TPF_Life = retail fp00 and a blinded host strategy while bound and alive.
float pc_p2_fuefuki_teki_param_f(const BTeki*, int idx, float fallback);
// InteractPress receiver (source pressCallBack). True when the actor is bound.
bool pc_p2_fuefuki_teki_pressed(BTeki*, Creature* presser);
// Draw hook: staged P2 pose bank (false -> host model draws).
bool pc_p2_fuefuki_teki_draw(BTeki*, Graphics&, const Matrix4f&, bool corpse = false);

// Piki-side seams (all are O(1) no-ops while nothing is bound).
// Piki::doAI: true when a live beetle owns this Pikmin (ActTeki follow ran).
bool pc_p2_fuefuki_follower_controls(Piki*);
// Navi::callPikis / Piki::changeMode(FormationMode): an ActTeki follower is not
// callable while its beetle lives (InteractFue::actPiki ACT_Teki branch).
bool pc_p2_fuefuki_follower_blocks_recruit(const Piki*);
// PikiPanicState: true when this Panic is the owner-death PIKIPANIC_Panic
// (astonish) release, not the gas panic.
bool pc_p2_fuefuki_panic_astonish(const Piki*);
void pc_p2_fuefuki_panic_end(Piki*, bool timedOut);
// Navi::callPikis: records a captain whistle reclaim of an astonished follower.
void pc_p2_fuefuki_note_whistle(Piki*, Navi*);
