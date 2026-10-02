#pragma once
#include "pc_midday_manager_pool.h"
namespace pc_midday {
// Ordinal IDs are local to this exact binding/generation. Engine lifetimes and
// opaque token64 values are distinct adapter state and MUST NOT be substituted.
struct InventoryScope {Binding binding;uint64_t generation=0,frame=0;};
bool sameInventoryScope(const InventoryScope&,const InventoryScope&);
struct InventoryRoot {const void* address=nullptr;Family family=Family::Captain;SlotLife life=SlotLife::Active;};
struct InventoryChannel {
 InventoryScope scope;std::string source;
 bool initialized=false,complete=false,stopped=false;uint64_t frameAfter=0;
 std::vector<InventoryRoot> roots;
};
using InventoryFactory=std::map<std::string,std::set<Family>>;
class CheckpointInventory {
 InventoryScope scope_;
 BirthLedger identities_;
 std::map<uint64_t,InventoryRoot> roots_;
 bool ready_=false;
 friend bool buildCheckpointInventory(const InventoryScope&,const InventoryFactory&,const std::vector<InventoryChannel>&,CheckpointInventory&,std::string&);
public:
 bool ready()const{return ready_;}
 // Every consumer must bind its exact epoch before obtaining native addresses
 // or the ledger. A matching actor ordinal from another snapshot is insufficient.
 const BirthLedger* identities(const InventoryScope&,std::string&)const;
 bool root(const InventoryScope&,uint64_t,InventoryRoot&,std::string&)const;
 bool ids(const InventoryScope&,std::set<uint64_t>&,std::string&)const;
};
bool buildCheckpointInventory(const InventoryScope&,const InventoryFactory&,const std::vector<InventoryChannel>&,CheckpointInventory&,std::string&);
// Pure bounded selection. Includes retained -2; free backing pointers are not
// actors. No native callbacks or fields are dereferenced here.
bool monoInventoryRoots(const MonoPoolView&,Family,std::vector<InventoryRoot>&,std::string&);
}
