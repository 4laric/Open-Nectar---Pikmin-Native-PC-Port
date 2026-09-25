// P2 Blowhog identity: Fiery Blowhog (Tank 24, InteractFire) + Watery Blowhog
// (Wtank 25, InteractBubble) on the P1 TEKI_Tank vehicle. Ports the source
// TankState.cpp 7-state FSM (Dead/Wait/Move/MoveTurn/ChaseTurn/Attack/Flick)
// with the hoppe-emitter breath cone and flick shake-off. Bridge setup binds
// via campaign_ids + bind_source (onion:p2:24/25); the P2 FSM decides every
// tick and the P1 host AI is suppressed. Legacy visual-only preview path
// (p2-tank-visual.txt) is retained when no identity bank is staged.
#include "pc_p2_tank.h"
#include "pc_p2_tank_policy.h"
#include "pc_p2_tank_phase.h"
#include "pc_p2_campaign_actor.h"
#include "pc_randomizer.h"
#include "pc_p2_animation.h"
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
#include "gl/pc_gfx.h"
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <cstdio>
namespace {
// Legacy visual-only bank (preview, p2-tank-visual.txt).
const char* vnames[]={"dead","move1","flick","attack","waitact1","waitact2","type5"};
struct VClip{int duration;std::vector<int> frames;std::vector<Shape*> poses;};
VClip vclips[7];std::map<BTeki*,unsigned> vactors;std::set<BTeki*> vlogged;
Shape* water=nullptr;unsigned waterId=0;float xyz[3]={},yaw=0;bool waterLogged=false;size_t vbytesTotal=0;
void vfail(){std::fputs("P2_TANK_VISUAL invalid profile\n",stderr);std::abort();}
Shape* vload(const std::string& name){
 std::ifstream file("assets/dataDir/courses/pikmin2room/"+name,std::ios::binary|std::ios::ate);if(!file)vfail();auto size=file.tellg();if(size<=0||size>16*1024*1024||vbytesTotal+size_t(size)>64*1024*1024)vfail();vbytesTotal+=size_t(size);file.seekg(0);
 std::vector<unsigned char> data(size_t(size),0),resources;if(!file.read(reinterpret_cast<char*>(data.data()),size)||!p2animation::resources(data,resources))vfail();
 Shape* shape=gameflow.loadShape(("courses/pikmin2room/"+name).c_str(),true);if(!shape)vfail();for(int t=0;t<shape->mTexAttrCount;++t)if(shape->mTexAttrList[t].mTexture)shape->mTexAttrList[t].mTexture->attach();return shape;
}
int vmotion(int native){switch(native){case TekiMotion::Move1:return 1;case TekiMotion::Flick:return 2;case TekiMotion::Attack:return 3;case TekiMotion::WaitAct1:return 4;case TekiMotion::WaitAct2:return 5;default:return -1;}}
// Identity FSM.
std::map<PelletView*,int> actors;
const char* ids[]={"Tank","Wtank"};
std::map<std::string,std::vector<Shape*>> animated[2];
std::map<std::string,p2animation::Clip> timing[2];
std::set<PelletView*> drawn,drawnCorpse;
enum TState { TNK_DEAD=0,TNK_WAIT=1,TNK_MOVE=2,TNK_MOVETURN=3,TNK_CHASETURN=4,TNK_ATTACK=5,TNK_FLICK=6 };
const float PI_F=3.14159265f;
constexpr float HOME_RADIUS=15.0f;
constexpr float TERRITORY=250.0f;
constexpr float TURN_RATE=2.0f;
constexpr float CHASE_TURN_RATE=3.5f;
constexpr float ATTACK_ANGLE=0.6f;
constexpr float FACE_OK_ANGLE=0.174533f;
constexpr int FLICK_STUCK_MIN=3;
constexpr float BREATH_TICK_S=1.0f/30.0f;
struct TankFsm {
    int kind=0;TState state=TNK_WAIT;float stateTime=0.0f;float heading=0.0f;
    Vector3f home;Vector3f targetPos;bool targetValid=false;
    float groundY=0.0f;bool blowing=false;bool breathDone=false;bool flickDone=false;bool escaped=false;
    unsigned rng=1;unsigned token=0;bool deadLogged=false;float deathPrior=0.0f;bool deathPriorSet=false;
    std::string clip="waitact1";float phase=0.0f;float logTimer=0.0f;float lastHealth=0.0f;
    float blowTimer=0.0f;
};
std::map<PelletView*,TankFsm> fsms;
bool ready=false;
unsigned nextRand(TankFsm& s){s.rng=s.rng*1664525u+1013904223u;return s.rng>>8;}
float wrapPi(float a){while(a>PI_F)a-=2.0f*PI_F;while(a<-PI_F)a+=2.0f*PI_F;return a;}
float distXZ(const Vector3f& a,const Vector3f& b){const float dx=a.x-b.x,dz=a.z-b.z;return std::sqrt(dx*dx+dz*dz);}
float clipSeconds(int kind,const std::string& name){auto it=timing[kind].find(name);return it==timing[kind].end()?1.0f:it->second.duration/30.0f;}
void loadAnimation(std::vector<p2animation::Clip> (&banks)[2]){
    size_t total=0;
    for(int kind=0;kind<2;++kind){std::vector<unsigned char> reference;
        for(const auto& clip:banks[kind]){size_t clipBytes=0;
            for(int i=0;i<clip.count;++i){char path[192];std::snprintf(path,sizeof(path),"assets/dataDir/courses/pikmin2room/tank_%s_%s_%02d.mod",ids[kind],clip.name.c_str(),i);
                std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)std::abort();auto size=file.tellg();
                if(size<=0||size>512*1024)std::abort();clipBytes+=size_t(size);total+=size_t(size);
                if(clipBytes>512*1024||total>10*1024*1024)std::abort();file.seekg(0);
                std::vector<unsigned char> bytes(size_t(size),0),resources;
                if(!file.read(reinterpret_cast<char*>(bytes.data()),size)||!p2animation::resources(bytes,resources))std::abort();
                if(!reference.empty()&&reference!=resources)std::abort();reference=resources;
            }
        }
    }
    for(int kind=0;kind<2;++kind){Shape* shared=nullptr;
        for(const auto& clip:banks[kind]){timing[kind][clip.name]=clip;
            for(int i=0;i<clip.count;++i){char path[160];std::snprintf(path,sizeof(path),"courses/pikmin2room/tank_%s_%s_%02d.mod",ids[kind],clip.name.c_str(),i);
                Shape* shape=gameflow.loadShape(path,true);if(!shape)std::abort();
                if(!shared){shared=shape;for(int t=0;t<shape->mTexAttrCount;++t)if(shape->mTexAttrList[t].mTexture)shape->mTexAttrList[t].mTexture->attach();}
                else{
                    if(shape->mMaterialCount!=shared->mMaterialCount||shape->mTexAttrCount!=shared->mTexAttrCount||shape->mTevInfoCount!=shared->mTevInfoCount)std::abort();
                    for(int j=0;j<shape->mTotalMatpolyCount;++j){auto* poly=shape->mMatpolyList[j];if(!poly||!poly->mMaterial)continue;int material=-1;
                        for(int m=0;m<shape->mMaterialCount;++m)if(poly->mMaterial==&shape->mMaterialList[m])material=m;
                        if(material<0)std::abort();poly->mMaterial=&shared->mMaterialList[material];}
                    shape->mMaterialList=shared->mMaterialList;shape->mTexAttrList=shared->mTexAttrList;shape->mTevInfoList=shared->mTevInfoList;
                }
                animated[kind][clip.name].push_back(shape);
            }
        }
    }
    std::printf("P2_TANK_BANK_READY mod_bytes=%zu gameplay=P1_unchanged\n",total);
}
void stop(BTeki* a){a->inputDrive(Vector3f(0.0f,0.0f,0.0f));a->mVelocity.x=0.0f;a->mVelocity.y=0.0f;a->mVelocity.z=0.0f;}
void walkTo(BTeki* a,TankFsm& s,const Vector3f& target,float speed,float dt){
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
void turnTo(BTeki* a,TankFsm& s,const Vector3f& target,float dt,float rate){
    const Vector3f pos=a->getPosition();
    const float desired=std::atan2(target.x-pos.x,target.z-pos.z);
    const float maxTurn=rate*dt;
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
int stuckPikminCount(Creature* creature){
    int n=0;
    for(Creature* s=creature->mStickListHead;s;s=s->mNextSticker){
        if(!s||!s->isPiki()||!s->isAlive())continue;
        ++n;
    }
    return n;
}
bool attackable(const TankFsm& s,const Vector3f& pos,const Creature* target,float range){
    if(!target)return false;const Vector3f tp=target->getPosition();
    if(distXZ(pos,tp)>=range)return false;
    const float ang=std::fabs(wrapPi(std::atan2(tp.x-pos.x,tp.z-pos.z)-s.heading));
    return ang<ATTACK_ANGLE;
}
bool shouldFlick(BTeki* actor){return stuckPikminCount(actor)>=FLICK_STUCK_MIN;}
// Source StateAttack: while mIsBlowing, isAttackable(true) + discharge per tick.
// Hoppe-emitter cone: face-dir cone of attackRange/attackRadius; Tank uses
// InteractFire, Wtank uses InteractBubble (Ftank.cpp/Wtank.cpp interactCreature).
int doBreath(BTeki* actor,TankFsm& s){
    const Vector3f pos=actor->getPosition();
    const p2tank::Params& p=p2tank::params(s.kind);
    int hit=0;
    auto inCone=[&](const Vector3f& q){
        const float dx=q.x-pos.x,dz=q.z-pos.z;
        const float d=std::sqrt(dx*dx+dz*dz);
        if(d>=p.attackRange+p.attackRadius)return false;
        const float ang=std::fabs(wrapPi(std::atan2(dx,dz)-s.heading));
        return ang<ATTACK_ANGLE||d<p.attackRadius;
    };
    // Snapshot targets first: InteractBubble can kill (remove from pikiMgr)
    // during stimulate, invalidating the live iterator (crash in Wtank runs).
    std::vector<Piki*> pikis;std::vector<Navi*> navis;
    if(pikiMgr){Iterator it(pikiMgr);CI_LOOP(it){Piki* q=static_cast<Piki*>(*it);if(!q||!q->isAlive())continue;
        if(inCone(q->getPosition()))pikis.push_back(q);if(pikis.size()>=6)break;}}
    if(naviMgr){Iterator it(naviMgr);CI_LOOP(it){Navi* n=static_cast<Navi*>(*it);if(!n||!n->isAlive())continue;
        if(inCone(n->getPosition()))navis.push_back(n);}}
    for(Piki* q:pikis){if(!q||!q->isAlive())continue;bool ok=false;
        if(s.kind==0)ok=q->stimulate(InteractFire(actor,p.attackDamage));
        else ok=q->stimulate(InteractBubble(actor,p.attackDamage));
        if(ok)++hit;}
    for(Navi* n:navis){if(!n||!n->isAlive())continue;bool ok=false;
        if(s.kind==0)ok=n->stimulate(InteractFire(actor,p.attackDamage));
        else ok=n->stimulate(InteractBubble(actor,p.attackDamage));
        if(ok)++hit;}
    return hit;
}
// Source StateFlick: flickNearbyNavi + flickNearbyPikmin + flickStickPikmin.
int doFlick(BTeki* actor,TankFsm& s){
    const Vector3f pos=actor->getPosition();
    const p2tank::Params& p=p2tank::params(s.kind);
    int hit=0;
    std::vector<Piki*> pikis;
    if(pikiMgr){Iterator it(pikiMgr);CI_LOOP(it){Piki* q=static_cast<Piki*>(*it);if(!q||!q->isAlive())continue;
        if(distXZ(q->getPosition(),pos)<p.flickRange)pikis.push_back(q);}}
    for(Piki* q:pikis){if(!q||!q->isAlive())continue;
        if(q->stimulate(InteractFlick(actor,300.0f,0.0f,FLICK_BACKWARDS_ANGLE)))++hit;}
    if(naviMgr){Navi* n=naviMgr->getNavi();if(n&&n->isAlive()&&distXZ(n->getPosition(),pos)<p.flickRange)
        if(n->stimulate(InteractFlick(actor,300.0f,0.0f,FLICK_BACKWARDS_ANGLE)))++hit;}
    return hit;
}
void setPhase(int kind,TankFsm& s){
    const float dur=clipSeconds(kind,s.clip);
    float ph=s.stateTime/dur;
    if(ph>1.0f)ph=1.0f;
    s.phase=ph;
}
void transition(BTeki* actor,TankFsm& s,TState st,const char* clip,unsigned gen){
    (void)actor;s.state=st;s.stateTime=0.0f;s.blowing=false;s.breathDone=false;s.flickDone=false;s.blowTimer=0.0f;if(clip)s.clip=clip;
    std::printf("P2_TANK_STATE species=%s generator=%u state=%s\n",ids[s.kind],gen,p2tank::stateName(st));
    std::fflush(stdout);
}
void die(BTeki* actor,TankFsm& s,unsigned gen,float priorHealth){
    if(!s.deadLogged){s.deadLogged=true;const unsigned sourceId=s.kind?25u:24u;std::printf("P2_TANK_DEAD species=%s generator=%u source_id=%u health=0 prior_health=%.1f\n",ids[s.kind],gen,sourceId,priorHealth);std::fflush(stdout);}
    transition(actor,s,TNK_DEAD,"dead",gen);
}
}
void pc_p2_tank_reset(){vactors.clear();vlogged.clear();for(auto& c:vclips)c=VClip{};water=nullptr;waterLogged=false;vbytesTotal=0;actors.clear();fsms.clear();drawn.clear();drawnCorpse.clear();for(auto& b:animated)b.clear();for(auto& b:timing)b.clear();ready=false;}
void pc_p2_tank_forget(BTeki* actor){vactors.erase(actor);vlogged.erase(actor);auto* v=static_cast<PelletView*>(actor);pc_randomizer_p2_forget_source(v);actors.erase(v);fsms.erase(v);drawn.erase(v);drawnCorpse.erase(v);}
float pc_p2_tank_param_f(const BTeki* actor,int idx,float fallback){
    auto i=actors.find(static_cast<PelletView*>(const_cast<BTeki*>(actor)));if(i==actors.end())return fallback;
    const p2tank::Params& p=p2tank::params(i->second);
    switch(idx){
    case TPF_Life:return p.health;
    case TPF_LifeRecoverRate:return 0.0f;
    case TPF_VisibleRange:return p.sight;
    case TPF_AttackableRange:return p.attackRange;
    case TPF_AttackPower:return p.attackDamage;
    default:return fallback;
    }
}
bool pc_p2_tank_suppress_ai(const BTeki* actor){return ready&&actors.count(static_cast<PelletView*>(const_cast<BTeki*>(actor)))!=0;}
void pc_p2_tank_setup(){
 pc_p2_tank_reset();
 std::printf("P2_TANK_SETUP\n");std::fflush(stdout);
 const bool bridge=pc_randomizer_p2_bridge()&&!pc_pikipelago_room_preview();
 const bool preview=pc_pikipelago_room_preview();
 // Identity path first (p2-tank.txt); bridge or preview both serve it.
 {
  std::ifstream input("p2-tank.txt");
  if(input){
   std::map<unsigned,int> wanted;std::vector<p2animation::Clip> banks[2];
   if(!p2tank::parse(input,wanted,banks))std::abort();
   if(bridge){wanted.clear();for(unsigned id:pc_p2_campaign_ids(24))wanted[id]=0;for(unsigned id:pc_p2_campaign_ids(25))wanted[id]=1;}
   if(!wanted.empty()){
    std::set<unsigned> seen;
    Iterator it(tekiMgr);CI_LOOP(it){Teki* teki=static_cast<Teki*>(*it);if(!teki||!teki->mGenerator)continue;
     const unsigned token=bridge?pc_p2_campaign_token(teki):teki->mGenerator->_70;
     auto found=wanted.find(token);if(found==wanted.end())continue;
     int kind=found->second;if(!seen.insert(found->first).second)std::abort();if(teki->mTekiType!=TEKI_Tank)std::abort();
     actors[static_cast<PelletView*>(teki)]=kind;
     teki->mHealth=p2tank::params(kind).health;
     TankFsm& f=fsms[static_cast<PelletView*>(teki)];
     f.kind=kind;f.home=teki->getPosition();f.heading=teki->getDirection();
     f.targetPos=f.home;f.targetValid=true;f.rng=(token*2654435761u)|1u;f.token=token;
     f.state=TNK_WAIT;f.clip="waitact1";f.phase=0.0f;f.lastHealth=teki->mHealth;
     const unsigned sourceId=kind?25u:24u;
     if(bridge){pc_randomizer_p2_bind_source(static_cast<PelletView*>(teki),sourceId,token);std::printf("P2_TANK_DELIVERY_BIND generator=%u source_id=%u\n",token,sourceId);}
     std::printf("P2_TANK_BIND generator=%u source_id=%u visual_only=0\n",token,sourceId);
     std::printf("P2_TANK_READY species=%s generator=%u health=%.1f max_health=%.1f behavior=source_fsm rewards=P1_unchanged\n",ids[kind],token,teki->mHealth,teki->getParameterF(TPF_Life));
     std::printf("P2_ENEMY_READY species=%s native_family=Tank generator=%u x=%.7f y=%.7f z=%.7f health=%.1f max_health=%.1f behavior=native source_FSM=implemented\n",ids[kind],token,teki->getPosition().x,teki->getPosition().y,teki->getPosition().z,teki->mHealth,p2tank::params(kind).health);
     std::printf("P2_TANK_STATE species=%s generator=%u state=wait\n",ids[kind],token);
     std::fflush(stdout);
    }
    if(seen.size()!=wanted.size()){std::printf("P2_TANK_ERROR missing_actor wanted=%zu found=%zu\n",wanted.size(),seen.size());std::abort();}
    loadAnimation(banks);ready=true;return;
   }
  }
 }
 // Legacy visual-only preview fallback.
 if(!preview)return;std::ifstream in("p2-tank-visual.txt");if(!in)return;std::string word;if(!(in>>word)||word!="P2_TANK_VISUAL_1"||!tekiMgr)vfail();
 for(int k=0;k<7;++k){auto& c=vclips[k];int count;if(!(in>>word>>c.duration>>count)||word!=vnames[k]||c.duration<2||c.duration>10000||count<2||count>40)vfail();for(int i=0;i<count;++i){int frame;if(!(in>>frame)||frame<0||frame>=c.duration||(i&&frame<=c.frames.back()))vfail();c.frames.push_back(frame);}if(c.frames.front()!=0||c.frames.back()!=c.duration-1)vfail();}
 int count;if(!(in>>count)||count<1||count>8)vfail();std::set<unsigned>wanted,found;
 for(int i=0;i<count;++i){unsigned long long id;int type;if(!(in>>id>>type)||id>0xffffffffULL||type!=TEKI_Tank||!wanted.insert(unsigned(id)).second)vfail();}
 int display;if(!(in>>display)||display<0||display>1)vfail();
 if(display){unsigned long long id;if(!(in>>id>>xyz[0]>>xyz[1]>>xyz[2]>>yaw)||id>0xffffffffULL||wanted.count(unsigned(id)))vfail();waterId=unsigned(id);for(float v:xyz)if(!std::isfinite(v)||std::fabs(v)>100000)vfail();if(!std::isfinite(yaw)||std::fabs(yaw)>360)vfail();}
 if(in>>word)vfail();Iterator it(tekiMgr);CI_LOOP(it){Teki* actor=static_cast<Teki*>(*it);if(!actor||!actor->mGenerator||!wanted.count(actor->mGenerator->_70))continue;unsigned id=actor->mGenerator->_70;if(actor->mTekiType!=TEKI_Tank||!found.insert(id).second)vfail();vactors.emplace(actor,id);}
 if(found!=wanted)vfail();
 for(int k=0;k<7;++k)for(size_t i=0;i<vclips[k].frames.size();++i){char file[96];std::snprintf(file,sizeof(file),"tank_fire_%s_%02u.mod",vnames[k],unsigned(i));vclips[k].poses.push_back(vload(file));}
 if(display){water=vload("tank_water_static.mod");std::printf("P2_WTANK_DISPLAY_READY display=%u xyz=%.6f,%.6f,%.6f noninteractive_static_no_actor_no_collision_no_receiver\n",waterId,xyz[0],xyz[1],xyz[2]);}
 for(auto& entry:vactors){auto* actor=entry.first;std::printf("P2_TANK_PROXY_READY generator=%u native_type=15 xyz=%.6f,%.6f,%.6f behavior=P1_fire_proxy\n",entry.second,actor->mSRT.t.x,actor->mSRT.t.y,actor->mSRT.t.z);}
}
void pc_p2_tank_update(BTeki* actor){
    if(!ready)return;
    auto it=actors.find(static_cast<PelletView*>(actor));if(it==actors.end())return;
    auto ft=fsms.find(static_cast<PelletView*>(actor));if(ft==fsms.end())return;
    TankFsm& s=ft->second;
    const float dt=gsys->getFrameTime();if(dt<=0.0f||dt>0.5f)return;
    const Vector3f pos=actor->getPosition();
    const unsigned live=actor->mGenerator?pc_p2_campaign_token(actor):0u;
    if(live)s.token=live;
    const unsigned gen=s.token?s.token:live;
    const unsigned sourceId=s.kind?25u:24u;
    const p2tank::Params& p=p2tank::params(s.kind);
    if(actor->mStoredDamage>0.0f)actor->makeDamaged();
    const float previousHealth=s.lastHealth;
    if(actor->mHealth<s.lastHealth&&actor->mHealth>0.0f){
        std::printf("P2_TANK_DAMAGE generator=%u source_id=%u health=%.1f\n",gen,sourceId,actor->mHealth);
        std::fflush(stdout);
    }
    if(actor->mHealth<=0.0f&&!s.deathPriorSet&&previousHealth>0.0f){s.deathPrior=previousHealth;s.deathPriorSet=true;}
    const float priorForDeath=s.deathPriorSet?s.deathPrior:previousHealth;
    s.lastHealth=actor->mHealth;
    s.stateTime+=dt;
    switch(s.state){
    case TNK_WAIT:{
        stop(actor);
        if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
        if(shouldFlick(actor)){s.targetPos=pos;s.targetValid=true;transition(actor,s,TNK_FLICK,"flick",gen);break;}
        if(s.stateTime>=clipSeconds(s.kind,s.clip)){
            s.stateTime=0.0f;
            Creature* t=nearestTarget(pos,p.sight);
            if(t){s.targetPos=t->getPosition();s.targetValid=true;
                if(attackable(s,pos,t,p.attackRange))transition(actor,s,TNK_ATTACK,"attack",gen);
                else if(std::fabs(wrapPi(std::atan2(s.targetPos.x-pos.x,s.targetPos.z-pos.z)-s.heading))>FACE_OK_ANGLE)
                    transition(actor,s,TNK_MOVETURN,"waitact1",gen);
            }
        }
        break;
    }
    case TNK_MOVETURN:{
        stop(actor);
        if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
        if(shouldFlick(actor)){transition(actor,s,TNK_FLICK,"flick",gen);break;}
        Creature* t=nearestTarget(pos,p.sight);
        if(t){turnTo(actor,s,t->getPosition(),dt,TURN_RATE);
            if(attackable(s,pos,t,p.attackRange)){s.targetPos=t->getPosition();s.targetValid=true;transition(actor,s,TNK_ATTACK,"attack",gen);}
            else if(std::fabs(wrapPi(std::atan2(t->getPosition().x-pos.x,t->getPosition().z-pos.z)-s.heading))<=FACE_OK_ANGLE)transition(actor,s,TNK_MOVE,"move1",gen);
            else if(s.stateTime>=clipSeconds(s.kind,"waitact1"))transition(actor,s,TNK_MOVE,"move1",gen);
        } else transition(actor,s,TNK_WAIT,"waitact1",gen);
        break;
    }
    case TNK_MOVE:{
        if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
        if(shouldFlick(actor)){transition(actor,s,TNK_FLICK,"flick",gen);break;}
        Creature* t=nearestTarget(pos,p.sight);
        if(t){s.targetPos=t->getPosition();s.targetValid=true;
            if(attackable(s,pos,t,p.attackRange)){transition(actor,s,TNK_ATTACK,"attack",gen);break;}
            walkTo(actor,s,s.targetPos,p.moveSpeed,dt);
            if(distXZ(pos,s.home)>TERRITORY)transition(actor,s,TNK_CHASETURN,"waitact1",gen);
        } else {
            walkTo(actor,s,s.home,p.moveSpeed,dt);
            if(distXZ(pos,s.home)<HOME_RADIUS)transition(actor,s,TNK_WAIT,"waitact1",gen);
        }
        break;
    }
    case TNK_CHASETURN:{
        stop(actor);
        if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
        if(shouldFlick(actor)){transition(actor,s,TNK_FLICK,"flick",gen);break;}
        Creature* t=nearestTarget(pos,p.sight);
        if(t){turnTo(actor,s,t->getPosition(),dt,CHASE_TURN_RATE);
            if(attackable(s,pos,t,p.attackRange)){s.targetPos=t->getPosition();s.targetValid=true;transition(actor,s,TNK_ATTACK,"attack",gen);}
            else if(std::fabs(wrapPi(std::atan2(t->getPosition().x-pos.x,t->getPosition().z-pos.z)-s.heading))<=FACE_OK_ANGLE||s.stateTime>=clipSeconds(s.kind,"waitact1"))
                transition(actor,s,TNK_MOVE,"move1",gen);
        } else transition(actor,s,TNK_MOVE,"move1",gen);
        break;
    }
    case TNK_ATTACK:{
        stop(actor);
        if(!s.blowing){s.blowing=true;s.blowTimer=0.0f;std::printf("P2_TANK_BREATH species=%s generator=%u source_id=%u start=1\n",ids[s.kind],gen,sourceId);std::fflush(stdout);}
        s.blowTimer+=dt;
        {
            int hit=doBreath(actor,s);
            if(hit>0)std::printf("P2_TANK_BREATH_HIT species=%s generator=%u source_id=%u hit=%d\n",ids[s.kind],gen,sourceId,hit),std::fflush(stdout);
        }
        if(s.stateTime>=clipSeconds(s.kind,"attack")){
            std::printf("P2_TANK_BREATH species=%s generator=%u source_id=%u start=0\n",ids[s.kind],gen,sourceId);std::fflush(stdout);
            if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
            if(shouldFlick(actor))transition(actor,s,TNK_FLICK,"flick",gen);
            else if(distXZ(pos,s.home)>TERRITORY)transition(actor,s,TNK_CHASETURN,"waitact1",gen);
            else transition(actor,s,TNK_WAIT,"waitact1",gen);
        }
        break;
    }
    case TNK_FLICK:{
        stop(actor);
        if(!s.flickDone){s.flickDone=true;int hit=doFlick(actor,s);std::printf("P2_TANK_FLICK species=%s generator=%u source_id=%u hit=%d\n",ids[s.kind],gen,sourceId,hit);std::fflush(stdout);}
        if(s.stateTime>=clipSeconds(s.kind,"flick")){
            if(actor->mHealth<=0.0f){die(actor,s,gen,priorForDeath);break;}
            Creature* t=nearestTarget(pos,p.sight);
            if(t&&attackable(s,pos,t,p.attackRange)){s.targetPos=t->getPosition();s.targetValid=true;transition(actor,s,TNK_ATTACK,"attack",gen);}
            else transition(actor,s,TNK_WAIT,"waitact1",gen);
        }
        break;
    }
    case TNK_DEAD:{
        stop(actor);
        if(!s.escaped&&s.stateTime>=clipSeconds(s.kind,"dead")){s.escaped=true;actor->pcEscapeNow();}
        break;
    }
    default:break;
    }
    setPhase(s.kind,s);
    s.logTimer+=dt;
    if(s.logTimer>=1.0f){s.logTimer=0.0f;const Vector3f now=actor->getPosition();
        std::printf("P2_TANK_FSM_POS species=%s generator=%u state=%s x=%.2f y=%.2f z=%.2f health=%.1f\n",
            ids[s.kind],gen,p2tank::stateName(s.state),now.x,now.y,now.z,actor->mHealth);std::fflush(stdout);}
}
bool pc_p2_tank_probe(const BTeki* actor,const char** state,const char** clip,float* phase){
    auto* view=static_cast<PelletView*>(const_cast<BTeki*>(actor));
    if(!actors.count(view))return false;
    auto ft=fsms.find(view);if(ft==fsms.end())return false;
    if(state)*state=p2tank::stateName(ft->second.state);
    if(clip)*clip=ft->second.clip.c_str();
    if(phase)*phase=ft->second.phase;
    return true;
}
bool pc_p2_tank_draw(BTeki* actor,Graphics& gfx,const Matrix4f& view,bool corpse){
 if(ready){
  auto it=actors.find(static_cast<PelletView*>(actor));if(it!=actors.end()){
   int kind=it->second;
   {
    auto* v=static_cast<PelletView*>(actor);
    auto ftok=fsms.find(v);
    const unsigned liveTok=actor->mGenerator?pc_p2_campaign_token(actor):0u;
    const unsigned token=liveTok?liveTok:(ftok!=fsms.end()?ftok->second.token:0u);
    const unsigned sourceId=kind?25u:24u;
    if(drawn.insert(v).second&&!corpse){
        std::printf("P2_TANK_DRAW generator=%u source_id=%u species=%s corpse=%d\n",token,sourceId,ids[kind],int(corpse));
        std::fflush(stdout);
    }
    if(corpse&&drawnCorpse.insert(v).second){
        std::printf("P2_TANK_CORPSE_DRAW generator=%u source_id=%u species=%s\n",token,sourceId,ids[kind]);
        std::fflush(stdout);
    }
   }
   auto ft=fsms.find(static_cast<PelletView*>(actor));
   const char* name=corpse?"dead":(ft!=fsms.end()?ft->second.clip.c_str():p2tank::motionClip(actor->mTekiAnimator->getCurrentMotionIndex()));
   Shape* shape=animated[kind].at("waitact1").front();
   if(name){
       float phase=corpse?1.0f:(ft!=fsms.end()?ft->second.phase:0.0f);
       shape=animated[kind].at(name).at(timing[kind].at(name).index(phase,corpse));
   }
   shape->updateAnim(gfx,view,nullptr,actor);
   pc_gfx_specular_family_scope(1);
   shape->drawshape(gfx,*gfx.mCamera,nullptr);
   pc_gfx_specular_family_scope(0);
   return true;
  }
 }
 if(!vactors.count(actor)||!actor->isAlive()||!gfx.mCamera||!actor->mTekiAnimator)return false;auto* anim=actor->mTekiAnimator;int k=vmotion(anim->getCurrentMotionIndex()),count=anim->getFrameCount();float counter=anim->getCounter();if(k<0||count<2||!std::isfinite(counter))return false;
 auto& c=vclips[k];float frame;if(!p2tankvisual::frame(counter,count,c.duration,frame))return false;size_t best=0;for(size_t i=1;i<c.frames.size();++i)if(std::fabs(c.frames[i]-frame)<std::fabs(c.frames[best]-frame))best=i;
 Shape* shape=c.poses[best];shape->updateAnim(gfx,view,nullptr,actor);shape->drawshape(gfx,*gfx.mCamera,nullptr);if(vlogged.insert(actor).second)std::printf("P2_TANK_PROXY_DRAW generator=%u source_clip=%s P1_gameplay_unchanged\n",vactors[actor],vnames[k]);return true;
}
void pc_p2_tank_draw_water(Graphics& gfx){
 if(!water||!gfx.mCamera)return;gfx.setPerspective(gfx.mCamera->mPerspectiveMatrix.mMtx,gfx.mCamera->mFov,gfx.mCamera->mAspectRatio,gfx.mCamera->mNear,gfx.mCamera->mFar,1.f);gfx.useMaterial(nullptr);gfx.setDepth(true);
 Matrix4f world,view;world.makeSRT(Vector3f(1,1,1),Vector3f(0,yaw*.0174532925199433f,0),Vector3f(xyz[0],xyz[1],xyz[2]));gfx.mCamera->mLookAtMtx.multiplyTo(world,view);water->updateAnim(gfx,view,nullptr,nullptr);water->drawshape(gfx,*gfx.mCamera,nullptr);
 if(!waterLogged){std::puts("P2_WTANK_DISPLAY_DRAW noninteractive_static_no_actor_no_collision_no_receiver");waterLogged=true;}
}
