#pragma once
constexpr unsigned KBBTN_A=1,KBBTN_B=2,KBBTN_DPAD_RIGHT=4,KBBTN_DPAD_LEFT=8,KBBTN_DPAD_UP=16,KBBTN_DPAD_DOWN=32;
class Kontroller {public:unsigned held=0,pressed=0,released=0;bool keyDown(unsigned b){return held&b;}bool keyClick(unsigned b){return pressed&b;}bool keyUnClick(unsigned b){return released&b;}};
