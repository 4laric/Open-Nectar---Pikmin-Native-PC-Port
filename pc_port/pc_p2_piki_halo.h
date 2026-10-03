#pragma once
#include "pc_p2_piki_jpa_bank.h"
#include <array>
#include <functional>
#include <list>
namespace p2original { namespace pikiJPA {
struct Position {float x=0,y=0,z=0;};
struct ContextId {const void* body=nullptr;std::uint64_t nativeLifetime=0;};
struct HaloParticle {Position position;unsigned species=0;float scale=1;std::uint16_t angle=0;};
class HaloEffects {
public:
 using Clipped=std::function<bool(Position,unsigned resourceId)>;
 // Explicit seeds are the actual JPA manager's six create-emitter results.
 // Caller retains selected scene identity and ticks only the Active scene.
 bool prepare(const Bank&,const std::array<std::uint32_t,6>&,std::size_t capacity,std::string&);
 bool sharedIdleHalo(ContextId,unsigned species,unsigned actualJPAId,Position,std::string&);
 bool follow(ContextId,Position,std::string&);
 bool removeIdleHalo(ContextId,std::string&);
 bool sourceFrame(const Clipped&,std::string&);
 const std::vector<HaloParticle>& particles()const{return mParticles;}
 std::size_t owners()const{return mContexts.size();}
 std::uint32_t seed(unsigned species)const{return mSeeds.at(species);}
 const Bank& bank()const{return mBank;}
private:
 struct Context {ContextId id;unsigned species;Position position;};
 Bank mBank;std::array<std::uint32_t,6> mSeeds{};std::size_t mCapacity=0;
 std::list<Context> mContexts;std::vector<HaloParticle> mParticles;
};
} }
