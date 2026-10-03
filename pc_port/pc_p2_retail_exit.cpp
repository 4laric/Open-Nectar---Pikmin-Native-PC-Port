#include "pc_p2_retail_exit.h"
#include "Creature.h"
#include "Graphics.h"
#include "Camera.h"
#include "MapMgr.h"
#include "sysNew.h"
#include "Dolphin/gx.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
namespace {
bool reject(std::string& e,const char* text){e=text;return false;}
bool same(const p2retail::Snapshot& a,const p2retail::Snapshot& b){
 return a.scene==b.scene&&a.cave==b.cave&&a.floor==b.floor&&a.maxFloor==b.maxFloor
  &&a.source==b.source&&a.sourceSha256==b.sourceSha256&&a.catalogSha256==b.catalogSha256
  &&a.story==b.story&&a.inCave==b.inCave;
}
class ExitBody final:public Creature {
public:
 explicit ExitBody(const p2retail::ExitSpec& spec):Creature(nullptr),geyser(spec.transition=="geyser"){
  mObjType=OBJTYPE_NULL;mHealth=1;
  init(Vector3f(spec.anchor.x,spec.anchor.y,spec.anchor.z));mSearchContext.exit();
  mSRT.r.set(0,spec.anchor.yawDegrees*0.01745329251994329577f,0);mFaceDirection=mSRT.r.y;
 }
 bool isOrganic()override{return false;}bool isAtari()override{return false;}
 bool isFixed()override{return true;}bool needShadow()override{return false;}
 void update()override{}
 bool available()const{return live;}
 void refresh(Graphics& gfx)override{
  const auto old=gfx.mPrimaryColour,aux=gfx.mAuxiliaryColour;
  const int blend=gfx.setCBlending(BLEND_Alpha),cull=gfx.mCullMode;
  const bool depth=gfx.setDepth(true),light=gfx.setLighting(false,nullptr);
  auto* texture=gfx.mActiveTexture[0];gfx.useMaterial(nullptr);gfx.useTexture(nullptr,0);
  gfx.useMatrix(gfx.mCamera->mLookAtMtx,0);gfx.setCullFront(2);
#if PIKI_USE_DGX
  GXSetChanCtrl(GX_COLOR0A0,GX_FALSE,GX_SRC_REG,GX_SRC_VTX,0,GX_DF_NONE,GX_AF_NONE);
#endif
  auto p=[&](float angle,float radius,float h){return Vector3f(mSRT.t.x+std::cos(angle)*radius,mSRT.t.y+h,mSRT.t.z+std::sin(angle)*radius);};
  auto tri=[&](Vector3f a,Vector3f b,Vector3f c,Colour color){const Vector3f v[]={a,b,c};const Vector2f uv[]={Vector2f(0,0),Vector2f(0,0),Vector2f(0,0)};gfx.setColour(color,true);gfx.drawOneTri(v,nullptr,uv,3);};
  for(unsigned i=0;i<24;++i){const float t=mFaceDirection+i*6.28318530718f/24,u=mFaceDirection+(i+1)*6.28318530718f/24;
   const auto a=p(t,36,15),b=p(u,36,15),c=p(t,52,18.8f),d=p(u,52,18.8f);
   tri(a,b,d,Colour(188,192,194,255));tri(a,d,c,Colour(160,164,170,255));
   tri(c,d,p(u,59,0),Colour(91,97,105,255));tri(c,p(u,59,0),p(t,59,0),Colour(91,97,105,255));
   tri(Vector3f(mSRT.t.x,mSRT.t.y+14,mSRT.t.z),b,a,Colour(15,20,27,255));
   if(geyser){const auto j=p(t,12,15),k=p(u,12,15),l=p(t,8,120),n=p(u,8,120);
    tri(j,k,n,Colour(66,197,245,255));tri(j,n,l,Colour(95,221,255,255));}
  }
  gfx.setColour(old,true);gfx.mAuxiliaryColour=aux;gfx.setCBlending(blend);
  gfx.useTexture(texture,0);gfx.setLighting(light,nullptr);gfx.setDepth(depth);gfx.setCullFront(cull);
 }
protected:void doKill()override{live=false;}
private:bool geyser,live=true;
};
struct HeapScope {int previous;HeapScope():previous(gsys->setHeap(SYSHEAP_App)){}~HeapScope(){gsys->setHeap(previous);}};
}
namespace p2retail {
struct ExitLifecycle::Impl {
 const SceneContext* context=nullptr;StageInfo* stage=nullptr;MapMgr* map=nullptr;
 ExitSpec spec;ExitBody* actor=nullptr;bool committed=false;
 bool current(std::string& error)const{
  const auto* live=pc_p2_retail_scene_prepared();
  if(!context||live!=context||live->stage()!=stage||live->map()!=map||mapMgr!=map
   ||live->selectionRevision()!=spec.revision||live->nativeSerial()!=spec.floor.scene.serial
   ||live->campaignSha256()!=spec.campaign||live->sessionSha256()!=spec.session
   ||live->plan().authenticatedBytes!=spec.planBytes||!same(live->snapshot(),spec.floor))
   return reject(error,"retail_exit_stale_owned_context");
  if(actor&&(actor->mSRT.t.x!=spec.anchor.x||actor->mSRT.t.y!=spec.anchor.y||actor->mSRT.t.z!=spec.anchor.z
   ||actor->mSRT.r.x!=0||actor->mSRT.r.z!=0||actor->mSRT.r.y!=spec.anchor.yawDegrees*0.01745329251994329577f
   ||actor->mHealth!=1))return reject(error,"retail_exit_live_body_transform");
  error.clear();return true;
 }
};
ExitLifecycle::ExitLifecycle():m(new Impl){}
ExitLifecycle::~ExitLifecycle(){if(m->context||m->actor)std::abort();}
bool ExitLifecycle::preflight(const SceneContext& context,std::string& error){
 if(m->context||m->actor||!gsys||!context.stage()||!context.map()||!context.routes()
  ||pc_p2_retail_scene_prepared()!=&context||mapMgr!=context.map()||!context.startsGrounded()
  ||context.phase()!=ScenePhase::Prepared)return reject(error,"retail_exit_preflight_owner");
 ExitSpec spec;if(!exitSpec(context.plan(),context.snapshot(),context.campaignSha256(),context.sessionSha256(),context.selectionRevision(),spec,error))return false;
#if defined(PIKI_PC_PORT)
 float height=0;auto* triangle=context.map()->getStaticGroundBelow(spec.anchor.x,spec.anchor.z,spec.anchor.y+1,height);
 if(!triangle||!std::isfinite(height)||std::fabs(height-spec.anchor.y)>10)return reject(error,"retail_exit_literal_ground_absent");
#else
 return reject(error,"retail_exit_requires_native_static_ground");
#endif
 m->context=&context;m->stage=context.stage();m->map=context.map();m->spec=std::move(spec);
 error.clear();return true;
}
bool ExitLifecycle::birth(std::string& error){
 if(m->actor||m->committed||!m->current(error))return reject(error,"retail_exit_birth_order_or_context");
 HeapScope heap;m->actor=new ExitBody(m->spec);
 std::printf("P2_RETAIL_EXIT_BIRTH cave=%s floor=%u serial=%llu unit=%u slot=%u kind=%s xyz=%g,%g,%g authored_presentation=1 boundary=unavailable\n",
  m->spec.floor.cave.c_str(),m->spec.floor.floor,(unsigned long long)m->spec.floor.scene.serial,
  m->spec.anchor.unit,m->spec.anchor.slot,m->spec.transition.c_str(),m->spec.anchor.x,m->spec.anchor.y,m->spec.anchor.z);
 error.clear();return true;
}
bool ExitLifecycle::commit(const Snapshot& floor,std::string& error){
 if(!m->actor||!m->actor->available()||m->committed||!same(floor,m->spec.floor)||!m->current(error))return reject(error,"retail_exit_commit_context");
 m->committed=true;error.clear();return true;
}
bool ExitLifecycle::canRelease(std::string& error)const{
 if(!m->context){error.clear();return true;}return m->current(error);
}
bool ExitLifecycle::release(std::string& error){
 if(!canRelease(error))return false;
 delete m->actor;m->actor=nullptr;m->context=nullptr;m->stage=nullptr;m->map=nullptr;m->spec={};m->committed=false;
 error.clear();return true;
}
bool ExitLifecycle::identity(const Creature* actor,ExitSpec& out)const{
 std::string error;if(!actor||actor!=m->actor||!m->actor->available()||!m->current(error))return false;out=m->spec;return true;
}
Creature* ExitLifecycle::body()const noexcept{return m->actor;}
bool ExitLifecycle::owned(const SceneContext& context)const noexcept{
 std::string error;return &context==m->context&&m->actor&&m->actor->available()&&m->current(error);
}
void ExitLifecycle::draw(Graphics& gfx){
 std::string error;if(!m->committed||!m->actor||!m->actor->available()||!gfx.mCamera||!m->current(error))return;
 gfx.setPerspective(gfx.mCamera->mPerspectiveMatrix.mMtx,gfx.mCamera->mFov,gfx.mCamera->mAspectRatio,gfx.mCamera->mNear,gfx.mCamera->mFar,1.f);
 m->actor->refresh(gfx);
}
}
namespace {
p2retail::ExitLifecycle& exitOwner(){static p2retail::ExitLifecycle owner;return owner;}
const p2retail::SceneContext* ownerContext=nullptr;
bool contextMatches(const p2retail::SceneContext& context,std::string& error){
 if(ownerContext!=&context)return reject(error,"retail_exit_foreign_lifecycle_context");
 return true;
}
}
bool pc_p2_retail_exit_preflight(const p2retail::SceneContext& context,std::string& error){
 if(ownerContext)return reject(error,"retail_exit_already_prepared");
 if(!exitOwner().preflight(context,error))return false;
 ownerContext=&context;return true;
}
bool pc_p2_retail_exit_birth(const p2retail::SceneContext& context,std::string& error){
 return contextMatches(context,error)&&exitOwner().birth(error);
}
bool pc_p2_retail_exit_commit(const p2retail::SceneContext& context,std::string& error){
 return contextMatches(context,error)&&exitOwner().commit(context.snapshot(),error);
}
bool pc_p2_retail_exit_owned(const p2retail::SceneContext& context)noexcept{
 return ownerContext==&context&&exitOwner().owned(context);
}
bool pc_p2_retail_exit_can_release(const p2retail::SceneContext& context,std::string& error){
 return contextMatches(context,error)&&exitOwner().canRelease(error);
}
bool pc_p2_retail_exit_release(const p2retail::SceneContext& context,std::string& error){
 if(!contextMatches(context,error)||!exitOwner().release(error))return false;
 ownerContext=nullptr;return true;
}
void pc_p2_retail_exit_draw(Graphics& gfx){exitOwner().draw(gfx);}
