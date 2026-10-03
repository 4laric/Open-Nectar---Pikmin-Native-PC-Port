#include "pc_p2_original_gate.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
using namespace p2original;
namespace {int checks=0;void check(bool v,const char* label){++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}}
int main(int argc,char** argv){
 check(argc==2,"literal manifest required");std::vector<GateRecord> rows;std::string e;
 check(readGates(argv[1],rows,e),e.c_str());check(rows.size()==3,"three literal gates");
 const unsigned uids[]={1376563400u,1380691721u,1387814303u};const float life[]={16000,10000,14000};
 for(unsigned i=0;i<3;++i){auto r=rows[i];check(r.uid==uids[i]&&r.segmentLife==life[i]&&r.color==0&&r.reserved==3,"literal source fields");
  auto s=gateInitial(r);check(gateStateValid(r,s,e),"initial valid");
  check(gateDamage(s,r.segmentLife)==GateAction::DamageMotion,"native damage starts motion");
  check(gateExec(s)==GateAction::None&&s.health==0&&s.segmentsDown==0,"zero health does not fall");
  gateAnimationKey(r,s);check(gateExec(s)==GateAction::Idle,"damage animation returns wait");
  check(gateDamage(s,1)==GateAction::DamageMotion,"subzero attack");
  check(gateExec(s)==GateAction::DownMotion&&s.segmentsDown==0,"fall waits animation key");
  std::vector<std::uint8_t> cache{7};check(!gateExport(r,s,cache,e)&&cache==std::vector<std::uint8_t>{7},"transient save refuses atomically");
  check(gateDamage(s,5)==GateAction::None&&s.damage==5,"damage during fall queues");
  check(gateAnimationKey(r,s)==GateAction::DamageMotion&&s.segmentsDown==1&&s.health==r.segmentLife,"one segment only, reset health");
  check(gateExec(s)==GateAction::None&&s.health==r.segmentLife-5,"queued damage hits next segment");
  gateAnimationKey(r,s);gateExec(s);
  check(gateExport(r,s,cache,e)&&cache.size()==112,"settled export");
  GateState restore=gateInitial(r);check(gateImport(r,cache,restore,e)&&restore.segmentsDown==1&&restore.health==s.health,"actual partial HP/stage restore");
  auto bad=cache;bad.back()^=1;check(!gateImport(r,bad,restore,e)&&restore.health==s.health,"checksum reject no mutation");
  auto moved=r;moved.position[0]+=1;check(!gateImport(moved,cache,restore,e),"full transform source binding");
  for(int stage=1;stage<3;++stage){gateDamage(s,r.segmentLife+100);check(gateExec(s)==GateAction::DownMotion,"one huge hit enters down");gateAnimationKey(r,s);check(s.segmentsDown==unsigned(stage+1)&&s.health==r.segmentLife,"excess damage discarded per source segment");}
  check(s.phase==GatePhase::Open&&gateDamage(s,10)==GateAction::None,"finished gate refuses damage");
  check(gateExport(r,s,cache,e)&&gateImport(r,cache,restore,e)&&restore.phase==GatePhase::Open,"destroyed gate restore");
  auto invalid=r;invalid.reserved=0;check(!validateGate(invalid,e),"cache flag mismatch");invalid=r;invalid.resurrectionDays=1;check(!validateGate(invalid,e),"unsupported respawn refuses");
  invalid=r;invalid.segmentLife=std::numeric_limits<float>::quiet_NaN();check(!validateGate(invalid,e),"nonfinite health refuses");
  invalid=r;invalid.color=2;check(!validateGate(invalid,e),"unsupported color refuses");
  check(gateDamage(restore,-1)==GateAction::None,"negative damage refuses");
 }
 std::printf("PASS ORIGINAL_GATE controls=%d literal_rows=3 physical_births=0\n",checks);
}
