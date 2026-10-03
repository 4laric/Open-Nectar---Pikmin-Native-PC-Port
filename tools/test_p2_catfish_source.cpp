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
 std::cout<<"Catfish source indexed clocks, observed keys, loop finish, 3D attack, retail flick bands and return states PASS\n";
}
