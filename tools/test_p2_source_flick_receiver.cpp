// Compile the actual receiver TU with observable engine doubles. This checks
// receiver decisions and state effects; it is not a gameplay acceptance test.
#include "engine.h"
#include "pc_p2_hanachirashi_receiver.h"
#include <cassert>
#include <limits>
#include <iostream>
FakeSystem systemControl;FakeSystem* gsys=&systemControl;
unsigned pc_p2_original_actor_token(const Creature*){return 17;}
void finish(Piki& p){KeyEvent k;MsgAnim m{&k};p.current->procAnimMsg(&p,&m);}
void finish(Navi& n){KeyEvent k;MsgAnim m{&k};n.current->procAnimMsg(&n,&m);}
int main(){
 BTeki owner;Piki p;Machine<Piki> pm;PikiState normal(0),panic(36),legacy(22),deadPiki(7),pressed(33);auto* blow=pc_p2_hanachirashi_piki_state_create();
 pm.states={{0,&normal},{36,&panic},{22,&legacy},{37,blow},{7,&deadPiki},{33,&pressed}};p.mFSM=&pm;pm.transit(&p,0);p.mFaceDirection=PI/2;
 assert(pc_p2_source_flick_piki(&owner,&p,80,-1000));assert(systemControl.draws==3);assert(p.mHealth==10&&p.mHappa==Flower);
 p.current->exec(&p);assert(std::fabs(p.mVelocity.x+84)<.001f);assert(std::fabs(p.mVelocity.z)<.001f);assert(std::fabs(p.mVelocity.y-131.25f)<.001f);
 // A previous accepted Flick entered source Blow: second pass must be accepted
 // and consume both new direction draws plus Blow's vertical randomization.
 assert(pc_p2_source_flick_piki(&owner,&p,80,-1000));assert(systemControl.draws==6&&p.action.resumed==2);
 pm.transit(&p,36);int prior=systemControl.draws;assert(!pc_p2_source_flick_piki(&owner,&p,80,0)&&systemControl.draws==prior);
 pm.transit(&p,22);assert(!pc_p2_source_flick_piki(&owner,&p,80,0)&&systemControl.draws==prior);
 pm.transit(&p,7);assert(!pc_p2_source_flick_piki(&owner,&p,80,0)&&systemControl.draws==prior);
 pm.transit(&p,33);assert(!pc_p2_source_flick_piki(&owner,&p,80,0)&&systemControl.draws==prior);
 pm.transit(&p,0);p.alive=false;assert(!pc_p2_source_flick_piki(&owner,&p,80,0)&&systemControl.draws==prior);p.alive=true;
 pm.transit(&p,0);p.mFaceDirection=PI/2;assert(pc_p2_source_flick_piki(&owner,&p,80,0));p.current->exec(&p);assert(std::fabs(p.mVelocity.x)<.001f&&std::fabs(p.mVelocity.z+84)<.001f);
 systemControl.random=.05f;MsgBounce bounce;p.current->procBounceMsg(&p,&bounce);assert(p.mHappa==Leaf&&p.mHealth==10);
 finish(p);systemControl.dt=.5f;p.current->exec(&p);p.current->exec(&p);assert(p.motion==PIKIANIM_GetUp);finish(p);assert(p.getState()==0);
 // Wind remains distinct: Purple strips but refuses; ordinary wind does no HP.
 p.mP2Purple=true;p.mHappa=Flower;prior=systemControl.draws;assert(!pc_p2_hanachirashi_wind_piki(&owner,&p,Vector3f(5,40,10)));assert(p.mHappa==Leaf&&systemControl.draws==prior);
 p.mP2Purple=false;assert(pc_p2_hanachirashi_wind_piki(&owner,&p,Vector3f(5,40,10)));assert(p.mHealth==10&&p.mHappa==Leaf);p.mIsWhistlePending=true;p.current->procBounceMsg(&p,&bounce);finish(p);p.current->exec(&p);p.current->exec(&p);finish(p);assert(p.mode==PikiMode::FormationMode&&!p.mIsWhistlePending);
 // Pre-bounce flute records recovery but keeps the one-second Koke timer.
 assert(pc_p2_hanachirashi_wind_piki(&owner,&p,Vector3f(5,40,10)));p.mIsWhistlePending=true;p.current->procBounceMsg(&p,&bounce);finish(p);systemControl.dt=.25f;p.current->exec(&p);assert(p.motion==PIKIANIM_JKoke);p.current->exec(&p);p.current->exec(&p);assert(p.motion==PIKIANIM_JKoke);p.current->exec(&p);assert(p.motion==PIKIANIM_GetUp);finish(p);assert(p.mode==PikiMode::FormationMode);
 // Post-bounce flute clears the timer, including a fresh flute after an earlier one.
 assert(pc_p2_hanachirashi_wind_piki(&owner,&p,Vector3f(5,40,10)));p.mIsWhistlePending=true;p.current->procBounceMsg(&p,&bounce);p.mIsWhistlePending=true;finish(p);p.current->exec(&p);assert(p.motion==PIKIANIM_GetUp);finish(p);
 Navi n;Machine<Navi> nm;NaviState walk(0),dead(29);auto* reaction=pc_p2_hanachirashi_navi_state_create();nm.states={{0,&walk},{29,&dead},{38,reaction}};n.mStateMachine=&nm;nm.transit(&n,0);systemControl.random=.5f;n.mFaceDirection=PI/2;prior=systemControl.draws;
 PcSourceNaviReactionGate gate;assert(!pc_p2_source_navi_reaction_gate(nullptr,gate));assert(!pc_p2_source_navi_reaction_gate(&n,gate)&&gate.kind==PcSourceNaviReactionKind::Unsupported);
 assert(pc_p2_source_flick_navi(&owner,&n,80,7,-1000));assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.kind==PcSourceNaviReactionKind::Flick&&gate.phase==0&&!gate.inheritedInvincible&&gate.activation);auto firstActivation=gate.activation;NaviState spoof(38);auto* owned=n.current;n.current=&spoof;assert(!pc_p2_source_navi_reaction_gate(&n,gate));n.current=owned;assert(pc_p2_source_navi_reaction_gate(&n,gate));assert(systemControl.draws==prior+2&&n.mHealth==100);n.current->exec(&n);assert(std::fabs(n.mVelocity.x+84)<.001f&&n.mVelocity.y==0);
 finish(n);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.kind==PcSourceNaviReactionKind::Flick&&gate.phase==1);finish(n);n.current->exec(&n);assert(n.mHealth==100&&n.mLifeGauge.updates==0&&n.motion==PIKIANIM_JKoke);n.current->procBounceMsg(&n,&bounce);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.kind==PcSourceNaviReactionKind::KokeDamage&&gate.phase==2);assert(n.mHealth==100);finish(n);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==3);assert(n.mHealth==93&&n.mLifeGauge.value==93&&n.mLifeGauge.updates==1);finish(n);assert(n.mHealth==93);
 systemControl.dt=.5f;n.current->exec(&n);n.current->exec(&n);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==4);finish(n);assert(n.current->getID()==0);assert(!pc_p2_source_navi_reaction_gate(&n,gate));
 // Retail addDamage enters Dead immediately at Koke END when HP is below 1.
 n.mHealth=7.5f;assert(pc_p2_source_flick_navi(&owner,&n,80,7,0));assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.activation!=firstActivation);n.current->procBounceMsg(&n,&bounce);finish(n);assert(n.mHealth==.5f&&n.current->getID()==29);
 n.mHealth=8;nm.transit(&n,0);assert(pc_p2_source_flick_navi(&owner,&n,80,7,0));n.current->procBounceMsg(&n,&bounce);finish(n);assert(n.mHealth==1&&n.current->getID()==38);systemControl.dt=.5f;n.current->exec(&n);n.current->exec(&n);assert(pc_p2_source_navi_reaction_gate(&n,gate)&&gate.phase==4);finish(n);assert(n.current->getID()==0);assert(!pc_p2_source_navi_reaction_gate(&n,gate));
 prior=systemControl.draws;assert(!pc_p2_source_flick_navi(&owner,&n,80,std::numeric_limits<float>::quiet_NaN(),0));assert(!pc_p2_source_flick_piki(&owner,&p,-1,0));assert(systemControl.draws==prior);
 delete blow;delete reaction;std::cout<<"actual source receiver RNG/reacceptance/vector/HP/leaf/Koke/whistle controls PASS\n";
}
