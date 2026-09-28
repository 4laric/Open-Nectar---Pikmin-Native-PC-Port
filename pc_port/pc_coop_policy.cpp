#include "pc_coop_policy.h"

#include <cstdio>
#include <cstring>

// Netplay M4 D-policy (issue #885). See pc_coop_policy.h for the rules.

bool pc_coop_any_live(const PcCoopCaptain caps[PC_COOP_CAPTAINS])
{
	for (int i = 0; i < PC_COOP_CAPTAINS; ++i)
		if (caps[i].live) return true;
	return false;
}

PcCoopHealPick pc_coop_pick_heal(const PcCoopCaptain caps[PC_COOP_CAPTAINS],
                                 const float prevHp[PC_COOP_CAPTAINS],
                                 const bool prevValid[PC_COOP_CAPTAINS])
{
	PcCoopHealPick pick = { 0, PC_COOP_HEAL_NONE };
	int triggers = 0, trigger = -1, lowest = -1;
	for (int i = 0; i < PC_COOP_CAPTAINS; ++i) {
		const PcCoopCaptain& c = caps[i];
		if (!c.live || !(c.hp < c.maxHp)) continue; // downed or full: never healed
		if (prevValid[i] && c.hp < prevHp[i]) {
			++triggers;
			trigger = i;
		}
		// Strict less-than keeps the lower index on a tie.
		if (lowest < 0 || c.hp < caps[lowest].hp) lowest = i;
	}
	if (lowest < 0) return pick;
	if (triggers == 1) {
		pick.captain = caps[trigger].id;
		pick.reason  = PC_COOP_HEAL_TRIGGER;
	} else {
		pick.captain = caps[lowest].id;
		pick.reason  = PC_COOP_HEAL_LOWEST;
	}
	return pick;
}

const char* pc_coop_heal_reason_name(PcCoopHealReason reason)
{
	switch (reason) {
	case PC_COOP_HEAL_TRIGGER: return "trigger";
	case PC_COOP_HEAL_LOWEST: return "lowest";
	default: return "none";
	}
}

const char* pc_coop_anchor_name(PcCoopAnchorKind kind)
{
	switch (kind) {
	case PC_COOP_ANCHOR_BOMB_TRAP: return "BOMB_TRAP";
	case PC_COOP_ANCHOR_PROGG: return "PROGG";
	case PC_COOP_ANCHOR_PRERELEASE: return "PRERELEASE";
	case PC_COOP_ANCHOR_FLOWERS: return "FLOWERS";
	default: return "?";
	}
}

void pc_coop_cursors_reset(PcCoopCursors& cursors)
{
	for (int k = 0; k < PC_COOP_ANCHOR_COUNT; ++k) cursors.next[k] = 1;
}

static int other_captain(int captain) { return captain == 1 ? 2 : 1; }

static int clamp_captain(int captain) { return captain == 2 ? 2 : 1; }

int pc_coop_anchor_order(const PcCoopCursors& cursors, PcCoopAnchorKind kind,
                         const bool live[PC_COOP_CAPTAINS], int order[PC_COOP_CAPTAINS])
{
	if (kind < 0 || kind >= PC_COOP_ANCHOR_COUNT) return 0;
	const int first = clamp_captain(cursors.next[kind]);
	const int second = other_captain(first);
	int n = 0;
	if (live[first - 1]) order[n++] = first;
	if (live[second - 1]) order[n++] = second;
	return n;
}

int pc_coop_anchor_advance(PcCoopCursors& cursors, PcCoopAnchorKind kind, int succeeded)
{
	if (kind < 0 || kind >= PC_COOP_ANCHOR_COUNT) return 1;
	cursors.next[kind] = other_captain(clamp_captain(succeeded));
	return cursors.next[kind];
}

// Locale-independent unsigned decimal: digits only.
static bool parse_uint(const char*& p, unsigned& out)
{
	if (*p < '0' || *p > '9') return false;
	unsigned long long v = 0;
	while (*p >= '0' && *p <= '9') {
		v = v * 10 + unsigned(*p - '0');
		if (v > 0xFFFFFFFFull) return false;
		++p;
	}
	out = unsigned(v);
	return true;
}

// Locale-independent fraction in [0, 1]: `0`, `1`, `0.5`, `.25`, `1.0`.
static bool parse_fraction(const char*& p, float& out)
{
	unsigned whole = 0;
	bool digits = false;
	if (*p >= '0' && *p <= '9') {
		if (!parse_uint(p, whole)) return false;
		digits = true;
	}
	double v = double(whole);
	if (*p == '.') {
		++p;
		double scale = 0.1;
		while (*p >= '0' && *p <= '9') {
			v += scale * double(*p - '0');
			scale *= 0.1;
			digits = true;
			++p;
		}
	}
	if (!digits || v < 0.0 || v > 1.0) return false;
	out = float(v);
	return true;
}

static void skip_blanks(const char*& p)
{
	while (*p == ' ' || *p == '\t') ++p;
}

static bool at_line_end(const char* p) { return *p == '\0' || *p == '\n' || *p == '\r' || *p == '#'; }

static bool parse_word(const char*& p, const char* word)
{
	const size_t n = std::strlen(word);
	if (std::strncmp(p, word, n) != 0) return false;
	const char c = p[n];
	if (!(c == ' ' || c == '\t' || c == '\0' || c == '\n' || c == '\r' || c == '#')) return false;
	p += n;
	return true;
}

int pc_coop_events_parse(const char* text, PcCoopEvent* out, int max, int* badLine)
{
	if (badLine) *badLine = 0;
	if (!text) return 0;
	int count = 0, lineNo = 0;
	const char* p = text;
	while (*p) {
		++lineNo;
		const char* line = p;
		while (*p && *p != '\n') ++p;
		const char* next = *p ? p + 1 : p;
		const char* q = line;
		skip_blanks(q);
		if (!at_line_end(q)) {
			PcCoopEvent ev;
			std::memset(&ev, 0, sizeof(ev));
			bool ok = parse_uint(q, ev.tick);
			skip_blanks(q);
			if (ok && parse_word(q, "HP")) {
				ev.kind = PC_COOP_EVENT_HP;
				skip_blanks(q);
				unsigned cap = 0;
				ok = parse_uint(q, cap) && (cap == 1 || cap == 2);
				ev.captain = int(cap);
				skip_blanks(q);
				ok = ok && parse_fraction(q, ev.fraction);
				if (ok) std::snprintf(ev.text, sizeof(ev.text), "HP %d %.3f", ev.captain, double(ev.fraction));
			} else if (ok && parse_word(q, "DOWN")) {
				ev.kind = PC_COOP_EVENT_DOWN;
				skip_blanks(q);
				unsigned cap = 0;
				ok = parse_uint(q, cap) && (cap == 1 || cap == 2);
				ev.captain = int(cap);
				if (ok) std::snprintf(ev.text, sizeof(ev.text), "DOWN %d", ev.captain);
			} else {
				ok = false;
			}
			skip_blanks(q);
			if (!ok || !at_line_end(q) || count >= max) {
				if (badLine) *badLine = lineNo;
				return -1;
			}
			out[count++] = ev;
		}
		p = next;
	}
	return count;
}
