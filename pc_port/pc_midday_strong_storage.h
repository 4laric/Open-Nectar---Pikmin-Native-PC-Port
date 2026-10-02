#pragma once
#include "pc_midday_actor_archive.h"
class Creature;
class FormPoint;
namespace pc_midday {
// Callback arguments are borrowed for this call only. Consumers copy strings
// if retaining a census. Addresses identify native storage, never wire IDs.
// Invoke at a stopped tick with fully validated payload and initialized native
// allocation topology. A failed walk may have emitted a partial census: discard
// it. Canonicalize owner through the typed scene catalog, then member/index;
// never infer ownership from a target pointer or from the wire key text.
class StrongStorageVisitor {
public:
 virtual ~StrongStorageVisitor()=default;
 virtual bool visit(const char* key,const StrongStorageSlot&,std::string& error)=0;
};
class StrongStorageArchive final:public ActorArchive {
 StrongStorageVisitor& visitor;std::string& error;double now;
public:
 StrongStorageArchive(StrongStorageVisitor& v,std::string& e,double clock=0):visitor(v),error(e),now(clock){}
 Mode mode()const override{return Mode::Capture;}
 double clock_now()const override{return now;}
 bool scalar(const char*,ScalarKind,void*)override{return true;}
 bool reference(const char*,RefKind,void*&)override{return true;}
 bool handle(const char*,RefKind,u32&)override{return true;}
 bool token64(const char*,RefKind,u64&)override{return true;}
 bool fail(const char* reason)override{if(error.empty())error=reason;return false;}
 bool strongReference(const char* key,RefKind kind,void*& target,const StrongStorageSlot& slot)override{
  if(kind!=RefKind::Creature||!key||!*key||!slot.storage||!slot.owner||!slot.ownerType||!*slot.ownerType||!slot.member||!*slot.member||slot.index< -1||slot.target!=target)return fail("invalid native strong storage metadata");
  return visitor.visit(key,slot,error);
 }
};
// Diagnostic snapshot only: no schema checks, clamping, or engine updates.
struct PlateCounts {int capacity;int used;int total;u32 platePikis;int happa[3];};
bool navi_plate_counts(Navi&,PlateCounts&,std::string&);
bool visit_navi_strong_storage(Navi&,const ActorFields&,StrongStorageVisitor&,std::string&);
bool visit_creature_strong_storage(Creature&,const ActorFields&,StrongStorageVisitor&,std::string&);
bool visit_formpoint_strong_storage(FormPoint&,StrongStorageVisitor&,std::string&);
}
