#pragma once
#include <cmath>
#include <utility>
#include <vector>
namespace p2original { namespace bulblax_snagret {
// SysShape::Animator::animate: strict integer key comparison, ordered keys,
// loop overshoot discarded, END at duration with pose clamped to duration-1.
class MotionClock {
 float timer=0;unsigned cursor=0;bool completed=false;
public:
 void reset(){timer=0;cursor=0;completed=false;}
 float frame()const{return timer;}
 std::vector<int> advance(float delta,const std::vector<std::pair<int,int>>& keys,int duration,bool loop){
  std::vector<int> out;
  if(completed||!std::isfinite(delta)||delta<=0||delta>1024||duration<=0)return out;
  timer+=delta;
  while(cursor<keys.size()&&keys[cursor].first<int(timer)){
   const auto key=keys[cursor++];out.push_back(key.second);
   if(loop&&key.second==1){
    int start=0;for(unsigned n=0;n<cursor;++n)if(keys[n].second==0)start=keys[n].first;
    timer=float(start);cursor=0;
    while(cursor<keys.size()&&keys[cursor].first<start)++cursor;
    return out;
   }
  }
  if(timer>=duration){timer=float(duration-1);completed=true;out.push_back(1000);}
  return out;
 }
};
inline bool delivered(const std::vector<int>& keys,int type){for(int k:keys)if(k==type)return true;return false;}
}}
