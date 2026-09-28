// Host test for the netplay M4 D-policy co-op randomizer rules (issue #885):
// heal selection (trigger / lowest / tie / downed), the round-robin anchor
// cursor with skip-dead and placement fallback, the any-live predicate and
// the test-event parser. No game, no window, no assets.

#include "pc_coop_policy.h"

#include <cmath>
#include <cstdio>
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

// Drives the cursor the way gameCoreSection.cpp does: try the order, the
// first captain whose placement succeeds consumes, then advance.
int attempt(PcCoopCursors& c, PcCoopAnchorKind kind, bool live1, bool live2, bool place1 = true, bool place2 = true)
{
	const bool live[2] = { live1, live2 };
	const bool place[2] = { place1, place2 };
	int order[2] = { 0, 0 };
	const int n = pc_coop_anchor_order(c, kind, live, order);
	for (int i = 0; i < n; ++i) {
		if (place[order[i] - 1]) {
			pc_coop_anchor_advance(c, kind, order[i]);
			return order[i];
		}
	}
	return 0;
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
}

} // namespace

int main()
{
	testAnyLive();
	testHeal();
	testAnchors();
	testEvents();
	if (sFailures) {
		std::printf("pc_coop_policy_test: %d/%d checks FAILED\n", sFailures, sChecks);
		return 1;
	}
	std::printf("pc_coop_policy_test: PASS (%d checks)\n", sChecks);
	return 0;
}
