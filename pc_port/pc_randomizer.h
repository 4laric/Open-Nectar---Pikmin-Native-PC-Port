#pragma once
// Standalone file-IPC adapter. No game state is touched before validation.
bool pc_randomizer_init(int argc, char** argv);
bool pc_randomizer_enabled();
void pc_randomizer_update();
bool pc_randomizer_ready();
bool pc_randomizer_has(const char* name);
bool pc_randomizer_checked(const char* name);
void pc_randomizer_check(const char* name);
bool pc_randomizer_goal();
int pc_randomizer_repairs();
const char* pc_randomizer_save_root();
// Repeat the last safe diary day; never index past vanilla's 30-entry arrays.
int pc_randomizer_next_day(int day);
