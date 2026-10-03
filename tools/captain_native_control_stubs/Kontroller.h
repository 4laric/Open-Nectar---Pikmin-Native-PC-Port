#pragma once
constexpr unsigned KBBTN_A=1u<<12,KBBTN_B=1u<<13,KBBTN_X=1u<<14,KBBTN_Y=1u<<15,KBBTN_Z=1u<<16,KBBTN_L=1u<<17,KBBTN_R=1u<<18,KBBTN_DPAD_UP=1u<<10,KBBTN_DPAD_DOWN=1u<<11,KBBTN_DPAD_LEFT=1u<<8,KBBTN_DPAD_RIGHT=1u<<9;
class Kontroller {public:unsigned mCurrentInput=0;float getMainStickX(){return 0;}float getMainStickY(){return 0;}float getSubStickX(){return 0;}float getSubStickY(){return 0;}};
