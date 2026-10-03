#pragma once
#include <string>
#include <cstdint>
#include "pc_campaign_ui_observer.h"
class Controller;
class Graphics;
struct P2CaveSaveChoiceSnapshot {
    bool active=false;
    int state=-1,slot=-1;
    PcDefaultFileSnapshot defaultFile;
};
P2CaveSaveChoiceSnapshot pc_p2_cave_campaign_save_choice();
bool pc_p2_cave_campaign_update_save_choice(Controller* input);
void pc_p2_cave_campaign_draw_save_choice(Graphics& gfx);
// Seed-owned ordinary section provider; historical seeds are inert.
void pc_p2_cave_campaign_prepare();
// After native card loading, continue an authenticated mid-scene living SAVE.
bool pc_p2_cave_campaign_resume_scene();
void pc_p2_cave_campaign_select_stage();
void pc_p2_cave_campaign_before_preload();
void pc_p2_cave_campaign_scene_setup();
void pc_p2_cave_campaign_scene_exit();
void pc_p2_cave_campaign_request();
void pc_p2_cave_campaign_tick();
bool pc_p2_cave_campaign_commit_transition();
bool pc_p2_cave_campaign_restored_party();
bool pc_p2_cave_campaign_owns_heads();
bool pc_p2_cave_campaign_survivor_permit(const std::string& sourceKey,
    std::uint32_t recordUid,std::uint32_t attempt,std::uint64_t activation,
    const std::string& catalogFingerprint,std::uint64_t* generation,std::uint8_t sha[32]);
int pc_p2_cave_campaign_floor();
std::string pc_p2_cave_campaign_token();
// Retire surface living-body replay before ordinary sunset storage/deposit.
void pc_p2_cave_campaign_before_day_cleanup();
