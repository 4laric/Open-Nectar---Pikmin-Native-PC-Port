#pragma once
class BTeki;
class Creature;

void pc_p2_white_poison_setup();
bool pc_p2_white_poison_predator(BTeki* predator);
bool pc_p2_white_poison_prepare(BTeki* predator, Creature* victim);
bool pc_p2_white_poison_finish(BTeki* predator, const Creature* victim, bool consumed);
