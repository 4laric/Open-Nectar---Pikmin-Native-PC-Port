#include "pc_p2_hanachirashi_receiver.h"
#include "PikiState.h"
#include "NaviState.h"
#include "NaviMgr.h"
#include "PikiAI.h"
#include "Interactions.h"
#include "teki.h"
#include "pc_p2_original_actor.h"
#include "sysNew.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_p2_original_captain_motion.h"
#include "pc_p2_original_captain_states.h"
#include "pc_p2_equipment.h"
#include <cstdio>
#include <map>
#include <cmath>
namespace {
struct Pending { Vector3f direction; Creature* owner; bool wither=true; float damage=0; };
std::map<Piki*,Pending> pikiPending;
std::map<Navi*,Pending> naviPending;
struct WindNaviState;
std::map<Navi*,WindNaviState*> activeNaviStates;
unsigned long long nextNaviActivation=0;
enum Phase { Hit, Fling, Koke, Timer, GetUp };
struct WindPikiState : PikiState {
    Vector3f direction; Phase phase=Hit; float timer=1; bool wither=true,whistled=false;
    WindPikiState():PikiState(PIKISTATE_HanachirashiBlow,"P2_HANA_BLOW"){}
    void init(Piki* p) override {
        auto arg=pikiPending.at(p);direction=arg.direction;wither=arg.wither;pikiPending.erase(p); phase=Hit;timer=1;whistled=false;
        p->endStickObject();p->mActiveAction->resume();p->mIsBeingDamaged=true;
        p->startMotion(PaniMotionInfo(PIKIANIM_JHit,p),PaniMotionInfo(PIKIANIM_JHit));
        p->mVelocity.y=direction.y*(1+.1f*gsys->getRand(1.f));
        p->mFaceDirection=roundAng(std::atan2(direction.x,direction.z)+PI);
        if(wither){p->mHappa=Leaf;p->setFlower(Leaf);}
    }
    void observeWhistle(Piki* p) {
        if(!p->mIsWhistlePending)return;
        whistled=true;p->mIsWhistlePending=false;
        if(phase>=Koke)timer=0;
    }
    void exec(Piki* p) override {
        observeWhistle(p);
        if(p->getStickObject())p->endStickObject();
        if(phase==Hit){p->mVelocity.x=direction.x;p->mVelocity.z=direction.z;}
        else if(phase==Fling){p->mVelocity.x*=.9f;p->mVelocity.z*=.9f;}
        else {p->mVelocity.set(0,0,0);p->mTargetVelocity.set(0,0,0);
            if(phase==Timer){timer-=gsys->getFrameTime();if(timer<=0){phase=GetUp;p->startMotion(PaniMotionInfo(PIKIANIM_GetUp,p),PaniMotionInfo(PIKIANIM_GetUp));}}}
    }
    void procBounceMsg(Piki* p,MsgBounce*) override {
        if(phase>Fling)return;observeWhistle(p);phase=Koke;timer=1;
        if(!wither&&gsys->getRand(1.f)<.1f){p->mHappa=Leaf;p->setFlower(Leaf);}
        p->startMotion(PaniMotionInfo(PIKIANIM_JKoke,p),PaniMotionInfo(PIKIANIM_JKoke));
    }
    void procAnimMsg(Piki* p,MsgAnim* m) override {
        observeWhistle(p);
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
        if(whistled||p->mIsWhistlePending){p->changeMode(PikiMode::FormationMode,p->mNavi);p->mIsWhistlePending=false;}
        else if(wither)p->changeMode(PikiMode::FreeMode,nullptr);
        else if(p->mActiveAction->resumable())p->mActiveAction->restart();
    }
};
using CaptainMotion=p2original::captain::Motion;
using CaptainListener=p2original::captain::Listener;
struct WindNaviState : NaviState,p2original::captain::State {
    Vector3f direction;Creature* owner=nullptr;Creature* heldCreature=nullptr;unsigned ownerToken=0;
    bool wither=true;Phase phase=Hit;float timer=1,damage=0;
    unsigned long long activation=0;std::uint64_t sceneIncarnation=0;std::string lastError;
    WindNaviState():NaviState(NAVISTATE_HanachirashiFlick){}
    const NaviState* nativeState()const override{return this;}
    p2original::captain::StateId sourceStateId()const override{return phase<=Fling?p2original::captain::StateId::Flick:p2original::captain::StateId::KokeDamage;}
    bool sourceAlive(const Navi& n)const override{return pc_p2_original_captain_actor_alive(&n);}
    bool sourceInvincible()const override{return false;}
    bool sourceVsUsableY()const override{return false;}
    bool invincible(Navi*)override{return false;}
    std::optional<std::uint8_t> actorInvincibleFrames(const Navi& n)const override{std::uint8_t f;if(!pc_p2_original_captain_actor_frames(&n,f))return {};return f;}
    bool canEnterSourceDead(const Navi& n)const override{return pc_p2_original_captain_can_enter_dead(&n);}
    void enterSourceDead(Navi& n)override{pc_p2_original_captain_enter_dead(&n);}
    void sourceDamageFeedback(Navi& n)override{auto* env=pc_p2_original_captain_walk_environment(&n);std::string e;if(!env||&env->scene()!=pc_p2_original_captain_loaded_scene()||!env->damageFeedback(n,e))report("source reaction damage feedback unavailable: "+e);}
    void report(const std::string& e){if(e!=lastError){std::fprintf(stderr,"[original captain reaction refused] %s\n",e.c_str());lastError=e;}}
    p2original::captain::SourceBank* bank(Navi* n,std::string& e)const{
        const auto* world=pc_p2_original_captain_world();bool alive;
        if(!world||world->incarnation()!=sceneIncarnation||!pc_p2_original_captain_actor_lifetime(n,alive)) {e="source reaction lifetime/scene unavailable";return nullptr;}
        auto* b=pc_p2_original_captain_source_bank();p2original::captain::MotionState motion;
        if(!b||!b->ready()||!b->stateAnimator(n,p2original::captain::Animator::Self,motion,e)){e="source reaction motion binding unavailable";return nullptr;}return b;
    }
    bool motion(Navi* n,CaptainMotion m,CaptainListener listener,std::string& e){auto* b=bank(n,e);return b&&b->startMotion(n,m,m,listener,CaptainListener::None,e);}
    void init(Navi* n)override{
        auto arg=pending(n);direction=arg.direction;owner=arg.owner;heldCreature=owner;ownerToken=pc_p2_original_actor_token(owner);wither=arg.wither;damage=arg.damage;
        phase=Hit;timer=1;activation=++nextNaviActivation;activeNaviStates[n]=this;
        const auto* world=pc_p2_original_captain_world();sceneIncarnation=world?world->incarnation():0;
        n->mVelocity.y=0;n->mFaceDirection=roundAng(std::atan2(direction.x,direction.z)+PI);
        std::string e;if(!motion(n,CaptainMotion::Jhit,CaptainListener::SourceActor,e))report(e);
    }
    Pending pending(Navi* n){auto arg=naviPending.at(n);naviPending.erase(n);return arg;}
    void cleanup(Navi* n)override{auto it=activeNaviStates.find(n);if(it!=activeNaviStates.end()&&it->second==this)activeNaviStates.erase(it);}
    bool koke(Navi* n,bool physicalBounce,std::string& e){
        if(physicalBounce){e="source reaction water contact authority unavailable";return false;}
        if(!motion(n,CaptainMotion::Jkoke,CaptainListener::SourceActor,e))return false;
        heldCreature=physicalBounce?nullptr:owner;phase=Koke;timer=1;return true;
    }
    void exec(Navi* n)override{
        std::string e;auto* b=bank(n,e);if(!b){report(e);return;}
        p2original::captain::MotionState m;if(!b->stateAnimator(n,p2original::captain::Animator::Self,m,e)){report(e);return;}
        if(phase==Hit){n->mVelocity.x=direction.x;n->mVelocity.z=direction.z;if(m.motion!=CaptainMotion::Jhit&&!koke(n,false,e))report(e);}
        else if(phase==Fling){n->mVelocity.x*=.9f;n->mVelocity.z*=.9f;report("source reaction bounce triangle authority unavailable");return;
        }else{
            const auto* world=pc_p2_original_captain_world();
            if(world->demo()==p2original::captain::Demo::Unknown){report("source reaction movie authority unavailable");return;}
            if(world->demo()==p2original::captain::Demo::Playing||world->phase()!=p2original::captain::Phase::GameWorldActive){if(!pc_p2_original_captain_transit(n,p2original::captain::StateId::Walk,e))report(e);return;}
            n->mVelocity.set(0,0,0);n->mTargetVelocity.set(0,0,0);
            if(phase==Timer){timer-=gsys->getFrameTime();if(timer<=0){if(!motion(n,CaptainMotion::Getup,CaptainListener::SourceActor,e)){report(e);return;}phase=GetUp;}}
            else if((phase==Koke&&m.motion!=CaptainMotion::Jkoke)||(phase==GetUp&&m.motion!=CaptainMotion::Getup)){if(!pc_p2_original_captain_recover_reaction(n,e))report(e);}
        }
    }
    void procBounceMsg(Navi* n,MsgBounce*)override{if(phase>Fling)return;std::string e;if(!koke(n,true,e))report(e);}
    // P1 animation messages never deliver keys into the source animator.
    void procAnimMsg(Navi*,MsgAnim*)override{}
    bool key(Navi* n,int sourceKey,std::string& e){
        if(sourceKey!=1000){e.clear();return true;}
        if(phase==Hit){if(!motion(n,CaptainMotion::Jkoke,CaptainListener::None,e))return false;phase=Fling;}
        else if(phase==Koke){phase=Timer;const auto result=p2original::captain::addDamage(n,damage,false);if(!result){e="source reaction addDamage refused "+std::to_string(static_cast<int>(result.refusal));report(e);return false;}}
        else if(phase==GetUp)return pc_p2_original_captain_recover_reaction(n,e);
        e.clear();return true;
    }
};
Vector3f flickDirection(float knockback,float angle){
    float magnitude=knockback*(1+.1f*gsys->getRand(1.f));
    return Vector3f(-std::sin(angle)*magnitude,100+50*gsys->getRand(1.f),-std::cos(angle)*magnitude);
}
struct WindInteraction : Interaction {
    Vector3f direction;bool wither;bool generated=false;float knockback=0,damage=0,angle=0;
    WindInteraction(BTeki* a,const Vector3f& d,bool w=true):Interaction(a),direction(d),wither(w){}
    WindInteraction(BTeki* a,float k,float d,float h):Interaction(a),wither(false),generated(true),knockback(k),damage(d),angle(h){}
    bool actPiki(Piki* p) immut override {
        if(!p->isAlive()||p->isStickToMouth())return false;
        const int id=p->getState();
        if(id==PIKISTATE_Pressed||id==PIKISTATE_Swallowed||id==PIKISTATE_Dying||id==PIKISTATE_Dead)return false;
        // The shared custom state represents retail Blow, not Flick. A second
        // sticker/nearby-pass acceptance must consume its own source RNG.
        if(!wither&&pc_p2_source_flick_reaction_blocked(id,PIKISTATE_Flick,PIKISTATE_Panic))return false;
        if(wither&&p->mP2Purple){p->mHappa=Leaf;p->setFlower(Leaf);return false;}
        pikiPending[p]={generated?flickDirection(knockback,angle< -10?p->mFaceDirection:angle):direction,mOwner,wither,0};p->mFSM->transit(p,PIKISTATE_HanachirashiBlow);return true;
    }
    bool actNavi(Navi* n) immut override {
        if(!n||!n->getCurrState()||!n->mStateMachine)return false;
        bool alive;const auto* world=pc_p2_original_captain_world();if(!world||world->phase()!=p2original::captain::Phase::GameWorldActive||!pc_p2_original_captain_actor_lifetime(n,alive))return false;
        int mapped=-1;if(pc_p2_original_captain_route_transition(n,NAVISTATE_HanachirashiFlick,mapped)!=PcOriginalCaptainRoute::Handled||mapped!=NAVISTATE_HanachirashiFlick)return false;
        if(generated&&p2original::captain::flickAdmission(mOwner,n)!=p2original::captain::Refusal::None)return false;
        if(wither&&pc_p2_equipment_has(p2equipment::Item::RepugnantAppendage))return false;
        auto* bank=pc_p2_original_captain_source_bank();std::string error;
        if(!bank||!bank->ready()||!bank->supports(n,CaptainMotion::Jhit,error)||!bank->supports(n,CaptainMotion::Jkoke,error)||!bank->supports(n,CaptainMotion::Getup,error))return false;
        if(wither&&n->getCurrState()->getID()==NAVISTATE_HanachirashiFlick){auto it=activeNaviStates.find(n);if(it==activeNaviStates.end()||it->second!=n->getCurrState())return false;auto* state=it->second;if((state->phase<=Fling?state->owner:state->heldCreature)==mOwner&&state->ownerToken==pc_p2_original_actor_token(mOwner))return false;}
        naviPending[n]={generated?flickDirection(knockback,angle< -10?n->mFaceDirection:angle):direction,mOwner,wither,damage};n->mStateMachine->transit(n,NAVISTATE_HanachirashiFlick);return true;
    }
};
}
PikiState* pc_p2_hanachirashi_piki_state_create(){return new WindPikiState;}
NaviState* pc_p2_hanachirashi_navi_state_create(){return new WindNaviState;}
bool pc_p2_hanachirashi_wind_piki(BTeki* a,Piki* p,const Vector3f& d){return p&&p->stimulate(WindInteraction(a,d));}
bool pc_p2_hanachirashi_wind_navi(BTeki* a,Navi* n,const Vector3f& d){return n&&n->stimulate(WindInteraction(a,d));}

bool pc_p2_source_flick_piki(BTeki* a,Piki* p,float knockback,float angle){
    if(!a||!p||!std::isfinite(knockback)||knockback<0||!std::isfinite(angle))return false;
    return p->stimulate(WindInteraction(a,knockback,0,angle));
}
bool pc_p2_source_flick_navi(BTeki* a,Navi* n,float knockback,float damage,float angle){
    if(!a||!n||!std::isfinite(knockback)||knockback<0||!std::isfinite(damage)||damage<0||!std::isfinite(angle))return false;
    return n->stimulate(WindInteraction(a,knockback,damage,angle));
}
bool pc_p2_hanachirashi_flick_piki(BTeki* a,Piki* p){return pc_p2_source_flick_piki(a,p,150,FLICK_BACKWARDS_ANGLE);}
bool pc_p2_hanachirashi_flick_navi(BTeki* a,Navi* n){return pc_p2_source_flick_navi(a,n,150,1,FLICK_BACKWARDS_ANGLE);}

bool pc_p2_source_navi_reaction_gate(Navi* n,PcSourceNaviReactionGate& out){
    out={};if(!n)return false;
    auto it=activeNaviStates.find(n);if(it==activeNaviStates.end()||n->getCurrState()!=it->second||n->getCurrState()->getID()!=NAVISTATE_HanachirashiFlick)return false;
    const auto* state=it->second;out.kind=state->phase<=Fling?PcSourceNaviReactionKind::Flick:PcSourceNaviReactionKind::KokeDamage;
    out.phase=static_cast<unsigned>(state->phase);out.activation=state->activation;out.inheritedInvincible=false;return true;
}

bool pc_p2_source_navi_reaction_animation_key(Navi* n,const NaviState* expectedState,
    std::uint64_t expectedSelfGeneration,int sourceKey,std::string& error){
    error.clear();auto refuse=[&](const char* message){error=message;return false;};
    if(!n||!expectedState||n->getCurrState()!=expectedState)return refuse("source reaction current state mismatch");
    auto it=activeNaviStates.find(n);
    if(it==activeNaviStates.end()||it->second!=expectedState||it->second->getID()!=NAVISTATE_HanachirashiFlick)return refuse("source reaction is not the active owned receiver");
    auto* state=it->second;const auto activation=state->activation;
    auto* typed=dynamic_cast<p2original::captain::State*>(n->getCurrState());
    if(!activation||typed!=static_cast<p2original::captain::State*>(state)||typed->nativeState()!=expectedState)return refuse("source reaction typed state mismatch");
    const auto* world=pc_p2_original_captain_world();if(!world||world->phase()!=p2original::captain::Phase::GameWorldActive)return refuse("source reaction world inactive");
    bool alive;if(!pc_p2_original_captain_actor_lifetime(n,alive))return refuse("source reaction actor lifetime unavailable");
    auto* bank=state->bank(n,error);p2original::captain::MotionState motion;
    if(!bank||!bank->stateAnimator(n,p2original::captain::Animator::Self,motion,error))return false;
    if(!expectedSelfGeneration||motion.generation!=expectedSelfGeneration)return refuse("source reaction Self generation mismatch");
    it=activeNaviStates.find(n);
    if(it==activeNaviStates.end()||it->second!=state||state->activation!=activation||n->getCurrState()!=expectedState)return refuse("source reaction activation changed");
    return state->key(n,sourceKey,error);
}
