#pragma once
#include "pc_midday_poly_pool.h"
#include <memory>
class ItemMgr;
class Creature;
namespace pc_midday {
class ConstructorFence;
class ItemPolyConcreteTypes final:public PolyConcreteTypes {
 std::map<int,PolyTemplateView> templates_;
public:
 bool bind(const ItemMgr&,std::string&);
 bool validateTemplate(const PolyTemplateView&,std::string&)const override;
 bool matches(int,const void*,bool&,std::string&)const override;
 std::set<int> allocatedClasses()const;
};
struct ItemPlacement {uint32_t slot=0;uint64_t actor=0;int classId=-1;};
bool preflightItemPlacement(const PolyPoolPlan&,const PolyPoolView&,const std::set<int>& allocated,
                            const std::set<int>& constructible,std::vector<ItemPlacement>&,std::string&);
// Source-backed placement for all12 compiled ItemMgr allocated classes.
// Invalid prototype/resource contracts refuse before ANY placement. This owns staged roots and
// destroys them under the actual ConstructorFence on abort; it does not install
// manager statuses, bind actor state or publish a world. Keep the fence alive
// longer than this object. Complete backend installation remains separate.
class ItemPolyRoots {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 ItemPolyRoots();~ItemPolyRoots();
 ItemPolyRoots(const ItemPolyRoots&)=delete;
 ItemPolyRoots&operator=(const ItemPolyRoots&)=delete;
 bool prepare(const ItemMgr&,const PolyPoolPlan&,ConstructorFence&,std::string&);
 const std::map<uint64_t,Creature*>& roots()const;
};
}
