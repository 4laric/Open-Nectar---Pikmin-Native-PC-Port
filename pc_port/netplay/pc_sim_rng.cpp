// Simulation vs cosmetic RNG for deterministic netplay, M1 (issue #878).
//
// Engine-free TU (see pc_netplay_det.cpp note).

#include "netplay/pc_sim_rng.h"
#include "netplay/pc_netplay_det.h"

namespace {

unsigned sSimState      = 1;
unsigned sCosmeticState = 0xC05E77u;

// MSL-compatible LCG step shared by both streams.
unsigned lcgNext(unsigned& state)
{
	state = state * 1103515245u + 12345u;
	return (state >> 16) & 0x7fffu;
}

} // namespace

int pc_sim_rand(void)
{
	if (!pc_netplay_deterministic()) return std::rand();
	return static_cast<int>(lcgNext(sSimState));
}

void pc_sim_srand(unsigned seed) { sSimState = seed; }

unsigned pc_sim_rng_state(void) { return sSimState; }

void pc_sim_rng_set_state(unsigned state) { sSimState = state; }

int pc_cosmetic_rand(void)
{
	if (!pc_netplay_deterministic()) return std::rand();
	return static_cast<int>(lcgNext(sCosmeticState));
}

void pc_cosmetic_srand(unsigned seed) { sCosmeticState = seed; }

unsigned pc_cosmetic_rng_state(void) { return sCosmeticState; }

void pc_cosmetic_rng_set_state(unsigned state) { sCosmeticState = state; }
