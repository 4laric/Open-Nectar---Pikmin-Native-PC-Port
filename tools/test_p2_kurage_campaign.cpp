// Kurage57 campaign contract (#871): engine-free pins for the campaign fixes.
//
//   * campaign mode = seed bridge WITHOUT the room preview. Only there are
//     the fixture concessions (captain park, recruit ring, forced modes,
//     carry/pellet mutations, ground pin + seek) skipped.
//   * fixture mode = room preview or any non-bridge run; concessions stay.
//   * hover height floats low (30-60u) while the host body stays grounded.
//   * corpse probe fires about once per second (every 60 frames) and never on
//     tick 0, so the tail cannot flood the probe log.
//
// Exit 0 only if every check passes; any failure prints FAIL and exits 1.
#include "pc_p2_kurage_campaign.h"

#include <cstdio>

namespace {

int failures = 0;

#define CHECK(cond, name) do { \
    if (cond) { std::printf("PASS %s\n", name); } \
    else { std::printf("FAIL %s\n", name); ++failures; } \
} while (0)

} // namespace

int main()
{
    using namespace p2kurage_campaign;

    // Campaign gate: bridge without preview is campaign; everything else keeps
    // the labelled fixture concessions the room fixtures depend on.
    CHECK(isCampaignMode(true, false), "campaign-bridge-no-preview");
    CHECK(!isCampaignMode(true, true), "fixture-bridge-plus-preview");
    CHECK(!isCampaignMode(false, false), "fixture-no-bridge");
    CHECK(!isCampaignMode(false, true), "fixture-preview-only");
    CHECK(!fixtureConcessionsAllowed(true, false), "campaign-skips-concessions");
    CHECK(fixtureConcessionsAllowed(true, true), "preview-keeps-concessions");
    CHECK(fixtureConcessionsAllowed(false, false), "plain-keeps-concessions");

    // Hover: Jellyfloat-low, inside the 30-60u band, so the drawn bell floats
    // while the grounded host body stays in Pikmin reach.
    CHECK(kHoverHeight >= kHoverMin && kHoverHeight <= kHoverMax, "hover-in-band");
    CHECK(kHoverHeight == 40.0f, "hover-is-40");

    // Corpse probe: about once per second at 60fps, never on tick 0, never
    // mid-interval (the old per-30-call line flooded ~1260 lines / 100 s when
    // the tail ran once per teki per frame).
    CHECK(corpseProbeDue(60), "probe-due-60");
    CHECK(corpseProbeDue(120), "probe-due-120");
    CHECK(!corpseProbeDue(0), "probe-silent-0");
    CHECK(!corpseProbeDue(30), "probe-silent-30");
    CHECK(!corpseProbeDue(59), "probe-silent-59");
    CHECK(!corpseProbeDue(61), "probe-silent-61");
    CHECK(kCorpseProbeIntervalFrames == 60, "probe-interval-60");

    if (failures == 0)
        std::printf("PASS P2_KURAGE_CAMPAIGN campaign_gated=1 hover=%.1f probe=1Hz\n",
                    double(kHoverHeight));
    else
        std::printf("P2_KURAGE_CAMPAIGN pass=0 failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
