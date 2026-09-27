// Kabuto 75 own identity on TEKI_Beatle: free larva FSM (Dead/Wait/Turn/Move/
// Flick/Attack) with mouth-joint stone fire (Stone 74, non-homing for 75).
// Bridge binds via campaign_ids + bind_source (onion:p2:75); P2 FSM decides
// every tick, host AI suppressed.
// #884: the attack births a travelling Stone on KEYEVENT_2 (attack frame 50)
// into a shooter-independent fleet (pc_p2_kabuto_stone_fleet.h) ticked at
// 30 Hz from gameCoreSection, instead of an instant cone strike.
#include "pc_p2_kabuto_fsm.h"
#include "pc_p2_kabuto_fsm_policy.h"
#include "pc_p2_kabuto_stone_fleet.h"
#include "pc_p2_rock_host.h"
#include "pc_p2_projectile_engine_receiver.h"
#include "pc_p2_campaign_actor.h"
#include "pc_randomizer.h"
#include "pc_bbft.h"
#include "teki.h"
#include "Generator.h"
#include "Shape.h"
#include "Texture.h"
#include "Graphics.h"
#include "Camera.h"
#include "gameflow.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "Interactions.h"
#include "MapMgr.h"
#include "MoviePlayer.h"
#include "AIConstant.h"
#include "ObjType.h"
#include "system.h"
#include "gl/pc_gfx.h"
#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace {
std::map<PelletView*,bool> actors;
std::map<std::string,std::vector<Shape*>> animated;
std::map<std::string,p2animation::Clip> timing;
std::set<PelletView*> drawn,drawnCorpse;
enum KState { KB_DEAD=0,KB_WAIT=1,KB_TURN=2,KB_MOVE=3,KB_FLICK=4,KB_ATTACK=5 };
const float PI_F=3.14159265f;
constexpr float HOME_RADIUS=15.0f;
constexpr float TERRITORY=150.0f;
constexpr float TURN_RATE=2.5f;
constexpr float ATTACK_ANGLE=0.5f;
constexpr float FACE_OK_ANGLE=0.174533f;
constexpr int FLICK_STUCK_MIN=3;
struct KabutoFsm {
    KState state=KB_WAIT;float stateTime=0.0f;float heading=0.0f;
    Vector3f home;Vector3f targetPos;bool targetValid=false;
    unsigned rng=1;unsigned token=0;bool deadLogged=false;bool fireDone=false;bool flickDone=false;bool escaped=false;float deathPrior=0.0f;bool deathPriorSet=false;
    std::string clip="wait";float phase=0.0f;float logTimer=0.0f;float lastHealth=0.0f;float poolFullCooldown=0.0f;
};
std::map<PelletView*,KabutoFsm> fsms;
bool ready=false;
// #884 Stone fleet. Stones outlive their shooter: they are owned here, not by
// the actor, and are cleared only by reset (teardown / re-entry).
p2kabutostone::Fleet fleet;
struct StoneMap{p2rockhost::TraceProxy proxy;unsigned long long calls=0,walls=0;} stoneMap;
double stoneDebt=0.0;
unsigned slotGen[p2kabutostone::kFleetCapacity]={};
int slotPosTicks[p2kabutostone::kFleetCapacity]={};
bool stoneDrawLogged=false;
std::map<std::uint64_t,BTeki*> shooters;
constexpr float STONE_DT=P2CannonStone::kSourceDelta;
constexpr int STONE_MAX_TICKS=4;
std::uint64_t tokenOf(Creature* c){return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(c));}
float wrapPi(float a){while(a>PI_F)a-=2.0f*PI_F;while(a<-PI_F)a+=2.0f*PI_F;return a;}
float distXZ(const Vector3f& a,const Vector3f& b){const float dx=a.x-b.x,dz=a.z-b.z;return std::sqrt(dx*dx+dz*dz);}
float clipSeconds(const std::string& name){auto it=timing.find(name);return it==timing.end()?1.0f:it->second.duration/30.0f;}
void loadAnimation(const std::vector<p2animation::Clip>& bank){
    size_t total=0;std::vector<unsigned char> reference;
    for(const auto& clip:bank){size_t clipBytes=0;
        for(int i=0;i<clip.count;++i){char path[192];std::snprintf(path,sizeof(path),"assets/dataDir/courses/pikmin2room/kabuto_Kabuto_%s_%02d.mod",clip.name.c_str(),i);
            std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)std::abort();auto size=file.tellg();
            if(size<=0||size>512*1024)std::abort();clipBytes+=size_t(size);total+=size_t(size);
            if(clipBytes>512*1024||total>10*1024*1024)std::abort();file.seekg(0);
            std::vector<unsigned char> bytes(size_t(size),0),resources;
            if(!file.read(reinterpret_cast<char*>(bytes.data()),size)||!p2animation::resources(bytes,resources))std::abort();
            if(!reference.empty()&&reference!=resources)std::abort();reference=resources;
        }
    }
    Shape* shared=nullptr;
    for(const auto& clip:bank){timing[clip.name]=clip;
        for(int i=0;i<clip.count;++i){char path[160];std::snprintf(path,sizeof(path),"courses/pikmin2room/kabuto_Kabuto_%s_%02d.mod",clip.name.c_str(),i);
            Shape* shape=gameflow.loadShape(path,true);if(!shape)std::abort();
            if(!shared){shared=shape;for(int t=0;t<shape->mTexAttrCount;++t)if(shape->mTexAttrList[t].mTexture)shape->mTexAttrList[t].mTexture->attach();}
            else{
                if(shape->mMaterialCount!=shared->mMaterialCount||shape->mTexAttrCount!=shared->mTexAttrCount||shape->mTevInfoCount!=shared->mTevInfoCount)std::abort();
                for(int j=0;j<shape->mTotalMatpolyCount;++j){auto* poly=shape->mMatpolyList[j];if(!poly||!poly->mMaterial)continue;int material=-1;
                    for(int m=0;m<shape->mMaterialCount;++m)if(poly->mMaterial==&shape->mMaterialList[m])material=m;
                    if(material<0)std::abort();poly->mMaterial=&shared->mMaterialList[material];}
                shape->mMaterialList=shared->mMaterialList;shape->mTexAttrList=shared->mTexAttrList;shape->mTevInfoList=shared->mTevInfoList;
            }
            animated[clip.name].push_back(shape);
        }
    }
    std::printf("P2_KABUTO_BANK_READY mod_bytes=%zu gameplay=P1_unchanged\n",total);
}
void stop(BTeki* a){a->inputDrive(Vector3f(0.0f,0.0f,0.0f));a->mVelocity.x=0.0f;a->mVelocity.y=0.0f;a->mVelocity.z=0.0f;}
void walkTo(BTeki* a,KabutoFsm& s,const Vector3f& target,float speed,float dt){
    const Vector3f pos=a->getPosition();
    const float desired=std::atan2(target.x-pos.x,target.z-pos.z);
    const float maxTurn=TURN_RATE*dt;
    float diff=wrapPi(desired-s.heading);
    if(diff>maxTurn)diff=maxTurn;if(diff<-maxTurn)diff=-maxTurn;
    s.heading=wrapPi(s.heading+diff);
    a->setDirection(s.heading);
    const Vector3f drive(std::sin(s.heading)*speed,0.0f,std::cos(s.heading)*speed);
    a->inputDrive(drive);a->mVelocity.set(drive);
}
void turnTo(BTeki* a,KabutoFsm& s,const Vector3f& target,float dt){
    const Vector3f pos=a->getPosition();
    const float desired=std::atan2(target.x-pos.x,target.z-pos.z);
    const float maxTurn=TURN_RATE*dt;
    float diff=wrapPi(desired-s.heading);
    if(diff>maxTurn)diff=maxTurn;if(diff<-maxTurn)diff=-maxTurn;
    s.heading=wrapPi(s.heading+diff);
    a->setDirection(s.heading);
}
Creature* nearestTarget(const Vector3f& pos,float sight){
    Creature* best=nullptr;float bestSq=sight*sight;
    if(naviMgr){Navi* n=naviMgr->getNavi();if(n&&n->isAlive()){const Vector3f p=n->getPosition();
        const float dx=p.x-pos.x,dz=p.z-pos.z,d=dx*dx+dz*dz;if(d<bestSq){bestSq=d;best=n;}}}
    if(pikiMgr){Iterator it(pikiMgr);CI_LOOP(it){Piki* p=static_cast<Piki*>(*it);if(!p||!p->isAlive())continue;
        const Vector3f q=p->getPosition();const float dx=q.x-pos.x,dz=q.z-pos.z,d=dx*dx+dz*dz;if(d<bestSq){bestSq=d;best=p;}}}
    return best;
}
int stuckPikminCount(Creature* c){int n=0;for(Creature* s=c->mStickListHead;s;s=s->mNextSticker){if(!s||!s->isPiki()||!s->isAlive())continue;++n;}return n;}
bool attackable(const KabutoFsm& s,const Vector3f& pos,const Creature* t,float range){
    if(!t)return false;const Vector3f tp=t->getPosition();
    if(distXZ(pos,tp)>=range)return false;
    return std::fabs(wrapPi(std::atan2(tp.x-pos.x,tp.z-pos.z)-s.heading))<ATTACK_ANGLE;
}
bool shouldFlick(BTeki* a){return stuckPikminCount(a)>=FLICK_STUCK_MIN;}
// Source StateAttack KEYEVENT_2 -> createStoneAttack (Kabuto.cpp:268-290):
// Stone 74 born at the mouth XZ and 25 over the Kabuto's own Y, facing the
// Kabuto, non-homing for 75. The Stone then travels (fleet tick below). The
// rock emit effect (createRockEmitEffect) is not reproduced.
void fireStone(BTeki* actor,KabutoFsm& s,unsigned gen){
    const Vector3f pos=actor->getPosition();
    const P2CannonStoneVec3 birth=p2kabutostone::birthPosition({pos.x,pos.y,pos.z},s.heading,p2kabutostone::kMouthForwardHostApprox);
    std::uint32_t id=0;
    const int slot=fleet.fire(tokenOf(actor),birth,s.heading,id);
    if(slot<0){
        // Rock manager birth failure is silently tolerated (Kabuto.cpp:283).
        if(s.poolFullCooldown<=0.0f){s.poolFullCooldown=1.0f;
            std::printf("P2_KABUTO_STONE_POOL_FULL generator=%u active=%d cap=%d\n",gen,fleet.active(),p2kabutostone::Fleet::capacity());std::fflush(stdout);}
        return;
    }
    slotGen[slot]=gen;slotPosTicks[slot]=0;
    const auto at=timing.find("attack");
    std::printf("P2_KABUTO_STONE_BIRTH generator=%u source_id=75 stone=%u stone_type=74 slot=%d homing=%d frame=%d t=%.4f clip_frames=%d birth=(%.2f,%.2f,%.2f) face_deg=%.1f mouth_source=host_approx active=%d\n",
        gen,id,slot,int(fleet.stone(slot).homing()),p2kabutostone::kAttackKey2Frame,s.stateTime,at==timing.end()?0:at->second.duration,
        birth.x,birth.y,birth.z,s.heading*180.0f/PI_F,fleet.active());
    std::fflush(stdout);
}
int doFlick(BTeki* actor){
    const Vector3f pos=actor->getPosition();
    int hit=0;
    std::vector<Piki*> pikis;
    if(pikiMgr){Iterator it(pikiMgr);CI_LOOP(it){Piki* q=static_cast<Piki*>(*it);if(!q||!q->isAlive())continue;
        if(distXZ(q->getPosition(),pos)<120.0f)pikis.push_back(q);}}
    for(Piki* q:pikis){if(!q||!q->isAlive())continue;
        if(q->stimulate(InteractFlick(actor,300.0f,0.0f,FLICK_BACKWARDS_ANGLE)))++hit;}
    if(naviMgr){Navi* n=naviMgr->getNavi();if(n&&n->isAlive()&&distXZ(n->getPosition(),pos)<120.0f)
        if(n->stimulate(InteractFlick(actor,300.0f,0.0f,FLICK_BACKWARDS_ANGLE)))++hit;}
    return hit;
}
void setPhase(KabutoFsm& s){const float d=clipSeconds(s.clip);float ph=s.stateTime/d;if(ph>1.0f)ph=1.0f;s.phase=ph;}
void transition(BTeki* a,KabutoFsm& s,KState st,const char* clip,unsigned gen){
    (void)a;s.state=st;s.stateTime=0.0f;s.fireDone=false;s.flickDone=false;if(clip)s.clip=clip;
    std::printf("P2_KABUTO_STATE generator=%u state=%s\n",gen,p2kabutofsm::stateName(st));std::fflush(stdout);
}
void die(BTeki* a,KabutoFsm& s,unsigned gen,float prior){
    if(!s.deadLogged){s.deadLogged=true;std::printf("P2_KABUTO_DEAD generator=%u source_id=75 health=0 prior_health=%.1f\n",gen,prior);std::fflush(stdout);}
    transition(a,s,KB_DEAD,"dead",gen);
}
}
void pc_p2_kabuto_fsm_reset(){
    if(fleet.active()>0){std::printf("P2_KABUTO_STONE_RESET active=%d\n",fleet.active());std::fflush(stdout);}
    fleet.reset();stoneDebt=0.0;stoneMap.proxy.clear();stoneDrawLogged=false;shooters.clear();
    for(int i=0;i<p2kabutostone::kFleetCapacity;++i){slotGen[i]=0;slotPosTicks[i]=0;}
    actors.clear();fsms.clear();drawn.clear();drawnCorpse.clear();animated.clear();timing.clear();ready=false;}
void pc_p2_kabuto_fsm_forget(BTeki* a){auto* v=static_cast<PelletView*>(a);
    // The shooter is gone; its stones keep flying and Press is no longer
    // attributed to it (no dangling actor pointer is kept).
    const std::uint64_t tok=tokenOf(a);const int orphaned=fleet.forgetOwner(tok);shooters.erase(tok);
    if(orphaned>0){auto f=fsms.find(v);std::printf("P2_KABUTO_STONE_ORPHAN generator=%u stones=%d\n",f!=fsms.end()?f->second.token:0u,orphaned);std::fflush(stdout);}
    pc_randomizer_p2_forget_source(v);actors.erase(v);fsms.erase(v);drawn.erase(v);drawnCorpse.erase(v);}
float pc_p2_kabuto_fsm_param_f(const BTeki* a,int idx,float fb){
    auto i=actors.find(static_cast<PelletView*>(const_cast<BTeki*>(a)));if(i==actors.end())return fb;
    const auto& p=p2kabutofsm::params();
    switch(idx){case TPF_Life:return p.health;case TPF_VisibleRange:return p.sight;
    case TPF_AttackableRange:return p.attackRange;case TPF_AttackPower:return p.attackDamage;default:return fb;}
}
bool pc_p2_kabuto_fsm_suppress_ai(const BTeki* a){return ready&&actors.count(static_cast<PelletView*>(const_cast<BTeki*>(a)))!=0;}
void pc_p2_kabuto_fsm_setup(){
    pc_p2_kabuto_fsm_reset();
    std::printf("P2_KABUTO_SETUP\n");std::fflush(stdout);
    const bool bridge=pc_randomizer_p2_bridge()&&!pc_pikipelago_room_preview();
    const bool preview=pc_pikipelago_room_preview();
    if(!bridge&&!preview)return;
    std::ifstream input("p2-kabuto.txt");if(!input)return;
    std::map<unsigned,std::string> wanted;std::vector<p2animation::Clip> bank;
    if(!p2kabutofsm::parse(input,wanted,bank))std::abort();
    for(const auto& c:bank)if(c.name=="attack"&&!p2kabutostone::clipHasKey2(c.duration)){
        std::printf("P2_KABUTO_ERROR key2_outside_attack duration=%d key2_frame=%d\n",c.duration,p2kabutostone::kAttackKey2Frame);std::fflush(stdout);std::abort();}
    if(bridge){wanted.clear();for(unsigned id:pc_p2_campaign_ids(75))wanted[id]="Kabuto";}
    if(wanted.empty())return;
    std::set<unsigned> seen;
    Iterator it(tekiMgr);CI_LOOP(it){Teki* teki=static_cast<Teki*>(*it);if(!teki||!teki->mGenerator)continue;
        const unsigned token=bridge?pc_p2_campaign_token(teki):teki->mGenerator->_70;
        if(wanted.find(token)==wanted.end())continue;
        if(!seen.insert(token).second)std::abort();if(teki->mTekiType!=TEKI_Beatle)std::abort();
        actors[static_cast<PelletView*>(teki)]=true;shooters[tokenOf(teki)]=teki;
        teki->mHealth=p2kabutofsm::params().health;
        KabutoFsm& f=fsms[static_cast<PelletView*>(teki)];
        f.home=teki->getPosition();f.heading=teki->getDirection();f.targetPos=f.home;f.targetValid=true;
        f.rng=(token*2654435761u)|1u;f.token=token;f.state=KB_WAIT;f.clip="wait";f.phase=0.0f;f.lastHealth=teki->mHealth;
        if(bridge){pc_randomizer_p2_bind_source(static_cast<PelletView*>(teki),75,token);std::printf("P2_KABUTO_DELIVERY_BIND generator=%u source_id=75\n",token);}
        std::printf("P2_KABUTO_BIND generator=%u source_id=75 visual_only=0\n",token);
        std::printf("P2_KABUTO_READY species=Kabuto generator=%u health=%.1f max_health=%.1f behavior=source_fsm rewards=P1_unchanged\n",token,teki->mHealth,teki->getParameterF(TPF_Life));
        std::printf("P2_ENEMY_READY species=Kabuto native_family=Kabuto generator=%u x=%.7f y=%.7f z=%.7f health=%.1f max_health=%.1f behavior=native source_FSM=implemented\n",token,teki->getPosition().x,teki->getPosition().y,teki->getPosition().z,teki->mHealth,p2kabutofsm::params().health);
        std::printf("P2_KABUTO_STATE generator=%u state=wait\n",token);std::fflush(stdout);
    }
    if(seen.size()!=wanted.size()){std::printf("P2_KABUTO_ERROR missing_actor wanted=%zu found=%zu\n",wanted.size(),seen.size());std::abort();}
    loadAnimation(bank);ready=true;
}
void pc_p2_kabuto_fsm_update(BTeki* actor){
    if(!ready)return;
    auto it=actors.find(static_cast<PelletView*>(actor));if(it==actors.end())return;
    auto ft=fsms.find(static_cast<PelletView*>(actor));if(ft==fsms.end())return;
    KabutoFsm& s=ft->second;
    const float dt=gsys->getFrameTime();if(dt<=0.0f||dt>0.5f)return;
    const Vector3f pos=actor->getPosition();
    const unsigned live=actor->mGenerator?pc_p2_campaign_token(actor):0u;
    if(live)s.token=live;
    const unsigned gen=s.token?s.token:live;
    const auto& p=p2kabutofsm::params();
    if(actor->mStoredDamage>0.0f)actor->makeDamaged();
    const float previousHealth=s.lastHealth;
    if(actor->mHealth<=0.0f&&!s.deathPriorSet&&previousHealth>0.0f){s.deathPrior=previousHealth;s.deathPriorSet=true;}
    const float priorForDeath=s.deathPriorSet?s.deathPrior:previousHealth;
    if(actor->mHealth<s.lastHealth&&actor->mHealth>0.0f){
        std::printf("P2_KABUTO_DAMAGE generator=%u source_id=75 health=%.1f\n",gen,actor->mHealth);std::fflush(stdout);}
    s.lastHealth=actor->mHealth;
    if(s.poolFullCooldown>0.0f)s.poolFullCooldown-=dt;
    const float prevStateTime=s.stateTime;
    s.stateTime+=dt;
    switch(s.state){
    case KB_WAIT:{
        stop(actor);
        if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
        if(shouldFlick(actor)){transition(actor,s,KB_FLICK,"flick",gen);break;}
        if(s.stateTime>=clipSeconds(s.clip)){
            s.stateTime=0.0f;Creature* t=nearestTarget(pos,p.sight);
            if(t){s.targetPos=t->getPosition();s.targetValid=true;
                if(attackable(s,pos,t,p.attackRange))transition(actor,s,KB_ATTACK,"attack",gen);
                else if(std::fabs(wrapPi(std::atan2(s.targetPos.x-pos.x,s.targetPos.z-pos.z)-s.heading))>FACE_OK_ANGLE)
                    transition(actor,s,KB_TURN,"wait",gen);
            }
        }
        break;}
    case KB_TURN:{
        stop(actor);
        if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
        if(shouldFlick(actor)){transition(actor,s,KB_FLICK,"flick",gen);break;}
        Creature* t=nearestTarget(pos,p.sight);
        if(t){turnTo(actor,s,t->getPosition(),dt);
            if(attackable(s,pos,t,p.attackRange)){s.targetPos=t->getPosition();s.targetValid=true;transition(actor,s,KB_ATTACK,"attack",gen);}
            else if(std::fabs(wrapPi(std::atan2(t->getPosition().x-pos.x,t->getPosition().z-pos.z)-s.heading))<=FACE_OK_ANGLE)transition(actor,s,KB_WAIT,"wait",gen);
            else if(s.stateTime>=clipSeconds("wait"))transition(actor,s,KB_WAIT,"wait",gen);
        } else transition(actor,s,KB_WAIT,"wait",gen);
        break;}
    case KB_MOVE:{
        if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
        if(shouldFlick(actor)){transition(actor,s,KB_FLICK,"flick",gen);break;}
        Creature* t=nearestTarget(pos,p.sight);
        if(t){s.targetPos=t->getPosition();s.targetValid=true;
            if(attackable(s,pos,t,p.attackRange)){transition(actor,s,KB_ATTACK,"attack",gen);break;}
            walkTo(actor,s,s.targetPos,p.moveSpeed,dt);
            if(distXZ(pos,s.home)>TERRITORY)transition(actor,s,KB_TURN,"wait",gen);
        } else {walkTo(actor,s,s.home,p.moveSpeed,dt);
            if(distXZ(pos,s.home)<HOME_RADIUS)transition(actor,s,KB_WAIT,"wait",gen);}
        break;}
    case KB_ATTACK:{
        stop(actor);
        // StateAttack::exec (KabutoState.cpp:350-358): health gate first, so a
        // Kabuto killed before frame 50 never fires; then KEYEVENT_2 (retail
        // attack event frame 50 at 30 fps) births the Stone exactly once.
        if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
        if(p2kabutostone::attackMayFire(actor->mHealth,s.fireDone,prevStateTime,s.stateTime)){s.fireDone=true;fireStone(actor,s,gen);}
        if(s.stateTime>=clipSeconds("attack")){
            if(shouldFlick(actor))transition(actor,s,KB_FLICK,"flick",gen);
            else if(distXZ(pos,s.home)>TERRITORY)transition(actor,s,KB_TURN,"wait",gen);
            else transition(actor,s,KB_WAIT,"wait",gen);
        }
        break;}
    case KB_FLICK:{
        stop(actor);
        if(!s.flickDone){s.flickDone=true;int hit=doFlick(actor);std::printf("P2_KABUTO_FLICK generator=%u source_id=75 hit=%d\n",gen,hit);std::fflush(stdout);}
        if(s.stateTime>=clipSeconds("flick")){
            if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
            Creature* t=nearestTarget(pos,p.sight);
            if(t&&attackable(s,pos,t,p.attackRange)){s.targetPos=t->getPosition();s.targetValid=true;transition(actor,s,KB_ATTACK,"attack",gen);}
            else transition(actor,s,KB_WAIT,"wait",gen);
        }
        break;}
    case KB_DEAD:{
        stop(actor);
        if(!s.escaped&&s.stateTime>=clipSeconds("dead")){s.escaped=true;actor->pcEscapeNow();}
        // KB_DEAD has no mDeadState assignment; silence unused-enum warning.
        break;}
    default:break;
    }
    // KB_MOVE is entered from KB_TURN only when the target leaves the attack
    // cone; keep the state reachable for the turn->chase->attack chain.
    if(s.state==KB_TURN&&s.stateTime>5.0f)transition(actor,s,KB_MOVE,"move",gen);
    setPhase(s);
    s.logTimer+=dt;
    if(s.logTimer>=1.0f){s.logTimer=0.0f;const Vector3f now=actor->getPosition();
        std::printf("P2_KABUTO_FSM_POS species=Kabuto generator=%u state=%s x=%.2f y=%.2f z=%.2f health=%.1f\n",
            gen,p2kabutofsm::stateName(s.state),now.x,now.y,now.z,actor->mHealth);std::fflush(stdout);}
}
bool pc_p2_kabuto_fsm_draw(BTeki* actor,Graphics& gfx,const Matrix4f& matrix,bool corpse){
    auto it=actors.find(static_cast<PelletView*>(actor));if(it==actors.end())return false;
    {
        auto* v=static_cast<PelletView*>(actor);
        auto ftok=fsms.find(v);
        const unsigned liveTok=actor->mGenerator?pc_p2_campaign_token(actor):0u;
        const unsigned token=liveTok?liveTok:(ftok!=fsms.end()?ftok->second.token:0u);
        if(drawn.insert(v).second&&!corpse){std::printf("P2_KABUTO_DRAW generator=%u source_id=75 species=Kabuto corpse=%d\n",token,int(corpse));std::fflush(stdout);}
        if(corpse&&drawnCorpse.insert(v).second){std::printf("P2_KABUTO_CORPSE_DRAW generator=%u source_id=75 species=Kabuto\n",token);std::fflush(stdout);}
    }
    auto ft=fsms.find(static_cast<PelletView*>(actor));
    const char* name=corpse?"dead":(ft!=fsms.end()?ft->second.clip.c_str():p2kabutofsm::motionClip(actor->mTekiAnimator->getCurrentMotionIndex()));
    Shape* shape=animated.at("wait").front();
    if(name){float phase=corpse?1.0f:(ft!=fsms.end()?ft->second.phase:0.0f);shape=animated.at(name).at(timing.at(name).index(phase,corpse));}
    shape->updateAnim(gfx,matrix,nullptr,actor);
    pc_gfx_specular_family_scope(1);
    shape->drawshape(gfx,*gfx.mCamera,nullptr);
    pc_gfx_specular_family_scope(0);
    return true;
}
namespace {
// Host map trace for the Stone in the P2 base-point convention (mPosition is
// the sphere bottom; P1 traceMove adds/subtracts the radius itself). Dynamic
// collision is included so gates/platforms stop a Stone like P2 walls do.
bool stoneTrace(void* ctx,const P2CannonStoneVec3& base,const P2CannonStoneVec3& vel,float dt,float radius,P2CannonStoneTraceResult& out){
    StoneMap& m=*static_cast<StoneMap*>(ctx);
    if(!mapMgr||!mapMgr->mMapModel)return false;
    if(!std::isfinite(base.x)||!std::isfinite(base.y)||!std::isfinite(base.z)||!std::isfinite(vel.x)||!std::isfinite(vel.y)||!std::isfinite(vel.z))return false;
    m.proxy.clear();
    MoveTrace mv(Vector3f(base.x,base.y,base.z),Vector3f(vel.x,vel.y,vel.z),radius,false);
    mapMgr->traceMove(&m.proxy,mv,dt);
    ++m.calls;
    out.position={mv.mPosition.x,mv.mPosition.y,mv.mPosition.z};
    out.velocity={mv.mVelocity.x,mv.mVelocity.y,mv.mVelocity.z};
    out.wall=m.proxy.wall;m.walls+=out.wall?1u:0u;
    return std::isfinite(out.position.x)&&std::isfinite(out.position.y)&&std::isfinite(out.position.z)
        &&std::isfinite(out.velocity.x)&&std::isfinite(out.velocity.y)&&std::isfinite(out.velocity.z);
}
struct StoneSnapshot{std::vector<p2kabutostone::Target> targets;std::vector<Creature*> creatures;std::vector<char> kinds;};
void snapshotAdd(StoneSnapshot& snap,Creature* c,P2CannonStoneContactKind kind,char code){
    if(!c||!c->isAlive()||!c->isAtari()||c->isBuried())return;
    const Vector3f centre=c->getCentre();
    p2kabutostone::Target t;t.token=tokenOf(c);t.centre={centre.x,centre.y,centre.z};t.radius=c->getCentreSize();
    t.kind=kind;t.onFloor=c->mGroundTriangle!=nullptr;t.alive=true;
    snap.targets.push_back(t);snap.creatures.push_back(c);snap.kinds.push_back(code);
}
// Host target snapshot: every Navi (co-op), live Pikmin, live Teki including
// the shooter (its contact is suppressed only by the 1 s source grace).
// getCentre/getCentreSize and mGroundTriangle stand in for the P2 CollTree
// and mFloorTriangle (Rock.cpp:212).
void buildSnapshot(StoneSnapshot& snap){
    snap.targets.clear();snap.creatures.clear();snap.kinds.clear();
    if(naviMgr){Iterator it(naviMgr);CI_LOOP(it){snapshotAdd(snap,static_cast<Navi*>(*it),P2CannonStoneContactKind::NaviPiki,'n');}}
    if(pikiMgr){Iterator it(pikiMgr);CI_LOOP(it){snapshotAdd(snap,static_cast<Piki*>(*it),P2CannonStoneContactKind::NaviPiki,'p');}}
    if(tekiMgr){Iterator it(tekiMgr);CI_LOOP(it){snapshotAdd(snap,static_cast<Teki*>(*it),P2CannonStoneContactKind::Teki,'t');}}
}
const char* targetName(char code){return code=='n'?"navi":code=='p'?"piki":"teki";}
void stoneTick(StoneSnapshot& snap){
    buildSnapshot(snap);
    p2kabutostone::Strike strikes[64];p2kabutostone::DeadEvent deads[p2kabutostone::kFleetCapacity];p2kabutostone::Released rel[p2kabutostone::kFleetCapacity];
    int sn=0,dn=0,rn=0;
    fleet.tick(AICONST.mGravity(),&stoneTrace,&stoneMap,snap.targets.data(),int(snap.targets.size()),
        strikes,64,sn,deads,p2kabutostone::kFleetCapacity,dn,rel,p2kabutostone::kFleetCapacity,rn);
    // Receivers run only after the whole snapshot/tick, since they may change
    // actor state (snapshot-before-stimulate).
    for(int i=0;i<sn;++i){const auto& k=strikes[i];
        int idx=-1;for(size_t j=0;j<snap.targets.size();++j)if(snap.targets[j].token==k.target){idx=int(j);break;}
        if(idx<0)continue;
        Creature* target=snap.creatures[size_t(idx)];const char code=snap.kinds[size_t(idx)];
        const bool attack=k.kind==P2CannonStoneStrikeKind::Attack;
        // Press is attributed to the live shooter (Rock.cpp:213-218); a Teki
        // Attack to the Stone itself, which has no Creature here (Rock.cpp:222).
        Creature* owner=nullptr;
        if(!attack&&k.owner){auto sh=shooters.find(k.owner);if(sh!=shooters.end())owner=sh->second;}
        P2ProjectileEngineHit hit;
        if(target&&target->isAlive())hit=p2_projectile_apply_engine_strike(target,owner,attack,code=='t',k.damage);
        std::printf("P2_KABUTO_STONE_HIT generator=%u stone=%u kind=%s target=%s token=%llx damage=%.1f applied=%d health=%.1f->%.1f stored=%.1f->%.1f owner=%d t_flight=%.3f travel=%.1f\n",
            slotGen[k.slot],k.stone,attack?"Attack":"Press",targetName(code),static_cast<unsigned long long>(k.target),k.damage,int(hit.applied),
            hit.healthBefore,hit.healthAfter,hit.storedDamageBefore,hit.storedDamageAfter,int(owner!=nullptr),k.flight,k.travel);
    }
    for(int i=0;i<dn;++i){const auto& d=deads[i];
        std::printf("P2_KABUTO_STONE_DEAD generator=%u stone=%u reason=%s t_flight=%.3f travel=%.1f max_lateral=%.3f hits=%d closest=%.1f x=%.1f y=%.1f z=%.1f\n",
            slotGen[d.slot],d.stone,p2kabutostone::deadReasonName(d.reason),d.flight,d.travel,d.maxLateral,d.hits,
            d.closestNaviPiki<99999.0f?d.closestNaviPiki:99999.0f,d.pos.x,d.pos.y,d.pos.z);}
    for(int i=0;i<rn;++i){
        std::printf("P2_KABUTO_STONE_RELEASE generator=%u stone=%u slot=%d\n",slotGen[rel[i].slot],rel[i].stone,rel[i].slot);
        slotGen[rel[i].slot]=0;slotPosTicks[rel[i].slot]=0;}
    bool pos=false;
    for(int i=0;i<p2kabutostone::kFleetCapacity;++i){
        if(!fleet.used(i)||!fleet.stone(i).isAlive())continue;
        if(++slotPosTicks[i]%15!=0)continue;
        const P2CannonStone& st=fleet.stone(i);pos=true;
        std::printf("P2_KABUTO_STONE_POS generator=%u stone=%u x=%.1f y=%.1f z=%.1f scale=%.3f t=%.3f\n",slotGen[i],fleet.id(i),st.position().x,st.position().y,st.position().z,st.scale(),st.timer());
    }
    if(sn||dn||rn||pos)std::fflush(stdout);
}
}
void pc_p2_kabuto_fsm_update_stones(){
    if(fleet.active()==0){stoneDebt=0.0;return;}
    if(!gsys||!mapMgr||!mapMgr->mMapModel)return;
    // Same pause/movie gate as pc_p2_projectiles_update.
    const bool active=!gameflow.mPauseAll&&!gameflow.mIsUIOverlayActive&&!(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive);
    if(!active)return;
    const float dt=gsys->getFrameTime();if(!(dt>0.0f))return;
    stoneDebt+=dt;
    int ticks=static_cast<int>(stoneDebt/STONE_DT);
    if(ticks>STONE_MAX_TICKS){ticks=STONE_MAX_TICKS;stoneDebt=0.0;}
    else stoneDebt-=ticks*static_cast<double>(STONE_DT);
    static StoneSnapshot snap;
    for(int i=0;i<ticks&&fleet.active()>0;++i)stoneTick(snap);
}
// Visual stand-in: the retail Rock/Stone model is not staged, so the P1 Rolling
// Boulder (Iwagon, Beatle's P1 spawn type) mesh is drawn at the Stone's base,
// scaled from its 24-unit radius to the Stone's 40 x scale.
void pc_p2_kabuto_fsm_draw_stones(Graphics& gfx){
    if(fleet.active()==0||!gfx.mCamera)return;
    TekiShapeObject* so=tekiMgr?tekiMgr->getTekiShapeObject(TEKI_Iwagon):nullptr;
    Shape* shape=so?so->mShape:nullptr;
    if(!stoneDrawLogged){stoneDrawLogged=true;std::printf("P2_KABUTO_STONE_DRAW model=%s\n",shape?"iwagon_standin":"none");std::fflush(stdout);}
    if(!shape)return;
    gfx.setPerspective(gfx.mCamera->mPerspectiveMatrix.mMtx,gfx.mCamera->mFov,gfx.mCamera->mAspectRatio,gfx.mCamera->mNear,gfx.mCamera->mFar,1.f);
    gfx.useMaterial(nullptr);gfx.setDepth(true);
    for(int i=0;i<p2kabutostone::kFleetCapacity;++i){
        if(!fleet.used(i))continue;
        const P2CannonStone& st=fleet.stone(i);
        if(st.phase()!=P2CannonStonePhase::Move&&st.phase()!=P2CannonStonePhase::Dead)continue;
        const float k=(p2kabutostone::kContactRadiusFull/24.0f)*st.scale();
        Matrix4f world,view;
        world.makeSRT(Vector3f(k,k,k),Vector3f(0.0f,st.faceDir(),0.0f),Vector3f(st.position().x,st.position().y,st.position().z));
        gfx.mCamera->mLookAtMtx.multiplyTo(world,view);
        // Frame pinned so the shared Iwagon animation context is not advanced.
        float frame=0.0f;
        shape->updateAnim(gfx,view,&frame,nullptr);
        shape->drawshape(gfx,*gfx.mCamera,nullptr);
    }
}
