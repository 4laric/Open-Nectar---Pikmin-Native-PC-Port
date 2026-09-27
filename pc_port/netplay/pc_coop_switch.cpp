#include "pc_coop_switch.h"
#include "pc_coop.h"

#include <cstdlib>
#include <cstring>

namespace {

struct CaptainName {
	const char* name;
	int value;
};

// PcCaptain values (pc_coop.h): 0 Olimar, 1 Louie, 2 Red, 3 Yellow, 4 Blue.
const CaptainName kCaptains[] = {
	{ "olimar", 0 },
	{ "louie", 1 },
	{ "pikmin-red", 2 },
	{ "pikmin-yellow", 3 },
	{ "pikmin-blue", 4 },
};

bool parseOne(const char* begin, const char* end, int* out)
{
	// Trim ASCII whitespace; the name itself must match exactly.
	while (begin < end && (*begin == ' ' || *begin == '\t')) ++begin;
	while (end > begin && (end[-1] == ' ' || end[-1] == '\t')) --end;
	for (const CaptainName& c : kCaptains) {
		const size_t n = std::strlen(c.name);
		if (size_t(end - begin) == n && std::memcmp(begin, c.name, n) == 0) {
			*out = c.value;
			return true;
		}
	}
	return false;
}

bool envOn(const char* name)
{
	const char* v = std::getenv(name);
	return v && *v && *v != '0';
}

} // namespace

bool pc_coop_switch_parse_captains(const char* text, int* outP1, int* outP2)
{
	if (!text || !outP1 || !outP2) return false;
	const char* comma = std::strchr(text, ',');
	if (!comma || !comma[1] || std::strchr(comma + 1, ',')) return false;
	int p1 = 0, p2 = 0;
	if (!parseOne(text, comma, &p1)) return false;
	if (!parseOne(comma + 1, text + std::strlen(text), &p2)) return false;
	*outP1 = p1;
	*outP2 = p2;
	return true;
}

PcCoopSwitch pc_coop_switch_parse(int argc, char** argv)
{
	PcCoopSwitch sw;
	const char* cliCaptains = nullptr;
	if (envOn("PIKMIN_COOP")) sw.coop = true;
	if (const char* envCaptains = std::getenv("PIKMIN_COOP_CAPTAINS")) {
		if (*envCaptains) {
			// Set (even if invalid): co-op on, default captains on bad input.
			sw.coop = true;
			pc_coop_switch_parse_captains(envCaptains, &sw.captainP1, &sw.captainP2);
		}
	}
	for (int i = 1; i < argc; ++i) {
		if (!argv[i]) continue;
		if (std::strcmp(argv[i], "--coop") == 0) {
			sw.coop = true;
		} else if (std::strncmp(argv[i], "--coop-captains=", 16) == 0) {
			cliCaptains = argv[i] + 16;
		}
	}
	if (cliCaptains) {
		// Explicit CLI captains imply co-op and win over the env pair.
		sw.coop = true;
		pc_coop_switch_parse_captains(cliCaptains, &sw.captainP1, &sw.captainP2);
	}
	return sw;
}

namespace {
bool sSwitchActive = false;
} // namespace

void pc_coop_switch_apply(const PcCoopSwitch& sw)
{
	if (!sw.coop) return;
	pc_coop_set_pending(true);
	pc_coop_set_captain(0, sw.captainP1);
	pc_coop_set_captain(1, sw.captainP2);
	sSwitchActive = true;
}

bool pc_coop_switch_active(void) { return sSwitchActive; }
