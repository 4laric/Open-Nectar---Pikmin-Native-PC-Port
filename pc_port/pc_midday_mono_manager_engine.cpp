#include "pc_midday_mono_manager.h"
#include "NaviMgr.h"
#include "PikiMgr.h"
#include <algorithm>
#include <typeinfo>
#include <type_traits>
namespace pc_midday { struct MonoManagerStageTag {}; }
// Inert local base construction. No props/load/animation/global AI setup.
NaviMgr::NaviMgr(const pc_midday::MonoManagerStageTag&):MonoObjectMgr(){
 mType=0;mEntryStatus=nullptr;mNaviShape=nullptr;
 mNaviShapeObject[0]=mNaviShapeObject[1]=nullptr;
 mMotionTable=nullptr;mNaviID=0;mNaviParms=nullptr;mMovieNavi=nullptr;
}
PikiMgr::PikiMgr(const pc_midday::MonoManagerStageTag&):MonoObjectMgr(){
 mType=0;mEntryStatus=nullptr;for(auto&leaf:mLeafModel)leaf=nullptr;
 mPikiShape=nullptr;mMapMgr=nullptr;_50=_54=_58=_5C=mDeadPikis=0;
 mMotionTable=nullptr;mPikiParms=nullptr;mNavi=nullptr;
 mUpdateFlag=mRefreshFlag=0;
}
namespace pc_midday {
namespace {
template<class M> class PrivateMono final:public M {
public:
 PrivateMono():M(MonoManagerStageTag{}){}
 // Backing is installed directly. create() would bypass owned arrays: forbidden.
 Creature*createObject()override{return nullptr;}
 void attach(std::vector<Creature*>&objects,std::vector<int>&statuses){
  this->mObjectList=objects.data();this->mEntryStatus=statuses.data();
  this->mMaxElements=int(objects.size());this->mNumObjects=0;
 }
 bool owns(const std::vector<Creature*>&objects,const std::vector<int>&statuses)const{
  return this->mObjectList==objects.data()&&this->mEntryStatus==statuses.data()&&
   this->mMaxElements==int(objects.size());
 }
};
bool blockedAliases(std::set<const void*>&out,std::string&e){
 // Reject physical roots of either existing global channel, including free
 // reusable backing and retained -2 roots. No checkpoint ID can waive this.
 for(const MonoObjectMgr*m:{static_cast<const MonoObjectMgr*>(naviMgr),static_cast<const MonoObjectMgr*>(pikiMgr)}){
  if(!m)continue;
  MonoPoolView v;if(!readMonoPoolView(*m,v,e))return false;
  for(auto*root:v.objects)if(root)out.insert(root);
 }
 return true;
}
template<class A>A* graphRoot(const ActorAllocationGraph& graph){if constexpr(std::is_same<A,Navi>::value)return graph.stagedNavi();else return graph.stagedPiki();}
template<class M,class A>struct Channel {
 ConstructorFence*fence=nullptr;MonoPoolPlan plan;bool installed=false;
 std::vector<Creature*>objects;std::vector<int>statuses;
 std::unique_ptr<PrivateMono<M>>manager;
 // Reverse disposal: exact owned actors before manager, then backing.
 std::vector<std::unique_ptr<ActorAllocationGraph>>actors;
 std::map<uint64_t,Creature*>roots;
 bool prepare(const M&source,const MonoPoolPlan&p,std::vector<std::unique_ptr<ActorAllocationGraph>>&input,ConstructorFence&f,std::string&e){
  if(!f.held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="Mono manager requires actual constructor owner";return false;}
  MonoPoolView view;if(!readMonoPoolView(source,view,e))return false;
  std::set<uint64_t>ids;for(const auto&slot:p.slots)if(slot.actor)ids.insert(slot.actor);
  if(!validateMonoPool(p,ids,e))return false;
  for(const auto&slot:p.slots)if(slot.life==SlotLife::Free){e="free Mono slot requires a source-defined reusable default graph factory";return false;}
  if(view.capacity!=p.capacity||input.size()!=p.capacity){e="Mono root/compiled capacity mismatch";return false;}
  std::set<const void*>forbidden(view.objects.begin(),view.objects.end()),unique;
  if(!blockedAliases(forbidden,e))return false;
  std::vector<Creature*>backing;std::map<uint64_t,Creature*>bindings;backing.reserve(p.capacity);
  for(size_t i=0;i<input.size();++i){
   if(!input[i]||!input[i]->heldBy(f)){e="Mono graph belongs to a different or released constructor fence";return false;}
   auto*root=graphRoot<A>(*input[i]);
   if(!root||typeid(*root)!=typeid(A)||forbidden.count(root)||!unique.insert(root).second){e="Mono root concrete type/ownership/alias mismatch";return false;}
   backing.push_back(root);if(p.slots[i].actor)bindings.emplace(p.slots[i].actor,root);
  }
  // Prepare all allocating bookkeeping before transferring actor ownership.
  auto stagedManager=std::make_unique<PrivateMono<M>>();
  std::vector<int>fresh(p.capacity,-1);MonoPoolPlan copied=p;
  objects=std::move(backing);statuses=std::move(fresh);plan=std::move(copied);
  manager=std::move(stagedManager);manager->attach(objects,statuses);
  roots=std::move(bindings);actors=std::move(input);fence=&f;e.clear();return true;
 }
 bool install(const RestoreGate&g,std::string&e){
  if(installed||!manager||!fence||!fence->held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="Mono channel requires uninstalled physically owned stage";return false;}
  if(!manager->owns(objects,statuses)||objects.size()!=actors.size()){e="Mono channel backing escaped stage ownership";return false;}
  for(size_t i=0;i<actors.size();++i)if(!actors[i]||!actors[i]->heldBy(*fence)||objects[i]!=graphRoot<A>(*actors[i])){e="Mono channel root ownership changed";return false;}
  std::map<uint64_t,Creature*>staged;
  if(!stageMonoPool(*manager,plan,g,staged,e))return false;
  // stageMonoPool completes allocation/validation before native writes. Its map
  // is deterministically derived from the exact unchanged owned backing.
  roots.swap(staged);installed=true;e.clear();return true;
 }
};
}
struct IsolatedNaviManager::Impl:Channel<NaviMgr,Navi>{};
struct IsolatedPikiManager::Impl:Channel<PikiMgr,ViewPiki>{};
IsolatedNaviManager::IsolatedNaviManager()=default;IsolatedNaviManager::~IsolatedNaviManager()=default;
IsolatedPikiManager::IsolatedPikiManager()=default;IsolatedPikiManager::~IsolatedPikiManager()=default;
NaviMgr*IsolatedNaviManager::manager()const{return impl_?impl_->manager.get():nullptr;}
PikiMgr*IsolatedPikiManager::manager()const{return impl_?impl_->manager.get():nullptr;}
const std::map<uint64_t,Creature*>&IsolatedNaviManager::roots()const{static const std::map<uint64_t,Creature*>empty;return impl_?impl_->roots:empty;}
const std::map<uint64_t,Creature*>&IsolatedPikiManager::roots()const{static const std::map<uint64_t,Creature*>empty;return impl_?impl_->roots:empty;}
bool IsolatedNaviManager::installStagedChannel(const RestoreGate&g,std::string&e){if(!impl_){e="Navi stage absent";return false;}return impl_->install(g,e);}
bool IsolatedPikiManager::installStagedChannel(const RestoreGate&g,std::string&e){if(!impl_){e="Piki stage absent";return false;}return impl_->install(g,e);}
bool IsolatedNaviManager::prepare(const NaviMgr&s,const MonoPoolPlan&p,std::vector<std::unique_ptr<ActorAllocationGraph>>&a,ConstructorFence&f,std::string&e){
 if(impl_){e="Navi stage already exists";return false;}
 if(p.capacity>2||!s.mNaviParms||!s.mNaviShape||!s.mMotionTable||!s.mNaviShapeObject[0]||(p.capacity==2&&!s.mNaviShapeObject[1])){e="Navi compiled content/captain capacity unavailable";return false;}
 try{auto stage=std::make_unique<Impl>();if(!stage->prepare(s,p,a,f,e))return false;
  auto&m=*stage->manager;m.mNaviParms=s.mNaviParms;m.mNaviShape=s.mNaviShape;m.mMotionTable=s.mMotionTable;
  m.mNaviShapeObject[0]=s.mNaviShapeObject[0];m.mNaviShapeObject[1]=s.mNaviShapeObject[1];
  // Mutable roster/ID/movie target are intentionally staged neutral, not restored.
  impl_=std::move(stage);e.clear();return true;
 }catch(const std::exception&x){e=std::string("Navi manager staging failed: ")+x.what();return false;}
}
bool IsolatedPikiManager::prepare(const PikiMgr&s,const MonoPoolPlan&p,std::vector<std::unique_ptr<ActorAllocationGraph>>&a,ConstructorFence&f,std::string&e){
 if(impl_){e="Piki stage already exists";return false;}
 if(!s.mPikiParms||!s.mMotionTable||!s.mPikiShape||!s.mMapMgr||!s.mLeafModel[0]||!s.mLeafModel[1]||!s.mLeafModel[2]){e="Piki compiled content unavailable";return false;}
 try{auto stage=std::make_unique<Impl>();if(!stage->prepare(s,p,a,f,e))return false;
  auto&m=*stage->manager;m.mPikiParms=s.mPikiParms;m.mMotionTable=s.mMotionTable;
  m.mPikiShape=s.mPikiShape;m.mMapMgr=s.mMapMgr;
  for(int i=0;i<3;++i)m.mLeafModel[i]=s.mLeafModel[i];
  // mNavi and mutable flags/counters require later typed scene binding.
  impl_=std::move(stage);e.clear();return true;
 }catch(const std::exception&x){e=std::string("Piki manager staging failed: ")+x.what();return false;}
}
}
