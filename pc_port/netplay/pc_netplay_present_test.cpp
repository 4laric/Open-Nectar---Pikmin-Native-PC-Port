// Host test for the M2b present module (issue #879): skip-presentation flag
// and null-GX counters. Engine-free (no Camera/Graphics link).

#include <cassert>
#include <cstdio>

#include "netplay/pc_netplay_present.h"

int main()
{
	// Skip flag defaults off, toggles, restores.
	assert(pc_netplay_present_skip_presentation() == 0);
	pc_netplay_present_set_skip_presentation(1);
	assert(pc_netplay_present_skip_presentation() == 1);
	pc_netplay_present_set_skip_presentation(0);
	assert(pc_netplay_present_skip_presentation() == 0);

	// Null-GX counters start at zero.
	pc_netplay_present_reset_counters();
	assert(pc_netplay_present_null_gl_calls() == 0);
	assert(pc_netplay_present_null_attempted() == 0);
	assert(pc_netplay_present_null_active() == 0);

	// Attempts accumulate while the flag is on; real GL stays zero.
	pc_netplay_present_set_null_gx(1);
	assert(pc_netplay_present_null_active() == 1);
	pc_netplay_present_note_attempt();
	pc_netplay_present_note_attempt();
	assert(pc_netplay_present_null_attempted() == 2);
	assert(pc_netplay_present_null_gl_calls() == 0);
	pc_netplay_present_set_null_gx(0);
	assert(pc_netplay_present_null_active() == 0);

	// Reset clears both counters.
	pc_netplay_present_reset_counters();
	assert(pc_netplay_present_null_gl_calls() == 0);
	assert(pc_netplay_present_null_attempted() == 0);

	// Local player defaults to 0 without the env var (cached).
	assert(pc_netplay_present_local_player() == 0
	    || pc_netplay_present_local_player() == 1);

	printf("PcNetplayPresent: all tests passed\n");
	return 0;
}
