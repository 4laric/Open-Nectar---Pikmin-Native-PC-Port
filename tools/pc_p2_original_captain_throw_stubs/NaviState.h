#pragma once
#include <vector>
class Navi;class NaviStateMachine;
class NaviState {
 int id_;
public:
 explicit NaviState(int id):id_(id){}
 virtual ~NaviState()=default;
 int getID()const{return id_;}
 virtual void init(Navi*){} virtual void exec(Navi*){} virtual void cleanup(Navi*){}
 virtual void resume(Navi*){} virtual void restart(Navi*){}
 virtual bool invincible(Navi*){return false;}
};
class Navi {public:NaviState* current=nullptr;NaviStateMachine* fsm=nullptr;NaviState* getCurrState()const{return current;}};
class NaviStateMachine {
public:
 std::vector<NaviState*> states;int last=-1;
 ~NaviStateMachine(){for(auto* s:states)delete s;}
 void registerState(NaviState* s){states.push_back(s);}
 NaviState* find(int id){for(auto* s:states)if(s->getID()==id)return s;return nullptr;}
 bool transit(Navi* n,int id){auto* next=find(id);if(!next)return false;if(n->current)n->current->cleanup(n);n->current=next;last=id;next->init(n);return true;}
};
