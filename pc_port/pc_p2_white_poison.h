#pragma once
class BTeki;
class Creature;

void pc_p2_white_poison_setup();
void pc_p2_white_poison_forget(BTeki* predator);
bool pc_p2_white_poison_predator(BTeki* predator);
bool pc_p2_white_poison_prepare(BTeki* predator, Creature* victim);
bool pc_p2_white_poison_finish(BTeki* predator, const Creature* victim, bool consumed);

void pc_p2_white_poison_reset();

#include <vector>
#include <utility>
#include <string>
// Local actor references require stable-ID resolution before encoding.
struct PcP2WhitePoisonCheckpoint {
    bool enabled = false;
    float damage = 0;
    std::vector<unsigned> predatorGenerators;
    std::vector<const BTeki*> predators;
    std::vector<std::pair<const void*, const void*>> pendingTokens;
};
bool pc_p2_white_poison_capture(PcP2WhitePoisonCheckpoint&, std::string& error);
