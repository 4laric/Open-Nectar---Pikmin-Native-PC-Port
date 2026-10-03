#pragma once
#include "pc_p2_retail_cave_context.h"
#include <functional>
class Pellet;
struct Suckable;

namespace p2originalpod {
// Retail Onyon type 3, object bank 1. Ship type 4 belongs to the surface owner.
constexpr unsigned sourceType=3, sourceObject=1;
constexpr const char* archive="user/Kando/pod/arc.szs";
constexpr const char* archiveSha256="90784394f69e8db32102e7b3e69c29b2fe737d4d0240efaf1dcdf32def783412";
constexpr const char* modelSha256="f562fb2926cc54be8875afb07d2d0effe2f2af7469f9d4ab5c7940917eb8b595";
struct Config {
 p2retail::Snapshot floor;
 // Authenticated authored-layout BaseGen type 7 placement, radians. Never
 // infer the source scene from an Onion, preview receiver or active P1 stage.
 unsigned unit=0,slot=0,baseGenType=7;
 float x=0,y=0,z=0,yaw=0;
 std::string model="pod.mod";
};
// Owner checks original session, selected SAVE fingerprint, cave floor and
// layout incarnation on every query; an input descriptor alone grants nothing.
using ContextProvider=std::function<bool(const p2retail::SceneIdentity&,p2retail::Snapshot&)>;
using CompletedCallback=std::function<bool(Pellet*,Suckable*,std::string&)>;
}
// Additive API; never activates implicitly or creates an economy ledger.
bool pc_p2_original_pod_birth(const p2originalpod::Config&,
                            p2originalpod::ContextProvider,std::string&);
bool pc_p2_original_pod_context(Suckable*,const p2retail::SceneIdentity&,p2retail::Snapshot&);
Suckable* pc_p2_original_pod_goal(const p2retail::SceneIdentity&);
bool pc_p2_original_pod_bind_cargo(Pellet*,const p2retail::BirthIdentity&,
                                 const p2retail::SceneIdentity&,
                                 p2originalpod::CompletedCallback,std::string&);
bool pc_p2_original_pod_owns(const Pellet*);
Suckable* pc_p2_original_pod_goal_for(Pellet*);
// True only within the callback emitted by native completed suction. There is
// no caller-supplied integer event token. Verify before committing the ledger.
bool pc_p2_original_pod_completed(Pellet*,Suckable*,const p2retail::SceneIdentity&);
// All unfinished bound cargo blocks unload, not only cargo currently sucking.
unsigned pc_p2_original_pod_pending();
bool pc_p2_original_pod_release(std::string&);

// Engine seam entrypoints. Consumers use completed(), not these transitions.
void pc_p2_original_pod_suction_begin(Pellet*);
bool pc_p2_original_pod_suction_done(Pellet*);
void pc_p2_original_pod_suction_cleanup(Pellet*);
void pc_p2_original_pod_forget_pellet(Pellet*);
