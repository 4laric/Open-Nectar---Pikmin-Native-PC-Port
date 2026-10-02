#pragma once
// Gameplay-thread hooks; unavailable providers refuse visibly, never quit.
bool pc_midday_request_save_quit();
// true requests immediate exit from System::run before another gameplay tick.
bool pc_midday_save_quit_tick(unsigned completedFrame);
