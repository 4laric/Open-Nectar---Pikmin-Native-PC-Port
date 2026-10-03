#include "pc_p2_piki_halo.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
namespace p2original { namespace pikiJPA {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool finite(Position p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool equal(ContextId a,ContextId b){return a.body==b.body&&a.nativeLifetime==b.nativeLifetime;}
std::string role(unsigned pid){char b[32];std::snprintf(b,sizeof(b),"piki-%04x.jpa",pid);return b;}
float random(std::uint32_t& s){s=s*0x19660du+0x3c6ef35fu;std::uint32_t b=(s>>9)|0x3f800000;float f;std::memcpy(&f,&b,4);return f-1;}
float zp(std::uint32_t& s){return 2*random(s)-1;}
float f32(const std::vector<unsigned char>& b,unsigned i){std::uint32_t bits=(std::uint32_t(b[i])<<24)|(unsigned(b[i+1])<<16)|(unsigned(b[i+2])<<8)|b[i+3];float f;std::memcpy(&f,&bits,4);return f;}
}
bool HaloEffects::prepare(const Bank& b,const std::array<std::uint32_t,6>& seeds,std::size_t capacity,std::string& e){
 if(!mContexts.empty()||!mParticles.empty())return fail(e,"Piki halo selected bank reload with retained owners/particles");
 // Source BaseGameSection owns a 2000-entry particle pool. The selected halo
 // backend admits no other particle effects, so births are bounded by it.
 if(!capacity||capacity>2000||!b.bytes("IP2_ringhalo_i.tex1"))return fail(e,"Piki halo selected texture/capacity absent");
 mBank=b;mSeeds=seeds;mCapacity=capacity;e.clear();return true;
}
bool HaloEffects::sharedIdleHalo(ContextId id,unsigned species,unsigned pid,Position p,std::string& e){
 if(!mCapacity||!id.body||!id.nativeLifetime||!finite(p)||!pid||pid!=haloId(species)||!mBank.bytes(role(pid)))return fail(e,"Piki halo unprepared or mismatched actual context/species/resource");
 for(auto i=mContexts.begin();i!=mContexts.end();++i)if(i->id.body==id.body){
  auto& c=*i;
  if(!equal(c.id,id)||c.species!=species)return fail(e,"Piki halo retained body lifetime/species conflict");
  c.position=p;mContexts.splice(mContexts.end(),mContexts,i);e.clear();return true;
 }
 if(mContexts.size()==mCapacity)return fail(e,"Piki halo retained context capacity exhausted");
 // CNode::add appends; callback traverses the retained source order.
 mContexts.push_back({id,species,p});e.clear();return true;
}
bool HaloEffects::follow(ContextId id,Position p,std::string& e){
 if(!finite(p))return fail(e,"Piki halo nonfinite actual context position");
 for(auto& c:mContexts)if(equal(c.id,id)){c.position=p;e.clear();return true;}
 return fail(e,"Piki halo actual retained context absent");
}
bool HaloEffects::removeIdleHalo(ContextId id,std::string& e){
 for(auto i=mContexts.begin();i!=mContexts.end();++i)if(equal(i->id,id)){mContexts.erase(i);e.clear();return true;}
 return fail(e,"Piki halo delete requires exact retained native lifetime");
}
bool HaloEffects::sourceFrame(const Clipped& clipped,std::string& e){
 if(!mCapacity||!clipped)return fail(e,"Piki halo source frame requires selected resources and scene resource clipping");
 // Lifetime1 particles die at age1. New callback births reach age0 during the
 // same JPAResource::calc pass. Erasing a context leaves its preceding particle
 // intact until this actual frame, without killing other contexts' emitter.
 // Allocate before either lifetime removal or any emitter RNG advance. Each
 // admitted context creates at most one particle; inserts cannot allocate.
 try{mParticles.reserve(mContexts.size());}
 catch(const std::bad_alloc&){return fail(e,"Piki halo particle storage allocation failed");}
 mParticles.clear();
 for(unsigned species=0;species<6;++species){
  auto* resource=mBank.bytes(role(haloId(species)));if(!resource)continue;
  auto& seed=mSeeds[species];(void)zp(seed); // rateRandom0 still draws
  for(const auto& c:mContexts){if(c.species!=species||clipped(c.position,haloId(species)))continue;
   for(unsigned i=0;i<3;++i)(void)random(seed); // JPAVolumePoint ZH
   (void)random(seed); // lifetimeRandom0
   (void)zp(seed); // initial velocity ratio
   (void)random(seed); // momentRandom0
   (void)random(seed); // color loop offset
   constexpr unsigned esp=196;
   HaloParticle p;p.position=c.position;p.species=species;
   p.scale=1+zp(seed)*f32(*resource,esp+0x24);
   p.angle=std::uint16_t(int(f32(*resource,esp+0x4c)+f32(*resource,esp+0x50)*(random(seed)-.5f)));
   auto spin=std::int16_t(f32(*resource,esp+0x54)*(1+f32(*resource,esp+0x58)*zp(seed)));
   if(zp(seed)>=f32(*resource,esp+0x5c))spin=std::int16_t(-spin);
   p.angle=std::uint16_t(p.angle+spin);mParticles.insert(mParticles.begin(),p);
  }
 }
 e.clear();return true;
}
} }
