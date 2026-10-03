#pragma once
#include <vector>
class Navi;
class NaviState {int id;public:explicit NaviState(int i):id(i){}virtual ~NaviState()=default;int getID()const{return id;}virtual void exec(Navi*){}};
class NaviStateMachine {std::vector<NaviState*> states;public:int mStateCount=0;NaviState** mStates=nullptr;void registerState(NaviState* s){states.push_back(s);mStateCount=int(states.size());mStates=states.data();}};
