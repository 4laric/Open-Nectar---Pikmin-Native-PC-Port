#pragma once
#include "pc_p2_watage_effect.h"
#include <memory>
class Graphics;
namespace p2watage {
// One owner per native plant; no pointer into the plant is retained by bursts.
class NativeEffect {
public:
 NativeEffect();~NativeEffect();
 bool load(std::string&);
 bool touch(Vec position,std::string&);
 bool tick(float seconds,std::string&);
 void draw(Graphics&);
 std::size_t particles()const;
 unsigned emissions()const;
private:
 struct Impl;std::unique_ptr<Impl> m;
};
}
