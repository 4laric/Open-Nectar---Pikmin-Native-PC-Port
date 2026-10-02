#include "pc_midday_render_descriptors.h"
#include "Shape.h"
#include "Material.h"
#include <algorithm>
namespace pc_midday {
namespace {
template<class T>bool keys(const T& a){return a.mSize<=65536&&(!a.mSize||a.mKeyframes);}
template<class T>void copyKeys(T& to,const T& from){to.mSize=from.mSize;to.mKeyframes=from.mSize?from.mKeyframes:nullptr;}
bool modelMaterial(BaseShape& model,u32 slot,Material*& out,std::string& e){
 if(model.mMaterialCount<0||model.mMaterialCount>256||!model.mMaterialList||slot>=u32(model.mMaterialCount)){e="installed model material slot invalid";return false;}
 out=&model.mMaterialList[slot];if(out->mIndex!=slot){e="installed material index disagrees with source slot";return false;}return true;
}
bool pvw(BaseShape& model,Material& m,std::string& e){
 if(!(m.mFlags&MATFLAG_PVW)||model.mTevInfoCount<0||model.mTevInfoCount>256||!model.mTevInfoList||m.mTevInfoIndex>=u32(model.mTevInfoCount)||m.mTevInfo!=&model.mTevInfoList[m.mTevInfoIndex]){e="installed PVW material TEV provenance invalid";return false;}
 return true;
}
bool materialBacking(BaseShape& model,Material& m,std::string& e){
 if(!(m.mFlags&MATFLAG_PVW))return true;
 if(!pvw(model,m,e))return false;
 auto& t=m.mTextureInfo;
 if(t.mTextureDataCount>256||t.mTexGenDataCount>8||t.mTevStageCount>16||(t.mTextureDataCount&&!t.mTextureData)||(t.mTexGenDataCount&&!t.mTexGenData)||!keys(m.mColourInfo.mColourInfo.mAnimInfo)||!keys(m.mColourInfo.mAlphaInfo.mAnimInfo)){e="installed material animation backing invalid";return false;}
 return true;
}
bool tevBacking(const PVWTevInfo& tev,std::string& e){if(tev.mTevStageCount>16||(tev.mTevStageCount&&!tev.mTevStages)){e="installed TEV stage backing invalid";return false;}for(const auto& c:tev.mTevColRegs)if(!keys(c.mColorAnimData.mInfo)||!keys(c.mAlphaAnimData.mInfo)){e="installed TEV key backing invalid";return false;}return true;}
bool textureBacking(const Material& m,std::string& e){if(!m.mTextureInfo.mTextureDataCount){e="empty texture allocation has no descriptor factory";return false;}for(u32 i=0;i<m.mTextureInfo.mTextureDataCount;++i){const auto& t=m.mTextureInfo.mTextureData[i];if(!keys(t.mScaleInfo.mInfo)||!keys(t.mRotationInfo.mInfo)||!keys(t.mTranslationInfo.mInfo)){e="installed texture animation key backing invalid";return false;}}return true;}
}
bool RenderDescriptorIndex::declare(u64 factory,RenderKind kind,BaseShape& model,const std::vector<u32>& slots,std::string& e){
 if(!factory||entries_.count(factory)||entries_.size()>=4096||slots.empty()||slots.size()>256||(kind!=RenderKind::Materials&&kind!=RenderKind::Tev&&kind!=RenderKind::Textures)||((kind!=RenderKind::Materials)&&slots.size()!=1)){e="render descriptor factory declaration invalid";return false;}
 std::set<u32> unique;for(u32 slot:slots){Material* m=nullptr;if(!unique.insert(slot).second||!modelMaterial(model,slot,m,e)||!materialBacking(model,*m,e))return false;
  if(kind!=RenderKind::Materials&&!pvw(model,*m,e))return false;
  if(kind==RenderKind::Tev&&!tevBacking(*m->mTevInfo,e))return false;
  if(kind==RenderKind::Textures&&!textureBacking(*m,e))return false;
 }
 entries_.emplace(factory,Entry{kind,&model,slots});e.clear();return true;
}
bool RenderDescriptorIndex::validate(const RenderGraph& graph,std::string& e)const{
 if(!graph.generation||graph.nodes.size()>4096){e="render descriptor graph epoch/bound invalid";return false;}
 std::set<u64> factories;std::map<u64,const RenderNode*> nodes;size_t materialSlots=0;
 for(const auto& node:graph.nodes){auto row=entries_.find(node.factory);if(row==entries_.end()||row->second.kind!=node.kind){e="render allocation factory absent/wrong kind";return false;}const auto& entry=row->second;factories.insert(node.factory);
  if(!node.id||!nodes.emplace(node.id,&node).second){e="render descriptor node ID invalid/duplicated";return false;}
  u32 expected=entry.materialSlots.size();Material* source=nullptr;
  for(u32 slot:entry.materialSlots)if(!modelMaterial(*entry.model,slot,source,e)||!materialBacking(*entry.model,*source,e))return false;
  if(node.kind==RenderKind::Tev){if(!pvw(*entry.model,*source,e)||!tevBacking(*source->mTevInfo,e))return false;expected=1;}
  if(node.kind==RenderKind::Textures){if(!pvw(*entry.model,*source,e)||!textureBacking(*source,e))return false;expected=source->mTextureInfo.mTextureDataCount;}
  if(node.count!=expected){e="render allocation disagrees with installed factory geometry";return false;}
  if(node.kind==RenderKind::Materials)materialSlots+=node.count;
 }
 if(factories.size()!=entries_.size()){e="render descriptor factory census incomplete";return false;}
 if(graph.links.size()!=materialSlots){e="render descriptor material link census incomplete";return false;}
 std::set<std::pair<u64,u32>> slots;
 for(const auto& link:graph.links){auto m=nodes.find(link.materials);if(m==nodes.end()||m->second->kind!=RenderKind::Materials||link.slot>=m->second->count||!slots.emplace(link.materials,link.slot).second){e="render descriptor material link invalid/duplicated";return false;}
  const auto& row=entries_.at(m->second->factory);const auto& source=row.model->mMaterialList[row.materialSlots[link.slot]];
  if(link.pvw!=bool(source.mFlags&MATFLAG_PVW)){e="render descriptor PVW factory discriminator mismatch";return false;}
  if(!link.pvw){if(link.tev||link.textures||link.textureCount){e="non-PVW descriptor has mutable PVW aliases";return false;}continue;}
  auto tev=nodes.find(link.tev);if(tev==nodes.end()||tev->second->kind!=RenderKind::Tev){e="render descriptor TEV target missing";return false;}
  const auto& tr=entries_.at(tev->second->factory);if(tr.model->mMaterialList[tr.materialSlots[0]].mTevInfo!=source.mTevInfo){e="render TEV target has foreign immutable backing";return false;}
  if(link.textureCount!=source.mTextureInfo.mTextureDataCount){e="render texture alias differs from installed material geometry";return false;}
  if(!link.textureCount){if(link.textures){e="empty render texture alias has identity";return false;}continue;}
  auto tex=nodes.find(link.textures);if(tex==nodes.end()||tex->second->kind!=RenderKind::Textures){e="render descriptor texture target missing";return false;}
  const auto& xr=entries_.at(tex->second->factory);if(xr.model->mMaterialList[xr.materialSlots[0]].mTextureInfo.mTextureData!=source.mTextureInfo.mTextureData){e="render texture target has foreign immutable backing";return false;}
 }
 e.clear();return true;
}
bool RenderDescriptorIndex::initializeBacking(const RenderGraph& graph,IsolatedRenderAllocations& owner,ConstructorFence& fence,std::string& e)const{
 if(!pc_sim_rng_constructor_suppression(true,e))return false;
 if(!owner.heldBy(fence)||!owner.matchesLayout(graph)||!validate(graph,e)){if(e.empty())e="render descriptor destination/fence/layout invalid";return false;}
 // All source backing/geometry and owned allocation identities checked above.
 // No allocation or callback occurs after this point; never copy object bytes.
 for(const auto& node:graph.nodes){const auto& entry=entries_.at(node.factory);
  if(node.kind==RenderKind::Materials){auto* dst=static_cast<Material*>(owner.allocation(node.id));for(u32 i=0;i<node.count;++i){auto& to=dst[i];const auto& from=entry.model->mMaterialList[entry.materialSlots[i]];
   to.mDisplayListPtr=from.mDisplayListPtr;to.mDisplayListSize=from.mDisplayListPtr?from.mDisplayListSize:0;
   to.mTevInfo=nullptr;to.mTextureInfo.mTextureData=nullptr;to.mTextureInfo.mTextureDataCount=0;
   if(!(from.mFlags&MATFLAG_PVW))continue;
   to.mColourInfo.mTotalFrameCount=from.mColourInfo.mTotalFrameCount;copyKeys(to.mColourInfo.mColourInfo.mAnimInfo,from.mColourInfo.mColourInfo.mAnimInfo);copyKeys(to.mColourInfo.mAlphaInfo.mAnimInfo,from.mColourInfo.mAlphaInfo.mAnimInfo);
   to.mPeInfo.mControlFlags=from.mPeInfo.mControlFlags;to.mPeInfo.mAlphaCompareFlags=from.mPeInfo.mAlphaCompareFlags;to.mPeInfo.mDepthTestFlags=from.mPeInfo.mDepthTestFlags;to.mPeInfo.mBlendModeFlags=from.mPeInfo.mBlendModeFlags;
   to.mTevInfoIndex=from.mTevInfoIndex;to.mTextureInfo.mTexGenDataCount=from.mTextureInfo.mTexGenDataCount;to.mTextureInfo.mTexGenData=from.mTextureInfo.mTexGenDataCount?from.mTextureInfo.mTexGenData:nullptr;to.mTextureInfo.mUseScale=from.mTextureInfo.mUseScale;to.mTextureInfo.mTevStageCount=from.mTextureInfo.mTevStageCount;
   to.mLightingInfo._UNUSED08=from.mLightingInfo._UNUSED08;
   auto link=std::find_if(graph.links.begin(),graph.links.end(),[&](const RenderLink& x){return x.materials==node.id&&x.slot==i;});
   to.mTevInfo=static_cast<PVWTevInfo*>(owner.allocation(link->tev));to.mTextureInfo.mTextureDataCount=link->textureCount;to.mTextureInfo.mTextureData=link->textureCount?static_cast<PVWTextureData*>(owner.allocation(link->textures)):nullptr;
  }}
  else if(node.kind==RenderKind::Tev){auto& to=*static_cast<PVWTevInfo*>(owner.allocation(node.id));const auto& from=*entry.model->mMaterialList[entry.materialSlots[0]].mTevInfo;to.mTevStageCount=from.mTevStageCount;to.mTevStages=from.mTevStageCount?from.mTevStages:nullptr;for(int i=0;i<3;++i){copyKeys(to.mTevColRegs[i].mColorAnimData.mInfo,from.mTevColRegs[i].mColorAnimData.mInfo);copyKeys(to.mTevColRegs[i].mAlphaAnimData.mInfo,from.mTevColRegs[i].mAlphaAnimData.mInfo);}}
  else{auto* to=static_cast<PVWTextureData*>(owner.allocation(node.id));const auto* from=entry.model->mMaterialList[entry.materialSlots[0]].mTextureInfo.mTextureData;for(u32 i=0;i<node.count;++i){copyKeys(to[i].mScaleInfo.mInfo,from[i].mScaleInfo.mInfo);copyKeys(to[i].mRotationInfo.mInfo,from[i].mRotationInfo.mInfo);copyKeys(to[i].mTranslationInfo.mInfo,from[i].mTranslationInfo.mInfo);to[i]._UNUSED0C=from[i]._UNUSED0C;to[i]._UNUSED0E=from[i]._UNUSED0E;to[i]._UNUSED10=from[i]._UNUSED10;to[i]._UNUSED11=from[i]._UNUSED11;to[i]._UNUSED12=from[i]._UNUSED12;to[i]._UNUSED13=from[i]._UNUSED13;}}
 }
 e.clear();return true;
}
}
