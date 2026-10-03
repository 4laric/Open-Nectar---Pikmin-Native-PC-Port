#include "pc_p2_original_snagret_flick.h"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace p2original::snagret_flick;
struct Host {
    std::vector<int> calls;
    void navis(float r,float k,float d,float a) {assert(r==40&&k==200&&d==1&&a<-10);calls.push_back(1);}
    void pikmin(float r,float k,float a) {assert(r==40&&k==200&&a==nearbyAngle);calls.push_back(2);}
    void stickers(float c,float k,float a) {assert(c==1&&a==stickerAngle&&a<-10);calls.push_back(k==200?3:4);assert(k==200||k==10);}
};
int main() {
    Host h;disappear(h);cleanup(h);
    assert((h.calls==std::vector<int>{1,2,3,4}));
    assert(nearby(0,39.9f,0));assert(!nearby(0,40,0));
    assert(!nearby(30,30,0));assert(nearby(30,0,20));
    assert(stickerAngle==nearbyAngle+tau); // single roundAng, sentinel preserved
    std::puts("original SnakeCrow flick policy PASS");
}
