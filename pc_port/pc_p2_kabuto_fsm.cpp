// Kabuto 75 own identity on TEKI_Beatle: free larva FSM (Dead/Wait/Turn/Move/
// Flick/Attack) with mouth-joint stone fire (Stone 74, non-homing for 75).
// Bridge binds via campaign_ids + bind_source (onion:p2:75); P2 FSM decides
// every tick, host AI suppressed.
#include "pc_p2_kabuto_fsm.h"
#include "pc_p2_kabuto_fsm_policy.h"
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
#include "gl/pc_gfx.h"
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
    std::string clip="wait";float phase=0.0f;float logTimer=0.0f;float lastHealth=0.0f;
};
std::map<PelletView*,KabutoFsm> fsms;
bool ready=false;
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
// Source StateAttack KEYEVENT_2: createStoneAttack (mouth joint, Stone 74,
// homing only for Rkabuto) + rock emit effect. Port: cone InteractAttack with
// stone identity logged; 75 is non-homing.
int doStoneFire(BTeki* actor,KabutoFsm& s,unsigned gen){
    const Vector3f pos=actor->getPosition();
    const auto& p=p2kabutofsm::params();
    int hit=0;
    auto inCone=[&](const Vector3f& q){
        const float dx=q.x-pos.x,dz=q.z-pos.z;
        if(std::sqrt(dx*dx+dz*dz)>=p.attackRange)return false;
        return std::fabs(wrapPi(std::atan2(dx,dz)-s.heading))<ATTACK_ANGLE;
    };
    std::vector<Piki*> pikis;std::vector<Navi*> navis;
    if(pikiMgr){Iterator it(pikiMgr);CI_LOOP(it){Piki* q=static_cast<Piki*>(*it);if(!q||!q->isAlive()||!inCone(q->getPosition()))continue;pikis.push_back(q);if(pikis.size()>=6)break;}}
    if(naviMgr){Iterator it(naviMgr);CI_LOOP(it){Navi* n=static_cast<Navi*>(*it);if(!n||!n->isAlive()||!inCone(n->getPosition()))continue;navis.push_back(n);}}
    for(Piki* q:pikis){if(!q||!q->isAlive())continue;if(q->stimulate(InteractAttack(actor,nullptr,p.attackDamage,false)))++hit;}
    for(Navi* n:navis){if(!n||!n->isAlive())continue;if(n->stimulate(InteractAttack(actor,nullptr,p.attackDamage,false)))++hit;}
    std::printf("P2_KABUTO_FIRE generator=%u source_id=75 stone=74 homing=0 hit=%d\n",gen,hit);
    std::fflush(stdout);
    return hit;
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
void pc_p2_kabuto_fsm_reset(){actors.clear();fsms.clear();drawn.clear();drawnCorpse.clear();animated.clear();timing.clear();ready=false;}
void pc_p2_kabuto_fsm_forget(BTeki* a){auto* v=static_cast<PelletView*>(a);pc_randomizer_p2_forget_source(v);actors.erase(v);fsms.erase(v);drawn.erase(v);drawnCorpse.erase(v);}
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
    if(bridge){wanted.clear();for(unsigned id:pc_p2_campaign_ids(75))wanted[id]="Kabuto";}
    if(wanted.empty())return;
    std::set<unsigned> seen;
    Iterator it(tekiMgr);CI_LOOP(it){Teki* teki=static_cast<Teki*>(*it);if(!teki||!teki->mGenerator)continue;
        const unsigned token=bridge?pc_p2_campaign_token(teki):teki->mGenerator->_70;
        if(wanted.find(token)==wanted.end())continue;
        if(!seen.insert(token).second)std::abort();if(teki->mTekiType!=TEKI_Beatle)std::abort();
        actors[static_cast<PelletView*>(teki)]=true;
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
        if(!s.fireDone&&s.stateTime>=clipSeconds("attack")*0.5f){s.fireDone=true;doStoneFire(actor,s,gen);}
        if(s.stateTime>=clipSeconds("attack")){
            if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
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
