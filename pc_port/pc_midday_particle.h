#pragma once
#include "pc_midday_actor_archive.h"
struct PermanentEffect;
namespace zen { class zenList; class zenListManager; class particleMdl; class particleChildMdl; class particleGenerator; class particleMdlManager; }
namespace pc_midday {
struct ParticleAllocationDescriptor { u32 models=0,children=0; };
bool validate_particle_allocations(const std::vector<ParticleAllocationDescriptor>&,u64 nodeBudget,std::string&);
struct ParticleGraphNode { LogicalRef identity,previous,next; };
bool validate_particle_graph(const std::vector<ParticleGraphNode>&,std::string&);
enum class ParticleRecordKind:int { Model=1,Child=2,Generator=3,Permanent=4,List=5,Manager=6 };
bool particle_fields(zen::particleMdlManager&,ActorArchive&);
bool allocate_particle_pool(zen::particleMdlManager&,u32 models,u32 children);
bool particle_fields(zen::particleMdl&,ActorArchive&);
bool particle_fields(zen::particleChildMdl&,ActorArchive&);
bool particle_fields(PermanentEffect&,ActorArchive&);
bool particle_fields(zen::particleGenerator&,ActorArchive&);
bool particle_list_fields(zen::zenList&,ActorArchive&);
bool particle_schema(const ActorFields&,ParticleRecordKind,const std::string&,std::vector<FieldSchema>&,std::string&);
}
