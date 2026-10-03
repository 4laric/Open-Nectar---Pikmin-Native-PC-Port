#pragma once
#include "pc_p2_original_number_profile.h"
#include <array>
#include <string>
#include <vector>
namespace p2originalnumber {
struct AnimationKey {float frame=0,value=0,inTangent=0,outTangent=0;};
struct AnimationTrack {std::vector<AnimationKey> keys;};
struct Animation {
 Size size=Size::One;unsigned angleScale=0;
 std::array<AnimationTrack,9> tracks;
 // Bounded descriptor parsing only. Actual resources must separately verify
 // the converted file hash; source SHA text alone is not authentication.
 bool read(const std::string& bytes,std::string& error);
 bool sample(float sourceFrame,std::array<float,12>& matrix)const noexcept;
};
class CarryPlayer {
public:
 bool start(float deltaTime)noexcept;
 void stop()noexcept{mStep=0;}
 void finish()noexcept{mFinish=true;}
 bool advance(float deltaTime,bool picked)noexcept;
 float timer()const noexcept{return mTimer;}
 // SysShape::Animator sets its J3D frame from (int)mTimer, not a blend.
 unsigned sampleFrame()const noexcept{return unsigned(mTimer);}
 bool finishing()const noexcept{return mFinish;}
 float step()const noexcept{return mStep;}
private:
 float mTimer=0,mStep=0;bool mFinish=false;
};
}
