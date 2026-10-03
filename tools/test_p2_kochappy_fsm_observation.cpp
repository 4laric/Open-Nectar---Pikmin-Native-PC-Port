#include "pc_p2_kochappy_fsm.h"
#include "pc_kochappy_gather_input.h"
#include <cstdio>
#include <initializer_list>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FSM observation check failed: %s line%d\n",#x,__LINE__);return 1;}}while(false)
int main(){
 CHECK(pc_kochappy_prefix_contact(true,true,true,1.f)==PcKochappyPrefixContact::Admit);
 CHECK(pc_kochappy_prefix_contact(true,true,true,.5f)==PcKochappyPrefixContact::Wait);
 CHECK(pc_kochappy_prefix_contact(true,true,true,std::nextafter(.5f,1.f))==PcKochappyPrefixContact::Admit);
 CHECK(pc_kochappy_prefix_contact(true,true,false,0.f)==PcKochappyPrefixContact::Wait);
 CHECK(pc_kochappy_prefix_contact(false,true,false,0.f)==PcKochappyPrefixContact::Refuse);
 CHECK(pc_kochappy_prefix_contact(true,false,false,0.f)==PcKochappyPrefixContact::Refuse);
 CHECK(pc_kochappy_prefix_contact(true,true,true,std::numeric_limits<float>::quiet_NaN())==PcKochappyPrefixContact::Refuse);
 PcKochappyPrefixContactGate contactWait;
 for(int i=0;i<90;++i)CHECK(contactWait.observe(false)==PcKochappyPrefixContact::Wait);
 CHECK(contactWait.observe(false)==PcKochappyPrefixContact::Refuse);
 CHECK(contactWait.observe(true)==PcKochappyPrefixContact::Refuse&&contactWait.waits==91);
 CHECK(contactWait.observe(false)==PcKochappyPrefixContact::Refuse);
 PcKochappyPrefixProgress prefix;
 CHECK(prefix.observe(20.f,true,4)==PcKochappyPrefixInput::Walk);
 CHECK(prefix.observe(.5f,true,4)==PcKochappyPrefixInput::Reached&&prefix.guide==1);
 CHECK(prefix.observe(.49f,true,4)==PcKochappyPrefixInput::Reached&&prefix.guide==2);
 CHECK(prefix.observe(0.f,true,4)==PcKochappyPrefixInput::Reached&&prefix.guide==3);
 CHECK(prefix.observe(.5f,true,4)==PcKochappyPrefixInput::Done&&prefix.guide==4);
 CHECK(prefix.observe(0.f,true,4)==PcKochappyPrefixInput::Done);
 CHECK(prefix.observe(0.f,false,4)==PcKochappyPrefixInput::Refuse);
 for(float bad:{-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
  PcKochappyPrefixProgress invalid;CHECK(invalid.observe(bad,true,4)==PcKochappyPrefixInput::Refuse);
 }
 PcKochappyPrefixProgress invalidCount;CHECK(invalidCount.observe(1.f,true,0)==PcKochappyPrefixInput::Refuse);
 PcKochappyPrefixProgress prefixStall;
 for(int i=0;i<90;++i)CHECK(prefixStall.observe(100.f,true,4)==PcKochappyPrefixInput::Walk);
 CHECK(prefixStall.observe(100.f,true,4)==PcKochappyPrefixInput::Refuse);
 PcKochappyPrefixProgress prefixCap;
 for(int i=0;i<179;++i)CHECK(prefixCap.observe(400.f-float(i)*1.1f,true,4)==PcKochappyPrefixInput::Walk);
 CHECK(prefixCap.observe(203.1f,true,4)==PcKochappyPrefixInput::Refuse);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,0,0,0,0,8)==22);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,1,0,0,0,30)==31);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,2,0,0,0,73)==74);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,0,0,0,0,74)==-1);
 CHECK(pc_kochappy_neutral_edge(true,true,false,true,0,0,0,0,8)==0);
 CHECK(pc_kochappy_neutral_edge(false,false,false,true,0,0,0,0,8)==0);
 CHECK(pc_kochappy_neutral_edge(false,true,true,true,0,0,0,0,8)==0);
 CHECK(pc_kochappy_neutral_edge(false,true,false,false,0,0,0,0,8)==0);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,0,.1f,0,0,8)==0);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,0,0,.1f,0,8)==0);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,0,0,0,50,8)==0);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,3,0,0,0,8)==-1);
 CHECK(pc_kochappy_neutral_edge(false,true,false,true,0,std::numeric_limits<float>::quiet_NaN(),0,0,8)==-1);
 using G=PcKochappyGatherInput;
 CHECK(pc_kochappy_gather_input(240,100,90,.1f,.65f)==G::Walk); // actual stalled target outside coverage
 CHECK(pc_kochappy_gather_input(145,100,90,.1f,.65f)==G::Cursor);
 CHECK(pc_kochappy_gather_input(145.01f,100,90,.1f,.65f)==G::Walk);
 CHECK(pc_kochappy_gather_input(0,100,90,.1f,.65f)==G::Cursor);
 CHECK(pc_kochappy_gather_input(200,150,120,.1f,.65f)==G::Cursor); // loaded radii, no hardcoded90
 CHECK(pc_kochappy_gather_input(240,100,90,.1f,65.f/74.f)==G::Refuse);
 CHECK(pc_kochappy_gather_input(240,100,90,22.f/74.f,.65f)==G::Refuse);
 CHECK(pc_kochappy_gather_input(240,100,90,.1f,.2f)==G::Refuse);
 CHECK(pc_kochappy_gather_input(-1,100,90,.1f,.65f)==G::Refuse);
 CHECK(pc_kochappy_gather_input(240,20,90,.1f,.65f)==G::Refuse);
 CHECK(pc_kochappy_gather_input(240,100,0,.1f,.65f)==G::Refuse);
 const float nan=std::numeric_limits<float>::quiet_NaN();
 for(int i=0;i<5;++i){float v[5]={240,100,90,.1f,.65f};v[i]=nan;
  CHECK(pc_kochappy_gather_input(v[0],v[1],v[2],v[3],v[4])==G::Refuse);}
 double spans[4]={300,40,80,1};
 CHECK(pc_kochappy_route_reentry(spans,4,3)==1); // closest future point3 excluded
 CHECK(pc_kochappy_route_reentry(spans,4,0)==-1);
 CHECK(pc_kochappy_route_reentry(nullptr,4,3)==-1);
 CHECK(pc_kochappy_route_reentry(spans,4,5)==-1);
 CHECK(pc_kochappy_route_reentry(spans,129,3)==-1);
 spans[0]=512;CHECK(pc_kochappy_route_reentry(spans,4,1)==-1);
 spans[0]=511.999;CHECK(pc_kochappy_route_reentry(spans,4,1)==0);
 spans[0]=-1;CHECK(pc_kochappy_route_reentry(spans,4,3)==-1);
 spans[0]=std::numeric_limits<double>::infinity();CHECK(pc_kochappy_route_reentry(spans,4,3)==-1);
 spans[0]=300;spans[2]=nan;CHECK(pc_kochappy_route_reentry(spans,4,3)==-1);
 PcKochappyReentryProgress progress;
 CHECK(!progress.begin(0,0));CHECK(!progress.begin(30,30));
 CHECK(progress.begin(30,8)&&progress.retainedNext==30);
 CHECK(!progress.mayBegin(8)&&!progress.mayBegin(30));
 CHECK(progress.begin(31,9));CHECK(progress.begin(32,10));CHECK(progress.begin(33,11));
 CHECK(!progress.begin(34,12)&&progress.count==4); // finite total reentry budget
 using C=PcKochappyCatchupInput;
 PcKochappyRouteCatchup catchup;
 CHECK(!catchup.begin(-1)&&!catchup.begin(128));
 CHECK(catchup.begin(29)&&!catchup.begin(30));
 CHECK(catchup.observe(true,200,145,160)==C::Hold);
 CHECK(catchup.observe(true,145,145,0)==C::Hold);
 CHECK(catchup.observe(true,145,145,0)==C::Hold);
 CHECK(catchup.observe(true,145,145,0)==C::Continue&&!catchup.active);
 CHECK(catchup.observe(true,0,145,0)==C::Refuse); // no invented visited boundary
 CHECK(catchup.begin(0)); // legitimate replay after separately guarded reentry
 CHECK(catchup.observe(true,100,145,2)==C::Hold); // native residual motion remains observed
 CHECK(catchup.observe(false,100,145,0)==C::Refuse);
 for(int field=0;field<3;++field){PcKochappyRouteCatchup invalid;CHECK(invalid.begin(0));
  float v[3]={200,145,0};v[field]=nan;
  CHECK(invalid.observe(true,v[0],v[1],v[2])==C::Refuse);
 }
 for(float limit:{0.f,-1.f,512.f}){PcKochappyRouteCatchup invalid;CHECK(invalid.begin(0));
  CHECK(invalid.observe(true,200,limit,0)==C::Refuse);}
 PcKochappyRouteCatchup stalled;CHECK(stalled.begin(1));
 for(int i=0;i<90;++i)CHECK(stalled.observe(true,200,145,0)==C::Hold);
 CHECK(stalled.observe(true,200,145,0)==C::Refuse);
 PcKochappyRouteCatchup improving;CHECK(improving.begin(1));
 for(int i=0;i<179;++i)CHECK(improving.observe(true,1000.f-i*2,145,0)==C::Hold);
 CHECK(improving.observe(true,642,145,0)==C::Refuse); // hard180 even with progress
 PcKochappyRouteCatchup unstable;CHECK(unstable.begin(1));
 CHECK(unstable.observe(true,140,145,0)==C::Hold);
 CHECK(unstable.observe(true,146,145,0)==C::Hold&&unstable.stable==0);
 CHECK(unstable.observe(true,140,145,0)==C::Hold);
 CHECK(unstable.observe(true,140,145,0)==C::Hold);
 CHECK(unstable.observe(true,140,145,0)==C::Continue);
 // A native Formation slot may be 181 units from the captain. Completion
 // concerns its own target's 30-unit flat walking rest zone, not recruitment.
 PcKochappyRouteCatchup formation;CHECK(formation.begin(1));
 CHECK(formation.observe(true,40,30,0)==C::Hold);
 CHECK(formation.observe(true,30,30,0)==C::Hold);
 CHECK(formation.observe(true,30.001f,30,0)==C::Hold&&formation.stable==0);
 CHECK(formation.observe(true,30,30,0)==C::Hold);
 CHECK(formation.observe(true,29,30,0)==C::Hold);
 CHECK(formation.observe(true,28,30,0)==C::Continue);
 PcKochappyRouteCatchup formationStalled;CHECK(formationStalled.begin(1));
 for(int i=0;i<90;++i)CHECK(formationStalled.observe(true,35,30,0)==C::Hold);
 CHECK(formationStalled.observe(true,35,30,0)==C::Refuse);
 // Modern neutral Crowd completion is strictly below60 and actually Formed.
 const float crowdLimit=std::nextafter(60.f,0.f);
 PcKochappyRouteCatchup crowd;CHECK(crowd.begin(0));
 CHECK(crowd.observe(true,crowdLimit,crowdLimit,0,false)==C::Hold&&crowd.stable==0);
 CHECK(crowd.observe(true,crowdLimit,crowdLimit,0,true)==C::Hold&&crowd.stable==1);
 CHECK(crowd.observe(true,60.f,crowdLimit,0,true)==C::Hold&&crowd.stable==0);
 CHECK(crowd.observe(true,59.f,crowdLimit,0,true)==C::Hold);
 CHECK(crowd.observe(true,59.f,crowdLimit,0,false)==C::Hold&&crowd.stable==0);
 CHECK(crowd.observe(true,crowdLimit,crowdLimit,0,true)==C::Hold);
 CHECK(crowd.observe(true,59.f,crowdLimit,0,true)==C::Hold);
 CHECK(crowd.observe(true,59.f,crowdLimit,0,true)==C::Continue);
 PcKochappyCrowdObservation owned;
 owned.actionOwner=owned.plateOwner=owned.slotsAvailable=owned.occupantOwner=owned.listenerOwner=owned.finiteGeometry=owned.neutral=true;
 owned.state=1;owned.slot=19;owned.used=20;owned.capacity=110;
 CHECK(owned.valid()&&owned.settled());
 auto initial=owned;initial.state=0;CHECK(initial.valid()&&!initial.settled());
 for(int i=0;i<16;++i){if(i==6||i==7)continue;auto bad=owned;
  switch(i){case 0:bad.actionOwner=false;break;case 1:bad.plateOwner=false;break;
   case 2:bad.slotsAvailable=false;break;case 3:bad.occupantOwner=false;break;
   case 4:bad.listenerOwner=false;break;case 5:bad.finiteGeometry=false;break;
   case 6:bad.tripping=true;break;case 7:bad.route=true;break;case 8:bad.state=2;break;
   case 9:bad.state=-1;break;case 10:bad.slot=-1;break;case 11:bad.slot=20;break;
   case 12:bad.used=111;break;case 13:bad.capacity=0;break;case 14:bad.used=0;break;
   case 15:bad.slot=110;break;}
  CHECK(!bad.valid()&&!bad.settled());
 }
 for(int i=0;i<2;++i){auto transient=owned;if(i==0)transient.tripping=true;else transient.route=true;
  CHECK(transient.valid()&&!transient.settled());
  PcKochappyRouteCatchup wait;CHECK(wait.begin(0));
  CHECK(wait.observe(true,59,crowdLimit,0,true)==C::Hold&&wait.stable==1);
  CHECK(wait.observe(true,59,crowdLimit,0,transient.settled())==C::Hold&&wait.stable==0);
 }
 auto nonneutral=owned;nonneutral.neutral=false;CHECK(nonneutral.valid()&&!nonneutral.settled());
 // Actual input is consumed on the following native idle. Do not infer
 // readiness from issuing a neutral command before observing neutral state.
 PcKochappyRouteCatchup neutralCadence;CHECK(neutralCadence.begin(0));
 auto previousInput=owned;previousInput.neutral=false;
 CHECK(neutralCadence.observe(true,10,crowdLimit,0,previousInput.settled())==C::Hold&&neutralCadence.stable==0);
 for(int i=0;i<2;++i)CHECK(neutralCadence.observe(true,10,crowdLimit,0,owned.settled())==C::Hold);
 CHECK(neutralCadence.observe(true,10,crowdLimit,0,owned.settled())==C::Continue);
 PcKochappyRouteCatchup selfPreventing;CHECK(selfPreventing.begin(0));
 for(int i=0;i<90;++i)CHECK(selfPreventing.observe(true,10,crowdLimit,0,previousInput.settled())==C::Hold);
 CHECK(selfPreventing.observe(true,10,crowdLimit,0,previousInput.settled())==C::Refuse);
 PcKochappyRouteCatchup slopeBlocked;CHECK(slopeBlocked.begin(0));
 for(int i=0;i<90;++i)CHECK(slopeBlocked.observe(true,173,crowdLimit,0,owned.settled())==C::Hold);
 CHECK(slopeBlocked.observe(true,173,crowdLimit,0,owned.settled())==C::Refuse); // neutral does not waive real lag
 PcKochappyFsmSnapshot paused;paused.available=true;paused.stunPaused=true;paused.state=3;paused.stateTime=.75f;paused.attackFired=true;
 CHECK(pc_kochappy_overlay_preserved(paused,paused));
 for(int i=0;i<8;++i){auto bad=paused;
  switch(i){case 0:bad.available=false;break;case 1:bad.stunPaused=false;break;case 2:bad.state=4;break;
   case 3:bad.stateTime+=.01f;break;case 4:bad.attackFired=false;break;case 5:bad.swallowFired=true;break;
   case 6:bad.flickFired=true;break;case 7:bad.terminal=true;break;}
  CHECK(!pc_kochappy_overlay_preserved(paused,bad));
 }
 auto now=paused;now.stunPaused=false;
 CHECK(!pc_kochappy_clock_resumed(paused,now));
 now.stateTime+=.01f;CHECK(pc_kochappy_clock_resumed(paused,now));
 now.stateTime=0;now.state=4;CHECK(pc_kochappy_clock_resumed(paused,now)); // natural transition resets clock
 now.terminal=true;CHECK(!pc_kochappy_clock_resumed(paused,now));
 now=paused;now.stateTime=std::numeric_limits<float>::quiet_NaN();
 CHECK(!pc_kochappy_overlay_preserved(paused,now));CHECK(!pc_kochappy_clock_resumed(paused,now));
 now=paused;now.available=false;CHECK(!pc_kochappy_clock_resumed(paused,now));
 PcKochappySwarmRecovery swarm;
    for(int age=1;age<30;++age)CHECK(!swarm.update(34,age,true,true,82,.3,10));
    for(int age=30;age<36;++age)CHECK(swarm.update(34,age,true,true,82,.3,10));
    for(int age=36;age<=180;++age)CHECK(!swarm.update(34,age,true,true,82,.3,10));
    CHECK(!swarm.update(35,30,false,true,82,.3,10));
    CHECK(!swarm.update(35,30,true,false,82,.3,10));
    CHECK(!swarm.update(35,30,true,true,59,.3,10));
    CHECK(!swarm.update(35,30,true,true,82,2,10));
    CHECK(!swarm.update(35,30,true,true,82,.3,512));
    CHECK(!swarm.update(35,30,true,true,82,.3,std::numeric_limits<float>::quiet_NaN()));
 PcKochappyGuidePulse guidePulse;
 CHECK(guidePulse.observe(0,20)==PcKochappyGuideInput::Walk);
 CHECK(guidePulse.observe(0,11)==PcKochappyGuideInput::Walk);
 CHECK(guidePulse.observe(0,10.9f)==PcKochappyGuideInput::Walk);
 CHECK(guidePulse.observe(0,10.9f)==PcKochappyGuideInput::Neutral);
 CHECK(guidePulse.observe(0,10.8f)==PcKochappyGuideInput::Walk);
 CHECK(guidePulse.observe(1,11)==PcKochappyGuideInput::Walk);
 PcKochappyGuidePulse slipCadence;
 for(int i=0;i<30;++i)CHECK(slipCadence.observe(0,8.f,true)==(i%3==2?PcKochappyGuideInput::Neutral:PcKochappyGuideInput::Walk));
 for(int i=0;i<6;++i)CHECK(slipCadence.observe(0,8.f,true)==PcKochappyGuideInput::Walk);
 CHECK(slipCadence.observe(0,8.f,true)==PcKochappyGuideInput::Neutral);
 PcKochappyGuidePulse captureCadence;
 for(int i=0;i<31;++i)captureCadence.observe(0,8.f,true);
 CHECK(captureCadence.observe(0,1.9f,true)==PcKochappyGuideInput::Walk);
 CHECK(captureCadence.observe(0,1.9f,true)==PcKochappyGuideInput::Neutral);
 PcKochappyGuidePulse leaveSlip;
 for(int i=0;i<31;++i)leaveSlip.observe(0,8.f,true);
 CHECK(leaveSlip.observe(0,7.9f,false)==PcKochappyGuideInput::Walk);
 CHECK(leaveSlip.observe(0,7.9f,false)==PcKochappyGuideInput::Neutral);
 PcKochappyGuidePulse progressingSlip;
 for(int i=0;i<30;++i)CHECK(progressingSlip.observe(0,8.f-.11f*i,true)==(i%3==2?PcKochappyGuideInput::Neutral:PcKochappyGuideInput::Walk));
 PcKochappyGuidePulse slipStall;
 for(int i=0;i<90;++i)CHECK(slipStall.observe(0,8.f,true)!=PcKochappyGuideInput::Refuse);
 CHECK(slipStall.observe(0,8.f,true)==PcKochappyGuideInput::Refuse);
 PcKochappyGuidePulse guideStall;
 for(int i=0;i<90;++i)CHECK(guideStall.observe(0,10)!=PcKochappyGuideInput::Refuse);
 CHECK(guideStall.observe(0,10)==PcKochappyGuideInput::Refuse);
 PcKochappyGuidePulse guideBound;
 for(int i=0;i<180;++i)CHECK(guideBound.observe(0,11-i*.011f)!=PcKochappyGuideInput::Refuse);
 CHECK(guideBound.observe(0,9)==PcKochappyGuideInput::Refuse);
 CHECK(guidePulse.observe(128,10)==PcKochappyGuideInput::Refuse);
 CHECK(guidePulse.observe(2,std::numeric_limits<float>::quiet_NaN())==PcKochappyGuideInput::Refuse);
 std::puts("P2_KOCHAPPY_FSM_OBSERVATION_POLICY_PASS actual_FSM_runtime=unexecuted");
 PcKochappyPrefixNeutralGate neutralSetup;
 for(int i=0;i<89;++i)CHECK(neutralSetup.observe(false)==0);
 CHECK(neutralSetup.observe(true)==1);
 CHECK(neutralSetup.observe(true)==-1);
 PcKochappyPrefixNeutralGate stalledSetup;
 for(int i=0;i<90;++i)CHECK(stalledSetup.observe(false)==0);
 CHECK(stalledSetup.observe(false)==-1);
 CHECK(pc_kochappy_current_wall_contact(8.499937478,8.5,2208.129883));
 CHECK(pc_kochappy_current_wall_contact(8.5,8.5,2208.129883));
 CHECK(!pc_kochappy_current_wall_contact(8.45,8.5,2208.129883));
 CHECK(!pc_kochappy_current_wall_contact(-1,8.5,2208.129883));
 CHECK(!pc_kochappy_current_wall_contact(NAN,8.5,2208.129883));
 CHECK(!pc_kochappy_current_wall_contact(8.5,8.5,NAN));
 CHECK(!pc_kochappy_current_wall_contact(8.5,8.5,1));
 CHECK(!(8.499937478>8.5+.10)); // Prospective reserve still refuses same touching pose.
 CHECK(!pc_kochappy_current_wall_contact(0,8.5,1e38));
 CHECK(!pc_kochappy_current_wall_contact(8.5,8.5,1e38));
 CHECK(!pc_kochappy_current_wall_contact(0,8.5,1000000));
 CHECK(pc_kochappy_current_wall_tolerance(8.5,2208.129883)>0);
 CHECK(pc_kochappy_current_wall_tolerance(8.5,1000000)<0);
 CHECK(pc_kochappy_enemy_path_clear(-860,2153,-950,2260,-1153,2231,95,80));
 CHECK(!pc_kochappy_enemy_path_clear(-860,2153,-1100,2260,-1153,2231,95,80));
 CHECK(!pc_kochappy_enemy_path_clear(0,0,200,0,100,0,95,8.5));
 CHECK(pc_kochappy_enemy_path_clear(0,0,0,0,120,0,95,8.5));
 CHECK(!pc_kochappy_enemy_path_clear(0,0,0,0,95,0,95,0));
 CHECK(!pc_kochappy_enemy_path_clear(0,0,0,0,120,0,-1,8.5));
 CHECK(!pc_kochappy_enemy_path_clear(0,0,0,0,120,0,NAN,8.5));
 CHECK(!pc_kochappy_enemy_path_clear(0,0,0,0,NAN,0,95,8.5));
 CHECK(pc_kochappy_air_contact_wait(true,true,false,0,40,.9f,48.5f,0,8.5f));
 CHECK(pc_kochappy_air_contact_wait(true,true,false,0,40,.9f,44,0,8.5f));
 CHECK(!pc_kochappy_air_contact_wait(true,true,false,0,40,.9f,48.5001f,0,8.5f));
 CHECK(!pc_kochappy_air_contact_wait(true,true,false,0,40,.9f,39.9f,0,8.5f));
 CHECK(!pc_kochappy_air_contact_wait(true,false,false,0,40,.9f,44,0,8.5f));
 CHECK(!pc_kochappy_air_contact_wait(true,true,false,0,40,.5f,44,0,8.5f));
 CHECK(!pc_kochappy_air_contact_wait(true,true,true,.9f,40,.9f,44,0,8.5f));
 CHECK(!pc_kochappy_air_contact_wait(true,true,false,0,NAN,.9f,44,0,8.5f));
 CHECK(!pc_kochappy_air_contact_wait(true,true,false,0,40,.9f,NAN,0,8.5f));
 PcKochappyRouteCatchup airWait;CHECK(airWait.begin(34));
 for(int i=0;i<90;++i){CHECK(airWait.observe(true,20,59.99f,0,false)==PcKochappyCatchupInput::Hold);CHECK(airWait.stable==0);}
 CHECK(airWait.observe(true,20,59.99f,0,false)==PcKochappyCatchupInput::Refuse);
 PcKochappyRouteCatchup landed;CHECK(landed.begin(34));
 CHECK(landed.observe(true,20,59.99f,0,false)==PcKochappyCatchupInput::Hold);
 CHECK(landed.observe(true,20,59.99f,0,true)==PcKochappyCatchupInput::Hold);
 CHECK(landed.observe(true,20,59.99f,0,true)==PcKochappyCatchupInput::Hold);
 CHECK(landed.observe(true,20,59.99f,0,true)==PcKochappyCatchupInput::Continue);
 const auto analog=pc_kochappy_analog_guide(4,-4,-1,0,8,10,.9f,.1f,.65f);
 CHECK(analog.valid&&analog.magnitude>.65f&&analog.magnitude<.9f&&analog.bearingError<.004f);
 CHECK(std::abs(analog.x)<=74&&std::abs(analog.y)<=74);
 for(int i=0;i<8;++i){const float a=i*.7853981633974483f;
  const auto selected=pc_kochappy_analog_guide(5*std::cos(a),5*std::sin(a),1,0,8,10,.9f,.1f,.65f);
  CHECK(selected.valid&&selected.magnitude>.65f&&selected.magnitude<.9f&&selected.bearingError<.004f);
 }
 CHECK(!pc_kochappy_analog_guide(4,-4,-1,0,74,10,.9f,.1f,.65f).valid);
 CHECK(!pc_kochappy_analog_guide(4,-4,-1,0,8,0,.9f,.1f,.65f).valid);
 CHECK(!pc_kochappy_analog_guide(4,-4,0,0,8,10,.9f,.1f,.65f).valid);
 CHECK(!pc_kochappy_analog_guide(4,-4,-1,0,8,10,.65f,.1f,.65f).valid);
 CHECK(!pc_kochappy_analog_guide(NAN,-4,-1,0,8,10,.9f,.1f,.65f).valid);
 CHECK(!pc_kochappy_analog_guide(.5f,0,-1,0,8,10,.9f,.1f,.65f).valid);
 CHECK(!pc_kochappy_analog_guide(12,0,-1,0,8,10,.9f,.1f,.65f).valid);
 const auto westEdge=pc_kochappy_directed_prefix_edge(-60,107,-1,0,22,8);
 CHECK(westEdge.valid&&westEdge.x==11&&westEdge.y==19&&westEdge.magnitude>.05f);
 CHECK(pc_kochappy_directed_prefix_edge(10,0,1,0,22,8).x==22);
 CHECK(pc_kochappy_directed_prefix_edge(0,10,1,0,22,8).y==-22);
 CHECK(!pc_kochappy_directed_prefix_edge(0,0,1,0,22,8).valid);
 CHECK(!pc_kochappy_directed_prefix_edge(10,0,1,0,22,22).valid);
 CHECK(!pc_kochappy_directed_prefix_edge(10,0,0,0,22,8).valid);
 CHECK(!pc_kochappy_directed_prefix_edge(NAN,0,1,0,22,8).valid);
 CHECK(!pc_kochappy_directed_prefix_edge(512,0,1,0,22,8).valid);
 PcKochappyGuideMotion uphill;uphill.speed=160;uphill.dt=1.f/30.f;uphill.tau=.1f;
 uphill.nx=-.2f;uphill.ny=std::sqrt(.96f);uphill.gravity=560;uphill.slipFactor=1;
 CHECK(uphill.valid());
 const auto slipAxes=pc_kochappy_analog_guide(5,0,1,0,8,10,.9f,.1f,.65f,&uphill);
 CHECK(slipAxes.valid&&slipAxes.x>0&&slipAxes.magnitude>.99f);
 float weakError=0;CHECK(uphill.error(5,0,160*.6517f,0,weakError));
 CHECK(slipAxes.bearingError<weakError);
 PcKochappyGuideMotion inertia=uphill;inertia.nx=0;inertia.ny=1;inertia.slipFactor=0;inertia.vx=160;
 const auto brakeAxes=pc_kochappy_analog_guide(.6f,0,1,0,8,10,.9f,.1f,.65f,&inertia);
 CHECK(brakeAxes.valid&&brakeAxes.x<0); // Ordinary stick brakes measured momentum.
 auto invalidMotion=uphill;invalidMotion.tau=.001f;
 CHECK(!pc_kochappy_analog_guide(5,0,1,0,8,10,.9f,.1f,.65f,&invalidMotion).valid);
 invalidMotion=uphill;invalidMotion.bx=NAN;
 CHECK(!pc_kochappy_analog_guide(5,0,1,0,8,10,.9f,.1f,.65f,&invalidMotion).valid);
 invalidMotion=uphill;invalidMotion.gravity=std::numeric_limits<float>::max();invalidMotion.slipFactor=std::numeric_limits<float>::max();
 CHECK(!pc_kochappy_analog_guide(5,0,1,0,8,10,.9f,.1f,.65f,&invalidMotion).valid);
}
