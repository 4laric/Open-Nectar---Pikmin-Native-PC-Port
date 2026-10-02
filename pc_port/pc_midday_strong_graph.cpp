#include "pc_midday_strong_graph.h"
namespace pc_midday {
namespace {
bool knownOwner(ReferenceOwner kind,uint64_t id,const std::map<uint64_t,int32_t>& actors,const std::set<uint32_t>& globals){
 switch(kind){case ReferenceOwner::Actor:return id&&actors.count(id);case ReferenceOwner::Global:return id&&id<=UINT32_MAX&&globals.count(uint32_t(id));default:return false;}
}
bool fail(std::string& e,const char* message){e=message;return false;}
}
bool extractStrongGraph(const std::vector<StrongPayload>& payloads,const StrongStorageResolver& storage,
 const std::map<uint64_t,int32_t>& observed,const std::set<uint32_t>& globals,
 std::vector<StrongReference>& out,std::map<uint64_t,int32_t>& counts,std::string& e){
 if(!storage||payloads.empty()||payloads.size()>MaxActors+1024)return fail(e,"missing/bounded complete strong-root payloads/storage resolver");
 std::map<StrongStorage,uint64_t> slots;std::set<std::pair<ReferenceOwner,uint64_t>> owners;
 for(const auto& payload:payloads){
  if(!payload.fields||!payload.schema||!payload.references||!knownOwner(payload.kind,payload.owner,observed,globals)||!owners.emplace(payload.kind,payload.owner).second)return fail(e,"invalid/duplicate strong-root payload owner");
  if(!validate_actor_fields(*payload.fields,*payload.schema,*payload.references,e))return false;
  if(payload.kind==ReferenceOwner::Actor){
   int count=0;if(!actor_i32(*payload.fields,"creature.referenceCount",count,e)||count!=observed.at(payload.owner))return fail(e,"wire native count disagrees with complete observed census");
  }
  for(const auto& schema:*payload.schema){
   if(schema.strength==ReferenceStrength::Weak)continue;
   if(schema.strength!=ReferenceStrength::StrongCreature||schema.category!=FieldCategory::Reference||schema.reference!=RefKind::Creature)return fail(e,"invalid compiled strong-reference contract");
   const auto& target=payload.fields->at(schema.key).target;
   if(target.resource||target.slot||(target.owner&&!observed.count(target.owner)))return fail(e,"strong reference does not name a tracked Creature root");
   StrongStorage identity;
   if(!storage(payload.kind,payload.owner,schema,identity,e)){if(e.empty())e="native strong storage identity unavailable";return false;}
   // Storage membership is supplied by compiled native factory topology. A
   // payload cannot attribute a foreign slot to itself to conceal a duplicate.
   if(identity.kind!=payload.kind||identity.owner!=payload.owner||identity.member.empty()||identity.member.size()>256||identity.member.find('\0')!=std::string::npos)return fail(e,"strong storage owner/member mismatch");
   auto inserted=slots.emplace(std::move(identity),target.owner);
   if(!inserted.second&&inserted.first->second!=target.owner)return fail(e,"aliased native strong storage has conflicting targets");
   if(slots.size()>MaxBytes/sizeof(uint64_t)/4)return fail(e,"native strong storage census exceeds bound");
  }
 }
 // Empty or zero-count actors/global families still require their typed payload;
 // otherwise a missing inactive SmartPtr could be concealed by the same count.
 for(const auto& actor:observed)if(!owners.count({ReferenceOwner::Actor,actor.first}))return fail(e,"missing actor strong-root payload");
 for(uint32_t family:globals)if(!owners.count({ReferenceOwner::Global,family}))return fail(e,"missing global strong-root payload");
 std::vector<StrongReference> references;references.reserve(slots.size());
 std::pair<ReferenceOwner,uint64_t> previous{ReferenceOwner::Actor,0};uint64_t ordinal=0;
 for(const auto& slot:slots){const auto owner=std::make_pair(slot.first.kind,slot.first.owner);if(owner!=previous){previous=owner;ordinal=0;}
  references.push_back({slot.first.kind,slot.first.owner,++ordinal,slot.second});
 }
 std::map<uint64_t,int32_t> reconciled;if(!reconcileReferenceCounts(observed,globals,references,reconciled,e))return false;
 out=std::move(references);counts=std::move(reconciled);e.clear();return true;
}
}
