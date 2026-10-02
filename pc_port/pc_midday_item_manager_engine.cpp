#include "pc_midday_item_manager.h"
#include "pc_midday_constructor.h"
#include "ItemMgr.h"
#include <algorithm>
#include <limits>
#include <typeinfo>
namespace pc_midday {struct ItemManagerStageTag {};}
// Distinct inert path: no addGenNode, buryMode, motion-table factory or loads.
ItemMgr::ItemMgr(const pc_midday::ItemManagerStageTag&):PolyObjectMgr(0){
 for(int i=0;i<3;++i){mPebbleShapeList[i]=nullptr;mGrassShapeList[i]=nullptr;}
 mType=0;mPikiHeadMgr=nullptr;mMeltingPotMgr=nullptr;mRootUseNode.mType=0;
 mItemShapes=nullptr;mUfoShape=nullptr;mItemMotionTable=nullptr;mUfoMotionTable=nullptr;
}
class PcMiddayIsolatedItemMgr final:public ItemMgr {
public:
 PcMiddayIsolatedItemMgr():ItemMgr(pc_midday::ItemManagerStageTag{}){
  // Base allocates its zero-length registration table. This stage replaces it
  // with independently owned descriptors, not an ordinary manager pool.
  delete[] mEntries;mEntries=nullptr;mEntryCount=0;mMaxClassLength=0;
  mMaxSize=0;mPoolCapacity=0;mActiveObjects=0;mObjectPool=nullptr;mObjectIndices=nullptr;
 }
 bool owns(uint32_t capacity,uint32_t stride,const std::vector<PolyData>& templates,
           const std::vector<int>&statuses,const std::unique_ptr<u8[]>&storage,
           const std::vector<ItemShapeObject*>&shapes)const{
  return mPoolCapacity==int(capacity)&&mMaxSize==int(stride)&&mEntries==templates.data()&&mEntryCount==templates.size()&&mMaxClassLength==int(templates.size())&&mObjectIndices==statuses.data()&&mObjectPool==storage.get()&&mItemShapes==shapes.data();
 }
 void install(const std::vector<int>& statuses,uint32_t count)noexcept{
  std::copy(statuses.begin(),statuses.end(),mObjectIndices);mActiveObjects=int(count);
 }
 void attach(uint32_t capacity,uint32_t stride,std::vector<PolyData>& templates,
             std::vector<int>&statuses,std::unique_ptr<u8[]>&storage,
             std::vector<ItemShapeObject*>&shapes){
  mPoolCapacity=int(capacity);mMaxSize=int(stride);mActiveObjects=0;
  mEntries=templates.data();mEntryCount=u32(templates.size());mMaxClassLength=int(templates.size());
  mObjectPool=storage.get();mObjectIndices=statuses.data();mItemShapes=shapes.data();
 }
};
namespace pc_midday {
namespace {
struct ContentInputs {
 std::vector<int>uses;std::vector<ItemShapeObject*>shapes;
 Shape*pebble[3]{};Shape*grass[3]{};UfoShapeObject*ufo=nullptr;
 PaniMotionTable*itemMotion=nullptr;PaniMotionTable*ufoMotion=nullptr;
 bool operator==(const ContentInputs&b)const{
  return uses==b.uses&&shapes==b.shapes&&std::equal(pebble,pebble+3,b.pebble)&&std::equal(grass,grass+3,b.grass)&&ufo==b.ufo&&itemMotion==b.itemMotion&&ufoMotion==b.ufoMotion;
 }
};
bool content(const ItemMgr&s,ContentInputs&out,std::string&e){
 if(!s.mItemShapes||!s.mItemMotionTable||!s.mUfoMotionTable||!s.mUfoShape){e="ItemMgr initialized content roots unavailable";return false;}
 ContentInputs c;c.shapes.assign(s.mItemShapes,s.mItemShapes+11);
 for(int i=0;i<3;++i){c.pebble[i]=s.mPebbleShapeList[i];c.grass[i]=s.mGrassShapeList[i];}
 c.ufo=s.mUfoShape;c.itemMotion=s.mItemMotionTable;c.ufoMotion=s.mUfoMotionTable;
 std::set<const CoreNode*>nodes;std::set<int>types;
 for(const CoreNode*n=s.mRootUseNode.mChild;n;n=n->mNext){
  if(nodes.size()>=4096||!nodes.insert(n).second||typeid(*n)!=typeid(ItemMgr::UseNode)||n->mParent!=&s.mRootUseNode||n->mChild){e="ItemMgr source use-list topology changed/invalid";return false;}
  int id=static_cast<const ItemMgr::UseNode*>(n)->mType;
  if(id<0||!types.insert(id).second){e="ItemMgr use-list class missing/duplicate";return false;}c.uses.push_back(id);
 }
 out=std::move(c);e.clear();return true;
}
}
struct IsolatedItemManager::Impl {
 // Reverse destruction: roots BEFORE manager BEFORE its independent backing.
 ConstructorFence*fence=nullptr;PolyPoolPlan plan;uint32_t stride=0;bool installed=false;
 std::vector<ItemMgr::PolyData>templates;std::vector<int>statuses;
 std::unique_ptr<u8[]>storage;std::vector<ItemShapeObject*>shapes;
 std::vector<std::unique_ptr<ItemMgr::UseNode>>uses;
 std::unique_ptr<PcMiddayIsolatedItemMgr>manager;
 ItemPolyRoots actors;
};
IsolatedItemManager::IsolatedItemManager()=default;IsolatedItemManager::~IsolatedItemManager()=default;
ItemMgr*IsolatedItemManager::manager()const{return impl_?impl_->manager.get():nullptr;}
const std::map<uint64_t,Creature*>&IsolatedItemManager::roots()const{static const std::map<uint64_t,Creature*>empty;return impl_?impl_->actors.roots():empty;}
bool IsolatedItemManager::installStagedChannel(std::string&e){
 if(!impl_||impl_->installed||!impl_->manager||!impl_->fence||!impl_->fence->held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="private ItemMgr channel requires uninstalled constructor-owned stage";return false;}
 auto&m=*impl_->manager;
 if(&m==itemMgr){e="private channel cannot target live ItemMgr singleton";return false;}
 if(!m.owns(impl_->plan.capacity,impl_->stride,impl_->templates,impl_->statuses,impl_->storage,impl_->shapes)){e="private ItemMgr native buffers are no longer stage-owned";return false;}
 PolyPoolView view;if(!readPolyPoolView(m,view,e))return false;
 const auto&p=impl_->plan;const auto&roots=impl_->actors.roots();
 if(view.capacity!=p.capacity||view.stride!=impl_->stride||view.count||roots.size()!=p.count||view.objects.size()!=p.capacity||view.statuses.size()!=p.capacity){e="private ItemMgr channel/root geometry changed";return false;}
 ItemPolyConcreteTypes types;if(!types.bind(m,e))return false;
 std::set<uint64_t>ids;for(const auto&root:roots)ids.insert(root.first);
 if(!validatePolyPool(p,ids,types.allocatedClasses(),e))return false;
 std::vector<int>statuses;statuses.reserve(p.capacity);
 for(size_t i=0;i<p.capacity;++i){
  const auto&slot=p.slots[i];if(view.objects[i]!=impl_->storage.get()+size_t(i)*impl_->stride){e="private ItemMgr backing identity changed";return false;}if(view.statuses[i]!=-1){e="private ItemMgr channel is no longer free";return false;}
  if(slot.life==SlotLife::Free){statuses.push_back(-1);continue;}
  auto found=roots.find(slot.actor);bool match=false;
  if(found==roots.end()||found->second!=view.objects[i]||!types.matches(slot.classId,found->second,match,e)||!match){e="private ItemMgr channel root/type/backing mismatch";return false;}
  statuses.push_back(slot.life==SlotLife::Retained?-2:slot.classId);
 }
 // Private destination only. Complete validations/allocation above; named int
 // writes below cannot allocate, call engine birth/init/kill or mutate globals.
 m.install(statuses,p.count);impl_->installed=true;e.clear();return true;
}
bool IsolatedItemManager::prepare(const ItemMgr&s,const PolyPoolPlan&p,ConstructorFence&f,std::string&e){
 if(impl_||!f.held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="isolated ItemMgr requires fresh allocation and physical constructor owner";return false;}
 ItemPolyConcreteTypes types;if(!types.bind(s,e))return false;
 PolyPoolView source;if(!readPolyPoolView(s,source,e))return false;
 std::set<uint64_t>ids;for(const auto&slot:p.slots)if(slot.actor)ids.insert(slot.actor);
 if(!validatePolyPool(p,ids,types.allocatedClasses(),e))return false;
 if(p.capacity>MaxActors||!source.stride||source.stride>16*1024*1024||p.capacity!=source.capacity||source.stride>std::numeric_limits<size_t>::max()/std::max<uint32_t>(1,p.capacity)){e="ItemMgr staging geometry incompatible";return false;}
 ContentInputs a,b;if(!content(s,a,e)||!content(s,b,e))return false;
 if(!(a==b)){e="ItemMgr content changed during stopped observation";return false;}
 for(const auto&t:source.templates)if(!types.validateTemplate(t,e))return false;
 // Whole source/prototype/content validation precedes any native manager ctor.
 try {
  auto stage=std::make_unique<Impl>();stage->fence=&f;stage->plan=p;stage->stride=source.stride;stage->templates.reserve(source.templates.size());
  for(const auto&t:source.templates)stage->templates.push_back({const_cast<Creature*>(static_cast<const Creature*>(t.prototype)),int(t.bytes),t.classId});
  stage->statuses.assign(p.capacity,-1);stage->storage=std::make_unique<u8[]>(size_t(p.capacity)*source.stride);stage->shapes=a.shapes;
  stage->uses.reserve(a.uses.size());for(int id:a.uses){auto node=std::make_unique<ItemMgr::UseNode>();node->mType=id;stage->uses.push_back(std::move(node));}
  stage->manager=std::make_unique<PcMiddayIsolatedItemMgr>();auto&m=*stage->manager;
  m.attach(p.capacity,source.stride,stage->templates,stage->statuses,stage->storage,stage->shapes);
  for(int i=0;i<3;++i){m.mPebbleShapeList[i]=a.pebble[i];m.mGrassShapeList[i]=a.grass[i];}
  m.mUfoShape=a.ufo;m.mItemMotionTable=a.itemMotion;m.mUfoMotionTable=a.ufoMotion;
  CoreNode*previous=nullptr;for(auto&node:stage->uses){node->mParent=&m.mRootUseNode;if(previous)previous->mNext=node.get();else m.mRootUseNode.mChild=node.get();previous=node.get();}
  if(!stage->actors.prepare(m,p,f,e))return false;
  // Forwarded MeltingPot/PikiHead channels remain null until separately staged.
  impl_=std::move(stage);e.clear();return true;
 }catch(const std::exception&x){e=std::string("isolated ItemMgr allocation failed: ")+x.what();return false;}
}
}
