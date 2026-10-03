#pragma once
#include "pc_p2_original_corpse_profile.h"
#include "pc_p2_original_corpse_ledger.h"
#include <string>
class BTeki; class Pellet; class PelletView; class PelletConfig; class GoalItem;
struct Matrix4f; struct Vector3f;
// Provider callback: validates the literal profile and live native pellet manager.
// Provider still owns admission of its genuine dead animation/body bank.
bool pc_p2_original_corpse_resources(unsigned source,std::string& error);
// Only explicit original actors are routed. Unknown original profiles refuse;
// source55 has no corpse. P1/AP and preview-only actors retain their own path.
bool pc_p2_original_corpse_leaves(BTeki*,bool ordinary);
PelletConfig* pc_p2_original_corpse_config(PelletView*,PelletConfig* ordinary);
void pc_p2_original_corpse_born(Pellet*,PelletView*);
const p2original::CorpseProfile* pc_p2_original_corpse_profile(const Pellet*);
void pc_p2_original_corpse_forget(Pellet*);
bool pc_p2_original_corpse_onion(Pellet*,GoalItem*,unsigned& grant);
void pc_p2_original_corpse_position(const Pellet*,Vector3f&,float direction);
void pc_p2_original_corpse_view_matrix(const Pellet*,Matrix4f&);
void pc_p2_original_corpse_collision(Pellet*);
// Explicit session boundary; refuse while actual native corpse bindings survive.
bool pc_p2_original_corpse_new_session(const std::string& catalog,std::string&);
// SAVE owner authenticates this address-free receipt state together with stock.
bool pc_p2_original_corpse_snapshot(p2original::CorpseSnapshot&,std::string&);
