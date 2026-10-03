#pragma once
#include <string>
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
void pc_p2_catfish_source_update(BTeki*);
void pc_p2_catfish_source_forget(BTeki*);
// Reports ownership/handled to suppress the P1 Pressed event. The source
// callback itself returned false and never entered fatal Press.
bool pc_p2_catfish_source_press(BTeki*,Creature*,float damage);
float pc_p2_catfish_source_param(const BTeki*,int index,float fallback);
