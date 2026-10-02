#if defined(PIKI_PC_PORT)
#include "pc_midday_actor_ancillary.h"
#include "pc_midday_allocation_owner.h"
#include "Navi.h"
#include "Piki.h"
#include "CPlate.h"
#include "Collision.h"
#include "Kontroller.h"
#include "UtEffect.h"
#include "Route.h"
#include <cmath>

CollInfo::CollInfo(MiddayRestoreTag, u16 capacity)
    : mUseDefaultMaxParts(false), mCollParts(nullptr), mPartIDs(nullptr),
      mPartsCount(0), mMaxParts(capacity), mShape(nullptr) {}

CPlate::CPlate(MiddayRestoreTag, int capacity, float offset, float length, float size)
{
    // Embedded Parameters/Parm constructors only link their own local list.
    // Never call ordinary CPlate ctor: it reads settings and loads cunit.bin.
    mCPlateParms.mStartOffset(offset);
    mCPlateParms.mLengthLimit(length);
    mCPlateParms.mMaxPosSize(size);
    mPlateLength=mPlateSize=10.0f;mInnerRadius=0.0f;
    mTotalSlotCount=0;mPlatePikiCount=0;mUsedSlotCount=0;
    mSlotList=nullptr;mSlotListSize=capacity;mDirectionAngle=0.0f;
    for(int& count:mHappaCounts)count=0;
    _C8=false;mIsNeutral=true;
}

struct PcMiddayAncillaryAccess {
    static CollInfo* collider(pc_midday::AllocationOwner& owner,int capacity) {
        auto* info=owner.construct<CollInfo>([&](void* at){new(at) CollInfo(CollInfo::MiddayRestoreTag{},u16(capacity));});
        auto* parts=owner.array<CollPart>(capacity);
        auto* ids=owner.array<u32>(capacity);
        for(int i=0;i<capacity;++i)parts[i].mParentInfo=info;
        info->mCollParts=parts;info->mPartIDs=ids;
        return info;
    }
    static CPlate* plate(pc_midday::AllocationOwner& owner,const pc_midday::NaviAncillaryConfig& c) {
        auto* plate=owner.construct<CPlate>([&](void* at){new(at) CPlate(CPlate::MiddayRestoreTag{},c.plateCapacity,c.plateStartOffset,c.plateLengthLimit,c.plateMaxPosSize);});
        plate->mSlotList=owner.array<CPlate::Slot>(c.plateCapacity);
        return plate;
    }
};

namespace pc_midday {
namespace {
bool range(float value,float lo,float hi){return std::isfinite(value)&&value>=lo&&value<=hi;}
bool fail(std::string& error,const char* text){error=text;return false;}
}
bool allocate_navi_ancillary(Navi& n,const NaviAncillaryConfig& c,AllocationOwner& owner,std::string& error) {
    if(c.collisionCapacity<1||c.collisionCapacity>1024||c.plateCapacity<1||c.plateCapacity>4096||
       c.controllerPort<1||c.controllerPort>4||!range(c.plateStartOffset,0,100)||
       !range(c.plateLengthLimit,10,1000)||!range(c.plateMaxPosSize,1,50))
        return fail(error,"invalid Navi ancillary allocation profile");
    if(n.mCollInfo||n.mPlateMgr||n.mKontroller||n.mShadowCaster.mShadowDrawer||n.mLoci||
       n.mBurnEffect||n.mRippleEffect||n.mSlimeEffect||n.mNaviLightEfx||n.mNaviLightGlowEfx||n.mCursorTrailEfx||n._780)
        return fail(error,"Navi ancillary targets are not fresh");
    try {
        auto* collider=PcMiddayAncillaryAccess::collider(owner,c.collisionCapacity);
        auto* plate=PcMiddayAncillaryAccess::plate(owner,c);
        auto* controller=owner.make<Kontroller>(c.controllerPort);
        auto* drawer=owner.make<NaviDrawer>(&n);
        auto* burn=owner.make<BurnEffect>(&n.mVelocity);
        auto* ripple=owner.make<RippleEffect>();
        auto* slime=owner.make<SlimeEffect>();slime->mObj=nullptr;
        auto* light=owner.make<PermanentEffect>();
        auto* glow=owner.make<PermanentEffect>();
        auto* trail=owner.make<PermanentEffect>();
        auto* extra=owner.make<PermanentEffect>();
        // No further allocations or callbacks after publishing these borrows.
        n.mCollInfo=collider;n.mPlateMgr=plate;n.mKontroller=controller;n.mShadowCaster.mShadowDrawer=drawer;
        n.mBurnEffect=burn;n.mRippleEffect=ripple;n.mSlimeEffect=slime;
        n.mNaviLightEfx=light;n.mNaviLightGlowEfx=glow;n.mCursorTrailEfx=trail;n._780=extra;
        return true;
    } catch(const std::bad_alloc&) {return fail(error,"Navi ancillary allocation failed");}
}
bool allocate_piki_ancillary(Piki& p,const PikiAncillaryConfig& c,AllocationOwner& owner,std::string& error) {
    if(c.collisionCapacity<1||c.collisionCapacity>1024||c.pathCapacity<0||c.pathCapacity>32767)
        return fail(error,"invalid Piki ancillary allocation profile");
    if(p.mCollInfo||p.mPathBuffers||p.mBurnEffect||p.mRippleEffect||p.mFreeLightEffect||p.mSlimeEffect||p.mPanickedEffect)
        return fail(error,"Piki ancillary targets are not fresh");
    try {
        auto* collider=PcMiddayAncillaryAccess::collider(owner,c.collisionCapacity);
        auto* path=c.pathCapacity?owner.array<PathFinder::Buffer>(c.pathCapacity):nullptr;
        auto* burn=owner.make<BurnEffect>(&p.mVelocity);
        auto* ripple=owner.make<RippleEffect>();
        auto* light=owner.make<FreeLightEffect>();
        auto* slime=owner.make<SlimeEffect>();slime->mObj=nullptr;
        auto* panic=owner.make<PermanentEffect>();
        p.mCollInfo=collider;p.mPathBuffers=path;p.mBurnEffect=burn;p.mRippleEffect=ripple;
        p.mFreeLightEffect=light;p.mSlimeEffect=slime;p.mPanickedEffect=panic;
        return true;
    } catch(const std::bad_alloc&) {return fail(error,"Piki ancillary allocation failed");}
}
}
#endif
