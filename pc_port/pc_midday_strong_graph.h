#pragma once
#include "pc_midday_actor_archive.h"
#include "pc_midday_refcounts.h"
#include <functional>
#include <tuple>
namespace pc_midday {
// Factory-derived identity of the native SmartPtr storage, not its target. An
// Action shared by two visitor paths has one logical owner/slot/member identity.
// Never use a process address, wire key prefix guess or target ID as this identity.
struct StrongStorage {
 ReferenceOwner kind=ReferenceOwner::Actor;
 uint64_t owner=0,slot=0;
 std::string member;
 bool operator<(const StrongStorage& other)const{return std::tie(kind,owner,slot,member)<std::tie(other.kind,other.owner,other.slot,other.member);}
};
using StrongStorageResolver=std::function<bool(ReferenceOwner,uint64_t,const FieldSchema&,StrongStorage&,std::string&)>;
struct StrongPayload {
 ReferenceOwner kind=ReferenceOwner::Actor;
 uint64_t owner=0;
 const ActorFields* fields=nullptr;
 const std::vector<FieldSchema>* schema=nullptr;
 const LogicalResolver* references=nullptr;
};
// All payloads and the complete native observed census are one transaction.
// Unique source storage receives collision-free ordinal topology IDs per owner
// in sorted (slot,member) order. Aliases agree or refuse; their counts are not added.
bool extractStrongGraph(const std::vector<StrongPayload>&,
 const StrongStorageResolver&,
 const std::map<uint64_t,int32_t>& observed,const std::set<uint32_t>& globals,
 std::vector<StrongReference>&,std::map<uint64_t,int32_t>&,std::string&);
}
