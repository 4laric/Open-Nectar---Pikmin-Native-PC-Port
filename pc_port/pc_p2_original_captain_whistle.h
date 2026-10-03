#pragma once
#include "pc_p2_original_captain_control.h"
#include <array>
#include <optional>
namespace p2original { namespace captain { namespace whistle {
using control::Vec3;
class Parameters {
public:
 struct Values {float minimum=0,maximum=0,wide=0,callTime=0,fadeTime=0,cursorRadius=0,cursorSpeed=0;};
 bool authenticated()const{return valid;}
 const Values& values()const{return data;}
private:
 Values data;bool valid=false;
 friend bool parseParameters(const std::string&,Parameters&,std::string&);
};
bool parseParameters(const std::string&,Parameters&,std::string&);
enum class Mode {Idle,Blowing,Ended};
struct State {
 Vec3 offset,position,normal;
 float radius=10,time=0;
 Mode mode=Mode::Idle;
 std::array<unsigned char,4> color{255,150,0,120};
 // Retail init leaves this field untouched; start/stop first establish it.
 std::optional<bool> isWhistleActive;
};
// Physical selected map/platform owner performs the actual CurrTriInfo query
// at actor+offset, updateOnNewMaxY=false. Height and normal are the real query
// result, including the higher platform where present. No fabricated floor.
class Terrain {
public:
 virtual ~Terrain()=default;
 virtual bool currentTriangle(Vec3 position,float& height,Vec3& normal,std::string&)const=0;
};
// Literal NaviWhistle state and cursor policy. The concrete captain owner must
// authenticate scene/body/bank before and after these terrain callbacks; this
// component does not grant actor lifetime or World activation.
bool initialize(const Parameters&,Vec3 actor,float face,const Terrain&,State&,std::string&);
bool update(const Parameters&,Vec3 actor,Vec3 stick,bool automatic,bool wide,
            float delta,const Terrain&,State&,std::string&);
void start(const Parameters&,State&);
void stop(State&);
bool timeout(const State&);
bool setFace(float,State&,std::string&);
}}}
