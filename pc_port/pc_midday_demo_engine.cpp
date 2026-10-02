#include "pc_midday_demo.h"
#include "Demo.h"
#include "Creature.h"
#include <set>
DemoFlags::DemoFlags(const pc_midday::DemoStageTag&){
 mFlagCount=32;mFlagDataNum=256;mCurrentDataIndex=0;mStoredFlags=nullptr;
 mFlagDataList=nullptr;mTargetCreature=nullptr;mWaitTimer=0;mCurrentDemoIndex=-1;
}
namespace pc_midday {
namespace {
bool content(const DemoFlags&s,DemoFields&v,std::string&e){
 static_assert(MAX_UFO_PARTS==30&&DEMOFLAG_UfoPartDiscoveryOffset==32,"Demo registration source changed");
 if(s.mFlagCount!=32||s.mFlagDataNum!=256||s.mCurrentDataIndex!=62||!s.mStoredFlags||!s.mFlagDataList){e="DemoFlags native allocation/registration mismatch";return false;}
 std::set<const DemoFlag*>unique;
 for(size_t i=0;i<256;++i){auto*d=s.mFlagDataList[i];
  if(i>=62){if(d){e="unknown DemoFlag registration";return false;}continue;}
  if(!d||!unique.insert(d).second||!d->mName||d->mIndex!=i){e="DemoFlag absent/aliased/unordered content";return false;}
  v.descriptors.push_back({d->mIndex,d->mMovieIndex,d->_08,d->_0A});
 }
 return true;
}
bool same(const std::vector<DemoDescriptor>&a,const std::vector<DemoDescriptor>&b){
 if(a.size()!=b.size())return false;
 for(size_t i=0;i<a.size();++i)if(a[i].index!=b[i].index||a[i].movie!=b[i].movie||a[i].part!=b[i].part||a[i].text!=b[i].text)return false;
 return true;
}
}
bool captureDemo(const DemoFlags&s,const BirthLedger&ledger,const PlayerCoreReadFence&f,Bytes&out,std::string&e){
 if(!f.sceneInitialized||!f.agreedReadOnlyFence||f.tickBefore!=f.tickAfter){e="Demo capture requires initialized stopped scene";return false;}
 DemoFields v;if(!content(s,v,e))return false;
 std::copy(s.mStoredFlags,s.mStoredFlags+32,v.stored.begin());v.current=s.mCurrentDemoIndex;v.timer=s.mWaitTimer;
 if(v.current<-1||v.current>=62){e="Demo native current index invalid";return false;}
 // Constructor/initGame/initCourse leave target indeterminate when inactive.
 // update() returns before reading it. Never inspect that pointer in this state.
 if(v.current!=-1&&s.mTargetCreature){auto*life=ledger.lookup(s.mTargetCreature);if(!life){e="active demo target unresolved/retired";return false;}v.target=life->id;}
 return encodeDemo(v,out,e);
}
struct IsolatedDemo::Impl {
 std::array<uint8_t,32>stored{};
 std::array<DemoFlag*,256>list{};std::vector<std::unique_ptr<DemoFlag>>descriptors;
 // Resource destroyed before its independently owned arrays/descriptors.
 std::unique_ptr<DemoFlags>resource;
};
IsolatedDemo::IsolatedDemo()=default;IsolatedDemo::~IsolatedDemo()=default;
DemoFlags*IsolatedDemo::resource()const{return impl_?impl_->resource.get():nullptr;}
bool IsolatedDemo::prepare(const DemoFlags&s,const Bytes&b,const std::map<uint64_t,Creature*>&roots,const RestoreGate&g,ConstructorFence&f,std::string&e){
 if(impl_||!g.freshProcess||!g.paused||!g.zeroInput||!g.birthEffectsSuppressed||!g.rewardsSuppressed||!g.rngDrawsSuppressed||!g.audioVoicesSuppressed||!f.held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="Demo staging requires fresh full constructor fence";return false;}
 DemoFields v,compiled;if(!decodeDemo(b,v,e)||!content(s,compiled,e))return false;
 if(!same(v.descriptors,compiled.descriptors)){e="Demo compiled descriptor topology changed";return false;}
 Creature*target=nullptr;if(v.target){auto found=roots.find(v.target);if(found==roots.end()||!found->second){e="Demo staged actor target unavailable";return false;}target=found->second;}
 // The scene must supply its validated exact snapshot-generation root map;
 // membership here does not independently prove complete actor graph coverage.
 try{auto stage=std::make_unique<Impl>();stage->stored=v.stored;stage->descriptors.reserve(62);
  for(size_t i=0;i<62;++i){auto d=std::make_unique<DemoFlag>();const auto&source=*s.mFlagDataList[i];
   d->mName=source.mName;d->mIndex=source.mIndex;d->mMovieIndex=source.mMovieIndex;d->_08=source._08;d->_0A=source._0A;
   stage->list[i]=d.get();stage->descriptors.push_back(std::move(d));}
  stage->resource=std::make_unique<DemoFlags>(DemoStageTag{});auto&r=*stage->resource;
  r.mStoredFlags=stage->stored.data();r.mFlagDataList=stage->list.data();r.mCurrentDataIndex=62;
  r.mCurrentDemoIndex=v.current;r.mWaitTimer=v.timer;r.mTargetCreature=target;
  impl_=std::move(stage);e.clear();return true;
 }catch(const std::exception&x){e=std::string("Demo staged resource allocation failed: ")+x.what();return false;}
}
}
