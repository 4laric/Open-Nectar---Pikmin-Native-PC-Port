#include "pc_midday_engine_census.h"
#include "ObjectMgr.h"
#include "Creature.h"
#include "Piki.h"
#include "Navi.h"
#include "teki.h"
#include "WorldClock.h"
#include <cstring>
#include <limits>
#include <cmath>

namespace pc_midday {
bool observeManager(ObjectMgr* manager,Family family,std::vector<Observation>& out,std::string& e){
    if(!manager){e="manager missing; empty census cannot substitute";return false;}
    int size=manager->getSize(),capacity=manager->getMax();
    if(size<0||capacity<0||size>capacity||capacity>int(MaxActors)){e="invalid manager count/capacity";return false;}
    std::vector<Observation> observed;std::set<int> indices;
    for(int index=manager->getFirst();!manager->isDone(index);index=manager->getNext(index)){
        if(index<0||index>=capacity||!indices.insert(index).second||indices.size()>size_t(capacity)){e="manager iteration cycle/out-of-bound";return false;}
        const Creature* creature=manager->getCreature(index);if(!creature){e="active manager entry has no actor";return false;}
        Observation a;a.address=creature;a.family=family;a.nativeObjectType=uint32_t(creature->mObjType);a.health=creature->mHealth;a.maxHealth=creature->mMaxHealth;
        const Vector3f& pos=creature->mSRT.t;const Vector3f& vel=creature->mVelocity;
        a.position[0]=pos.x;a.position[1]=pos.y;a.position[2]=pos.z;a.velocity[0]=vel.x;a.velocity[1]=vel.y;a.velocity[2]=vel.z;
        a.references.push_back({creature->mStickTarget,Role::AnyActor});
        a.references.push_back({creature->mHoldingCreature.mPtr,Role::AnyActor});
        a.references.push_back({creature->mGrabbedCreature.mPtr,Role::AnyActor});
        if(family==Family::Captain){
            if(creature->mObjType!=OBJTYPE_Navi){e="captain manager type mismatch";return false;}
            const auto* navi=static_cast<const Navi*>(creature);a.captainSlot=navi->mNaviID;
            a.state=navi->mCurrState?navi->mCurrState->getID():-1;
            a.missingState="captain FSM/action timers, controller role/basis, formation and prompt state are not exported";
        }else if(family==Family::Pikmin){
            if(creature->mObjType!=OBJTYPE_Piki){e="Pikmin manager type mismatch";return false;}
            const auto* piki=static_cast<const Piki*>(creature);a.legacyColor=piki->mColor;a.maturity=piki->mHappa;a.state=piki->mCurrentState?piki->mCurrentState->getID():-1;
            unsigned flags=unsigned(piki->mP2Purple)+unsigned(piki->mP2White)+unsigned(piki->mP2Bulbmin);
            if(flags>1){e="contradictory Purple/White/Bulbmin identity";return false;}
            if(piki->mP2Purple)a.species=TypedSpecies::Purple;
            else if(piki->mP2White)a.species=TypedSpecies::White;
            else if(piki->mP2Bulbmin)a.species=TypedSpecies::Bulbmin;
            else if(a.legacyColor==0)a.species=TypedSpecies::NativeBlue;
            else if(a.legacyColor==1)a.species=TypedSpecies::NativeRed;
            else if(a.legacyColor==2)a.species=TypedSpecies::NativeYellow;
            a.references.push_back({piki->mNavi,Role::CaptainOwner});a.references.push_back({piki->mLeaderCreature,Role::AnyActor});
            a.missingState="Pikmin action/FSM timers, jobs, bomb fuse, Purple flight/stun and White attribution are not exported";
        }else if(family==Family::Enemy){
            if(creature->mObjType!=OBJTYPE_Teki){e="enemy manager type mismatch";return false;}
            const auto* enemy=static_cast<const BTeki*>(creature);a.state=enemy->mStateID;a.enemySpecies=uint32_t(enemy->mTekiType);
            a.missingState="enemy species/FSM/animation event boundaries and pointer-keyed P2 family state are not exported";
        }else a.missingState="boss/cargo/item/structure/projectile scalar/FSM/attachments adapter incomplete";
        observed.push_back(std::move(a));
    }
    if(observed.size()!=size_t(size)||manager->getSize()!=size){e="manager census changed or omitted actors";return false;}
    out.insert(out.end(),observed.begin(),observed.end());e.clear();return true;
}
AdapterOutput observeClock(const WorldClock& clock){
    AdapterOutput out;out.reason="clock preview lacks scene/day-end/first-update fence and transition coverage";
    if(sizeof(float)!=4||!std::numeric_limits<float>::is_iec559){out.status=Completeness::Invalid;out.reason="unsupported float representation";return out;}
    for(float f:{clock.mRealSecsPerGameHour,clock.mRealSecsPerGameDay,clock.mPrevTimeOfDay,clock.mRealSecsIntoHour,clock.mTimeOfDay,clock.mDeltaTimeOfDay}){
        if(!std::isfinite(f)){out.status=Completeness::Invalid;out.reason="nonfinite clock state";return out;}
        uint32_t bits;std::memcpy(&bits,&f,4);for(int i=0;i<4;++i)out.state.push_back(uint8_t(bits>>(8*i)));
    }
    return out;
}
AdapterOutput rejectUnimplementedEngineState(const Observation& observed){
    AdapterOutput out;out.reason=observed.missingState.empty()?"typed engine state adapter not implemented":observed.missingState;return out;
}
std::vector<Globals> observeRequiredGlobals(const WorldClock& clock,const BirthLedger& identities){
    // Every required family is represented; unknown/incomplete state has a
    // visible refusal, rather than being omitted or treated as an empty section.
    return {
        {Global::SceneClock,observeClock(clock)},
        {Global::BirthLedger,captureBirthLedger(identities)},
        {Global::StockEconomy,{Completeness::Unsupported,{},"typed stock, sprouts, pending Onion births and cargo economy export absent"}},
        {Global::GameplayRng,{Completeness::Unsupported,{},"offline libc rand not persistable; complete deterministic stream coverage and profile required"}},
        {Global::LogicalAudio,{Completeness::Unsupported,{},"logical event slots/free count/action/context export absent; voices cannot substitute"}},
        {Global::APLedger,{Completeness::Unsupported,{},"consumed/pending rewards, logical grant result and durable acknowledgement/outbox paired generation export absent"}},
        {Global::CaveGraph,{Completeness::Unsupported,{},"same-floor generated descriptor, live party/treasure/route graph export absent; transfer state insufficient"}},
        {Global::Jobs,{Completeness::Unsupported,{},"typed work/carry/held/attack attachments, timers and role graph export absent"}}
    };
}
}
