#pragma once
#include "NaviState.h"
class Creature { public: virtual ~Creature()=default; };
class Navi:public Creature {public:float mHealth=10;NaviState* current=nullptr;NaviState* getCurrState(){return current;}};
