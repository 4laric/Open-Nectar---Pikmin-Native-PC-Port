#pragma once
struct Vector3f {float x=0,y=0,z=0;void set(float a,float b,float c){x=a;y=b;z=c;}};
class NaviState;class NaviStateMachine;
// Physical native actor double only. Production source fields stay in Owner.
class Navi {public:struct {Vector3f t;}mSRT;Vector3f mVelocity,mTargetVelocity;float mFaceDirection=0;NaviState* current=nullptr;NaviStateMachine* mStateMachine=nullptr;NaviState* getCurrState(){return current;}};
