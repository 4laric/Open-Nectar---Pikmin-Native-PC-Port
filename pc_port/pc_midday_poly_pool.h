#pragma once
#include "pc_midday_inventory.h"
struct PolyObjectMgr;
namespace pc_midday {
struct PolyTemplateView {int classId=-1; uint32_t bytes=0; const void* prototype=nullptr;};
struct PolyPoolView {
 uint32_t capacity=0,count=0,stride=0;
 std::vector<int> statuses;
 std::vector<const void*> objects;
 std::vector<PolyTemplateView> templates;
};
// Compiled concrete factory contract. Never classify by a broad mObjType alone
// or inspect free raw slots. Retained -2 has lost its source class index.
class PolyConcreteTypes {
public:
 virtual ~PolyConcreteTypes()=default;
 // Must compare exact compiled sizeof/subtype and source prototype identity.
 virtual bool validateTemplate(const PolyTemplateView&,std::string& error)const {
  error="compiled poly template layout unavailable";return false;
 }
 virtual bool matches(int classId,const void* actor,bool& match,std::string& error)const {
  (void)classId;(void)actor;(void)match;error="compiled poly concrete factory unavailable";return false;
 }
};
struct PolySlot {SlotLife life=SlotLife::Free;uint64_t actor=0;int classId=-1;};
struct PolyPoolPlan {uint32_t capacity=0,count=0;std::set<int> classes;std::vector<PolySlot> slots;};
bool readPolyPoolView(const PolyObjectMgr&,PolyPoolView&,std::string&);
bool planPolyPool(const PolyPoolView&,const CheckpointInventory&,const InventoryScope&,
                  const PolyConcreteTypes&,PolyPoolPlan&,std::string&);
bool validatePolyPool(const PolyPoolPlan&,const std::set<uint64_t>&,const std::set<int>&,std::string&);
bool encodePolyPool(const PolyPoolPlan&,Bytes&,std::string&);
bool decodePolyPool(const Bytes&,PolyPoolPlan&,std::string&);
}
