// Engine-free test for the Kabuto 75 identity policy (inst2-frogs #871).
#include <cassert>
#include <cstdio>
#include <sstream>
#include "pc_p2_kabuto_fsm_policy.h"
int main(){
 const auto& p=p2kabutofsm::params();
 assert(p.health==850.0f);assert(p.sight==350.0f);assert(p.attackRange==180.0f);assert(p.attackDamage==10.0f);
 assert(std::string(p2kabutofsm::stateName(0))=="dead");
 assert(std::string(p2kabutofsm::stateName(5))=="attack");
 assert(std::string(p2kabutofsm::stateName(99))=="null");
 {
  std::stringstream in;in<<"P2_KABUTO_1\n1\n123 Kabuto\nKabuto\n";
  for(const char* c:{"dead","move","flick","attack","wait"})in<<c<<" 2 2 0 1\n";
  std::map<unsigned,std::string> actors;std::vector<p2animation::Clip> bank;
  assert(p2kabutofsm::parse(in,actors,bank));
  assert(actors.size()==1&&actors[123]=="Kabuto");assert(bank.size()==5);
 }
 {
  std::stringstream bad;bad<<"P2_KABUTO_0\n1\n123 Kabuto\n";
  std::map<unsigned,std::string> actors;std::vector<p2animation::Clip> bank;
  assert(!p2kabutofsm::parse(bad,actors,bank));
 }
 std::printf("p2_kabuto_fsm_policy_test: all checks passed\n");return 0;
}
