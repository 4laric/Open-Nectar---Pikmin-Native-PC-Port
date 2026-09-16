"""UmiMushi71 observer fixture source (shard lane, issue #374).

Tracks P2_UMIMUSHI_* natural death/corpse + rebirth on a live bound actor
(generator 374004, source 71; Blind 374006/101 bound alongside). Family
modules stay untouched.

// MUSE-UMIMUSHI-INCLUDES-BEGIN
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "Demo.h"
#include "GameStat.h"
#include "GameStat.h"
#include "Generator.h"
#include "Interactions.h"
#include "ItemMgr.h"
#include "ObjType.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "Pellet.h"
#include "PelletView.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiAI.h"
#include "PikiState.h"
#include "PlayerState.h"
#include "TekiPersonality.h"
// Vendored verbatim from scripts/p2_fixture_captain_guard.h (sha256
// d2f678c9eda75e151eb534077dff9e30ad36ae4796881d971bbd09945f3c3474,
// equivalent tested guard, observation-only.
inline bool p2_fixture_captain_down(bool orimaDead, bool deadState, float hp) {
    return orimaDead || deadState || !std::isfinite(hp) || hp <= 1.0f;
}
inline void p2_fixture_require_captain(bool orimaDead, bool deadState, float hp, int tick) {
    if (!p2_fixture_captain_down(orimaDead, deadState, hp)) return;
    std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f orima_dead=%d dead_state=%d outcome=BLOCKED\n",
                tick, hp, int(orimaDead), int(deadState));
    std::fflush(nullptr);
    std::_Exit(86); // interrupted observation, never a successful fixture exit
}
// MUSE-UMIMUSHI-INCLUDES-END

// MUSE-UMIMUSHI-APP-BEGIN
class RoomApp : public PlugPikiApp {
 int observed=0,frames=0,throws=0,lastThrow=-10000,deathTick=-1;
 bool staged=false,deadSeen=false,funneled=false,goneSeen=false,corpseFound=false,rebound=false;
 Teki* umi=nullptr;
 void* stalePtr=nullptr;
 int alivePikis(){int c=0;Iterator it(pikiMgr);CI_LOOP(it){Creature* p=*it;if(p&&p->isAlive())++c;}return c;}
 bool actorPresent(Teki* a){Iterator it(tekiMgr);CI_LOOP(it){if(static_cast<Teki*>(*it)==a)return true;}return false;}
 public:int idle() override {
  int result=PlugPikiApp::idle();require(++frames<60000,"umimushi observer timeout");
  if(!naviMgr||!tekiMgr||!pikiMgr)return result;
  Navi* n=naviMgr->getNavi();if(!n)return result;
  p2_fixture_require_captain(GameStat::orimaDead,!n->isAlive(),n->mHealth,observed);
  if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
  if(!pc_p2_preview_cargo_free_ready())return result;
  if(gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
  ++observed;
  if(observed==1){
   for(int i=0;i<DEMOFLAG_COUNT;++i)playerState->mDemoFlags.setFlagOnly(i); // suppress one-shot discovery cutscenes
   std::ifstream input("umimushi-positions.txt");unsigned id;float x,y,z;int count=0;
   while(input>>id>>x>>y>>z){
    Teki* actor=nullptr;int matches=0;Iterator iter(tekiMgr);CI_LOOP(iter){Teki* a=static_cast<Teki*>(*iter);if(a->mGenerator&&a->mGenerator->_70==id){actor=a;++matches;}}
    require(matches==1,"umimushi roster identity");
    Vector3f birth=actor->mPersonality->mPosition;
    require(std::fabs(birth.x-x)<.02&&std::fabs(birth.y-y)<.02&&std::fabs(birth.z-z)<.02,"umimushi birth XYZ");
    std::printf("P2_UMIMUSHI_BIRTH id=%u type=%d x=%.3f y=%.3f z=%.3f\n",id,actor->mTekiType,birth.x,birth.y,birth.z);
    if(id==374004)umi=actor;++count;}
   require(count>=1&&umi,"umimushi TARGET missing");
   std::ifstream stale("umimushi-pass1-ptr.txt");unsigned long long v=0;
   if(stale>>std::hex>>v){stalePtr=reinterpret_cast<void*>(v);rebound=true;
    std::printf("P2_UMIMUSHI_REBOUND stale=0x%llx fresh=0x%llx generator=374004\n",(unsigned long long)stalePtr,(unsigned long long)umi);std::fflush(stdout);
    require(stalePtr!=umi,"stale/fresh pointer identical after stage boundary");}
   else{
    Vector3f b=umi->getPosition();
    Vector3f stagedPos(b.x+400.0f,b.y,b.z);
    n->resetPosition(stagedPos);n->mVelocity.set(0,0,0);n->mTargetVelocity.set(0,0,0);
    staged=true;
    std::printf("P2_UMIMUSHI_THROW_STAGED nx=%.3f ny=%.3f nz=%.3f bx=%.3f by=%.3f bz=%.3f\n",stagedPos.x,stagedPos.y,stagedPos.z,b.x,b.y,b.z);
    std::ofstream ptr("umimushi-pass1-ptr.txt");ptr<<std::hex<<(unsigned long long)umi;ptr.close();}
   std::fflush(stdout);}
  if(observed==60){int squad=alivePikis();std::printf("P2_UMIMUSHI_SQUAD pikis=%d\n",squad);std::fflush(stdout);require(squad==20,"starting squad size");}
  if(umi&&!deadSeen&&umi->isAlive()&&observed%200==0){Vector3f vp=umi->getPosition();float vg=mapMgr?mapMgr->getMinY(vp.x,vp.z,true):vp.y;std::printf("P2_UMIMUSHI_VITALS tick=%d hp=%.1f x=%.1f y=%.1f z=%.1f ground=%.1f\n",observed,umi->mHealth,vp.x,vp.y,vp.z,vg);std::fflush(stdout);}
  if(rebound){
   require(umi&&umi->isAlive()&&actorPresent(umi),"rebirth actor not live");
   require(alivePikis()>=1,"squad extinct on rebirth");
   std::puts("PASS P2_UMIMUSHI_REBIRTH rebound1 control_alive");std::fflush(stdout);std::_Exit(0);}
  if(umi&&umi->isAlive()&&staged&&!deadSeen&&throws<400&&observed-lastThrow>=25){
   int loosed=0;
   Iterator jt(pikiMgr);CI_LOOP(jt){Piki* q=static_cast<Piki*>(*jt);
    if(!q||!q->isAlive()||q->getStickObject()||q->getState()!=PIKISTATE_Normal||!q->isThrowable())continue;
    Vector3f aim=umi->getPosition();
    q->mFSM->transit(q,PIKISTATE_Flying);
    n->throwPiki(q,aim);
    ++throws;++loosed;lastThrow=observed;
    Vector3f d=aim;d.sub(n->mSRT.t);
    std::printf("P2_UMIMUSHI_THROW n=%d generator=374004 dist=%.1f hp=%.1f\n",throws,d.length(),umi->mHealth);std::fflush(stdout);
    if(loosed>=2)break;}
   if(!loosed){int normal=0,can=0;Iterator kt(pikiMgr);CI_LOOP(kt){Piki* q=static_cast<Piki*>(*kt);if(q&&q->isAlive()&&q->getState()==PIKISTATE_Normal){++normal;if(q->isThrowable())++can;}}std::printf("P2_UMIMUSHI_THROW_SKIP tick=%d normal=%d throwable=%d\n",observed,normal,can);std::fflush(stdout);lastThrow=observed-15;}}
  if(umi&&!umi->isAlive()&&!deadSeen){
   deadSeen=true;deathTick=observed;
   Vector3f dp=umi->getPosition();
   float ground=mapMgr?mapMgr->getMinY(dp.x,dp.z,true):dp.y;
   std::printf("P2_UMIMUSHI_NATURAL_DEATH tick=%d throws=%d umi_alive=0\n",observed,throws);
   std::printf("P2_UMIMUSHI_DEATH_POS x=%.2f y=%.2f z=%.2f ground=%.2f\n",dp.x,dp.y,dp.z,ground);std::fflush(stdout);
   require(ground>1.0f&&dp.y>=ground-2.0f,"umimushi died off the arena floor (fall, not combat)");}
  if(deadSeen&&!funneled&&observed>=deathTick+5){
   // Drive the public engine death funnel once (doAI is suppressed for
   // family actors, so dieSoon would never run). No state is written by the
   // fixture; all damage is prior natural combat. Address use only after.
   static_cast<BTeki*>(umi)->pcEscapeNow();
   funneled=true;
   std::puts("P2_UMIMUSHI_FUNNEL_DROVE engine=dieSoon");std::fflush(stdout);}
  if(funneled&&!goneSeen){
   if(!actorPresent(umi)){
    goneSeen=true;
    std::printf("P2_UMIMUSHI_GONE tick=%d\n",observed);std::fflush(stdout);}}
  if(funneled&&!corpseFound){
   Iterator it(pelletMgr);CI_LOOP(it){Pellet* p=static_cast<Pellet*>(*it);if(p&&p->isAlive()&&p->mPelletView==static_cast<PelletView*>(umi)){corpseFound=true;break;}}
   if(corpseFound){std::puts("P2_UMIMUSHI_CORPSE_PRESENT engine_funnel");std::fflush(stdout);}}
  if(deadSeen&&funneled&&!goneSeen&&!corpseFound&&observed>deathTick+900){
   std::puts("P2_UMIMUSHI_FUNNEL_STALLED no_removal_no_corpse");std::fflush(stdout);
   require(false,"death funnel produced neither removal nor corpse");}
  if(throws>=400&&!deadSeen&&umi&&umi->isAlive()){
   std::printf("P2_UMIMUSHI_THROW_BUDGET_EXHAUSTED throws=%d\n",throws);std::fflush(stdout);
   require(false,"umimushi natural-death budget exhausted without death");}
  if(goneSeen||corpseFound){
   if(!corpseFound){std::puts("P2_UMIMUSHI_NO_CORPSE source_no_loot");std::fflush(stdout);}
   require(alivePikis()>=1,"squad extinct");
   require(throws>=1,"no throws recorded");
   std::puts("PASS P2_UMIMUSHI_NATURAL_DEATH death1 gone1 squad_alive");std::fflush(stdout);std::_Exit(0);}
  std::fflush(stdout);return result;
 }};
// MUSE-UMIMUSHI-APP-END