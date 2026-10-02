#include "pc_midday_item_factory.h"
#include "pc_midday_constructor.h"
#include "ItemMgr.h"
#include "ItemObject.h"
#include "BombItem.h"
#include "MizuItem.h"
#include "RopeCreature.h"
#include "SeedItem.h"
#include "KeyItem.h"
#include "DoorItem.h"
#include "ObjType.h"
#include "ItemAI.h"
#include <typeinfo>
#include <new>
#include <exception>
#include <type_traits>
struct PcMiddayItemFactoryAccess {
 static Shape* seed(const SeedItem& s){return s.mSeedShape;}
 static Shape* planted(const SeedItem& s){return s.mPlantedShape;}
 static Shape* key(const KeyItem& s){return s.mModel;}
};
namespace pc_midday {
namespace {
bool forwarded(int id){switch(id){case OBJTYPE_Kusa:case OBJTYPE_BoBase:case OBJTYPE_RockGen:case OBJTYPE_GrassGen:case OBJTYPE_SluiceSoft:case OBJTYPE_SluiceHard:case OBJTYPE_SluiceBomb:case OBJTYPE_SluiceBombHard:case OBJTYPE_Fish:case OBJTYPE_Ufo:case OBJTYPE_Goal:return true;default:return false;}}
std::set<int> allocated(){return {OBJTYPE_SunsetStart,OBJTYPE_SunsetGoal,OBJTYPE_Bomb,OBJTYPE_Water,OBJTYPE_FallWater,OBJTYPE_Rope,OBJTYPE_Fulcrum,OBJTYPE_Seed,OBJTYPE_Key,OBJTYPE_Door,OBJTYPE_Gate,OBJTYPE_BombGen};}
std::set<int> constructible(){return allocated();}
template<class T>bool concrete(const Creature&c,int id){return typeid(c)==typeid(T)&&c.mObjType==id&&static_cast<const void*>(&c)==static_cast<const void*>(static_cast<const T*>(&c));}
bool concreteClass(int id,const Creature&c,uint32_t&bytes){
#define CLASS(ID,T) case ID:bytes=sizeof(T);return concrete<T>(c,id)
 switch(id){CLASS(OBJTYPE_SunsetStart,NaviDemoSunsetStart);CLASS(OBJTYPE_SunsetGoal,NaviDemoSunsetGoal);CLASS(OBJTYPE_Bomb,BombItem);CLASS(OBJTYPE_Water,MizuItem);CLASS(OBJTYPE_FallWater,MizuItem);CLASS(OBJTYPE_Rope,RopeItem);CLASS(OBJTYPE_Fulcrum,Fulcrum);CLASS(OBJTYPE_Seed,SeedItem);CLASS(OBJTYPE_Key,KeyItem);CLASS(OBJTYPE_Door,DoorItem);CLASS(OBJTYPE_Gate,DoorItem);CLASS(OBJTYPE_BombGen,BombGenItem);default:return false;}
#undef CLASS
}
struct Placed {Creature*pointer=nullptr;void(*destroy)(Creature*)=nullptr;};
template<class T>Placed place(void*slot){auto*p=new(slot)T();return {p,[](Creature*c){static_cast<T*>(c)->~T();}};}
Placed placeBombGen(void*slot){auto*p=new(slot)BombGenItem(nullptr);return {p,[](Creature*c){static_cast<BombGenItem*>(c)->~BombGenItem();}};}
bool fits(int id,const void*p,uint32_t stride){
#define FIT(ID,T) case ID:return sizeof(T)<=stride&&reinterpret_cast<uintptr_t>(p)%alignof(T)==0
 switch(id){FIT(OBJTYPE_SunsetStart,NaviDemoSunsetStart);FIT(OBJTYPE_SunsetGoal,NaviDemoSunsetGoal);FIT(OBJTYPE_Fulcrum,Fulcrum);FIT(OBJTYPE_BombGen,BombGenItem);FIT(OBJTYPE_Bomb,BombItem);FIT(OBJTYPE_Water,MizuItem);FIT(OBJTYPE_FallWater,MizuItem);FIT(OBJTYPE_Rope,RopeItem);FIT(OBJTYPE_Seed,SeedItem);FIT(OBJTYPE_Key,KeyItem);FIT(OBJTYPE_Door,DoorItem);FIT(OBJTYPE_Gate,DoorItem);default:return false;}
#undef FIT
}
}
bool ItemPolyConcreteTypes::bind(const ItemMgr&m,std::string&e){
 PolyPoolView v;if(!readPolyPoolView(m,v,e))return false;
 std::map<int,PolyTemplateView>table;std::set<int>classes;
 for(const auto&t:v.templates){
  if(t.classId<0||!table.emplace(t.classId,t).second){e="ItemMgr duplicate factory registration";return false;}
  if(!t.bytes&&!t.prototype){if(!forwarded(t.classId)){e="unknown ItemMgr forwarding class";return false;}continue;}
  uint32_t size=0;if(!t.prototype||!t.bytes||t.bytes>v.stride||!concreteClass(t.classId,*static_cast<const Creature*>(t.prototype),size)||size!=t.bytes){e="ItemMgr source prototype/concrete sizeof mismatch";return false;}
  classes.insert(t.classId);
 }
 if(classes!=allocated()){e="ItemMgr compiled allocated factory matrix changed";return false;}
 templates_.swap(table);e.clear();return true;
}
bool ItemPolyConcreteTypes::validateTemplate(const PolyTemplateView&t,std::string&e)const{
 auto i=templates_.find(t.classId);
 if(i==templates_.end()||i->second.bytes!=t.bytes||i->second.prototype!=t.prototype){e="ItemMgr template is outside bound source factory";return false;}
 if(!t.bytes){if(t.prototype||!forwarded(t.classId)){e="ItemMgr forwarding metadata mismatch";return false;}}
 else {uint32_t size=0;if(!t.prototype||!concreteClass(t.classId,*static_cast<const Creature*>(t.prototype),size)||size!=t.bytes){e="ItemMgr prototype changed after factory binding";return false;}}
 e.clear();return true;
}
bool ItemPolyConcreteTypes::matches(int id,const void*p,bool&match,std::string&e)const{
 auto i=templates_.find(id);if(i==templates_.end()||!i->second.bytes||!p){e="ItemMgr match requires bound allocated class and live actor";return false;}
 uint32_t size=0;match=concreteClass(id,*static_cast<const Creature*>(p),size);e.clear();return true;
}
std::set<int>ItemPolyConcreteTypes::allocatedClasses()const{std::set<int>ids;for(const auto&t:templates_)if(t.second.bytes)ids.insert(t.first);return ids;}
struct ItemPolyRoots::Impl {
 ConstructorFence* fence=nullptr;
 std::vector<Placed>owned;
 std::map<uint64_t,Creature*>roots;
 ~Impl(){if(owned.empty())return;if(!fence||!fence->held())std::terminate();std::string e;if(!pc_sim_rng_constructor_suppression(true,e))std::terminate();for(auto i=owned.rbegin();i!=owned.rend();++i)i->destroy(i->pointer);}
};
ItemPolyRoots::ItemPolyRoots()=default;ItemPolyRoots::~ItemPolyRoots()=default;
const std::map<uint64_t,Creature*>&ItemPolyRoots::roots()const{static const std::map<uint64_t,Creature*>empty;return impl_?impl_->roots:empty;}
bool ItemPolyRoots::prepare(const ItemMgr&m,const PolyPoolPlan&p,ConstructorFence&f,std::string&e){
 if(impl_||!f.held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="ItemMgr placement requires fresh allocation object and actual constructor owner";return false;}
 ItemPolyConcreteTypes types;if(!types.bind(m,e))return false;
 PolyPoolView v;if(!readPolyPoolView(m,v,e))return false;
 for(const auto&t:v.templates)if(!types.validateTemplate(t,e))return false;
 std::vector<ItemPlacement>tasks;if(!preflightItemPlacement(p,v,types.allocatedClasses(),constructible(),tasks,e))return false;
 for(const auto&t:tasks)if(!fits(t.classId,v.objects[t.slot],v.stride)){e="native item placement alignment/stride mismatch";return false;}
 // Extract all constructor inputs from exact compiled fresh prototypes before
 // any placement. These borrowed resources must outlive staged roots. Their
 // mutable canonical payloads are restored separately, not cloned here.
 struct Inputs {CreatureProp* props=nullptr;Shape* first=nullptr;Shape* second=nullptr;ItemShapeObject* itemShape=nullptr;SimpleAI* ai=nullptr;};
 std::map<int,Inputs> inputs;
 for(const auto&t:v.templates){
  if(!t.bytes)continue;
  const auto*c=static_cast<const Creature*>(t.prototype);Inputs in;in.props=c->mProps;
  auto prop=[&](const std::type_info& expected){return in.props&&typeid(*in.props)==expected;};
  switch(t.classId){
   case OBJTYPE_Bomb:case OBJTYPE_Water:case OBJTYPE_FallWater:{
    const auto*i=static_cast<const ItemCreature*>(c);in.itemShape=i->mItemShapeObject;in.ai=dynamic_cast<SimpleAI*>(i->mSAICtx.mStateMachine);
    if(!in.itemShape||!in.ai||!prop(t.classId==OBJTYPE_Bomb?typeid(BombItemProp):typeid(MizuItemProp))||typeid(*in.ai)!=(t.classId==OBJTYPE_Bomb?typeid(BombAI):(t.classId==OBJTYPE_Water?typeid(WaterAI):typeid(FallWaterAI)))){e="native item prototype props/shape/AI contract changed";return false;}break;}
   case OBJTYPE_Rope:if(!prop(typeid(RopeProp))){e="Rope prototype property contract changed";return false;}in.first=static_cast<const RopeItem*>(c)->mModel;break;
   case OBJTYPE_Seed:if(!prop(typeid(SeedProp))){e="Seed prototype property contract changed";return false;}in.first=PcMiddayItemFactoryAccess::seed(*static_cast<const SeedItem*>(c));in.second=PcMiddayItemFactoryAccess::planted(*static_cast<const SeedItem*>(c));break;
   case OBJTYPE_Key:if(!prop(typeid(KeyProp))){e="Key prototype property contract changed";return false;}in.first=PcMiddayItemFactoryAccess::key(*static_cast<const KeyItem*>(c));break;
   case OBJTYPE_Door:case OBJTYPE_Gate:if(!prop(typeid(DoorProp))){e="Door prototype property contract changed";return false;}in.first=static_cast<const DoorItem*>(c)->mItemShape;break;
   default:if(in.props||static_cast<const ItemObject*>(c)->mItemShape){e="passive item prototype has unsupported constructor inputs";return false;}
  }
  inputs.emplace(t.classId,in);
 }
 std::unique_ptr<Impl> staged;
 try {
  staged=std::make_unique<Impl>();staged->fence=&f;staged->owned.reserve(tasks.size());
  for(const auto&t:tasks){Placed root;void*slot=const_cast<void*>(v.objects[t.slot]);const auto& in=inputs.at(t.classId);
   auto keep=[&](auto* p){using T=std::remove_pointer_t<decltype(p)>;return Placed{p,[](Creature*c){static_cast<T*>(c)->~T();}};};
   switch(t.classId){
    case OBJTYPE_SunsetStart:root=place<NaviDemoSunsetStart>(slot);break;
    case OBJTYPE_SunsetGoal:root=place<NaviDemoSunsetGoal>(slot);break;
    case OBJTYPE_Fulcrum:root=place<Fulcrum>(slot);break;
    case OBJTYPE_BombGen:root=placeBombGen(slot);break;
    case OBJTYPE_Bomb:root=keep(new(slot)BombItem(in.props,in.itemShape,in.ai));break;
    case OBJTYPE_Water:case OBJTYPE_FallWater:root=keep(new(slot)MizuItem(t.classId,in.props,in.itemShape,in.ai));break;
    case OBJTYPE_Rope:root=keep(new(slot)RopeItem(in.props,in.first));break;
    case OBJTYPE_Seed:{Shape* shapes[2]={in.first,in.second};root=keep(new(slot)SeedItem(in.props,shapes));break;}
    case OBJTYPE_Key:root=keep(new(slot)KeyItem(in.props,in.first));break;
    case OBJTYPE_Door:case OBJTYPE_Gate:root=keep(new(slot)DoorItem(t.classId,in.props,in.first));break;
    default:e="item constructor not implemented";return false;
   }
   staged->owned.push_back(root);staged->roots.emplace(t.actor,root.pointer);
  }
 }catch(const std::exception&x){e=std::string("native item constructor allocation failed: ")+x.what();return false;}
 impl_=std::move(staged);e.clear();return true;
}
}
