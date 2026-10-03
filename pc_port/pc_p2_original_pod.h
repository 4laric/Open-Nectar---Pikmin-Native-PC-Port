#pragma once
#include "pc_p2_retail_cave_context.h"
#include <functional>
class Pellet;
struct PelletGoalState;
struct Suckable;

namespace p2originalpod {
// Retail Onyon type 3, object bank 1. Ship type 4 belongs to the surface owner.
constexpr unsigned sourceType=3, sourceObject=1;
constexpr const char* archive="user/Kando/pod/arc.szs";
constexpr const char* archiveSha256="90784394f69e8db32102e7b3e69c29b2fe737d4d0240efaf1dcdf32def783412";
constexpr const char* originalModelSha256="e567b76127b7802f88fe28cdd956260fededba0b5d30255b935bc15c28f25c0e";
constexpr const char* originalCollisionSha256="548f59a9d8eceb3be896011a33765e8357217c66cd1587ac258253aa675ac005";
constexpr const char* convertedModelSha256="f562fb2926cc54be8875afb07d2d0effe2f2af7469f9d4ab5c7940917eb8b595";
struct Config {
 p2retail::Snapshot floor;
 // Authenticated authored-layout BaseGen type 7 placement, radians. Never
 // infer the source scene from an Onion, preview receiver or active P1 stage.
 unsigned unit=0,slot=0,baseGenType=7;
 float x=0,y=0,z=0,yaw=0;
 std::string model="pod.mod";
 std::string sourceArchive="pod/arc.szs",sourceModel="pod/pot.bmd",sourceCollision="pod/coll.txt";
};
// Owner checks original session, selected SAVE fingerprint, cave floor and
// layout incarnation on every query; an input descriptor alone grants nothing.
using ContextProvider=std::function<bool(const p2retail::SceneIdentity&,p2retail::Snapshot&)>;
using CompletedCallback=std::function<bool(Pellet*,Suckable*,std::string&)>;
// Read-only graph capture. No ledger copy, synthetic consumption or save
// authority. Until SAVE provides an atomic cargo/ledger transaction, pending
// entries must continue to reject card writes and floor exits.
struct PendingCargo {p2retail::BirthIdentity birth;unsigned phase=0;};
struct Snapshot {unsigned version=1;p2retail::Snapshot floor;unsigned unit=0,slot=0;
 std::vector<PendingCargo> pending;bool committed=false;};
}
// Additive API; never activates implicitly or creates an economy ledger.
bool pc_p2_original_pod_preflight(const p2originalpod::Config&,
                                p2originalpod::ContextProvider,std::string&);
bool pc_p2_original_pod_birth(const p2originalpod::Config&,
                            p2originalpod::ContextProvider,std::string&);
// Floor owner commits only after all geometry/content/exit bindings succeed.
bool pc_p2_original_pod_commit_floor(const p2retail::SceneIdentity&,std::string&);
// Rollback only before commit and before any observable suction/receipt.
bool pc_p2_original_pod_abort_prepared(const p2retail::SceneIdentity&,std::string&);
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
bool pc_p2_original_pod_snapshot(const p2retail::SceneIdentity&,p2originalpod::Snapshot&);
bool pc_p2_original_pod_release(std::string&);

// Only the actual native goal state may issue receipt authority. Consumers
// cannot invoke these transitions or construct a synthetic completion token.
struct P2OriginalPodNativeSeam {
private:
 friend struct PelletGoalState;
 static void begin(Pellet*);
 static bool done(Pellet*,const PelletGoalState&);
 static void cleanup(Pellet*);
};
void pc_p2_original_pod_forget_pellet(Pellet*);
