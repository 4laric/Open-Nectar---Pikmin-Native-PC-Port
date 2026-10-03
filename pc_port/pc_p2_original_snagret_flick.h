#pragma once

namespace p2original { namespace snagret_flick {
// Retail SnakeCrow general fp16..19; keep AP/P1 adapter values independent.
constexpr float chance=1.0f, knockback=200.0f, damage=1.0f, range=40.0f;
constexpr float pi=3.14159265358979323846f, tau=2*pi;
constexpr float nearbyAngle=-1000.0f+pi;
// EnemyFunc::flickStickPikmin calls roundAng once, even for the sentinel.
constexpr float stickerAngle=nearbyAngle+tau;
inline bool nearby(float x,float y,float z) {return x*x+y*y+z*z<range*range;}
template<class Host> void disappear(Host& h) {
    h.navis(range,knockback,damage,nearbyAngle);
    h.pikmin(range,knockback,nearbyAngle);
    h.stickers(chance,knockback,stickerAngle);
}
template<class Host> void cleanup(Host& h) {h.stickers(1.0f,10.0f,stickerAngle);}
}}
