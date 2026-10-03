#include "pc_p2_catfish_source_policy.h"
#include "pc_p2_original_catfish_bank.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <limits>
using namespace p2catfishsource;
bool has(const std::vector<Key>& events,int type){for(auto e:events)if(e.type==type)return true;return false;}
int main(int argc,char** argv){
 Motion motion;motion.start(AttackAnim);
 assert(motion.advance(17).empty());assert(has(motion.advance(1),2)&&motion.frame()==18);
 assert(!has(motion.advance(57),3)&&motion.frame()==75);assert(has(motion.advance(1),3)&&motion.frame()==76);
 assert(has(motion.advance(9),1000)&&motion.frame()==84);assert(motion.advance(30).empty());
 motion.start(AttackAnim);assert(has(motion.advance(18),2));motion.start(EatAnim);
 assert(has(motion.advance(16),1000)&&motion.frame()==15);assert(motion.advance(100).empty());
 motion.start(WaitAnim);auto loop=motion.advance(30);assert(has(loop,0)&&has(loop,1)&&!has(loop,1000)&&motion.frame()==0);
 motion.finish();auto finished=motion.advance(30);assert(has(finished,1000)&&motion.frame()==29);
 motion.start(TurnAnim);assert(!has(motion.advance(90),1000)&&motion.frame()==0); // source discards overshoot
 motion.finish();assert(!has(motion.advance(29),1000));assert(has(motion.advance(1),1000));
 motion.start(MoveAnim);assert(!has(motion.advance(25),1000)&&motion.frame()==0);motion.finish();assert(has(motion.advance(25),1000));
 motion.start(FlickAnim);assert(!has(motion.advance(25),2));assert(has(motion.advance(1),2));
 assert(!has(motion.advance(21),3));assert(has(motion.advance(1),3));assert(has(motion.advance(22),1000));
 assert(motion.advance(0).empty()&&motion.advance(-1).empty()&&motion.advance(std::numeric_limits<float>::infinity()).empty());
 CorpseMotion corpse;corpse.prepare();corpse.advance(30);
 assert(corpse.motion().id()==CarryAnim&&corpse.motion().frame()==0); // pre-carry is paused
 corpse.start();corpse.advance(10);assert(corpse.motion().frame()==10);
 corpse.advance(19);assert(corpse.motion().frame()==29);corpse.advance(1);
 assert(corpse.motion().frame()==10); // key29 is observed at30, loops to10
 corpse.advance(40);assert(corpse.motion().frame()==10); // discard overshoot
 corpse.finish();corpse.advance(30);assert(corpse.motion().frame()==39);
 corpse.advance(30);assert(corpse.motion().frame()==39); // completed END remains stable
 corpse.start(true);assert(corpse.motion().frame()==0&&!corpse.motion().finishing());
 corpse.advance(11);assert(corpse.motion().frame()==11);
 corpse.stop();corpse.advance(18);assert(corpse.motion().frame()==11);
 corpse.start();corpse.advance(18);assert(corpse.motion().frame()==29);
 NonStoneGate gate;gate.reset();assert(!gate.noInterrupt()&&gate.clearSerial()==0);
 gate.set();assert(gate.noInterrupt());gate.reset();assert(!gate.noInterrupt()&&gate.clearSerial()==1);
 gate.reset();assert(gate.clearSerial()==1); // init/cleanup after KEY3 does not repeat down-effect request
 gate.set();gate.reset();assert(gate.clearSerial()==2); // interrupted Flick cleanup clears before KEY3
 assert(registration(WaitAnim).keys.size()==2&&registration(TurnAnim).keys.size()==2&&registration(PressAnim).keys.empty());
 assert(!startFlick(1,0)&&startFlick(2,0)&&startFlick(2,1));
 assert(!startFlick(2,2)&&startFlick(3,2)&&startFlick(3,3));
 assert(!startFlick(256,0)); // source rounds then narrows to u8
 assert(std::fabs(turnStep(rad(180))-rad(10))<1e-6f);
 assert(std::fabs(turnStep(rad(-20))-rad(-2))<1e-6f);
 assert(attackable({0,0,0},{0,0,49},rad(25)));
 assert(!attackable({0,0,0},{0,11,49},0)); // full 3D separation
 assert(!attackable({0,0,0},{0,0,50},0));
 assert(!attackable({0,0,0},{0,0,49},rad(25.01f)));
 assert(outOfRange({0,0,0},{0,0,201},0));assert(!outOfRange({0,0,0},{0,51,201},0)); // literal source height clause
 assert(attackEnd(false,false)==TurnToHome&&attackEnd(true,false)==Turn&&attackEnd(true,true)==Attack);
 assert(flickReturn(Walk)==Walk&&flickReturn(Wait,Turn)==Turn);
 if(argc==2){std::ifstream bank(argv[1]);std::string e;assert(p2original::catfish::validateCatfishBank(bank,e));}
 std::cout<<"Catfish source indexed clocks, corpse carry lifecycle, observed keys, loop finish, 3D attack, retail flick bands and return states PASS\n";
}
