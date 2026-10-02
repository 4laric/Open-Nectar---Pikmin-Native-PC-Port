#pragma once
#include "pc_midday_item_factory.h"
namespace pc_midday {
// No public constructor/tag: only the owned native stage can instantiate it.
struct ItemManagerStageTag;
class IsolatedItemManager {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 IsolatedItemManager();~IsolatedItemManager();
 IsolatedItemManager(const IsolatedItemManager&)=delete;
 IsolatedItemManager&operator=(const IsolatedItemManager&)=delete;
 bool prepare(const ItemMgr& contentSource,const PolyPoolPlan&,ConstructorFence&,std::string&);
 // Installs only the private manager channel after root/subobject/typed bind
 // staging. No live-world publication; retained -2 stays separately enumerable.
 bool installStagedChannel(std::string&);
 ItemMgr* manager()const;
 const std::map<uint64_t,Creature*>& roots()const;
 // Source manager/resource lifetime, complete stopped world, all typed binding,
 // separate forwarded managers and final ownership publication remain backend
 // obligations. This object never installs a gameflow/global root.
};
}
