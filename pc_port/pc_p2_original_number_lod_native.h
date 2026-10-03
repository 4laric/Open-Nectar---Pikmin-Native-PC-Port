#pragma once
#include "pc_p2_original_number_lod.h"
class GameCoreSection;

enum class PcOriginalNumberViewScope {
 OrdinarySingle,Split,Netplay,MovieOrTransition,NonGameMovie,Memcard,InitialSetup,MissingOwner
};
// Actual outer GameCourseSection publishes its already-selected roster once
// before Node::update; count comes from its real mSplitViews branch. This
// adapter never chooses a roster from Graphics' last-render camera.
bool pc_p2_original_number_lod_publish_views(GameCoreSection*,unsigned actualViewportCount,
 PcOriginalNumberViewScope,std::string& error);
// Actual outer owner calls immediately after Node::update (current Pcam
// cameras) and before gamecore->updateAI. Ordinary single viewport only;
// unsupported mode/read failures clear availability, not force Near/Far.
bool pc_p2_original_number_lod_capture_views(GameCoreSection*,std::string& error);
bool pc_p2_original_number_lod_views_ready(std::string& error);
// Valid only between capture and close, on the gameplay thread. Parent native
// binding authenticates number/body identity and supplies its actual current
// source CollTree center; the LOD adapter does not grant resource identity.
bool pc_p2_original_number_lod_current(unsigned number,p2originalnumber::lod::Vec3 actualCollTreeCenter,
 bool sourcePikiInCell,bool sourceMovieActor,p2originalnumber::lod::Result&,std::string& error);
// Close after updateAI, including skipped AI paths; release before section
// teardown. No frame counter or camera pointer survives a closed phase.
void pc_p2_original_number_lod_close_views(GameCoreSection*) noexcept;
void pc_p2_original_number_lod_release_views(GameCoreSection*) noexcept;
