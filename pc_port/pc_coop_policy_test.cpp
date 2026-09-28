// Host test for the netplay M4 D-policy co-op randomizer rules (issue #885):
// heal selection (trigger / lowest / tie / downed), the round-robin anchor
// cursor with skip-dead and placement fallback (through pc_coop_anchor_try,
// the loop the engine runs), the any-live predicate, the per-stage reset
// edge, and the test-event parser and loader. No game, no window, no assets.

#include "pc_coop_policy.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

int sFailures = 0;
int sChecks = 0;

void check(bool condition, const char* what)
{
	++sChecks;
	if (!condition) {
		std::printf("FAIL: %s\n", what);
		++sFailures;
	}
}

PcCoopCaptain cap(int id, bool live, float hp, float maxHp = 100.0f)
{
	PcCoopCaptain c = { id, live, hp, maxHp };
	return c;
}

PcCoopHealPick pick(PcCoopCaptain a, PcCoopCaptain b, float prevA, float prevB, bool validA = true, bool validB = true)
{
	const PcCoopCaptain caps[2] = { a, b };
	const float prev[2] = { prevA, prevB };
	const bool valid[2] = { validA, validB };
	return pc_coop_pick_heal(caps, prev, valid);
}

void testAnyLive()
{
	const PcCoopCaptain both[2] = { cap(1, true, 100), cap(2, true, 100) };
	const PcCoopCaptain p1Down[2] = { cap(1, false, 0), cap(2, true, 40) };
	const PcCoopCaptain p2Down[2] = { cap(1, true, 40), cap(2, false, 0) };
	const PcCoopCaptain none[2] = { cap(1, false, 0), cap(2, false, 0) };
	check(pc_coop_any_live(both), "any-live: both live");
	check(pc_coop_any_live(p1Down), "any-live: P1 down, P2 live");
	check(pc_coop_any_live(p2Down), "any-live: P2 down, P1 live");
	check(!pc_coop_any_live(none), "any-live: both down");
}

void testHeal()
{
	PcCoopHealPick p;
	// Nobody hurt: the heal stays pending.
	p = pick(cap(1, true, 100), cap(2, true, 100), 100, 100);
	check(p.captain == 0 && p.reason == PC_COOP_HEAL_NONE, "heal: both full -> none");
	// heal-p2-only: P1 full, P2 hurt (steady) -> P2 by lowest.
	p = pick(cap(1, true, 100), cap(2, true, 50), 100, 50);
	check(p.captain == 2 && p.reason == PC_COOP_HEAL_LOWEST, "heal: only P2 hurt -> P2 lowest");
	// heal-lowest, no trigger this tick.
	p = pick(cap(1, true, 30), cap(2, true, 60), 30, 60);
	check(p.captain == 1 && p.reason == PC_COOP_HEAL_LOWEST, "heal: P1 30 P2 60 -> P1 lowest");
	p = pick(cap(1, true, 60), cap(2, true, 30), 60, 30);
	check(p.captain == 2 && p.reason == PC_COOP_HEAL_LOWEST, "heal: P1 60 P2 30 -> P2 lowest");
	// Tie breaks to P1.
	p = pick(cap(1, true, 50), cap(2, true, 50), 50, 50);
	check(p.captain == 1 && p.reason == PC_COOP_HEAL_LOWEST, "heal: tie -> P1");
	// Trigger: P2 dropped this tick even though P1 is lower.
	p = pick(cap(1, true, 30), cap(2, true, 60), 30, 100);
	check(p.captain == 2 && p.reason == PC_COOP_HEAL_TRIGGER, "heal: single trigger P2 beats lower P1");
	p = pick(cap(1, true, 70), cap(2, true, 20), 90, 20);
	check(p.captain == 1 && p.reason == PC_COOP_HEAL_TRIGGER, "heal: single trigger P1 beats lower P2");
	// heal-trigger per the brief: both full, then P2 takes damage.
	p = pick(cap(1, true, 100), cap(2, true, 50), 100, 100);
	check(p.captain == 2 && p.reason == PC_COOP_HEAL_TRIGGER, "heal: both full then P2 damaged -> P2 trigger");
	// Several triggers -> lowest (tie to P1).
	p = pick(cap(1, true, 40), cap(2, true, 30), 100, 100);
	check(p.captain == 2 && p.reason == PC_COOP_HEAL_LOWEST, "heal: two triggers -> lowest P2");
	p = pick(cap(1, true, 30), cap(2, true, 30), 100, 100);
	check(p.captain == 1 && p.reason == PC_COOP_HEAL_LOWEST, "heal: two triggers tie -> P1");
	// No previous sample (first tick of a stage): no trigger possible.
	p = pick(cap(1, true, 30), cap(2, true, 60), 100, 100, false, false);
	check(p.captain == 1 && p.reason == PC_COOP_HEAL_LOWEST, "heal: no prev sample -> lowest");
	// HP going up is not a trigger.
	p = pick(cap(1, true, 30), cap(2, true, 60), 30, 40);
	check(p.captain == 1 && p.reason == PC_COOP_HEAL_LOWEST, "heal: rising HP is not a trigger");
	// Downed captain is never healed, even when it "dropped" and is lowest.
	p = pick(cap(1, false, 0), cap(2, true, 50), 100, 50);
	check(p.captain == 2 && p.reason == PC_COOP_HEAL_LOWEST, "heal: P1 downed (dropped) -> P2");
	p = pick(cap(1, false, 0), cap(2, true, 100), 100, 100);
	check(p.captain == 0, "heal: P1 downed, P2 full -> none");
	p = pick(cap(1, false, 0), cap(2, false, 0), 100, 100);
	check(p.captain == 0, "heal: both downed -> none");
	p = pick(cap(1, true, 80), cap(2, false, 0), 80, 50);
	check(p.captain == 1 && p.reason == PC_COOP_HEAL_LOWEST, "heal: P2 downed -> P1");
	check(std::string_view(pc_coop_heal_reason_name(PC_COOP_HEAL_TRIGGER)) == "trigger", "heal: reason name trigger");
	check(std::string_view(pc_coop_heal_reason_name(PC_COOP_HEAL_LOWEST)) == "lowest", "heal: reason name lowest");
}

// Scripted placement outcomes per captain, plus a record of the calls made.
struct PlaceScript {
	PcCoopPlaceResult result[2];
	int calls[4];
	int callCount;
};

PcCoopPlaceResult scriptedPlace(int captain, void* ctx)
{
	PlaceScript& s = *static_cast<PlaceScript*>(ctx);
	if (s.callCount < 4) s.calls[s.callCount] = captain;
	++s.callCount;
	return s.result[captain - 1];
}

// Drives the cursor through pc_coop_anchor_try, the exact loop the engine
// runs (gameCoreSection.cpp coopAnchored): returns the consuming captain.
int attempt(PcCoopCursors& c, PcCoopAnchorKind kind, bool live1, bool live2, bool place1 = true, bool place2 = true)
{
	const bool live[2] = { live1, live2 };
	PlaceScript s = { { place1 ? PC_COOP_PLACE_CONSUMED : PC_COOP_PLACE_FAILED,
	                    place2 ? PC_COOP_PLACE_CONSUMED : PC_COOP_PLACE_FAILED }, {}, 0 };
	const PcCoopAnchorAttempt a = pc_coop_anchor_try(c, kind, live, scriptedPlace, &s);
	return a.captain > 0 ? a.captain : 0;
}

// The attempt record itself: skips, STOP, and that the loop never calls a
// captain it must not.
void testAnchorTry()
{
	PcCoopCursors c;
	pc_coop_cursors_reset(c);
	const bool both[2] = { true, true };
	const bool p1Down[2] = { false, true };
	PlaceScript s = { { PC_COOP_PLACE_CONSUMED, PC_COOP_PLACE_CONSUMED }, {}, 0 };
	PcCoopAnchorAttempt a = pc_coop_anchor_try(c, PC_COOP_ANCHOR_FLOWERS, both, scriptedPlace, &s);
	check(a.captain == 1 && a.cursor == 1 && a.next == 2 && a.skipCount == 0 && s.callCount == 1 && s.calls[0] == 1,
	      "try: cursor P1 consumes, one call, no skip");
	// Cursor P2, P2 placement fails, P1 consumes: one placement skip, cursor to P2.
	s = { { PC_COOP_PLACE_CONSUMED, PC_COOP_PLACE_FAILED }, {}, 0 };
	a = pc_coop_anchor_try(c, PC_COOP_ANCHOR_FLOWERS, both, scriptedPlace, &s);
	check(a.captain == 1 && a.cursor == 2 && a.next == 2 && s.callCount == 2 && s.calls[0] == 2 && s.calls[1] == 1,
	      "try: P2 fails, P1 consumes in the same attempt");
	check(a.skipCount == 1 && a.skipCaptain[0] == 2 && a.skipReason[0] == PC_COOP_SKIP_PLACEMENT, "try: P2 placement skip recorded");
	// Cursor P2 still; P2 STOPs: no fallback, no consume, cursor unchanged.
	s = { { PC_COOP_PLACE_CONSUMED, PC_COOP_PLACE_STOP }, {}, 0 };
	a = pc_coop_anchor_try(c, PC_COOP_ANCHOR_FLOWERS, both, scriptedPlace, &s);
	check(a.captain == -1 && a.next == 2 && c.next[PC_COOP_ANCHOR_FLOWERS] == 2 && s.callCount == 1,
	      "try: STOP ends the attempt without trying the other captain");
	// Both fail: nobody, cursor unchanged, two placement skips.
	s = { { PC_COOP_PLACE_FAILED, PC_COOP_PLACE_FAILED }, {}, 0 };
	a = pc_coop_anchor_try(c, PC_COOP_ANCHOR_FLOWERS, both, scriptedPlace, &s);
	check(a.captain == 0 && c.next[PC_COOP_ANCHOR_FLOWERS] == 2 && a.skipCount == 2 && s.callCount == 2,
	      "try: both fail -> none, cursor kept, two skips");
	// FAILED never advances the cursor on its own.
	check(a.next == a.cursor, "try: failed attempt reports the unchanged cursor");
	// Cursor on a downed P1: not-live skip first, P1 never called, P2 consumes.
	pc_coop_cursors_reset(c);
	s = { { PC_COOP_PLACE_CONSUMED, PC_COOP_PLACE_CONSUMED }, {}, 0 };
	a = pc_coop_anchor_try(c, PC_COOP_ANCHOR_BOMB_TRAP, p1Down, scriptedPlace, &s);
	check(a.captain == 2 && a.cursor == 1 && a.next == 1 && s.callCount == 1 && s.calls[0] == 2,
	      "try: downed cursor captain is skipped, never placed");
	check(a.skipCount == 1 && a.skipCaptain[0] == 1 && a.skipReason[0] == PC_COOP_SKIP_NOT_LIVE, "try: not-live skip recorded");
	check(std::string_view(pc_coop_skip_reason_name(PC_COOP_SKIP_NOT_LIVE)) == "not-live"
	          && std::string_view(pc_coop_skip_reason_name(PC_COOP_SKIP_PLACEMENT)) == "placement",
	      "try: skip reason names");
	check(pc_coop_anchor_try(c, PC_COOP_ANCHOR_BOMB_TRAP, p1Down, nullptr, &s).captain == 0, "try: null placement -> none");
}

// Per-stage reset edge (review round 1: repeated day 29 and same-day reloads).
void testStageKey()
{
	const char* why = nullptr;
	const PcCoopStageKey a = { 1, 2, 9.5f };
	check(pc_coop_stage_changed(false, a, a, &why) && std::string_view(why) == "start", "stage: first call resets (start)");
	check(!pc_coop_stage_changed(true, a, a, &why) && why == nullptr, "stage: same key -> no reset");
	const PcCoopStageKey later = { 1, 2, 9.6f };
	check(!pc_coop_stage_changed(true, a, later, &why), "stage: clock moving forward -> no reset");
	const PcCoopStageKey otherStage = { 2, 2, 9.6f };
	check(pc_coop_stage_changed(true, a, otherStage, &why) && std::string_view(why) == "stage", "stage: stage change resets");
	const PcCoopStageKey nextDay = { 1, 3, 7.0f };
	check(pc_coop_stage_changed(true, a, nextDay, &why) && std::string_view(why) == "day", "stage: day change resets");
	// Repeated day 29 on the same stage (pc_randomizer_next_day pins 29), and a
	// same-day reload: stage and day equal, the clock restarts at the start hour.
	const PcCoopStageKey d29End = { 1, 29, 18.9f }, d29Again = { 1, 29, 7.0f };
	check(pc_coop_stage_changed(true, d29End, d29Again, &why) && std::string_view(why) == "clock",
	      "stage: repeated day 29 on the same stage resets (clock)");
	const PcCoopStageKey d2Mid = { 1, 2, 12.0f }, d2Reload = { 1, 2, 7.0f };
	check(pc_coop_stage_changed(true, d2Mid, d2Reload, &why) && std::string_view(why) == "clock", "stage: same-day reload resets (clock)");
}

void testAnchors()
{
	PcCoopCursors c;
	pc_coop_cursors_reset(c);
	for (int k = 0; k < PC_COOP_ANCHOR_COUNT; ++k) check(c.next[k] == 1, "anchor: every cursor starts on P1");
	// Alternation: 3 bomb traps -> 1, 2, 1; 2 flowers -> 1, 2 (independent).
	check(attempt(c, PC_COOP_ANCHOR_BOMB_TRAP, true, true) == 1, "anchor: bomb trap #1 -> P1");
	check(attempt(c, PC_COOP_ANCHOR_FLOWERS, true, true) == 1, "anchor: flowers #1 -> P1 (own cursor)");
	check(attempt(c, PC_COOP_ANCHOR_BOMB_TRAP, true, true) == 2, "anchor: bomb trap #2 -> P2");
	check(attempt(c, PC_COOP_ANCHOR_BOMB_TRAP, true, true) == 1, "anchor: bomb trap #3 -> P1");
	check(attempt(c, PC_COOP_ANCHOR_FLOWERS, true, true) == 2, "anchor: flowers #2 -> P2");
	check(c.next[PC_COOP_ANCHOR_PROGG] == 1 && c.next[PC_COOP_ANCHOR_PRERELEASE] == 1, "anchor: untouched kinds stay on P1");
	// Skip-dead: cursor on P1, P1 down -> P2, cursor then points after P2 (P1).
	pc_coop_cursors_reset(c);
	check(attempt(c, PC_COOP_ANCHOR_PROGG, false, true) == 2, "anchor: P1 down -> P2");
	check(c.next[PC_COOP_ANCHOR_PROGG] == 1, "anchor: after P2 the cursor points at P1");
	check(attempt(c, PC_COOP_ANCHOR_PROGG, false, true) == 2, "anchor: P1 still down -> P2 again");
	check(attempt(c, PC_COOP_ANCHOR_PROGG, true, true) == 1, "anchor: P1 back -> P1 (cursor was on P1)");
	// Cursor on P2 but P2 down -> P1.
	pc_coop_cursors_reset(c);
	attempt(c, PC_COOP_ANCHOR_FLOWERS, true, true); // -> P1, cursor P2
	check(attempt(c, PC_COOP_ANCHOR_FLOWERS, true, false) == 1, "anchor: cursor P2 down -> P1");
	check(c.next[PC_COOP_ANCHOR_FLOWERS] == 2, "anchor: after P1 the cursor points at P2");
	// Placement fallback: cursor captain live but placement fails -> other captain, same attempt.
	pc_coop_cursors_reset(c);
	check(attempt(c, PC_COOP_ANCHOR_BOMB_TRAP, true, true, false, true) == 2, "anchor: P1 placement fails -> P2");
	check(c.next[PC_COOP_ANCHOR_BOMB_TRAP] == 1, "anchor: fallback success advances past P2");
	check(attempt(c, PC_COOP_ANCHOR_PRERELEASE, true, true, true, false) == 1, "anchor: prerelease P1 walking -> P1");
	check(attempt(c, PC_COOP_ANCHOR_PRERELEASE, true, true, true, false) == 1, "anchor: P2 not walking -> fallback P1");
	check(c.next[PC_COOP_ANCHOR_PRERELEASE] == 2, "anchor: cursor after fallback P1 is P2");
	// Both fail: nothing consumed, cursor unchanged.
	const int before = c.next[PC_COOP_ANCHOR_BOMB_TRAP];
	check(attempt(c, PC_COOP_ANCHOR_BOMB_TRAP, true, true, false, false) == 0, "anchor: both placements fail -> none");
	check(c.next[PC_COOP_ANCHOR_BOMB_TRAP] == before, "anchor: failed attempt keeps the cursor");
	check(attempt(c, PC_COOP_ANCHOR_BOMB_TRAP, false, false) == 0, "anchor: nobody live -> none");
	// Order output.
	pc_coop_cursors_reset(c);
	const bool bothLive[2] = { true, true };
	int order[2] = { 0, 0 };
	check(pc_coop_anchor_order(c, PC_COOP_ANCHOR_FLOWERS, bothLive, order) == 2 && order[0] == 1 && order[1] == 2,
	      "anchor: order from P1 is 1,2");
	c.next[PC_COOP_ANCHOR_FLOWERS] = 2;
	check(pc_coop_anchor_order(c, PC_COOP_ANCHOR_FLOWERS, bothLive, order) == 2 && order[0] == 2 && order[1] == 1,
	      "anchor: order from P2 is 2,1");
	// Single live captain degenerates to "always that captain".
	pc_coop_cursors_reset(c);
	for (int i = 0; i < 4; ++i) check(attempt(c, PC_COOP_ANCHOR_FLOWERS, false, true) == 2, "anchor: only P2 live -> always P2");
	check(std::string_view(pc_coop_anchor_name(PC_COOP_ANCHOR_BOMB_TRAP)) == "BOMB_TRAP", "anchor: name BOMB_TRAP");
	check(std::string_view(pc_coop_anchor_name(PC_COOP_ANCHOR_PRERELEASE)) == "PRERELEASE", "anchor: name PRERELEASE");
}

void testEvents()
{
	PcCoopEvent ev[PC_COOP_EVENTS_MAX];
	int bad = -7;
	int n = pc_coop_events_parse("# comment\r\n300 HP 2 0.5\r\n\r\n  900 DOWN 1  # late\n1200 HP 1 1\n40 HP 1 .25", ev, PC_COOP_EVENTS_MAX, &bad);
	check(n == 4 && bad == 0, "events: 4 valid lines with comments/CRLF");
	if (n == 4) {
		check(ev[0].tick == 300 && ev[0].kind == PC_COOP_EVENT_HP && ev[0].captain == 2 && std::fabs(ev[0].fraction - 0.5f) < 1e-6f,
		      "events: 300 HP 2 0.5");
		check(std::string_view(ev[0].text) == "HP 2 0.500", "events: canonical HP text");
		check(ev[1].tick == 900 && ev[1].kind == PC_COOP_EVENT_DOWN && ev[1].captain == 1, "events: 900 DOWN 1");
		check(std::string_view(ev[1].text) == "DOWN 1", "events: canonical DOWN text");
		check(ev[2].fraction == 1.0f && ev[3].tick == 40 && std::fabs(ev[3].fraction - 0.25f) < 1e-6f, "events: 1 and .25");
	}
	const char* badLines[] = { "10 HP 3 0.5", "10 HP 1 1.5", "10 DOWN", "HP 1 0.5", "10 JUMP 1", "10 DOWN 1 x", "10 HP 1 -0.5",
	                           "10 HP1 0.5", "x10 DOWN 1" };
	for (const char* line : badLines) {
		bad = 0;
		check(pc_coop_events_parse(line, ev, PC_COOP_EVENTS_MAX, &bad) == -1 && bad == 1, line);
	}
	check(pc_coop_events_parse("1 DOWN 1\n2 DOWN 2\n3 DOWN 1", ev, 2, &bad) == -1 && bad == 3, "events: more lines than max");
	check(pc_coop_events_parse("", ev, PC_COOP_EVENTS_MAX, &bad) == 0, "events: empty file");
	// Ticks are 1-based: tick 0 would never fire, so it is rejected.
	bad = 0;
	check(pc_coop_events_parse("0 DOWN 1", ev, PC_COOP_EVENTS_MAX, &bad) == -1 && bad == 1, "events: tick 0 rejected");
	bad = 0;
	check(pc_coop_events_parse("1 DOWN 1\n0 HP 2 0.5", ev, PC_COOP_EVENTS_MAX, &bad) == -1 && bad == 2, "events: tick 0 on line 2");
	check(pc_coop_events_parse("1 DOWN 1", ev, PC_COOP_EVENTS_MAX, &bad) == 1 && ev[0].tick == 1, "events: tick 1 accepted");
}

std::string tempPath(const char* name)
{
	const char* dir = std::getenv("TEMP");
	if (!dir) dir = std::getenv("TMPDIR");
	std::string path = dir ? dir : ".";
	path += "/pc_coop_policy_test_";
	path += name;
	return path;
}

bool writeFile(const std::string& path, const std::string& text)
{
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return false;
	const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
	std::fclose(f);
	return ok;
}

void testEventsLoad()
{
	PcCoopEvent ev[PC_COOP_EVENTS_MAX];
	const char* why = nullptr;
	int bad = 0;
	const std::string good = tempPath("good.txt");
	check(writeFile(good, "40 HP 2 0.5\r\n1000 DOWN 1\r\n"), "load: write good file");
	check(pc_coop_events_load(good.c_str(), ev, PC_COOP_EVENTS_MAX, &why, &bad) == 2 && why == nullptr, "load: good file -> 2 events");
	// Exactly at the limit is read whole: comments padded to the limit.
	// "5 DOWN 2\n" (9) + "#" + padding + "\n" = exactly the limit.
	const std::string atLimit = "5 DOWN 2\n#" + std::string(PC_COOP_EVENTS_FILE_MAX - 11, 'x') + "\n";
	const std::string limit = tempPath("limit.txt");
	check(atLimit.size() == PC_COOP_EVENTS_FILE_MAX && writeFile(limit, atLimit), "load: write at-limit file");
	check(pc_coop_events_load(limit.c_str(), ev, PC_COOP_EVENTS_MAX, &why, &bad) == 1, "load: file at the limit parses");
	// One byte over: rejected whole, never truncated into a torn last line.
	const std::string over = tempPath("over.txt");
	check(writeFile(over, atLimit + "7 HP 1 0."), "load: write oversized file");
	check(pc_coop_events_load(over.c_str(), ev, PC_COOP_EVENTS_MAX, &why, &bad) == -1 && why && std::string_view(why) == "too-large",
	      "load: oversized file rejected as too-large");
	check(pc_coop_events_load(tempPath("missing.txt").c_str(), ev, PC_COOP_EVENTS_MAX, &why, &bad) == -1 && why
	          && std::string_view(why) == "unreadable",
	      "load: missing file -> unreadable");
	const std::string badFile = tempPath("bad.txt");
	check(writeFile(badFile, "5 DOWN 1\n0 DOWN 2\n"), "load: write bad file");
	check(pc_coop_events_load(badFile.c_str(), ev, PC_COOP_EVENTS_MAX, &why, &bad) == -1 && why && std::string_view(why) == "bad-line"
	          && bad == 2,
	      "load: bad line reported");
	std::remove(good.c_str());
	std::remove(limit.c_str());
	std::remove(over.c_str());
	std::remove(badFile.c_str());
	// This test is not built with PIKI_NETPLAY_BUILD: the knob is compiled
	// out, so even with both variables set it reads nothing.
#if defined(_WIN32)
	_putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1");
	_putenv_s("PIKMIN_NETPLAY_TEST_COOP_EVENTS", "x.txt");
#else
	setenv("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1", 1);
	setenv("PIKMIN_NETPLAY_TEST_COOP_EVENTS", "x.txt", 1);
#endif
	check(pc_coop_events_knob_path() == nullptr, "knob: compiled out without PIKI_NETPLAY_BUILD");
}

} // namespace

int main()
{
	testAnyLive();
	testHeal();
	testAnchors();
	testAnchorTry();
	testStageKey();
	testEvents();
	testEventsLoad();
	if (sFailures) {
		std::printf("pc_coop_policy_test: %d/%d checks FAILED\n", sFailures, sChecks);
		return 1;
	}
	std::printf("pc_coop_policy_test: PASS (%d checks)\n", sChecks);
	return 0;
}
