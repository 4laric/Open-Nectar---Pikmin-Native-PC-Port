#pragma once
// Engine-free native layout double for actual coreStates TU controls only.
class NaviState;class NaviStateMachine;
struct Vector3f {float x=0,y=0,z=0;void set(float a,float b,float c){x=a;y=b;z=c;}};
class Navi {public:NaviStateMachine* mStateMachine=nullptr;NaviState* current=nullptr;float mHealth=50;Vector3f mTargetVelocity,mVelocity;NaviState* getCurrState(){return current;}void setCurrState(NaviState* state){current=state;}};
