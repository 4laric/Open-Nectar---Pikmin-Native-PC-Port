#ifndef PC_COOP_POLICY_H
#define PC_COOP_POLICY_H

// Netplay M4 D-policy (issue #885): the owner's co-op randomizer policy as
// pure functions over small structs, so every decision is unit-testable
// without the engine (pc_coop_policy_test.cpp). The engine side
// (gameCoreSection.cpp, co-op branch only) fills the structs from the two
// captains and applies the result. Single-captain play never calls this.
//
// Owner decisions (final):
//   1. Heal goes to the triggering captain, otherwise to the live captain
//      with the lowest HP.
//   2. Trap and delivery anchors alternate between live captains,
//      round-robin (one cursor per anchored kind).
//   3. Benefits apply while ANY captain lives.
//   4. DeathLink uses a combined Pikmin pool (count Pikmin, not captains).
//
// Captain index: 1 = P1 (GameCoreSection::mNavi), 2 = P2 (mNavi2). Ties
// always break to the lower index. No RNG and no wall clock anywhere, so
// the same inputs give the same answer on both lockstep peers.

enum { PC_COOP_CAPTAINS = 2 };

struct PcCoopCaptain {
	int id;       // 1 or 2
	bool live;    // pcIsLastNaviStanding predicate: hp > 1 and not NAVISTATE_Dead
	float hp;     // Navi::mHealth
	float maxHp;  // C_NAVI_PARM(navi, mHealth)
};

// Rule 3: benefits (and the randomizer tick) run while any captain lives.
bool pc_coop_any_live(const PcCoopCaptain caps[PC_COOP_CAPTAINS]);

// Rule 1: heal target.
enum PcCoopHealReason { PC_COOP_HEAL_NONE = 0, PC_COOP_HEAL_TRIGGER, PC_COOP_HEAL_LOWEST };
struct PcCoopHealPick {
	int captain;              // 1 or 2; 0 = nobody qualifies (heal stays pending)
	PcCoopHealReason reason;
};
// Candidates are the live captains with hp < maxHp. A candidate "triggered"
// when prevValid[i] and caps[i].hp < prevHp[i] (its HP dropped since the
// previous co-op tick's sample). Exactly one triggering candidate wins
// (reason TRIGGER); otherwise (none or several) the candidate with the
// lowest hp wins (reason LOWEST), ties to captain 1. A downed captain is
// never a candidate.
PcCoopHealPick pc_coop_pick_heal(const PcCoopCaptain caps[PC_COOP_CAPTAINS],
                                 const float prevHp[PC_COOP_CAPTAINS],
                                 const bool prevValid[PC_COOP_CAPTAINS]);
const char* pc_coop_heal_reason_name(PcCoopHealReason reason);

// Rule 2: captain-anchored grants, one round-robin cursor per kind.
// Onion-anchored grants (BOMBS, DELIVERY) have no captain anchor.
enum PcCoopAnchorKind {
	PC_COOP_ANCHOR_BOMB_TRAP = 0,
	PC_COOP_ANCHOR_PROGG,
	PC_COOP_ANCHOR_PRERELEASE,
	PC_COOP_ANCHOR_FLOWERS,
	PC_COOP_ANCHOR_COUNT
};
const char* pc_coop_anchor_name(PcCoopAnchorKind kind);

struct PcCoopCursors {
	int next[PC_COOP_ANCHOR_COUNT]; // captain (1 or 2) tried first for each kind
};
// Every cursor starts on P1.
void pc_coop_cursors_reset(PcCoopCursors& cursors);
// Try order for one attempt: the cursor's captain first if it is live, then
// the other live captain (same tick). live[i] is captain i+1. Writes up to
// two captain ids into order[] and returns how many.
int pc_coop_anchor_order(const PcCoopCursors& cursors, PcCoopAnchorKind kind,
                         const bool live[PC_COOP_CAPTAINS], int order[PC_COOP_CAPTAINS]);
// After a successful consume by captain `succeeded` (1 or 2), point the
// cursor at the captain after it (with two captains: the other one).
// Returns the new cursor value.
int pc_coop_anchor_advance(PcCoopCursors& cursors, PcCoopAnchorKind kind, int succeeded);

// Test knob (PIKMIN_NETPLAY_TEST_COOP_EVENTS=<file>): at most
// PC_COOP_EVENTS_MAX lines, each `<tick> HP <1|2> <fraction 0..1>` or
// `<tick> DOWN <1|2>`. Blank lines and `#` comments are skipped. <tick>
// counts co-op randomizer updateAI calls since the stage started.
enum { PC_COOP_EVENTS_MAX = 64, PC_COOP_EVENT_TEXT = 48 };
enum PcCoopEventKind { PC_COOP_EVENT_HP = 0, PC_COOP_EVENT_DOWN };
struct PcCoopEvent {
	unsigned tick;
	PcCoopEventKind kind;
	int captain;       // 1 or 2
	float fraction;    // HP only, 0..1 of max HP
	char text[PC_COOP_EVENT_TEXT]; // canonical form for the log line
};
// Parses the whole file text. Returns the event count (0..max), or -1 on a
// malformed line or too many lines (*badLine = 1-based line number).
int pc_coop_events_parse(const char* text, PcCoopEvent* out, int max, int* badLine);

#endif // PC_COOP_POLICY_H
