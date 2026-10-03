#include "pc_p2_original_captain_walk.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <limits>
using namespace p2original::captain;
namespace w=walk;
void require(bool c,const char* name){if(!c){std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}}
bool has(const w::Output& o,w::Kind kind){for(const auto& c:o.commands)if(c.kind==kind)return true;return false;}
const w::Command* command(const w::Output& o,w::Kind kind){for(const auto& c:o.commands)if(c.kind==kind)return &c;return nullptr;}
bool transit(const w::Output& o,StateId id){for(const auto& c:o.commands)if(c.kind==w::Kind::TransitSelf&&c.state==id)return true;return false;}
w::Frame frame(){w::Frame f;f.actor=w::Actor{};f.actor->alive=true;f.actor->hasController=true;f.world=w::World{};f.world->demoInactive=true;f.world->storyMode=true;f.world->activeActor=true;f.buttons=w::Buttons{};f.stickCount=0;f.onionQueryComplete=true;f.deltaTime=.1f;f.postControlSceneAnimationTimer=0;f.cellCandidates=std::vector<w::Candidate>{};return f;}
w::Candidate candidate(std::uint64_t id,control::Vec3 pos){w::Candidate c;c.identity=id;c.position=pos;c.sphereRadius=5;c.alive=true;return c;}
int main(int argc,char** argv){
 require(argc==2,"private authored resource argument");std::ifstream file(argv[1],std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(file)),{}),e;control::Params params;require(control::parseParameters(bytes,params,e),e.c_str());
 w::State s;w::Output o;auto f=frame();require(w::init(*f.actor,s,o,e)&&has(o,w::Kind::StartMotion)&&!s.dismissTimer,"source Walk init and uninitialized dismissal preserved");
 require(!w::checkpointValid(s,e),"unknown source transient refuses SAVE");require(w::step(params,f,s,o,e)&&has(o,w::Kind::MakeVelocity)&&has(o,w::Kind::MakeCStick)&&has(o,w::Kind::Rappa)&&has(o,w::Kind::FindNextThrowPiki)&&s.dismissTimer==0,"ordinary source control commands establish not-held dismiss timer");
 require(w::checkpointValid(s,e),"finite actual initialized checkpoint");
 auto missing=f;missing.postControlSceneAnimationTimer.reset();w::State saved=s;require(!w::step(params,missing,s,o,e)&&s.idleTimer==saved.idleTimer,"missing post-control timer refuses atomically");
 f.world->movieFlagActive=true;require(w::step(params,f,s,o,e)&&!has(o,w::Kind::MakeVelocity)&&has(o,w::Kind::MakeCStick),"source active movie skips only MakeVelocity");f=frame();
 f.buttons->aDown=true;f.buttons->bDown=true;f.onion=w::Onion{42,false};require(w::step(params,f,s,o,e)&&transit(o,StateId::Container)&&o.continuation==w::Stage::None,"onion A precedes Gather B");
 f.onion->isPod=true;require(w::step(params,f,s,o,e)&&transit(o,StateId::Gather),"pod excludes container then Gather priority");f.buttons->bDown=false;
 require(w::step(params,f,s,o,e)&&o.continuation==w::Stage::ActionButton,"actual action callback required");require(!w::resumeActionButton(false,{},o,e),"no guessed throwable");require(w::resumeActionButton(false,true,o,e)&&transit(o,StateId::ThrowWait),"source throwable fallthrough");require(!w::resumeActionButton(false,true,o,e),"action continuation consumed once");
 require(w::step(params,f,s,o,e)&&w::resumeActionButton(true,{},o,e)&&!transit(o,StateId::ThrowWait),"handled real action requires no throw query");
 f=frame();f.buttons->upDown=true;f.buttons->downDown=true;require(w::step(params,f,s,o,e)&&transit(o,StateId::Dope)&&command(o,w::Kind::TransitSelf)->bitter,"Bitter before Spicy");
 f=frame();f.buttons->xDown=true;f.buttons->xHeld=true;f.buttons->yDown=true;f.world->switchUnlocked=true;f.other=w::OtherCaptain{99,true,StateId::Follow,true};
 require(w::step(params,f,s,o,e)&&o.continuation==w::Stage::Dismiss&&!has(o,w::Kind::TogglePlayer),"release callback before switch");
 require(w::resumeDismiss(f,false,s,o,e)&&s.dismissTimer==21&&has(o,w::Kind::TogglePlayer)&&has(o,w::Kind::WhistleSelfPreserveParties)&&has(o,w::Kind::TransitOtherChange),"actual release failure + Follow switch preserves parties");
 f.buttons->xDown=false;s.dismissTimer=35;f.world->napsackReceipt=true;require(w::step(params,f,s,o,e)&&transit(o,StateId::Pellet)&&!has(o,w::Kind::TogglePlayer)&&s.dismissTimer==0,"strict >35 authenticated napsack takes priority");
 f.world->napsackReceipt=false;s.dismissTimer=35;require(w::step(params,f,s,o,e)&&!transit(o,StateId::Pellet)&&has(o,w::Kind::TogglePlayer),"missing story napsack receipt prevents Pellet");
 f.buttons->xHeld=false;f.world->softPaused=true;require(w::step(params,f,s,o,e)&&!has(o,w::Kind::TogglePlayer),"soft pause switch gate");f.world->softPaused=false;f.world->multiplayer=true;require(w::step(params,f,s,o,e)&&!has(o,w::Kind::TogglePlayer),"multiplayer switch gate");f.world->multiplayer=false;
 for(auto exclusion:{StateId::Nuku,StateId::NukuAdjust,StateId::Punch}){f.other->state=exclusion;require(w::step(params,f,s,o,e)&&!has(o,w::Kind::TogglePlayer),"other state switch exclusion");}
 f.other->state=StateId::Walk;f.other->alive=false;require(w::step(params,f,s,o,e)&&!has(o,w::Kind::TogglePlayer),"other CFalive switch gate");
 f=frame();f.actor->hasController=false;s={};require(w::step(params,f,s,o,e)&&s.ai==w::AI::Wait&&std::fabs(s.idleTimer-1.9f)<.0001f,"uncontrolled captain executes actual WaitAI");
 f.cellCandidates.reset();saved=s;require(!w::step(params,f,s,o,e)&&s.idleTimer==saved.idleTimer,"missing actual source cells refuses");f.cellCandidates=std::vector<w::Candidate>{};
 s.idleTimer=.1f;f.randomValues={.75f,.5f};require(w::step(params,f,s,o,e)&&s.ai==w::AI::Animation&&s.animation==w::Motion::Jump&&s.idleTimer==2.5f,"uncontrolled fourth idle choice Jump and random next timer");
 require(w::keyEventEnd(s,o,e)&&s.ai==w::AI::Wait&&command(o,w::Kind::StartMotion)->motion==w::Motion::Step,"actual KEYEVENT_END returns Step wait");
 f=frame();*f.postControlSceneAnimationTimer=9;require(w::step(params,f,s,o,e)&&s.ai==w::AI::Control,"idle threshold exact9 stays control");*f.postControlSceneAnimationTimer=9.01f;f.randomValues={.75f};f.assertIdleMotion=true;require(w::step(params,f,s,o,e)&&s.animation==w::Motion::Exercise,"controlled fourth idle choice Exercise after strict9");
 f=frame();f.actor->hasController=false;s={};auto other=candidate(101,{20,0,0});other.navi=true;auto enemy=candidate(102,{0,0,21});enemy.teki=true;enemy.living=true;enemy.emotionNonzero=true;enemy.weakBitterDrop=false;f.cellCandidates=std::vector<w::Candidate>{other,enemy};f.randomValues={.5f};require(w::step(params,f,s,o,e)&&s.target==102&&s.ai==w::AI::Escape&&s.escapeCCW==true,"first emotional enemy supersedes captain fallback");
 f.currentTarget=enemy;f.cellCandidates=std::vector<w::Candidate>{};require(w::step(params,f,s,o,e)&&has(o,w::Kind::AddVelocity)&&std::fabs(command(o,w::Kind::AddVelocity)->vector.x+160)<.001f,"source escape tangent above15 and <=35");
 w::wallHit(s);require(s.escapeCCW==false&&s.escapeTimer==10,"wall flips source tangent and starts10");w::wallHit(s);require(s.escapeCCW==false&&s.escapeTimer==10,"wall cannot retrigger during cooldown");require(w::step(params,f,s,o,e)&&s.escapeTimer==9,"escape cooldown counts updates");
 f.currentTarget->position={0,0,40};require(w::step(params,f,s,o,e)&&s.ai==w::AI::Escape,"escape clearance exact35 remains");f.currentTarget->position={0,0,40.01f};require(w::step(params,f,s,o,e)&&s.ai==w::AI::Wait&&s.idleTimer==5&&s.target==102,"escape clearance >35 returnswait retaining source target");
 s.ai=w::AI::Attack;f.currentTarget->position={0,0,14};require(w::step(params,f,s,o,e)&&transit(o,StateId::Punch)&&has(o,w::Kind::TurnTo)&&std::fabs(command(o,w::Kind::AddVelocity)->vector.z-80)<.001f,"attack source halfspeed+clearance<10 Punch");
 s.ai=w::AI::Attack;f.currentTarget->position={0,0,15};require(w::step(params,f,s,o,e)&&!transit(o,StateId::Punch),"attack exact10 no Punch");f.currentTarget->alive=false;require(w::step(params,f,s,o,e)&&s.ai==w::AI::Wait&&s.target==0,"dead attack target clears identity");
 f=frame();f.actor->hasController=false;s={};f.cellCandidates=std::vector<w::Candidate>{enemy};f.cellCandidates->front().weakBitterDrop.reset();require(!w::step(params,f,s,o,e),"missing actual EnemyInfo category refuses");f.cellCandidates->front().weakBitterDrop=true;require(w::step(params,f,s,o,e)&&s.ai==w::AI::Wait&&s.target==102,"weak bitter enemy retains WaitAI");
 require(std::string(w::authoredClip(w::Motion::Exercise))=="gattu.bca","source authored idle mapping");
 f=frame();f.world->versusMode=true;w::Collision hit;hit.identity=204;hit.teki=true;hit.alive=true;hit.sourceEnemyBomb=true;hit.controllerStick=control::Vec2{1,0};s.collisionTimer=58;require(w::collision(f,hit,s,o,e)&&s.collisionTimer==61&&transit(o,StateId::CarryBomb),"source Bomb strict>60 carry threshold");
 s.collisionTimer=99;require(w::collision(f,hit,s,o,e)&&s.collisionTimer==102,"source Bomb collision counter can reach102");require(w::collision(f,hit,s,o,e)&&s.collisionTimer==102,"source Bomb threshold prevents further increment");hit.captured=true;require(w::collision(f,hit,s,o,e)&&!transit(o,StateId::CarryBomb),"captured Bomb excluded");
 hit={};hit.identity=205;hit.honey=true;hit.absorbable=true;require(w::collision(f,hit,s,o,e)&&transit(o,StateId::Absorb),"source absorbable nonyellow drop");hit.yellowHoney=true;require(w::collision(f,hit,s,o,e)&&!transit(o,StateId::Absorb),"source yellow honey excluded");
 f.actor->slot=1;f.world->debtPaid=true;require(w::jumpKey200(f,o,e)&&command(o,w::Kind::JumpLandVoice)->scalar==2,"source President jump voice");f.world->frozen=true;require(w::jumpKey200(f,o,e)&&!has(o,w::Kind::JumpLandVoice),"frozen source sound gate");
 std::cout<<"PASS retail Walk action priority/callbacks, source switch parties/gates, live-cell Wait/idle/escape/attack sequencing and finite capture refusal\n";
}
