#include "pc_p2_piki_jpa_native.h"
#include "pc_p2_piki_jpa_owner_guard.h"
#include "Graphics.h"
#include "Camera.h"
#include "Dolphin/gx.h"
#include "gl/pc_gfx.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <list>
namespace p2original { namespace pikiJPA {
namespace {
ContextId id(const PcP2SourceBody& b){return {b.nativeBody,b.nativeLifetime};}
bool same(const PcP2SourceBody& a,const PcP2SourceBody& b){return a.nativeBody==b.nativeBody&&a.nativeLifetime==b.nativeLifetime;}
bool finite(Position p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
std::string role(unsigned pid){char b[32];std::snprintf(b,sizeof(b),"piki-%04x.jpa",pid);return b;}
float f32(const std::vector<unsigned char>& b,unsigned i){std::uint32_t bits=(std::uint32_t(b[i])<<24)|(unsigned(b[i+1])<<16)|(unsigned(b[i+2])<<8)|b[i+3];float f;std::memcpy(&f,&bits,4);return f;}
bool reject(std::string& e,const char* reason){e=reason;return false;}
}
struct NativeEffects::Impl {
 Scene& scene;HaloEffects halo;GXTexObj texture{};bool attached=false;std::string lastCleanupRefusal;
 struct Owner {PcP2SourceBody body;unsigned species;};std::list<Owner> owners;
 explicit Impl(Scene& s):scene(s){}
 bool current(std::string& e)const{return scene.selectedCurrent(halo.bank().selected(),e);}
 bool resolve(const PcP2SourceBody& b,Position& p,std::string& e)const{
  if(!b.nativeBody||!b.nativeLifetime||b.kind==PcP2SourceBodyKind::None||b.kind==PcP2SourceBodyKind::Unavailable)return reject(e,"Piki JPA absent authoritative source body lifetime");
  if(!current(e)||!scene.position(b,p,e))return false;
  if(!finite(p))return reject(e,"Piki JPA nonfinite actual scene position");
  e.clear();return true;
 }
};
NativeEffects::NativeEffects(Scene& s):m(std::make_unique<Impl>(s)){}
NativeEffects::~NativeEffects(){
 detail::requireRetiredOwners(m->owners.size(),m->halo.owners(),m->lastCleanupRefusal.c_str());
 if(m->attached)pc_gfx_release_texture(&m->texture);
}
bool NativeEffects::prepare(const Bank& b,const std::array<std::uint32_t,6>& seeds,std::size_t capacity,std::string& e){
 if(!m->owners.empty())return reject(e,"Piki JPA prepare with retained native contexts");
 if(!m->scene.selectedCurrent(b.selected(),e)||!m->halo.prepare(b,seeds,capacity,e))return false;
 if(m->attached)pc_gfx_release_texture(&m->texture);
 // Authenticated GPVE01 ring TEX1: genuine 64x64 I8, image at64.
 auto* t=m->halo.bank().bytes("IP2_ringhalo_i.tex1");
 GXInitTexObj(&m->texture,const_cast<unsigned char*>(t->data()+64),64,64,GX_TF_I8,GX_CLAMP,GX_CLAMP,GX_FALSE);
 GXInitTexObjLOD(&m->texture,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);m->attached=true;e.clear();return true;
}
bool NativeEffects::sharedIdleHalo(const PcP2SourceBody& body,unsigned species,unsigned pid,std::string& e){
 Position p;if(!m->resolve(body,p,e))return false;
 if(species!=body.state.species)return reject(e,"Piki JPA species differs from authoritative retained body");
 for(const auto& o:m->owners)if(o.body.nativeBody==body.nativeBody){
  if(!same(o.body,body)||o.species!=species)return reject(e,"Piki JPA retained body incarnation/species conflict");
  return m->halo.sharedIdleHalo(id(body),species,pid,p,e);
 }
 // Acquire the native retained record before core registration so allocation
 // exceptions cannot leave an untracked core context.
 m->owners.push_back({body,species});
 if(!m->halo.sharedIdleHalo(id(body),species,pid,p,e)){m->owners.pop_back();return false;}
 e.clear();return true;
}
bool NativeEffects::canRemoveIdleHalo(const PcP2SourceBody& body,std::string& e)const{
 for(const auto& o:m->owners)if(same(o.body,body)){Position p;return m->resolve(o.body,p,e);}
 return reject(e,"Piki JPA removal requires actual retained context lifetime");
}
bool NativeEffects::removeIdleHalo(const PcP2SourceBody& body,std::string& e){
 if(!canRemoveIdleHalo(body,e)){m->lastCleanupRefusal=e;return false;}
 if(!m->halo.removeIdleHalo(id(body),e)){m->lastCleanupRefusal=e;return false;}
 for(auto i=m->owners.begin();i!=m->owners.end();++i)if(same(i->body,body)){m->owners.erase(i);break;}
 e.clear();return true;
}
bool NativeEffects::sourceFrame(std::string& e){
 if(!m->scene.active()||!m->attached)return reject(e,"Piki JPA source clock requires actual Active selected scene");
 if(!m->current(e))return false;
 struct Query {ContextId context;Position p;unsigned pid;bool clipped;};std::vector<Query> queries;
 // Refusal retains owners and clock. Do not partially update before all actual
 // source/lifetime and clipping callbacks are resolved.
 for(const auto& o:m->owners){Query q{id(o.body),{},haloId(o.species),false};
  if(!m->resolve(o.body,q.p,e)||!m->scene.clipped(q.p,q.pid,q.clipped,e))return false;
  queries.push_back(q);
 }
 for(const auto& q:queries)if(!m->halo.follow(q.context,q.p,e))return false;
 return m->halo.sourceFrame([&](Position p,unsigned pid){for(const auto& q:queries)if(q.pid==pid&&q.p.x==p.x&&q.p.y==p.y&&q.p.z==p.z)return q.clipped;return true;},e);
}
bool NativeEffects::draw(Graphics& g,std::string& e){
 if(!m->scene.active()||!m->attached||!g.mCamera)return reject(e,"Piki JPA draw requires actual Active selected scene and camera");
 if(!m->current(e))return false;
 for(const auto& o:m->owners){Position p;if(!m->resolve(o.body,p,e))return false;}
 if(m->halo.particles().empty()){e.clear();return true;}
 // Snapshot exact transport state before Graphics helpers overwrite it.
 const auto pipeline=pc_gfx_get_pipeline_state();
 if(!m->scene.beginHaloDraw(g,e))return false;
 bool light=g.setLighting(false,nullptr);int blend=g.setCBlending(BLEND_Alpha);int cull=g.mCullMode;g.setCullFront(2);bool depth=g.setDepth(false);
 g.useMatrix(g.mCamera->mLookAtMtx,0);g.useTexture(nullptr,0);GXLoadTexObj(&m->texture,GX_TEXMAP0);
 GXSetNumTevStages(1);GXSetNumTexGens(1);GXSetTexCoordGen2(GX_TEXCOORD0,GX_TG_MTX2X4,GX_TG_TEX0,GX_IDENTITY,GX_FALSE,GX_PTIDENTITY);
 GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR_NULL);GXSetNumIndStages(0);GXSetTevDirect(GX_TEVSTAGE0);GXSetTevDirect(GX_TEVSTAGE1);
 GXSetTevColorIn(GX_TEVSTAGE0,GX_CC_C1,GX_CC_C0,GX_CC_TEXC,GX_CC_ZERO);GXSetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_TEXA,GX_CA_A0,GX_CA_ZERO);
 GXSetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);GXSetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
 // Literal halo BSP1 0459/e7/ref0/z25, source alpha always, additive,
 // LEQUAL without depth writes. Genuine shape2 rotated billboard.
 GXSetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_ONE,GX_LO_SET);GXSetZMode(GX_TRUE,GX_LEQUAL,GX_FALSE);GXSetZCompLoc(GX_TRUE);
 GXSetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);GXSetCullMode(GX_CULL_NONE);
 GXClearVtxDesc();GXSetVtxDesc(GX_VA_POS,GX_DIRECT);GXSetVtxDesc(GX_VA_TEX0,GX_DIRECT);
 GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
 const auto& camera=g.mCamera->mLookAtMtx;
 for(const auto& p:m->halo.particles()){
  const auto& r=*m->halo.bank().bytes(role(haloId(p.species)));constexpr unsigned bsp=132;
  // Lifetime1 reaches age0 only: source primary color table key0 at BSP+34.
  GXSetTevColor(GX_TEVREG0,GXColor{r[bsp+0x36],r[bsp+0x37],r[bsp+0x38],r[bsp+0x39]});
  GXSetTevColor(GX_TEVREG1,GXColor{r[bsp+0x2a],r[bsp+0x2b],r[bsp+0x2c],r[bsp+0x2d]});
  float theta=float(p.angle>>5)*6.2831853071795864769f/2048,c=std::cos(theta),s=std::sin(theta);
  float halfX=25*f32(r,bsp+0x10)*p.scale,halfY=25*f32(r,bsp+0x14)*p.scale;
  GXBegin(GX_QUADS,GX_VTXFMT0,4);
  for(int i=0;i<4;++i){float x=(i==0||i==3)?-halfX:halfX,y=i<2?halfY:-halfY;float a=c*x-s*y,h=s*x+c*y;
   GXPosition3f32(p.position.x+camera.mMtx[0][0]*a+camera.mMtx[1][0]*h,p.position.y+camera.mMtx[0][1]*a+camera.mMtx[1][1]*h,p.position.z+camera.mMtx[0][2]*a+camera.mMtx[1][2]*h);
   GXTexCoord2f32((i==0||i==3)?0:1,i<2?0:1);
  }GXEnd();
 }
 g.useTexture(nullptr,0);g.setCullFront(cull);g.setCBlending(blend);g.setDepth(depth);g.setLighting(light,nullptr);pc_gfx_set_pipeline_state(pipeline);
 // The renderer-owner boundary is required to restore fields outside the
 // exposed transport snapshot; a refused end keeps every body owner intact.
 if(!m->scene.endHaloDraw(g,e))return false;
 e.clear();return true;
}
bool NativeEffects::sharedNageKira(const PcP2SourceBody&,std::string& e){return reject(e,"P2_PIKI_NAGEKIRA_UNSUPPORTED: source sphere dynamics/ESP and shared fade backend required");}
bool NativeEffects::perbodyNageBlur(const PcP2SourceBody&,unsigned,Matrix4f&,std::string& e){return reject(e,"P2_PIKI_NAGEBLUR_UNSUPPORTED: source stripe-X/ETX1 secondary texture pipeline required");}
bool NativeEffects::voice(const PcP2SourceBody&,unsigned,unsigned,std::string& e){return reject(e,"P2_PIKI_PSM_UNSUPPORTED: selected AAF/ASN sequence/instrument/wave sound-ID routing required");}
std::size_t NativeEffects::owners()const{return m->owners.size();}
} }
