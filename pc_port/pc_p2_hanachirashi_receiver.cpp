#include "pc_p2_hanachirashi_receiver.h"
#include "PikiState.h"
#include "NaviState.h"
#include "PikiAI.h"
#include "Interactions.h"
#include "teki.h"
#include "pc_p2_original_actor.h"
#include "sysNew.h"
#include <map>
#include <cmath>
namespace {
struct Pending { Vector3f direction; Creature* owner; bool wither=true; };
std::map<Piki*,Pending> pikiPending;
std::map<Navi*,Pending> naviPending;
enum Phase { Hit, Fling, Koke, Timer, GetUp };
struct WindPikiState : PikiState {
    Vector3f direction; Phase phase=Hit; float timer=1; bool wither=true;
    WindPikiState():PikiState(PIKISTATE_HanachirashiBlow,"P2_HANA_BLOW"){}
    void init(Piki* p) override {
        auto arg=pikiPending.at(p);direction=arg.direction;wither=arg.wither;pikiPending.erase(p); phase=Hit;timer=1;
        p->endStickObject();p->mActiveAction->resume();p->mIsBeingDamaged=true;
        p->startMotion(PaniMotionInfo(PIKIANIM_JHit,p),PaniMotionInfo(PIKIANIM_JHit));
        p->mVelocity.y=direction.y*(1+.1f*gsys->getRand(1.f));
        p->mFaceDirection=roundAng(std::atan2(direction.x,direction.z)+PI);
        if(wither){p->mHappa=Leaf;p->setFlower(Leaf);}
    }
    void exec(Piki* p) override {
        if(p->getStickObject())p->endStickObject();
        if(phase==Hit){p->mVelocity.x=direction.x;p->mVelocity.z=direction.z;}
        else if(phase==Fling){p->mVelocity.x*=.9f;p->mVelocity.z*=.9f;}
        else {p->mVelocity.set(0,0,0);p->mTargetVelocity.set(0,0,0);
            if(phase==Timer){timer-=gsys->getFrameTime();if(timer<=0||p->mIsWhistlePending){phase=GetUp;p->startMotion(PaniMotionInfo(PIKIANIM_GetUp,p),PaniMotionInfo(PIKIANIM_GetUp));}}}
    }
    void procBounceMsg(Piki* p,MsgBounce*) override {
        if(phase>Fling)return;phase=Koke;timer=1;
        if(!wither&&gsys->getRand(1.f)<.1f){p->mHappa=Leaf;p->setFlower(Leaf);}
        p->startMotion(PaniMotionInfo(PIKIANIM_JKoke,p),PaniMotionInfo(PIKIANIM_JKoke));
    }
    void procAnimMsg(Piki* p,MsgAnim* m) override {
        if(m->mKeyEvent->mEventType!=KEY_Finished)return;
        if(phase==Hit){phase=Fling;p->startMotion(PaniMotionInfo(PIKIANIM_JKoke,p),PaniMotionInfo(PIKIANIM_JKoke));}
        else if(phase==Fling)transit(p,PIKISTATE_Normal);
        else if(phase==Koke)phase=Timer;
        else if(phase==GetUp)transit(p,PIKISTATE_Normal);
    }
    void cleanup(Piki* p) override {
        p->mIsBeingDamaged=false;
        if(!p->isAlive()||pikiPending.count(p))return;
        if(phase<Koke){if(p->mActiveAction->resumable())p->mActiveAction->restart();return;}
        if(p->mIsWhistlePending){p->changeMode(PikiMode::FormationMode,p->mNavi);p->mIsWhistlePending=false;}
        else if(wither)p->changeMode(PikiMode::FreeMode,nullptr);
        else if(p->mActiveAction->resumable())p->mActiveAction->restart();
    }
};
struct WindNaviState : NaviState {
    Vector3f direction;Creature* owner=nullptr;unsigned ownerToken=0;bool wither=true;Phase phase=Hit;float timer=1;
    WindNaviState():NaviState(NAVISTATE_HanachirashiFlick){}
    void init(Navi* n) override {
        auto arg=naviPending.at(n);naviPending.erase(n);direction=arg.direction;owner=arg.owner;ownerToken=pc_p2_original_actor_token(owner);wither=arg.wither;phase=Hit;timer=1;
        n->mVelocity.y=0;n->mFaceDirection=roundAng(std::atan2(direction.x,direction.z)+PI);
        n->startMotion(PaniMotionInfo(PIKIANIM_JHit,n),PaniMotionInfo(PIKIANIM_JHit));
    }
    void exec(Navi* n) override {
        if(phase==Hit){n->mVelocity.x=direction.x;n->mVelocity.z=direction.z;}
        else if(phase==Fling){n->mVelocity.x*=.9f;n->mVelocity.z*=.9f;}
        else {n->mVelocity.set(0,0,0);n->mTargetVelocity.set(0,0,0);
            if(phase==Timer){timer-=gsys->getFrameTime();if(timer<=0){phase=GetUp;n->startMotion(PaniMotionInfo(PIKIANIM_GetUp,n),PaniMotionInfo(PIKIANIM_GetUp));}}}
    }
    void procBounceMsg(Navi* n,MsgBounce*) override {
        if(phase>Fling)return;phase=Koke;timer=1;n->startMotion(PaniMotionInfo(PIKIANIM_JKoke,n),PaniMotionInfo(PIKIANIM_JKoke));
    }
    void procAnimMsg(Navi* n,MsgAnim* m) override {
        if(m->mKeyEvent->mEventType!=KEY_Finished)return;
        if(phase==Hit){phase=Fling;n->startMotion(PaniMotionInfo(PIKIANIM_JKoke,n),PaniMotionInfo(PIKIANIM_JKoke));}
        else if(phase==Fling){phase=Koke;n->startMotion(PaniMotionInfo(PIKIANIM_JKoke,n),PaniMotionInfo(PIKIANIM_JKoke));}
        else if(phase==Koke){phase=Timer;if(!wither)n->mHealth-=1;}
        else if(phase==GetUp)transit(n,NAVISTATE_Walk);
    }
};
struct WindInteraction : Interaction {
    Vector3f direction;bool wither;
    WindInteraction(BTeki* a,const Vector3f& d,bool w=true):Interaction(a),direction(d),wither(w){}
    bool actPiki(Piki* p) immut override {
        if(!p->isAlive()||p->isStickToMouth())return false;
        const int id=p->getState();
        if(id==PIKISTATE_Pressed||id==PIKISTATE_Swallowed||id==PIKISTATE_Dying||id==PIKISTATE_Dead)return false;
        if(!wither&&(id==PIKISTATE_Flick||id==PIKISTATE_Panic))return false;
        if(wither&&p->mP2Purple){p->mHappa=Leaf;p->setFlower(Leaf);return false;}
        pikiPending[p]={direction,mOwner,wither};p->mFSM->transit(p,PIKISTATE_HanachirashiBlow);return true;
    }
    bool actNavi(Navi* n) immut override {
        if(!n->isAlive())return false;
        if(wither&&n->getCurrState()->getID()==NAVISTATE_HanachirashiFlick){auto* state=static_cast<WindNaviState*>(n->getCurrState());if(state->owner==mOwner&&state->ownerToken==pc_p2_original_actor_token(mOwner))return false;}
        naviPending[n]={direction,mOwner,wither};n->mStateMachine->transit(n,NAVISTATE_HanachirashiFlick);return true;
    }
};
}
PikiState* pc_p2_hanachirashi_piki_state_create(){return new WindPikiState;}
NaviState* pc_p2_hanachirashi_navi_state_create(){return new WindNaviState;}
bool pc_p2_hanachirashi_wind_piki(BTeki* a,Piki* p,const Vector3f& d){return p&&p->stimulate(WindInteraction(a,d));}
bool pc_p2_hanachirashi_wind_navi(BTeki* a,Navi* n,const Vector3f& d){return n&&n->stimulate(WindInteraction(a,d));}

namespace {
Vector3f flickDirection(float angle){float strength=150+15*gsys->getRand(1.f);return Vector3f(-std::sin(angle)*strength,100+50*gsys->getRand(1.f),-std::cos(angle)*strength);}
}
bool pc_p2_hanachirashi_flick_piki(BTeki* a,Piki* p){return p&&p->stimulate(WindInteraction(a,flickDirection(p->mFaceDirection),false));}
bool pc_p2_hanachirashi_flick_navi(BTeki* a,Navi* n){return n&&n->stimulate(WindInteraction(a,flickDirection(n->mFaceDirection),false));}
