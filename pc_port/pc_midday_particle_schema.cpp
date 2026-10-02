#include "pc_midday_particle.h"
#include <tuple>
namespace pc_midday {
bool validate_particle_allocations(const std::vector<ParticleAllocationDescriptor>&pools,u64 budget,std::string&e){
 if(budget>65536){e="particle aggregate budget bound";return false;}u64 total=0;for(const auto&p:pools){if(p.models>16384||p.children>16384||u64(p.models)+p.children>budget-total){e="particle aggregate allocation overflow";return false;}total+=u64(p.models)+p.children;}return true;
}
bool validate_particle_graph(const std::vector<ParticleGraphNode>&nodes,std::string&e){
 if(nodes.size()>65536){e="particle graph node bound";return false;}
 using Key=std::tuple<u64,u64,u32>;auto key=[](const LogicalRef&r){return Key(r.owner,r.resource,r.slot);};std::map<Key,const ParticleGraphNode*> all;
 for(const auto&n:nodes){if((!n.identity.owner&&!n.identity.resource&&!n.identity.slot)||!all.emplace(key(n.identity),&n).second){e="duplicate/null particle graph node";return false;}}
 for(const auto&n:nodes){auto prev=all.find(key(n.previous)),next=all.find(key(n.next));if(prev==all.end()||next==all.end()||key(prev->second->next)!=key(n.identity)||key(next->second->previous)!=key(n.identity)){e="particle list closure/reciprocity mismatch";return false;}}
 return true;
}
bool particle_schema(const ActorFields&f,ParticleRecordKind kind,const std::string&prefix,std::vector<FieldSchema>&s,std::string&e){
 const std::string p=prefix.empty()?"":prefix+".";
 auto val=[&](const std::string&k,ScalarKind t){s.push_back(FieldSchema::value((p+k).c_str(),t));};
 auto vec=[&](const std::string&k){for(auto x:{"x","y","z"})val(k+"."+x,ScalarKind::F32);};
 auto color=[&](const std::string&k){for(auto x:{"r","g","b","a"})val(k+"."+x,ScalarKind::U8);};
 auto ref=[&](const std::string&k,RefKind r,bool n,const char*t,ReferenceOwnership own=ReferenceOwnership::AnyLive){s.push_back(FieldSchema::ref((p+k).c_str(),r,n,t,own));};
 if(kind==ParticleRecordKind::Generator){
 int draw=0,rotation=0;if(!actor_i32(f,(p+"drawCallback").c_str(),draw,e)||!actor_i32(f,(p+"rotationCallback").c_str(),rotation,e)||draw<0||draw>2||rotation<0||rotation>7){e="particle callback selector";return false;}

 for(auto group:{"links.","mainList.","childList."})for(auto k:{"identity","prev","next"})ref(std::string(group)+k,RefKind::ParticleNode,false,"zen::zenList");
 vec("mEmitPos");
 vec("mEmitVelocity");
 val("mLengthScale",ScalarKind::F32);
 val("mPivotOffsetY",ScalarKind::F32);
 val("mScaleRate1",ScalarKind::F32);
 val("mScaleRate2",ScalarKind::F32);
 val("mAlphaRate1",ScalarKind::F32);
 val("mAlphaRate2",ScalarKind::F32);
 val("mControlFlags",ScalarKind::U32);
 val("mParticleFlags",ScalarKind::U32);
 val("mPartialParticleCount",ScalarKind::F32);
 val("mPassTimer",ScalarKind::F32);
 val("mCurrentFrame",ScalarKind::S16);
 val("mCurrentPass",ScalarKind::U8);
 vec("mEmitPosOffset");
 vec("mEmitDir");
 vec("mEmissionBoxSize");
 val("mEmissionRate",ScalarKind::F32);
 val("mEmissionRateJitter",ScalarKind::F32);
 val("mEmissionSpread",ScalarKind::F32);
 val("mEmissionRadiusScale",ScalarKind::F32);
 val("mEmissionRadius",ScalarKind::F32);
 val("mInitVel",ScalarKind::F32);
 val("mInitialVelocityJitter",ScalarKind::F32);
 val("mDrag",ScalarKind::F32);
 val("mDragJitter",ScalarKind::F32);
 val("mMaxVel",ScalarKind::F32);
 val("mScaleThreshold1",ScalarKind::F32);
 val("mScaleThreshold2",ScalarKind::F32);
 val("mMinScaleFactor1",ScalarKind::F32);
 val("mMinScaleFactor2",ScalarKind::F32);
 val("mScaleSize",ScalarKind::F32);
 val("mSizeJitter",ScalarKind::F32);
 val("mAlphaThreshold1",ScalarKind::F32);
 val("mAlphaThreshold2",ScalarKind::F32);
 val("mAlphaJitter",ScalarKind::F32);
 val("mRotSpeedMin",ScalarKind::S16);
 val("mRotSpeedJitter",ScalarKind::S16);
 val("mRotAngle",ScalarKind::S16);
 val("mLifetimeJitter",ScalarKind::F32);
 val("mBaseLifetime",ScalarKind::S16);
 val("mChildScaleFactor",ScalarKind::F32);
 val("mChildAlphaMultiplier",ScalarKind::F32);
 val("mChildPosJitter",ScalarKind::F32);
 color("mChildColor");
 color("mTint");
 val("mHasTint",ScalarKind::Bool);
 val("_124",ScalarKind::U8);
 val("mChildSpawnInterval",ScalarKind::U8);
 vec("mGravFieldAccel");
 vec("mAirFieldVelocity");
 vec("mVortexCenter");
 val("mVortexRotationSpeed",ScalarKind::F32);
 val("mVortexStrength",ScalarKind::F32);
 val("mVortexFalloffFactor",ScalarKind::F32);
 val("mVortexFalloffDivisor",ScalarKind::F32);
 vec("mDampedNewtonFieldDir");
 val("mDampedNewtonFieldStrength",ScalarKind::F32);
 vec("mNewtonFieldDir");
 val("mNewtonFieldStrength",ScalarKind::F32);
 vec("mSolidFieldForceMultiplier");
 val("mSolidFieldGridScale",ScalarKind::U8);
 val("mSolidFieldSampleOffset",ScalarKind::U8);
 val("mSolidFieldType",ScalarKind::U8);
 val("mJitterStrength",ScalarKind::F32);
 vec("mLineFieldAxis");
 val("mLineFieldAxialForce",ScalarKind::F32);
 val("mLineFieldRadialForce",ScalarKind::F32);
 val("mFreePtclMotionTime",ScalarKind::U8);
 val("mEmissionRateKeyCount",ScalarKind::U8);
 val("mEmissionRadiusKeyCount",ScalarKind::U8);
 val("mInitialVelocityKeyCount",ScalarKind::U8);
 val("mMaxFrame",ScalarKind::S16);
 val("mMaxPasses",ScalarKind::U8);
 val("mBlendFactor",ScalarKind::U8);
 val("mZMode",ScalarKind::U8);
 vec("mOrientedNormal");
 ref("mEmitPosPtr",RefKind::Vector3,true,"Vector3f",ReferenceOwnership::AnyLive);
 ref("mTexture",RefKind::Texture,true,"Texture",ReferenceOwnership::Content);
 ref("mChildTexture",RefKind::Texture,true,"Texture",ReferenceOwnership::Content);
 ref("mSolidTexFieldData",RefKind::ParticleData,true,"u16[]",ReferenceOwnership::Content);
 ref("mEmissionRateKeyframes",RefKind::ParticleData,true,"f32[]",ReferenceOwnership::Content);
 ref("mEmissionRateValues",RefKind::ParticleData,true,"f32[]",ReferenceOwnership::Content);
 ref("mEmissionRadiusKeyframes",RefKind::ParticleData,true,"f32[]",ReferenceOwnership::Content);
 ref("mEmissionRadiusValues",RefKind::ParticleData,true,"f32[]",ReferenceOwnership::Content);
 ref("mInitVelIntpThresholds",RefKind::ParticleData,true,"f32[]",ReferenceOwnership::Content);
 ref("mInitVelIntpValues",RefKind::ParticleData,true,"f32[]",ReferenceOwnership::Content);
 ref("mMdlMgr",RefKind::ParticleManager,true,"zen::particleMdlManager",ReferenceOwnership::AnyLive);
 ref("mCallBack1",RefKind::ParticleCallback,true,"zen::CallBack1<zen::particleGenerator*>",ReferenceOwnership::AnyLive);
 ref("mCallBack2",RefKind::ParticleCallback,true,"zen::CallBack2<zen::particleGenerator*,zen::particleMdl*>",ReferenceOwnership::AnyLive);
 for(auto k:{"mOrientationSource","mIsDoubleSided","mFlipNormal"})val(std::string("drawConfig.")+k,ScalarKind::Bool);
 val("drawCallback",ScalarKind::S32);val("rotationCallback",ScalarKind::S32);
 for(auto k:{"blend","duration","flags","maxFrame"})val(std::string("animData.")+k,ScalarKind::U8);
 ref("animData.thresholds",RefKind::ParticleData,true,"f32[]",ReferenceOwnership::Content);ref("animData.primColors",RefKind::ParticleData,true,"Colour[]",ReferenceOwnership::Content);ref("animData.envColors",RefKind::ParticleData,true,"Colour[]",ReferenceOwnership::Content);
 // Counts describe content-bound typed array extents. The resource resolver
 // must additionally verify the referenced content span has this many items.
 for(auto group:{0,1,2,3}){const char*counts[]={"mEmissionRateKeyCount","mEmissionRadiusKeyCount","mInitialVelocityKeyCount","animData.maxFrame"};const char*arrays[][3]={{"mEmissionRateKeyframes","mEmissionRateValues",nullptr},{"mEmissionRadiusKeyframes","mEmissionRadiusValues",nullptr},{"mInitVelIntpThresholds","mInitVelIntpValues",nullptr},{"animData.thresholds","animData.primColors","animData.envColors"}};auto count=f.find(p+counts[group]);if(count==f.end()){e="particle array count missing";return false;}if(count->second.bits){for(auto name:arrays[group]){if(!name)continue;for(auto&field:s)if(field.key==p+name)field.nullable=false;}}}
 return true;
 }
 if(kind==ParticleRecordKind::Manager){u32 models=0,children=0;if(!actor_u32(f,(p+"models").c_str(),models,e)||!actor_u32(f,(p+"children").c_str(),children,e)||models>16384||children>16384){e="particle pool descriptor bounds";return false;}val("models",ScalarKind::U32);val("children",ScalarKind::U32);
 for(auto group:{"sleeping.","childSleeping."})for(auto k:{"identity","prev","next"})ref(std::string(group)+k,RefKind::ParticleNode,false,"zen::zenList");
 for(u32 i=0;i<models;++i)ref("model."+std::to_string(i),RefKind::ParticleNode,false,"zen::particleMdl",ReferenceOwnership::ResourceSubobject);
 for(u32 i=0;i<children;++i)ref("child."+std::to_string(i),RefKind::ParticleNode,false,"zen::particleChildMdl",ReferenceOwnership::ResourceSubobject);return true;}
 if(kind==ParticleRecordKind::Permanent){vec("position");ref("generator",RefKind::ParticleGenerator,true,"zen::particleGenerator");return true;}
 if(kind==ParticleRecordKind::List){for(auto k:{"identity","prev","next"})ref(k,RefKind::ParticleNode,false,"zen::zenList");return true;}
 if(kind!=ParticleRecordKind::Model&&kind!=ParticleRecordKind::Child){e="particle record subtype not implemented";return false;}
 for(auto k:{"identity","prev","next"})ref(std::string("links.")+k,RefKind::ParticleNode,false,"zen::zenList");
 vec("localPosition");vec("globalPosition");val("size",ScalarKind::F32);color("primaryColor");
 if(kind==ParticleRecordKind::Child){val("timer",ScalarKind::F32);for(auto k:{"counter0","counter1","counter2"})val(k,ScalarKind::U8);return true;}
 val("lifetime",ScalarKind::S16);val("age",ScalarKind::S16);val("ageTimer",ScalarKind::F32);vec("velocity");vec("acceleration");val("scale",ScalarKind::F32);val("alpha",ScalarKind::F32);val("rotAngle",ScalarKind::U16);val("rotSpeed",ScalarKind::S16);vec("normal");color("envColor");val("colourAnim.progress",ScalarKind::F32);val("colourAnim.frame",ScalarKind::U8);val("colourAnim.duration",ScalarKind::S16);
 ref("colourAnim.data",RefKind::ParticleData,true,"zen::bBoardColourAnimData",ReferenceOwnership::AnyLive);ref("texture",RefKind::Texture,true,"Texture",ReferenceOwnership::Content);ref("callback",RefKind::ParticleCallback,true,"zen::CallBack1<zen::particleMdl*>");return true;
}
}
