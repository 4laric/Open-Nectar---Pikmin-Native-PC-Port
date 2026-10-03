#pragma once
#include "NaviState.h"
#include "Controller.h"
class Creature { public: virtual ~Creature()=default; };
class Navi:public Creature {public:float mHealth=10;struct {float x=0,y=0,z=0;} mTargetVelocity;Controller* mKontroller=nullptr;NaviState* current=nullptr;NaviState* getCurrState(){return current;}};
