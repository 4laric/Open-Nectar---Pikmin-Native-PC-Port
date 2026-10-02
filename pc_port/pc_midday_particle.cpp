#if defined(PIKI_PC_PORT)
#include "pc_midday_particle.h"
#include "zen/particle.h"
#include "UtEffect.h"
#include <memory>
using namespace pc_midday;
struct PcMiddayParticleAccess {
 static bool allocate(zen::particleMdlManager&o,u32 models,u32 children){
 if(models>16384||children>16384||o.mPtclList||o.mChildPtclList)return false;
 std::unique_ptr<zen::particleMdl[]> m(new zen::particleMdl[models]);std::unique_ptr<zen::particleChildMdl[]> c(new zen::particleChildMdl[children]);o.mPtclList=m.release();o.mChildPtclList=c.release();o.mMiddayParticleCapacity=models;o.mMiddayChildCapacity=children;return true;
 }
 static bool fields(zen::particleMdlManager&o,ActorArchive&a){
 u32 models=a.mode()==Mode::Capture?o.mMiddayParticleCapacity:0,children=a.mode()==Mode::Capture?o.mMiddayChildCapacity:0;
 if(!a.scalar("models",ScalarKind::U32,&models)||!a.scalar("children",ScalarKind::U32,&children)||models>16384||children>16384||models!=o.mMiddayParticleCapacity||children!=o.mMiddayChildCapacity||(models&&!o.mPtclList)||(children&&!o.mChildPtclList))return a.fail("particle pool factory mismatch");
 PrefixArchive sleeping(a,"sleeping"),childSleeping(a,"childSleeping");if(!particle_list_fields(*o.mSleepPtclList.getOrigin(),sleeping)||!particle_list_fields(*o.mSleepPtclChildList.getOrigin(),childSleeping))return false;
 for(u32 i=0;i<models;++i){zen::particleMdl*p=&o.mPtclList[i];if(!a.ref(("model."+std::to_string(i)).c_str(),RefKind::ParticleNode,p)||p!=&o.mPtclList[i])return a.fail("particle model allocation identity");}
 for(u32 i=0;i<children;++i){zen::particleChildMdl*p=&o.mChildPtclList[i];if(!a.ref(("child."+std::to_string(i)).c_str(),RefKind::ParticleNode,p)||p!=&o.mChildPtclList[i])return a.fail("particle child allocation identity");}return true;
 }

 static bool fields(zen::particleGenerator&o,ActorArchive&a){
 PrefixArchive links(a,"links"),mainList(a,"mainList"),childList(a,"childList");
 if(!particle_list_fields(o,links)||!particle_list_fields(*o.mPtclMdlListManager.getOrigin(),mainList)||!particle_list_fields(*o.mPtclChildListManager.getOrigin(),childList))return false;
 if(!a.field("mEmitPos",o.mEmitPos))return false;
 if(!a.field("mEmitVelocity",o.mEmitVelocity))return false;
 if(!a.field("mLengthScale",o.mLengthScale))return false;
 if(!a.field("mPivotOffsetY",o.mPivotOffsetY))return false;
 if(!a.field("mScaleRate1",o.mScaleRate1))return false;
 if(!a.field("mScaleRate2",o.mScaleRate2))return false;
 if(!a.field("mAlphaRate1",o.mAlphaRate1))return false;
 if(!a.field("mAlphaRate2",o.mAlphaRate2))return false;
 if(!a.field("mControlFlags",o.mControlFlags))return false;
 if(!a.field("mParticleFlags",o.mParticleFlags))return false;
 if(!a.field("mPartialParticleCount",o.mPartialParticleCount))return false;
 if(!a.field("mPassTimer",o.mPassTimer))return false;
 if(!a.field("mCurrentFrame",o.mCurrentFrame))return false;
 if(!a.field("mCurrentPass",o.mCurrentPass))return false;
 if(!a.field("mEmitPosOffset",o.mEmitPosOffset))return false;
 if(!a.field("mEmitDir",o.mEmitDir))return false;
 if(!a.field("mEmissionBoxSize",o.mEmissionBoxSize))return false;
 if(!a.field("mEmissionRate",o.mEmissionRate))return false;
 if(!a.field("mEmissionRateJitter",o.mEmissionRateJitter))return false;
 if(!a.field("mEmissionSpread",o.mEmissionSpread))return false;
 if(!a.field("mEmissionRadiusScale",o.mEmissionRadiusScale))return false;
 if(!a.field("mEmissionRadius",o.mEmissionRadius))return false;
 if(!a.field("mInitVel",o.mInitVel))return false;
 if(!a.field("mInitialVelocityJitter",o.mInitialVelocityJitter))return false;
 if(!a.field("mDrag",o.mDrag))return false;
 if(!a.field("mDragJitter",o.mDragJitter))return false;
 if(!a.field("mMaxVel",o.mMaxVel))return false;
 if(!a.field("mScaleThreshold1",o.mScaleThreshold1))return false;
 if(!a.field("mScaleThreshold2",o.mScaleThreshold2))return false;
 if(!a.field("mMinScaleFactor1",o.mMinScaleFactor1))return false;
 if(!a.field("mMinScaleFactor2",o.mMinScaleFactor2))return false;
 if(!a.field("mScaleSize",o.mScaleSize))return false;
 if(!a.field("mSizeJitter",o.mSizeJitter))return false;
 if(!a.field("mAlphaThreshold1",o.mAlphaThreshold1))return false;
 if(!a.field("mAlphaThreshold2",o.mAlphaThreshold2))return false;
 if(!a.field("mAlphaJitter",o.mAlphaJitter))return false;
 if(!a.field("mRotSpeedMin",o.mRotSpeedMin))return false;
 if(!a.field("mRotSpeedJitter",o.mRotSpeedJitter))return false;
 if(!a.field("mRotAngle",o.mRotAngle))return false;
 if(!a.field("mLifetimeJitter",o.mLifetimeJitter))return false;
 if(!a.field("mBaseLifetime",o.mBaseLifetime))return false;
 if(!a.field("mChildScaleFactor",o.mChildScaleFactor))return false;
 if(!a.field("mChildAlphaMultiplier",o.mChildAlphaMultiplier))return false;
 if(!a.field("mChildPosJitter",o.mChildPosJitter))return false;
 if(!a.field("mChildColor.r",o.mChildColor.r))return false;
 if(!a.field("mChildColor.g",o.mChildColor.g))return false;
 if(!a.field("mChildColor.b",o.mChildColor.b))return false;
 if(!a.field("mChildColor.a",o.mChildColor.a))return false;
 if(!a.field("mTint.r",o.mTint.r))return false;
 if(!a.field("mTint.g",o.mTint.g))return false;
 if(!a.field("mTint.b",o.mTint.b))return false;
 if(!a.field("mTint.a",o.mTint.a))return false;
 if(!a.field("mHasTint",o.mHasTint))return false;
 if(!a.field("_124",o._124))return false;
 if(!a.field("mChildSpawnInterval",o.mChildSpawnInterval))return false;
 if(!a.field("mGravFieldAccel",o.mGravFieldAccel))return false;
 if(!a.field("mAirFieldVelocity",o.mAirFieldVelocity))return false;
 if(!a.field("mVortexCenter",o.mVortexCenter))return false;
 if(!a.field("mVortexRotationSpeed",o.mVortexRotationSpeed))return false;
 if(!a.field("mVortexStrength",o.mVortexStrength))return false;
 if(!a.field("mVortexFalloffFactor",o.mVortexFalloffFactor))return false;
 if(!a.field("mVortexFalloffDivisor",o.mVortexFalloffDivisor))return false;
 if(!a.field("mDampedNewtonFieldDir",o.mDampedNewtonFieldDir))return false;
 if(!a.field("mDampedNewtonFieldStrength",o.mDampedNewtonFieldStrength))return false;
 if(!a.field("mNewtonFieldDir",o.mNewtonFieldDir))return false;
 if(!a.field("mNewtonFieldStrength",o.mNewtonFieldStrength))return false;
 if(!a.field("mSolidFieldForceMultiplier",o.mSolidFieldForceMultiplier))return false;
 if(!a.field("mSolidFieldGridScale",o.mSolidFieldGridScale))return false;
 if(!a.field("mSolidFieldSampleOffset",o.mSolidFieldSampleOffset))return false;
 if(!a.field("mSolidFieldType",o.mSolidFieldType))return false;
 if(!a.field("mJitterStrength",o.mJitterStrength))return false;
 if(!a.field("mLineFieldAxis",o.mLineFieldAxis))return false;
 if(!a.field("mLineFieldAxialForce",o.mLineFieldAxialForce))return false;
 if(!a.field("mLineFieldRadialForce",o.mLineFieldRadialForce))return false;
 if(!a.field("mFreePtclMotionTime",o.mFreePtclMotionTime))return false;
 if(!a.field("mEmissionRateKeyCount",o.mEmissionRateKeyCount))return false;
 if(!a.field("mEmissionRadiusKeyCount",o.mEmissionRadiusKeyCount))return false;
 if(!a.field("mInitialVelocityKeyCount",o.mInitialVelocityKeyCount))return false;
 if(!a.field("mMaxFrame",o.mMaxFrame))return false;
 if(!a.field("mMaxPasses",o.mMaxPasses))return false;
 if(!a.field("mBlendFactor",o.mBlendFactor))return false;
 if(!a.field("mZMode",o.mZMode))return false;
 if(!a.field("mOrientedNormal",o.mOrientedNormal))return false;
 if(!a.ref("mEmitPosPtr",RefKind::Vector3,o.mEmitPosPtr))return false;
 if(!a.ref("mTexture",RefKind::Texture,o.mTexture))return false;
 if(!a.ref("mChildTexture",RefKind::Texture,o.mChildTexture))return false;
 if(!a.ref("mSolidTexFieldData",RefKind::ParticleData,o.mSolidTexFieldData))return false;
 if(!a.ref("mEmissionRateKeyframes",RefKind::ParticleData,o.mEmissionRateKeyframes))return false;
 if(!a.ref("mEmissionRateValues",RefKind::ParticleData,o.mEmissionRateValues))return false;
 if(!a.ref("mEmissionRadiusKeyframes",RefKind::ParticleData,o.mEmissionRadiusKeyframes))return false;
 if(!a.ref("mEmissionRadiusValues",RefKind::ParticleData,o.mEmissionRadiusValues))return false;
 if(!a.ref("mInitVelIntpThresholds",RefKind::ParticleData,o.mInitVelIntpThresholds))return false;
 if(!a.ref("mInitVelIntpValues",RefKind::ParticleData,o.mInitVelIntpValues))return false;
 if(!a.ref("mMdlMgr",RefKind::ParticleManager,o.mMdlMgr))return false;
 if(!a.ref("mCallBack1",RefKind::ParticleCallback,o.mCallBack1))return false;
 if(!a.ref("mCallBack2",RefKind::ParticleCallback,o.mCallBack2))return false;
 {bool bit=a.mode()==Mode::Capture?bool(o.mOrientedDrawConfig.mOrientationSource):false;if(!a.field("drawConfig.mOrientationSource",bit))return false;if(a.mode()==Mode::Apply)o.mOrientedDrawConfig.mOrientationSource=bit;}
 {bool bit=a.mode()==Mode::Capture?bool(o.mOrientedDrawConfig.mIsDoubleSided):false;if(!a.field("drawConfig.mIsDoubleSided",bit))return false;if(a.mode()==Mode::Apply)o.mOrientedDrawConfig.mIsDoubleSided=bit;}
 {bool bit=a.mode()==Mode::Capture?bool(o.mOrientedDrawConfig.mFlipNormal):false;if(!a.field("drawConfig.mFlipNormal",bit))return false;if(a.mode()==Mode::Apply)o.mOrientedDrawConfig.mFlipNormal=bit;}
 const zen::PtclDrawCallBack draws[]={nullptr,&zen::particleGenerator::drawPtclBillboard,&zen::particleGenerator::drawPtclOriented};
 const zen::RotAxisCallBack rotations[]={nullptr,&zen::particleGenerator::RotAxisX,&zen::particleGenerator::RotAxisY,&zen::particleGenerator::RotAxisZ,&zen::particleGenerator::RotAxisXY,&zen::particleGenerator::RotAxisXZ,&zen::particleGenerator::RotAxisYZ,&zen::particleGenerator::RotAxisXYZ};
 int draw=0,rot=0;if(a.mode()==Mode::Capture){draw=-1;rot=-1;for(int i=0;i<3;++i)if(o.mDrawCallBack==draws[i])draw=i;for(int i=0;i<8;++i)if(o.mRotAxisCallBack==rotations[i])rot=i;}
 if(!a.scalar("drawCallback",ScalarKind::S32,&draw)||!a.scalar("rotationCallback",ScalarKind::S32,&rot)||draw<0||draw>2||rot<0||rot>7)return a.fail("unknown particle member callback");
 if(a.mode()==Mode::Apply){o.mDrawCallBack=draws[draw];o.mRotAxisCallBack=rotations[rot];}
 PrefixArchive anim(a,"animData");if(!anim.field("blend",o.mAnimData.mBlendMode)||!anim.field("duration",o.mAnimData.mDuration)||!anim.field("flags",o.mAnimData.mFlags.all)||!anim.field("maxFrame",o.mAnimData.mMaxFrame)||!anim.ref("thresholds",RefKind::ParticleData,o.mAnimData.mFrameThresholds)||!anim.ref("primColors",RefKind::ParticleData,o.mAnimData.mPrimColors)||!anim.ref("envColors",RefKind::ParticleData,o.mAnimData.mEnvColors))return false;
 return true;
 }
};
namespace pc_midday {
bool allocate_particle_pool(zen::particleMdlManager&o,u32 m,u32 c){return PcMiddayParticleAccess::allocate(o,m,c);}
bool particle_fields(zen::particleMdlManager&o,ActorArchive&a){return PcMiddayParticleAccess::fields(o,a);}
bool particle_fields(zen::particleGenerator&o,ActorArchive&a){return PcMiddayParticleAccess::fields(o,a);}
namespace {
bool colour(ActorArchive&a,const char*k,Colour&c){PrefixArchive p(a,k);return p.field("r",c.r)&&p.field("g",c.g)&&p.field("b",c.b)&&p.field("a",c.a);}
bool base(zen::particleMdlBase&o,ActorArchive&a){PrefixArchive l(a,"links");return particle_list_fields(o,l)&&a.field("localPosition",o.mLocalPosition)&&a.field("globalPosition",o.mGlobalPosition)&&a.field("size",o.mSize)&&colour(a,"primaryColor",o.mPrimaryColor);}
}
bool particle_list_fields(zen::zenList&o,ActorArchive&a){zen::zenList*self=&o;if(!a.ref("identity",RefKind::ParticleNode,self)||self!=&o)return a.fail("particle node identity mismatch");return a.ref("prev",RefKind::ParticleNode,o.mPrev)&&a.ref("next",RefKind::ParticleNode,o.mNext);}
bool particle_fields(zen::particleMdl&o,ActorArchive&a){
 if(!base(o,a)||!a.field("lifetime",o.mLifeTime)||!a.field("age",o.mAge)||!a.field("ageTimer",o.mAgeTimer)||!a.field("velocity",o.mVelocity)||!a.field("acceleration",o.mAcceleration)||!a.field("scale",o.mScaleFactor)||!a.field("alpha",o.mAlphaFactor)||!a.field("rotAngle",o.mRotAngle)||!a.field("rotSpeed",o.mRotSpeed)||!a.field("normal",o.mOrientedNormal)||!colour(a,"envColor",o.mEnvColor))return false;
 PrefixArchive b(a,"colourAnim");return b.field("progress",o.mBBoardColourAnim.mProgress)&&b.field("frame",o.mBBoardColourAnim.mCurrentFrame)&&b.field("duration",o.mBBoardColourAnim.mDuration)&&b.ref("data",RefKind::ParticleData,o.mBBoardColourAnim.mAnimData)&&a.ref("texture",RefKind::Texture,o.mSimpleTex)&&a.ref("callback",RefKind::ParticleCallback,o.mPtclCallBack);
}
bool particle_fields(zen::particleChildMdl&o,ActorArchive&a){return base(o,a)&&a.field("timer",o._2C)&&a.field("counter0",o._30)&&a.field("counter1",o._31)&&a.field("counter2",o._32);}
bool particle_fields(PermanentEffect&o,ActorArchive&a){return a.field("position",o.mPosition)&&a.ref("generator",RefKind::ParticleGenerator,o.mPtclGen);}
}
#endif
