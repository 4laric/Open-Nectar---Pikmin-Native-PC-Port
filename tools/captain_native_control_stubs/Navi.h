#pragma once
struct Vector3f {float x=0,y=0,z=0;void set(float a,float b,float c){x=a;y=b;z=c;}};
class NaviState;class Kontroller;class Camera;
constexpr unsigned CF_UsePriorityFaceDir=1u<<10;
class Navi {public:NaviState* current=nullptr;Kontroller* mKontroller=nullptr;Camera* camera=nullptr;Vector3f mTargetVelocity;float mFaceDirection=0;unsigned flags=0;NaviState* getCurrState(){return current;}Camera* controlCamera(){return camera;}void resetCreatureFlag(unsigned f){flags&=~f;}void setCreatureFlag(unsigned f){flags|=f;}};
