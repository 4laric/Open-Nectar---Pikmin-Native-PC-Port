#pragma once
#include "Navi.h"
#include <vector>
// Native FSM contract double: cleanup still sees old current state, then init
// sees new state. The production core-state factories/methods remain actual.
constexpr int NAVISTATE_Walk=0,NAVISTATE_Dead=29;
class NaviState {
 int id;
public:
 explicit NaviState(int value):id(value){}virtual ~NaviState()=default;
 int getID()const{return id;}
 virtual void init(Navi*){}virtual void exec(Navi*){}virtual void cleanup(Navi*){}
 virtual bool invincible(Navi*){return false;}
};
void captain_state_stub_transition(Navi*,int);
template<class T>class StateMachine {
 std::vector<NaviState*> owned;
public:
 int mStateLimit=75,mStateCount=0;NaviState** mStates=nullptr;
 virtual ~StateMachine(){for(auto* state:owned)delete state;}
 void registerState(NaviState* state){owned.push_back(state);mStates=owned.data();mStateCount=int(owned.size());}
 void transit(T* actor,int id){
  NaviState* next=nullptr;for(auto* s:owned)if(s->getID()==id){next=s;break;}if(!next)return;
  captain_state_stub_transition(actor,id);if(actor->current)actor->current->cleanup(actor);
  actor->current=next;next->init(actor);
 }
};
class NaviStateMachine:public StateMachine<Navi>{};
