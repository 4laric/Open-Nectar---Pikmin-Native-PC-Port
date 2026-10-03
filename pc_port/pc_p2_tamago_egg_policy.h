#pragma once
#include "pc_p2_tamago_egg.h"
#include "pc_p2_tamago_policy.h"
#include <cmath>

namespace p2tamago {
inline bool preflightHoney(const P2TamagoHoneyProvider& p,std::string& error) {
    if(!p.birth||!p.preflight){error="typed original Mitite Honey provider unavailable (#1252)";return false;}
    return p.preflight(p.context,error);
}
inline void retireMember(MemberFrontier& m) {
    m.retired=true;
    if(m.terminal==Terminal::None)m.terminal=Terminal::Scene;
}
inline bool retireHoney(MemberFrontier& m,bool consumed) {
    if(!m.honeyBorn||m.honeyRetired)return false;
    m.honeyRetired=true;m.honeyConsumed=m.honeyConsumed||consumed;return true;
}
inline bool consumeHoney(MemberFrontier& m) {
    if(!m.honeyBorn||m.honeyRetired||m.honeyConsumed)return false;
    m.honeyConsumed=true;return true;
}
template<class Draw,class Birth> bool attemptHoney(MemberFrontier& m,float rate,Draw draw,Birth birth) {
    if(m.honeyAttempted)return false;
    m.honeyAttempted=true;
    const float chance=draw(); // consumed even at rate1
    if(chance>rate)return true;
    const auto result=birth();
    m.honeyBorn=result==P2TamagoHoneyBirth::Born;
    m.honeyFault=result==P2TamagoHoneyBirth::ResourceError;
    return true;
}
inline bool orphanLeaderRole(unsigned member){return member==0;}
template<class Map,class Group> bool claimGroup(Map& groups,const GroupIdentity& id,const Group& group) {
    return groups.emplace(id,group).second; // includes null/partial births: attempt is consumed
}
inline bool valid(const EggGroupRequest& r) {
    const auto& id = r.identity.parent;
    if (r.identity.slot || id.catalog.size()!=64 || !id.generator || !id.activation) return false;
    for (char c : id.catalog) if (!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false;
    return std::isfinite(r.position.x)&&std::isfinite(r.position.y)&&std::isfinite(r.position.z)
        &&std::isfinite(r.velocity.x)&&std::isfinite(r.velocity.y)&&std::isfinite(r.velocity.z)&&std::isfinite(r.facing);
}
struct Initial {
    float activeMax, turnFactor, speedFactor, moveFactor;
    p2original::Position goal;
    int appearWait;
    float appearFrame;
};
// Tamago::onInit37-80, setGoalDirect436-446, StateAppear::init132-164.
// Every draw is sequenced, including state init immediately replaced by Wait.
template<class Draw> Initial initialize(const p2original::Position& p, float face, Draw draw) {
    Initial s;
    s.activeMax=p2tamagopolicy::activeMaxTicks(draw());
    s.turnFactor=0.7f+0.3f*draw();
    s.speedFactor=0.7f+0.3f*draw();
    s.moveFactor=0.3f+0.7f*draw();
    // Source TamagoMushi::Parms constructor: mNextGoalPosMaxRadius80.
    const float radius=80.0f*(0.5f*draw()+0.5f);
    s.goal=p; s.goal.x+=radius*std::sin(face); s.goal.z+=radius*std::cos(face);
    s.appearWait=int(p2tamagopolicy::randomTicks(p2tamagopolicy::AppearMinTicks,p2tamagopolicy::AppearMaxTicks,draw()));
    s.appearFrame=15.0f*draw();
    return s;
}
// Exact manager ordering: gate, leader birth/init/ball/velocity, self-leader,
// one initial radius RNG even though explode forces1, then9 follower attempts.
// IO init owns source onInit RNG; follower setLeader inherits Ball from leader.
template<class IO> int createEggGroup(const EggGroupRequest& r, IO& io) {
    if (!valid(r)||!io.available()||io.freeSlots()<10) return 0;
    if (!io.birth(0,r.position,r.facing)) return 0;
    io.init(0); io.ball(0); io.velocity(0,r.velocity); io.leader(0,0);
    const float discardedRadius=0.5f*io.draw()+0.5f; (void)discardedRadius;
    io.place(0,r.position,0.0f);
    int born=1;
    for (unsigned i=0;i<9;++i) {
        const float angle=6.28318531f*float(i)/10.0f;
        auto p=r.position; p.x-=10.0f*std::sin(angle); p.z-=10.0f*std::cos(angle);
        if (!io.birth(i+1,p,-angle)) continue;
        io.init(i+1); io.velocity(i+1,io.leaderVelocity()); io.leader(i+1,0); ++born;
    }
    return born;
}
}
