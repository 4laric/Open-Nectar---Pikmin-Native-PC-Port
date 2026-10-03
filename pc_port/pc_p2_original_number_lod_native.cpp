#include "pc_p2_original_number_lod_native.h"
#include "GameCoreSection.h"
#include "Navi.h"
#include "MoviePlayer.h"
#include "gameflow.h"
#include "netplay/pc_netplay_present.h"
#include "timing/pc_render_phase.h"

// Actual native sequencing: newPikiGame.cpp selects mSplitViews, renders,
// then Node::update -> GameCoreSection::update cameraMgr/updateCoopCameras,
// then updateAI -> pelletMgr->update. No renderer camera fallback here.
namespace {
enum class Phase { Empty,Published,Captured };
struct Views {
 Phase phase=Phase::Empty;
 GameCoreSection* owner=nullptr;
 p2originalnumber::lod::CameraSnapshot camera;
};
Views views;
void clear() noexcept { views=Views{}; }
bool fail(std::string& e,const char* reason){clear();e=reason;return false;}
bool supported(GameCoreSection* section,std::string& e){
 if(!section||!section->mNavi||!section->mNavi->mNaviCamera)return fail(e,"Number LOD real section/captain camera unavailable");
 if(pc_netplay_present_two_pass_active())return fail(e,"Number LOD source camera roster is not qualified for netplay");
 if(!pc_render_is_authoritative())return fail(e,"Number LOD camera capture outside authoritative simulation");
 if(section->mNavi2||section->mGameCamera2||section->mCameraMgr2)return fail(e,"Number LOD source camera roster is not qualified for split/co-op");
 if(!gameflow.mMoviePlayer)return fail(e,"Number LOD movie owner unavailable");
 if(gameflow.mMoviePlayer->mIsActive||gameflow.mMoviePlayer->mCamTransitionFactor>0)return fail(e,"Number LOD movie/transition viewport is not qualified");
 if(gameflow.mDemoFlags&CinePlayerFlags::NonGameMovie)return fail(e,"Number LOD non-game movie viewport is not qualified");
 if(gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return fail(e,"Number LOD current camera phase is paused/covered");
 if(GameCoreSection::inPause())return fail(e,"Number LOD current core AI phase is paused");
 e.clear();return true;
}
const char* reason(PcOriginalNumberViewScope scope){
 switch(scope){
 case PcOriginalNumberViewScope::OrdinarySingle:return "Number LOD ordinary roster invalid";
 case PcOriginalNumberViewScope::Split:return "Number LOD split viewport roster is not qualified";
 case PcOriginalNumberViewScope::Netplay:return "Number LOD netplay viewport roster is not qualified";
 case PcOriginalNumberViewScope::MovieOrTransition:return "Number LOD movie/transition viewport roster is not qualified";
 case PcOriginalNumberViewScope::NonGameMovie:return "Number LOD non-game movie viewport roster is not qualified";
 case PcOriginalNumberViewScope::Memcard:return "Number LOD memcard viewport roster is not qualified";
 case PcOriginalNumberViewScope::InitialSetup:return "Number LOD initial setup has no active AI viewport";
 case PcOriginalNumberViewScope::MissingOwner:return "Number LOD actual outer section unavailable";
 }
 return "Number LOD viewport scope invalid";
}
}
bool pc_p2_original_number_lod_publish_views(GameCoreSection* section,unsigned count,PcOriginalNumberViewScope scope,std::string& e){
 // Invalidate last tick BEFORE any refusal; a failed publication cannot reuse
 // old camera visibility/projection as this tick's physical policy.
 clear();
 if(scope!=PcOriginalNumberViewScope::OrdinarySingle||count!=1)return fail(e,reason(scope));
 if(!supported(section,e))return false;
 views.owner=section;views.phase=Phase::Published;e.clear();return true;
}
bool pc_p2_original_number_lod_capture_views(GameCoreSection* section,std::string& e){
 if(views.phase!=Phase::Published||views.owner!=section)return fail(e,"Number LOD capture lacks this section's actual published roster");
 if(!supported(section,e))return false;
 p2originalnumber::lod::CameraSnapshot snapshot;
 // Single ordinary GameCoreSection owns mNaviCamera. getViewCamera(0) is
 // intentionally avoided: its co-op merge setting can select mViewCam even
 // outside split mode, and it changes behavior by presentation phase.
 if(!p2originalnumber::lod::snapshotNativeCamera(section->mNavi->mNaviCamera,snapshot,e)){clear();return false;}
 views.camera=snapshot;views.phase=Phase::Captured;e.clear();return true;
}
bool pc_p2_original_number_lod_views_ready(std::string& e){
 if(views.phase!=Phase::Captured){e="Number LOD actual current simulation viewport not captured";return false;}
 if(!supported(views.owner,e))return false;
 e.clear();return true;
}
bool pc_p2_original_number_lod_current(unsigned number,p2originalnumber::lod::Vec3 center,bool pikiInCell,bool movieActor,p2originalnumber::lod::Result& out,std::string& e){
 if(!pc_p2_original_number_lod_views_ready(e))return false;
 p2originalnumber::lod::Sphere sphere;
 if(!p2originalnumber::lod::numberSphere(number,center,sphere,e))return false;
 // Viewable is established by the actual outer OrdinarySingle draw branch,
 // not synthesized from a camera visibility test or a default roster.
 p2originalnumber::lod::Viewport viewport{true,&views.camera};p2originalnumber::lod::Result result;
 if(!p2originalnumber::lod::evaluate(sphere,&viewport,1,pikiInCell,result,e))return false;
 if(movieActor)p2originalnumber::lod::movieActorOverride(result);
 out=result;e.clear();return true;
}
void pc_p2_original_number_lod_close_views(GameCoreSection* section)noexcept{if(views.owner==section)clear();}
void pc_p2_original_number_lod_release_views(GameCoreSection* section)noexcept{pc_p2_original_number_lod_close_views(section);}
