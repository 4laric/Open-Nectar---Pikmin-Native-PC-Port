#include "pc_p2_cave_visible.h"
#include "pc_p2_cave_visible_policy.h"
#include "pc_p2_cave_campaign_boundary.h"
#include "pc_p2_teki_lifetime.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Controller.h"
#include "Kontroller.h"
#include "Font.h"
#include "Graphics.h"
#include "Camera.h"
#include "MapMgr.h"
#include "MoviePlayer.h"
#include "gameflow.h"
#include "system.h"
#include <cstdio>

// The qualified source line may ship before the optional cave provider. A
// composed provider enables this only after linking the real transition owner.
#if defined(PIKMIN_P2_CAVE_CAMPAIGN_PROVIDER)
P2CaveBoundarySnapshot pc_p2_cave_campaign_boundary();
bool pc_p2_cave_campaign_request_boundary(const P2CaveBoundarySnapshot&);
__attribute__((weak)) bool pc_netplay_session_active();
namespace {
P2CaveVisibleInput input;
unsigned long drawnScene=0;
bool online(){return pc_netplay_session_active && pc_netplay_session_active();}
bool snapshot(P2CaveBoundarySnapshot& source,P2CaveVisibleBoundary& actor){
    if(!mapMgr || online())return false;
    source=pc_p2_cave_campaign_boundary();
    // A temporarily unsafe Walk/UI state keeps the same scene identity. This
    // preserves the latch across the real SAVE choice and cancellation.
    if(!source.sceneGeneration || source.sceneGeneration!=pc_p2_scene_generation()
        || source.cave!="forest_1" || source.token.empty())return false;
    actor.present=true;actor.ready=source.ready;actor.scene=source.sceneGeneration;
    actor.seed=source.seed;actor.floor=source.floor;actor.cave=source.cave;actor.token=source.token;
    actor.returning=source.action==P2CaveBoundaryAction::Return;
    actor.x=source.x;actor.z=source.z;actor.radius=source.radius;
    actor.y=mapMgr->getMinY(actor.x,actor.z,true);
    return actor.valid();
}
bool safe(Navi* n){
    return n && naviMgr && n==naviMgr->getActiveNavi() && n->mKontroller
        && n->getCurrState() && n->getCurrState()->getID()==NAVISTATE_Walk
        && std::isfinite(n->mHealth) && n->mHealth>1
        && !gameflow.mPauseAll && !gameflow.mIsUIOverlayActive && !gameflow.mIsDayEndActive
        && gameflow.mMoviePlayer && !gameflow.mMoviePlayer->mIsActive && !online();
}
Vector3f ring(const P2CaveVisibleBoundary& a,float angle,float r,float y){
    return Vector3f(a.x+std::cos(angle)*r,a.y+y,a.z+std::sin(angle)*r);
}
void tri(Graphics& gfx,const Vector3f& a,const Vector3f& b,const Vector3f& c,const Colour& color){
    const Vector3f vertices[]={a,b,c};const Vector2f uv[]={Vector2f(0,0),Vector2f(0,0),Vector2f(0,0)};
    gfx.setColour(color,true);gfx.drawOneTri(vertices,nullptr,uv,3);
}
void mesh(Graphics& gfx,const P2CaveVisibleBoundary& a){
    // Authored geometry, deliberately independent of legal imported meshes.
    // A recessed dark mouth with a solid irregular stone rim; the returning
    // actor adds a tall tapered water jet and a broad visible splash crown.
    constexpr int segments=24;constexpr float tau=6.28318530718f;
    for(int i=0;i<segments;++i){
        const float t=i*tau/segments,u=(i+1)*tau/segments;
        const float r=48.f+(i%3)*3.f,s=48.f+((i+1)%3)*3.f;
        const float h=11.f+(i%4)*2.f,k=11.f+((i+1)%4)*2.f;
        auto inner=ring(a,t,30,4),next=ring(a,u,30,4);
        auto outer=ring(a,t,r,h),outNext=ring(a,u,s,k);
        const Colour stone(i%2?Colour(139,123,99,255):Colour(166,149,118,255));
        tri(gfx,inner,next,outNext,stone);tri(gfx,inner,outNext,outer,stone);
        tri(gfx,outer,outNext,ring(a,u,s+7,1),Colour(89,78,61,255));
        tri(gfx,outer,ring(a,u,s+7,1),ring(a,t,r+7,1),Colour(89,78,61,255));
        tri(gfx,Vector3f(a.x,a.y+3,a.z),next,inner,Colour(17,21,27,255));
        if(a.returning){
            auto low=ring(a,t,16,5),lowNext=ring(a,u,16,5);
            auto high=ring(a,t,8,104),highNext=ring(a,u,8,104);
            tri(gfx,low,lowNext,highNext,Colour(75,195,239,255));
            tri(gfx,low,highNext,high,Colour(128,225,255,255));
            tri(gfx,high,highNext,ring(a,u,23,115+(i%3)*5),Colour(200,247,255,255));
        }
    }
}
}
bool pc_p2_cave_visible_interact(Navi* n){
    // Inactive/co-op captains must not reset the active captain's latch.
    if(!naviMgr || n!=naviMgr->getActiveNavi() || !n || !n->mKontroller)return false;
    P2CaveBoundarySnapshot source;P2CaveVisibleBoundary actor;
    if(!snapshot(source,actor)){input.reset();return false;}
    const bool click=input.sample(actor,n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z,
        n->mKontroller->keyDown(KBBTN_A),n->mKontroller->keyClick(KBBTN_A),safe(n));
    if(!click || !pc_p2_cave_campaign_request_boundary(source))return false;
    input.accepted();
    std::printf("P2_CAVE_VISIBLE_ACTIVATED action=%s scene=%lu ordinary_A=1 authored=1\n",
        actor.returning?"return":"enter",source.sceneGeneration);
    return true;
}
void pc_p2_cave_visible_reset(){input.reset();drawnScene=0;}
void pc_p2_cave_visible_draw(Graphics& gfx){
    P2CaveBoundarySnapshot source;P2CaveVisibleBoundary actor;
    if(!gfx.mCamera || !snapshot(source,actor))return;
    if(drawnScene!=source.sceneGeneration){drawnScene=source.sceneGeneration;
        std::printf("P2_CAVE_VISIBLE_DRAW kind=%s scene=%lu x=%.3f y=%.3f z=%.3f authored=1\n",
            actor.returning?"geyser":"hole",drawnScene,actor.x,actor.y,actor.z);}
    const Colour color=gfx.mPrimaryColour,aux=gfx.mAuxiliaryColour;
    const int blend=gfx.setCBlending(BLEND_Alpha),cull=gfx.setCullFront(2);
    const bool depth=gfx.setDepth(true);
    Texture* texture=gfx.mActiveTexture[0];const bool light=gfx.setLighting(false,nullptr);
    gfx.setPerspective(gfx.mCamera->mPerspectiveMatrix.mMtx,gfx.mCamera->mFov,
        gfx.mCamera->mAspectRatio,gfx.mCamera->mNear,gfx.mCamera->mFar,1.f);
    gfx.useMaterial(nullptr);gfx.useTexture(nullptr,0);gfx.useMatrix(gfx.mCamera->mLookAtMtx,0);
    mesh(gfx,actor);
    auto* n=naviMgr?naviMgr->getActiveNavi():nullptr;
    if(safe(n) && actor.ready && input.prompt() && actor.near(n->mSRT.t.x,n->mSRT.t.y,n->mSRT.t.z)
        && gsys && gsys->mConsFont){
        const char* label=actor.returning?"A: Return to surface":"A: Enter cave";
        gfx.setColour(Colour(255,255,230,255),true);
        gfx.perspPrintf(gsys->mConsFont,Vector3f(actor.x,actor.y+(actor.returning?142:62),actor.z),
            -gsys->mConsFont->stringWidth(const_cast<char*>(label))/2,0,"%s",label);
    }
    gfx.setColour(color,true);gfx.mAuxiliaryColour=aux;gfx.setCBlending(blend);
    gfx.useTexture(texture,0);gfx.setLighting(light,nullptr);gfx.setDepth(depth);gfx.setCullFront(cull);
}
#else
bool pc_p2_cave_visible_interact(Navi*){return false;}
void pc_p2_cave_visible_draw(Graphics&){}
void pc_p2_cave_visible_reset(){}
#endif
