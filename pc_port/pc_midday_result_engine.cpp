#include "pc_midday_result.h"
#include "ResultFlags.h"
#include <algorithm>
ResultFlags::ResultFlags(const pc_midday::ResultStageTag&){mLength=38;mActiveCount=0;mTableSize=152;mStates=nullptr;mScreenToTableList=nullptr;for(auto&day:mDaysSeen)day=-1;}
bool PcMiddayResultAccess::read(const ResultFlags&s,pc_midday::ResultFields&out,std::string&e){
 using namespace pc_midday;ResultFields v;if(!compiled(v.descriptors,e))return false;
 if(s.mLength!=38||s.mTableSize!=152||s.mActiveCount!=v.descriptors.size()||!s.mStates||!s.mScreenToTableList){e="Result native allocated topology mismatch";return false;}
 // Ordinary constructor allocates148 map entries, but writes registered screens
 // only. Never inspect its uninitialized map holes or the misleading152 size.
 for(size_t i=0;i<v.descriptors.size();++i)if(s.mScreenToTableList[v.descriptors[i].screen]!=i){e="Result registered native screen mapping changed";return false;}
 std::copy(s.mStates,s.mStates+38,v.states.begin());std::copy(s.mDaysSeen,s.mDaysSeen+30,v.days.begin());out=std::move(v);e.clear();return true;
}
void PcMiddayResultAccess::bind(ResultFlags&r,uint8_t*states,uint32_t*map,const pc_midday::ResultFields&v){
 r.mStates=states;r.mScreenToTableList=map;r.mActiveCount=uint16_t(v.descriptors.size());std::copy(v.days.begin(),v.days.end(),r.mDaysSeen);
}
namespace pc_midday {
bool captureResult(const ResultFlags&s,const PlayerCoreReadFence&f,Bytes&out,std::string&e){
 if(!f.sceneInitialized||!f.agreedReadOnlyFence||f.tickBefore!=f.tickAfter){e="Result capture requires initialized stopped scene";return false;}
 ResultFields v;if(!PcMiddayResultAccess::read(s,v,e))return false;return encodeResult(v,out,e);
}
struct IsolatedResult::Impl{
 std::array<uint8_t,38>states{};std::array<uint32_t,148>map{};
 std::unique_ptr<ResultFlags>resource;
};
IsolatedResult::IsolatedResult()=default;IsolatedResult::~IsolatedResult()=default;
ResultFlags*IsolatedResult::resource()const{return impl_?impl_->resource.get():nullptr;}
bool IsolatedResult::prepare(const Bytes&b,const RestoreGate&g,ConstructorFence&f,std::string&e){
 if(impl_||!g.freshProcess||!g.paused||!g.zeroInput||!g.birthEffectsSuppressed||!g.rewardsSuppressed||!g.rngDrawsSuppressed||!g.audioVoicesSuppressed||!f.held()||!pc_sim_rng_constructor_suppression(true,e)){if(e.empty())e="Result staging requires full physical constructor fence";return false;}
 ResultFields v;std::vector<ResultDescriptor>compiled;if(!decodeResult(b,v,e)||!PcMiddayResultAccess::compiled(compiled,e))return false;
 if(compiled.size()!=v.descriptors.size()){e="Result saved compiled count differs";return false;}
 for(size_t i=0;i<compiled.size();++i){const auto&a=compiled[i];const auto&d=v.descriptors[i];if(a.screen!=d.screen||a.priority!=d.priority||a.store!=d.store||a.autoSet!=d.autoSet){e="Result saved compiled descriptor differs";return false;}}
 try{auto stage=std::make_unique<Impl>();stage->states=v.states;
  for(size_t i=0;i<v.descriptors.size();++i)stage->map[v.descriptors[i].screen]=uint32_t(i);
  stage->resource=std::make_unique<ResultFlags>(ResultStageTag{});PcMiddayResultAccess::bind(*stage->resource,stage->states.data(),stage->map.data(),v);
  impl_=std::move(stage);e.clear();return true;
 }catch(const std::exception&x){e=std::string("Result resource staging failed: ")+x.what();return false;}
}
}
