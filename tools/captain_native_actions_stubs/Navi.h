#pragma once
#include "Vector.h"
class NaviState;class Kontroller;constexpr unsigned CF_UsePriorityFaceDir=1u<<10;
class Navi {public:NaviState* current=nullptr;Kontroller* mKontroller=nullptr;struct {Vector3f t;} mSRT;Vector3f mVelocity;float mFaceDirection=0;unsigned flags=0;NaviState* getCurrState(){return current;}void resetCreatureFlag(unsigned f){flags&=~f;}void setCreatureFlag(unsigned f){flags|=f;}};
