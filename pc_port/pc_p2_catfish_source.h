#pragma once
#include <string>
#include "pc_p2_original_catalog.h"
class BTeki;class Creature;
// The receiver must implement source flick geometry/vectors and report failure.
// It receives the logical EnemyFunc angle (faceDir or backward sentinel), before
// EnemyFunc's PI adjustment. Native admission refuses an absent receiver.
using P2CatfishSourceFlick=bool(*)(BTeki*,bool stuckOnly,float angle,float range,float knockback,float damage);
bool pc_p2_catfish_source_set_flick_receiver(P2CatfishSourceFlick,std::string&);
bool pc_p2_catfish_source_resources(std::string&);
bool pc_p2_catfish_source_birth(BTeki*,unsigned uid,unsigned ordinal,std::string&);
bool pc_p2_catfish_source_registry(BTeki*,unsigned token,std::string&);
bool pc_p2_catfish_source_clip(const BTeki*,const char*&,float& phase);
bool pc_p2_catfish_source_corpse_clip(const BTeki*,const char*&,float& phase);
// Actual pellet view lifecycle: pre-carry is prepared/paused at corpse birth.
// Start resumes; restart=true corresponds to view_start_carrymotion.
bool pc_p2_catfish_source_carry_start(BTeki*,bool restart=false);
bool pc_p2_catfish_source_carry_stop(BTeki*);
bool pc_p2_catfish_source_carry_finish(BTeki*);
struct P2CatfishSourceGate {
 p2original::InstanceIdentity identity;unsigned token=0;
 bool alive=false,dead=false,bitterImmune=false,invulnerable=false,noInterrupt=false;
 int state=0,animation=0;float sourceFrame=0,health=0;
 std::uint64_t nonStoneClearSerial=0;
};
bool pc_p2_catfish_source_gate(const BTeki*,P2CatfishSourceGate&);
// Only actual lifecycle owner calls these after real Stone entry/exit;
// they implement family virtual callbacks, never accept/create Stone.
bool pc_p2_catfish_source_do_start_stone(BTeki*,const p2original::InstanceIdentity&);
bool pc_p2_catfish_source_do_finish_stone(BTeki*,const p2original::InstanceIdentity&);
void pc_p2_catfish_source_update(BTeki*);
void pc_p2_catfish_source_forget(BTeki*);
// Reports ownership/handled to suppress the P1 Pressed event. The source
// callback itself returned false and never entered fatal Press.
bool pc_p2_catfish_source_press(BTeki*,Creature*,float damage);
float pc_p2_catfish_source_param(const BTeki*,int index,float fallback);
