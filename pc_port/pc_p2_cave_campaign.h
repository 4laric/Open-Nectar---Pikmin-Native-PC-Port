#pragma once
#include <string>
// Seed-owned ordinary section provider; historical seeds are inert.
void pc_p2_cave_campaign_prepare();
void pc_p2_cave_campaign_select_stage();
void pc_p2_cave_campaign_before_preload();
void pc_p2_cave_campaign_scene_setup();
void pc_p2_cave_campaign_scene_exit();
void pc_p2_cave_campaign_request();
void pc_p2_cave_campaign_tick();
bool pc_p2_cave_campaign_commit_transition();
bool pc_p2_cave_campaign_restored_party();
bool pc_p2_cave_campaign_owns_heads();
int pc_p2_cave_campaign_floor();
std::string pc_p2_cave_campaign_token();
// Retire surface living-body replay before ordinary sunset storage/deposit.
void pc_p2_cave_campaign_before_day_cleanup();
