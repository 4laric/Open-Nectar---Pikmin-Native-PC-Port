// Netplay M3 delay-based lockstep session over GekkoNet (issue #880).
//
// Design (brief section "Design", item by item):
//  1. Build: the `gekkonet` static target (CMake, netplay builds only).
//  2. Input: PcNetplayInput, fixed 16 bytes (pc_netplay_gekko_input.h).
//  3. Driver: pc_netplay_session_drive(), called from System::run; see below.
//  4. Transport: pc_netplay_udp.* (Winsock UDP + lossy wrapper).
//  5. Session start: --netplay-host / --netplay-join (+ env), co-op + det
//     forced on, host = P0, joiner = P1, PIKMIN_NETPLAY_DELAY (default 2),
//     Gekko config 2/0/0/16/8/desync/check7.
//  6. Handshake on channel 0x01 before the GekkoNet session starts.
//  7. Local-only UI: F1 menu neutralises only the local submitted input.
//  8. PIKMIN_NETPLAY_LOCAL_INPUT_FILE: scripted local input for tests.
//
// The per-tick block when netplay is active (brief item 3):
//   1. mControllerMgr.update() (pumps SDL, samples the local pad; the F1
//      consume path inside pc_window_poll_events already zeroes the pads
//      while the menu is open).
//   2. Build the local input (physical pad 0 + local control yaw, or the
//      next scripted record).
//   3. gekko_add_local_input for the local player (skipped while ahead).
//   4. gekko_update_session.
//   5. Per Advance: inject both inputs into sControllerPad[0]/[1], set each
//      pad's control yaw, run exactly one tick (updateSysClock,
//      pc_netplay_on_tick_begin, app->idle, state-hash hook).
//   6. Per Save: 8-byte handle {frame} + M1 curated hash checksum.
//   7. Load: cannot happen at window 0 -> log + abort.
//   8. Session events: Connected/Started/Disconnected/DesyncDetected.
//   9. No Advance (waiting): no tick; the turn still pumped the network and
//      window events via update_session + PADRead's poll.
//
// Pacing: one network turn per System::run loop iteration, slept to 30 Hz
// (real time) unless PIKMIN_NETPLAY_UNTHROTTLED=1, which runs as fast as the
// session allows (tests). gekko_frames_ahead() slows us down when ahead.

#include "netplay/pc_netplay_session.h"

#include "netplay/pc_netplay_det.h"
#include "netplay/pc_netplay_gekko_input.h"
#include "netplay/pc_netplay_pad.h"
#include "netplay/pc_netplay_udp.h"
#include "netplay/pc_input_log.h"
#include "netplay/pc_state_hash.h"
#include "netplay/pc_coop_switch.h"
#include "pc_coop.h"
#include "pc_window.h"
#include "settings/pc_settings.h"

#include "gekkonet.h"

#include "system.h"
#include "BaseApp.h"
#include "Controller.h"
#include "Dolphin/pad.h"

#include <SDL2/SDL.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// pc_state_hash additions (M3): capture the last tick's hashes even when no
// hash log file is open, so Save events and desync dumps have checksums.
// Declared here to avoid a header dependency cycle; defined in
// pc_state_hash.cpp. Inert unless pc_state_hash_set_netplay_capture(true).
void pc_state_hash_set_netplay_capture(bool on);
bool pc_state_hash_current(uint64_t* total, uint64_t subs[6], uint64_t* tick);
// Runs the registered pre-sim yaw capture hook now (M2c hook), without the
// record/replay logic of pc_input_log_tick(). Defined in pc_input_log.cpp.
void pc_input_log_capture_yaw(void);
// B1: clears the yaw slots before running the hook, so the submitted local
// yaw follows the live camera instead of freezing at the first injected
// value. Defined in pc_input_log.cpp.
void pc_input_log_capture_yaw_fresh(void);
// Per-tick record/file hook (no-op with no record/replay active).
void pc_input_log_tick_end(void);
// Per-frame engine work the lockstep tick must keep (M4): audio event
// timers / gameplay-audio unpause (jaudio) and the thread liveness check.
#include "jaudio/interface.h"
#include "Dolphin/os.h"
// M1 det profile note (every 600 ticks when PIKMIN_NETPLAY_PROFILE_LOG is
// set). Defined in pc_netplay_det.cpp; system.cpp's static helper defers to
// it too.
void pc_netplay_det_profile_note_tick(void);
// Passive F1 menu query (no input polling side effects). Defined in
// pc_settings.cpp; the existing pc_settings_consume_game_input() polls and
// latches, so the session must not call it.
bool pc_settings_menu_open(void);
// Forces deterministic mode on (netplay requires it). Defined in
// pc_netplay_det.cpp; re-reads the unthrottled env gate.
void pc_netplay_det_force_on(void);

namespace {

// ---- tiny SHA-256 (public-domain style, written fresh for M3) ----
// Used for the exe hash and the bootstrap hash. Correctness over speed:
// it runs once at startup / on fixed small buffers.

struct Sha256 {
	uint32_t h[8];
	uint64_t total = 0;
	uint8_t buf[64];
	size_t buflen = 0;

	static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

	void init()
	{
		h[0] = 0x6a09e667; h[1] = 0xbb67ae85; h[2] = 0x3c6ef372; h[3] = 0xa54ff53a;
		h[4] = 0x510e527f; h[5] = 0x9b05688c; h[6] = 0x1f83d9ab; h[7] = 0x5be0cd19;
		total = 0;
		buflen = 0;
	}

	static void block(Sha256& s, const uint8_t* p)
	{
		static const uint32_t k[64] = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
			0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
			0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
			0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
			0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
			0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
			0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
			0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
		};
		uint32_t w[64];
		for (int i = 0; i < 16; ++i)
			w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16)
			     | ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
		for (int i = 16; i < 64; ++i) {
			uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
			uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i]        = w[i - 16] + s0 + w[i - 7] + s1;
		}
		uint32_t a = s.h[0], b = s.h[1], c = s.h[2], d = s.h[3];
		uint32_t e = s.h[4], f = s.h[5], g = s.h[6], hh = s.h[7];
		for (int i = 0; i < 64; ++i) {
			uint32_t S1  = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
			uint32_t ch  = (e & f) ^ (~e & g);
			uint32_t t1  = hh + S1 + ch + k[i] + w[i];
			uint32_t S0  = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
			uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
			uint32_t t2  = S0 + maj;
			hh = g; g = f; f = e; e = d + t1;
			d = c; c = b; b = a; a = t1 + t2;
		}
		s.h[0] += a; s.h[1] += b; s.h[2] += c; s.h[3] += d;
		s.h[4] += e; s.h[5] += f; s.h[6] += g; s.h[7] += hh;
	}

	void update(const uint8_t* data, size_t len)
	{
		total += len;
		while (len > 0) {
			size_t take = 64 - buflen;
			if (take > len) take = len;
			memcpy(buf + buflen, data, take);
			buflen += take;
			data += take;
			len -= take;
			if (buflen == 64) {
				block(*this, buf);
				buflen = 0;
			}
		}
	}

	void final(uint8_t out[32])
	{
		uint64_t bitlen = total * 8;
		uint8_t pad     = 0x80;
		update(&pad, 1);
		uint8_t zero = 0;
		while (buflen != 56) update(&zero, 1);
		uint8_t lenb[8];
		for (int i = 0; i < 8; ++i) lenb[i] = (uint8_t)((bitlen >> (56 - i * 8)) & 0xFF);
		// Append directly without re-padding: feed the block manually.
		memcpy(buf + buflen, lenb, 8);
		buflen += 8;
		block(*this, buf);
		buflen = 0;
		for (int i = 0; i < 8; ++i) {
			out[i * 4]     = (uint8_t)((h[i] >> 24) & 0xFF);
			out[i * 4 + 1] = (uint8_t)((h[i] >> 16) & 0xFF);
			out[i * 4 + 2] = (uint8_t)((h[i] >> 8) & 0xFF);
			out[i * 4 + 3] = (uint8_t)(h[i] & 0xFF);
		}
	}
};

std::string to_hex(const uint8_t* data, size_t len)
{
	static const char* digits = "0123456789abcdef";
	std::string s;
	s.reserve(len * 2);
	for (size_t i = 0; i < len; ++i) {
		s.push_back(digits[(data[i] >> 4) & 0xF]);
		s.push_back(digits[data[i] & 0xF]);
	}
	return s;
}

// ---- session state ----

enum Phase {
	kIdle,     // no netplay switch: drive() returns false immediately
	kHandshake,
	kSession,
	kDone,     // clean end requested: keep owning the loop until the process quits
};

struct Config {
	bool isHost = false;
	bool active = false;
	uint16_t hostPort = 0;      // --netplay-host <port>
	uint32_t joinIp = 0;        // --netplay-join <ip:port>
	uint16_t joinPort = 0;
	unsigned localDelay = 2;    // PIKMIN_NETPLAY_DELAY
	uint32_t seed = 0;          // PIKMIN_NETPLAY_SEED
	std::string bootstrapPath;  // --randomizer-seed <file> or bootstrap.txt
	std::string localInputFile; // PIKMIN_NETPLAY_LOCAL_INPUT_FILE
	uint64_t exitAfter = 0;     // PIKMIN_NETPLAY_EXIT_AFTER_TICKS (0 = run)
};

int sArgc = 0;
char** sArgv = nullptr;
Config sCfg;
Phase sPhase = kIdle;
bool sInitialised = false;

// Handshake constants.
constexpr char kHsMagic[4] = { 'N', 'P', 'H', '3' };
constexpr uint16_t kProtocolVersion = 1;
constexpr uint8_t kHsHello  = 1;
constexpr uint8_t kHsAck    = 2;
constexpr uint8_t kHsRefuse = 3;
// Refuse field ids (logged as names).
constexpr uint8_t kFieldProto = 1;
constexpr uint8_t kFieldExe = 2;
constexpr uint8_t kFieldConfig = 3;
constexpr uint8_t kFieldBootstrap = 4;
constexpr uint8_t kFieldSeed = 5;
constexpr size_t kHsLen = 4 + 1 + 2 + 32 + 32 + 32 + 4 + 1; // 108

const char* field_name(uint8_t f)
{
	switch (f) {
	case kFieldProto: return "protocol";
	case kFieldExe: return "exe";
	case kFieldConfig: return "config";
	case kFieldBootstrap: return "bootstrap";
	case kFieldSeed: return "seed";
	default: return "unknown";
	}
}

struct Hello {
	uint8_t exe[32];
	uint8_t cfg[32];
	uint8_t boot[32];
	uint32_t seed = 0;
};

Hello sLocal;
uint8_t sExeHex[128] = { 0 };
std::string sExeHexStr;
std::string sCfgHexStr;
std::string sBootHexStr;

// Transport / session objects (owned by the session TU, created at handshake).
pc_netplay_transport::UdpSocket* sSock = nullptr;
pc_netplay_transport::GekkoLink* sLink = nullptr;
pc_netplay_transport::LossyLink* sLossy = nullptr;
GekkoNetAdapter* sAdapter = nullptr;
GekkoSession* sGekko = nullptr;
uint8_t sRemoteAddrBlob[6] = { 0 };
bool sHaveRemote = false;
uint32_t sRemoteIp = 0;
uint16_t sRemotePort = 0;
int sLocalHandle = 0;
int sLocalRole = 0; // 0 = host/P1, 1 = joiner/P2
bool sGekkoStarted = false;
bool sForcedModes = false;

// Handshake progress.
bool sSentAck = false;
bool sGotAck = false;
double sHsStartMs = 0;
double sHsLastSendMs = 0;
int sRefuseSent = 0;

// Run stats.
uint64_t sSessionTicks = 0;
uint64_t sAdvances = 0;
uint64_t sStalls = 0;
uint64_t sSaves = 0;
// B2: number of local inputs actually submitted to GekkoNet. A submit is
// only accepted when it targets the session's current frame (InputBuffer
// drops non-sequential frames), so the driver submits at most one input per
// Advance (sSubmitted == sAdvances once started) and consumes one script
// record / pad sample per submit.
uint64_t sSubmitted = 0;
double sRunStartMs = 0;
double sNextTurnMs = 0;
bool sAheadLogged = false;
// M5: wall-clock stall accounting. sStallMs accumulates the wall time of
// loop turns that produced no Advance; stall % = 100 * sStallMs / session
// wall time. (The old sStalls turn counter is kept for the log line, but
// under UNTHROTTLED the loop spins ~1500 turns per advance, so the turn
// ratio is not a wall-clock stall.)
double sStallMs = 0;
double sSessionStartMs = 0;
// M3: ring of per-tick hashes so a desync report can dump the desynced
// frame's sub-hashes, not the latest tick's. GekkoNet frame F maps to hash
// tick F+1 (ticks are 1-based, frames 0-based). 256 deep: well past the
// check_distance-7 health lag, even at 100 ms latency.
struct HashEntry {
	bool valid = false;
	uint64_t tick = 0;
	uint64_t total = 0;
	uint64_t subs[6] = { 0, 0, 0, 0, 0, 0 };
};
constexpr size_t kHashRing = 256;
HashEntry sHashRing[kHashRing];

void hash_ring_store(uint64_t tick, uint64_t total, const uint64_t subs[6])
{
	HashEntry& e = sHashRing[tick % kHashRing];
	e.valid      = true;
	e.tick       = tick;
	e.total      = total;
	for (int i = 0; i < 6; ++i) e.subs[i] = subs[i];
}

const HashEntry* hash_ring_find(uint64_t tick)
{
	const HashEntry& e = sHashRing[tick % kHashRing];
	return (e.valid && e.tick == tick) ? &e : nullptr;
}

// Scripted local input (PIKMIN_NETPLAY_LOCAL_INPUT_FILE, pkni v2).
std::vector<uint8_t> sScriptBytes;
size_t sScriptTicks = 0;
size_t sScriptIdx = 0;
bool sScriptActive = false;

double now_ms()
{
	using namespace std::chrono;
	return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

const char* getenv_nonempty(const char* name)
{
	const char* v = std::getenv(name);
	return (v != nullptr && *v != '\0') ? v : nullptr;
}

unsigned read_unsigned_env(const char* name, unsigned fallback)
{
	const char* v = getenv_nonempty(name);
	if (v == nullptr) return fallback;
	char* end     = nullptr;
	unsigned long n = strtoul(v, &end, 10);
	if (end == v || *end != '\0') return fallback;
	return (unsigned)n;
}

double read_double_env(const char* name, double fallback)
{
	const char* v = getenv_nonempty(name);
	if (v == nullptr) return fallback;
	char* end   = nullptr;
	double n    = strtod(v, &end);
	if (end == v || *end != '\0') return fallback;
	return n;
}

const char* argv_value(int argc, char** argv, const char* flag)
{
	for (int i = 1; i + 1 < argc; ++i) {
		if (argv[i] != nullptr && std::strcmp(argv[i], flag) == 0) return argv[i + 1];
	}
	return nullptr;
}

uint32_t read_u32_env(const char* name, uint32_t fallback)
{
	const char* v = getenv_nonempty(name);
	if (v == nullptr) return fallback;
	char* end       = nullptr;
	unsigned long n = strtoul(v, &end, 0);
	if (end == v || *end != '\0' || n > 0xFFFFFFFFul) return fallback;
	return (uint32_t)n;
}

// ---- hashing helpers ----

void sha_bytes(const uint8_t* data, size_t len, uint8_t out[32])
{
	Sha256 s;
	s.init();
	if (len > 0) s.update(data, len);
	s.final(out);
}

void sha_text(const std::string& text, uint8_t out[32])
{
	sha_bytes(reinterpret_cast<const uint8_t*>(text.data()), text.size(), out);
}

std::string exe_path()
{
#ifdef _WIN32
	char path[4096];
	DWORD n = GetModuleFileNameA(nullptr, path, sizeof(path));
	if (n == 0 || n >= sizeof(path)) return std::string();
	return std::string(path, n);
#else
	return std::string();
#endif
}

bool sha_file(const char* path, uint8_t out[32])
{
	// M2: ferror() must run before fclose() (the old order used a freed
	// FILE*), and a mid-file read error must fail instead of hashing a
	// truncated exe.
	FILE* f = fopen(path, "rb");
	if (f == nullptr) return false;
	Sha256 s;
	s.init();
	uint8_t chunk[65536];
	bool readErr = false;
	while (true) {
		size_t n = fread(chunk, 1, sizeof(chunk), f);
		if (n > 0) s.update(chunk, n);
		if (n < sizeof(chunk)) {
			if (ferror(f)) readErr = true;
			break;
		}
	}
	// Capture the error state before closing (fclose invalidates f).
	bool err = readErr || ferror(f);
	fclose(f);
	if (err) return false;
	s.final(out);
	return true;
}

// Session-config hash input. EXACT list (brief item 6 requires it named):
//   sim Deriv PREFIX "m3-config-v1"
//   fpsMode, chainActions, holdToPluck, instantWhistle, whistleRadiusPct,
//   pikiLimit, dayMinutes, infiniteDay, noDayAdvance, unlockZones, allOnions,
//   pikiInvincible, allFlowers, carrySpeedScale(bits), naviSpeedScale(bits),
//   naviHealthPct, tekiHealthPct, betterPathfinding, bluesOnlyWater,
//   throwSpeedScale(bits), throwCancelB, noTrip, onionStep10, lockOn, charge,
//   throwWhileMoving, firstPerson, freeCamera, idleCounter, debugKeys,
//   gyroEnabled, disableTutorials,
//   coopPending(forced 1 in netplay), captainP1, captainP2, coopSplit,
//   coopMergeCamera,
//   windowWidth, windowHeight (pre-M2b: both peers must render the same view),
//   netplaySeed, netplayDelay, protocolVersion.
// Deliberately EXCLUDED (local-only, documented in the handoff): display/
// render settings (shadows, gamma, resolution scale, vsync), audio settings,
// key/gamepad bindings, gyro calibration (sensitivity/invert/bias), touch
// settings, photo-mode state (no sim getter; local overlay), VS rules/mode
// (co-op sessions only), language.
std::string build_config_string()
{
	char fbuf[64];
	std::string s = "m3-config-v1;";
	auto addi = [&](const char* k, long long v) {
		char b[96];
		snprintf(b, sizeof(b), "%s=%lld;", k, v);
		s += b;
	};
	auto addf = [&](const char* k, float v) {
		uint32_t bits = 0;
		memcpy(&bits, &v, sizeof(bits));
		snprintf(fbuf, sizeof(fbuf), "%s=0x%08x;", k, bits);
		s += fbuf;
	};
	addi("fpsMode", pc_settings_get_fps_mode());
	addi("chainActions", pc_settings_get_chain_actions());
	addi("holdToPluck", pc_settings_get_hold_to_pluck());
	addi("instantWhistle", pc_settings_get_instant_whistle());
	addi("whistleRadiusPct", pc_settings_get_whistle_radius_pct());
	addi("pikiLimit", pc_settings_get_piki_limit());
	addi("dayMinutes", pc_settings_get_day_minutes());
	addi("infiniteDay", pc_settings_get_infinite_day());
	addi("noDayAdvance", pc_settings_get_no_day_advance());
	addi("unlockZones", pc_settings_get_unlock_zones());
	addi("allOnions", pc_settings_get_all_onions());
	addi("pikiInvincible", pc_settings_get_piki_invincible());
	addi("allFlowers", pc_settings_get_all_flowers());
	addf("carrySpeedScale", pc_settings_get_carry_speed_scale());
	addf("naviSpeedScale", pc_settings_get_navi_speed_scale());
	addi("naviHealthPct", pc_settings_get_navi_health_pct());
	addi("tekiHealthPct", pc_settings_get_teki_health_pct());
	addi("betterPathfinding", pc_settings_get_better_pathfinding());
	addi("bluesOnlyWater", pc_settings_get_blues_only_water());
	addf("throwSpeedScale", pc_settings_get_throw_speed_scale());
	addi("throwCancelB", pc_settings_get_throw_cancel_b());
	addi("noTrip", pc_settings_get_no_trip());
	addi("onionStep10", pc_settings_get_onion_step10());
	addi("lockOn", pc_settings_get_lock_on());
	addi("charge", pc_settings_get_charge());
	addi("throwWhileMoving", pc_settings_get_throw_while_moving());
	addi("firstPerson", pc_settings_get_first_person());
	addi("freeCamera", pc_settings_get_free_camera());
	addi("idleCounter", pc_settings_get_idle_counter());
	addi("debugKeys", pc_settings_get_debug_keys());
	addi("gyroEnabled", pc_settings_get_gyro_enabled());
	// m5: disableTutorials gates room-preview flow (newPikiGame.cpp), so it
	// is sim-relevant and hashed. (The brief's `whistlePluck` name does not
	// exist in this tree; the covered whistle knobs are holdToPluck,
	// instantWhistle and whistleRadiusPct.)
	addi("disableTutorials", pc_settings_get_disable_tutorials());
	addi("coopPending", 1);
	addi("captainP1", pc_coop_captain(0));
	addi("captainP2", pc_coop_captain(1));
	addi("coopSplit", pc_settings_get_coop_split());
	addi("coopMergeCamera", pc_settings_get_coop_merge_camera());
	addi("windowWidth", pc_window_get_width());
	addi("windowHeight", pc_window_get_height());
	addi("netplaySeed", (long long)sCfg.seed);
	addi("netplayDelay", (long long)sCfg.localDelay);
	addi("protocolVersion", (long long)kProtocolVersion);
	(void)fbuf;
	return s;
}

// Bootstrap hash: file bytes with any per-run SESSION token line removed
// (brief item 6). Falls back to hashing empty input when no bootstrap file
// is present (plain non-randomizer boot); both peers must match.
std::string read_bootstrap_stripped()
{
	std::string path = sCfg.bootstrapPath;
	FILE* f          = fopen(path.c_str(), "rb");
	if (f == nullptr) return std::string();
	std::string raw;
	char chunk[8192];
	while (true) {
		size_t n = fread(chunk, 1, sizeof(chunk), f);
		if (n > 0) raw.append(chunk, n);
		if (n < sizeof(chunk)) break;
	}
	fclose(f);
	std::string out;
	size_t pos = 0;
	while (pos <= raw.size()) {
		size_t eol = raw.find('\n', pos);
		std::string line = (eol == std::string::npos) ? raw.substr(pos) : raw.substr(pos, eol - pos);
		if (!(line.compare(0, 7, "SESSION") == 0
		      && (line.size() == 7 || line[7] == ' ' || line[7] == '\t' || line[7] == '\r'))) {
			out += line;
			out += '\n';
		}
		if (eol == std::string::npos) break;
		pos = eol + 1;
	}
	return out;
}

bool local_ui_open()
{
	// F1 settings overlay: local-only (brief item 7). No F8 tracker exists
	// in this tree (see handoff deviations); this is the extension point.
	return pc_settings_menu_open();
}

void load_scripted_file()
{
	const char* path = sCfg.localInputFile.c_str();
	if (path[0] == '\0') return;
	FILE* f = fopen(path, "rb");
	if (f == nullptr) {
		printf("[netplay] local input file: cannot open %s\n", path);
		fflush(stdout);
		std::exit(3);
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 10) {
		printf("[netplay] local input file: truncated %s\n", path);
		fflush(stdout);
		fclose(f);
		std::exit(3);
	}
	sScriptBytes.resize((size_t)size);
	size_t got = fread(sScriptBytes.data(), 1, (size_t)size, f);
	fclose(f);
	sScriptBytes.resize(got);
	if (got < 10 || memcmp(sScriptBytes.data(), "PKNI", 4) != 0) {
		printf("[netplay] local input file: bad magic %s\n", path);
		fflush(stdout);
		std::exit(3);
	}
	uint16_t version = (uint16_t)(sScriptBytes[4] | ((uint16_t)sScriptBytes[5] << 8));
	uint16_t pads    = (uint16_t)(sScriptBytes[6] | ((uint16_t)sScriptBytes[7] << 8));
	uint16_t rec     = (uint16_t)(sScriptBytes[8] | ((uint16_t)sScriptBytes[9] << 8));
	if (version != 2 || pads != 4 || rec != 56) {
		printf("[netplay] local input file: need pkni v2 4x14, got v%u %ux%u (%s)\n",
		       (unsigned)version, (unsigned)pads, (unsigned)rec, path);
		fflush(stdout);
		std::exit(3);
	}
	sScriptTicks  = (sScriptBytes.size() - 10) / 56;
	sScriptActive = true;
	printf("[netplay] local input file: %s (%llu ticks)\n", path,
	       (unsigned long long)sScriptTicks);
	fflush(stdout);
}

PcNetplayInput scripted_record(size_t idx)
{
	PcNetplayInput in;
	if (!sScriptActive || idx >= sScriptTicks) return in; // neutral past the end
	const uint8_t* rec = sScriptBytes.data() + 10 + idx * 56;
	// Pad 0 of the 4-pad record (14 bytes: 11 PADStatus + yaw u16 + flags).
	in.buttons   = (uint16_t)(rec[0] | ((uint16_t)rec[1] << 8));
	in.stickX    = (int8_t)rec[2];
	in.stickY    = (int8_t)rec[3];
	in.substickX = (int8_t)rec[4];
	in.substickY = (int8_t)rec[5];
	in.triggerL  = rec[6];
	in.triggerR  = rec[7];
	in.controlYaw = (uint16_t)(rec[11] | ((uint16_t)rec[12] << 8));
	in.flags     = 0;
	return in;
}

PcNetplayInput build_local_input()
{
	// Scripted input for tests (brief item 8): pad-0 records (+ yaw) from
	// the file, one per local input submission, instead of the pad.
	if (sScriptActive) {
		PcNetplayInput in = scripted_record(sScriptIdx++);
		if (local_ui_open()) in = pc_netplay_input_neutral();
		return in;
	}
	PADStatus* pads = pc_netplay_pad_status();
	PADStatus s     = pads[0]; // post-PADRead sample (F1 consume applied)
	PcNetplayInput in;
	in.buttons    = s.button;
	in.stickX     = s.stickX;
	in.stickY     = s.stickY;
	in.substickX  = s.substickX;
	in.substickY  = s.substickY;
	in.triggerL   = s.triggerLeft;
	in.triggerR   = s.triggerRight;
	// Local control yaw: this peer's own captain camera slot (host = pad 0,
	// joiner = pad 1), filled by the M2c pre-sim capture hook just before.
	if (pc_input_log_yaw_valid(sLocalRole)) in.controlYaw = pc_input_log_yaw_raw(sLocalRole);
	in.flags = 0;
	if (local_ui_open()) in = pc_netplay_input_neutral();
	return in;
}

void inject_input(int pad, const PcNetplayInput& in)
{
	PADStatus* pads = pc_netplay_pad_status();
	pads[pad].button       = in.buttons;
	pads[pad].stickX       = in.stickX;
	pads[pad].stickY       = in.stickY;
	pads[pad].substickX    = in.substickX;
	pads[pad].substickY    = in.substickY;
	pads[pad].triggerLeft  = in.triggerL;
	pads[pad].triggerRight = in.triggerR;
	// The 16-byte record carries no analogA/B (GC analog shoulders arrive
	// as triggerLeft/Right; the port leaves analogA/B at 0 for real pads
	// too in practice). Deterministic constant on both peers either way.
	pads[pad].analogA = 0;
	pads[pad].analogB = 0;
	pads[pad].err     = 0; // connected, deterministic on both peers
	pc_input_log_yaw_set(pad, in.controlYaw, pc_input_log::kFlagsNone);
}

void inject_neutral_pad(int pad)
{
	PADStatus* pads = pc_netplay_pad_status();
	pads[pad].button = 0;
	pads[pad].stickX = pads[pad].stickY = 0;
	pads[pad].substickX = pads[pad].substickY = 0;
	pads[pad].triggerLeft = pads[pad].triggerRight = 0;
	pads[pad].analogA = pads[pad].analogB = 0;
	pads[pad].err     = (pad < 2) ? 0 : -1;
	pc_input_log_yaw_set(pad, 0, pc_input_log::kFlagsNone);
}

uint32_t fold_hash64(uint64_t v) { return (uint32_t)(v ^ (v >> 32)); }

void stop_session()
{
	if (sGekko != nullptr) {
		GekkoSession* s = sGekko;
		sGekko          = nullptr;
		gekko_destroy(&s);
	}
	delete sLossy;
	sLossy   = nullptr;
	sAdapter = nullptr;
	delete sLink;
	sLink = nullptr;
	if (sSock != nullptr) {
		sSock->close();
		delete sSock;
		sSock = nullptr;
	}
}

void request_quit()
{
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_QUIT;
	SDL_PushEvent(&ev);
}

void parse_config()
{
	sCfg = Config();
	// CLI first, env second (CLI wins).
	const char* hostCli = argv_value(sArgc, sArgv, "--netplay-host");
	const char* joinCli = argv_value(sArgc, sArgv, "--netplay-join");
	const char* hostEnv = getenv_nonempty("PIKMIN_NETPLAY_HOST");
	const char* joinEnv = getenv_nonempty("PIKMIN_NETPLAY_JOIN");
	const char* hostVal = hostCli != nullptr ? hostCli : hostEnv;
	const char* joinVal = joinCli != nullptr ? joinCli : joinEnv;
	if (hostVal != nullptr && joinVal != nullptr) {
		printf("[netplay] --netplay-host and --netplay-join are exclusive\n");
		fflush(stdout);
		std::exit(2);
	}
	if (hostVal != nullptr) {
		char* end  = nullptr;
		long p     = strtol(hostVal, &end, 10);
		if (end == hostVal || *end != '\0' || p <= 0 || p > 65535) {
			printf("[netplay] bad --netplay-host port %s\n", hostVal);
			fflush(stdout);
			std::exit(2);
		}
		sCfg.isHost   = true;
		sCfg.active   = true;
		sCfg.hostPort = (uint16_t)p;
	} else if (joinVal != nullptr) {
		uint32_t ip   = 0;
		uint16_t port = 0;
		if (!pc_netplay_transport::parse_endpoint(joinVal, &ip, &port)) {
			printf("[netplay] bad --netplay-join endpoint %s (want ip:port)\n", joinVal);
			fflush(stdout);
			std::exit(2);
		}
		sCfg.isHost   = false;
		sCfg.active   = true;
		sCfg.joinIp   = ip;
		sCfg.joinPort = port;
	}
	if (!sCfg.active) {
		sPhase = kIdle;
		return;
	}
	// m5: the TEST-ONLY autoplay bot (PIKMIN_RANDOMIZER_AUTOPLAY) overrides
	// the synced pads inside updateController, silently defeating lockstep.
	// Refuse to start a session with it set instead of desyncing mid-run.
	// (The in-process p2 script hook has no env gate; fixtures must not
	// enable it in netplay — documented in the handoff.)
	if (const char* ap = getenv_nonempty("PIKMIN_RANDOMIZER_AUTOPLAY")) {
		if (!(ap[0] == '0' && ap[1] == '\0')) {
			printf("[netplay] PIKMIN_RANDOMIZER_AUTOPLAY is set: refusing netplay session\n");
			fflush(stdout);
			std::exit(2);
		}
	}
	sCfg.localDelay = read_unsigned_env("PIKMIN_NETPLAY_DELAY", 2);
	if (sCfg.localDelay > 8) sCfg.localDelay = 8;
	sCfg.seed = read_u32_env("PIKMIN_NETPLAY_SEED", 0);
	const char* bootCli = argv_value(sArgc, sArgv, "--randomizer-seed");
	sCfg.bootstrapPath  = bootCli != nullptr ? bootCli : "bootstrap.txt";
	const char* lif     = getenv_nonempty("PIKMIN_NETPLAY_LOCAL_INPUT_FILE");
	if (lif != nullptr) sCfg.localInputFile = lif;
	uint64_t exitAfter = 0;
	if (const char* ea = getenv_nonempty("PIKMIN_NETPLAY_EXIT_AFTER_TICKS")) {
		char* end     = nullptr;
		unsigned long n = strtoul(ea, &end, 10);
		if (end != ea && *end == '\0' && n > 0) exitAfter = n;
	}
	sCfg.exitAfter = exitAfter;
	sLocalRole    = sCfg.isHost ? 0 : 1;
	sPhase        = kHandshake;
}

void compute_local_hello()
{
	// Exe SHA-256, once at startup, from the running module file.
	uint8_t exe[32] = { 0 };
	std::string path = exe_path();
	bool ok          = !path.empty() && sha_file(path.c_str(), exe);
	memcpy(sLocal.exe, exe, 32);
	sExeHexStr = ok ? to_hex(exe, 32) : std::string(64, '0');
	if (!ok) {
		printf("[netplay] warning: exe hash failed for %s; handshake uses zeros\n", path.c_str());
		fflush(stdout);
	}
	// Config hash over the exact list in build_config_string().
	std::string cfg = build_config_string();
	sha_text(cfg, sLocal.cfg);
	sCfgHexStr = to_hex(sLocal.cfg, 32);
	// Bootstrap hash with the SESSION token line removed.
	sha_text(read_bootstrap_stripped(), sLocal.boot);
	sBootHexStr  = to_hex(sLocal.boot, 32);
	sLocal.seed  = sCfg.seed;
}

void send_hello_msg(uint8_t type, uint8_t refuseField)
{
	uint8_t msg[kHsLen];
	msg[0] = (uint8_t)kHsMagic[0];
	msg[1] = (uint8_t)kHsMagic[1];
	msg[2] = (uint8_t)kHsMagic[2];
	msg[3] = (uint8_t)kHsMagic[3];
	msg[4] = type;
	msg[5] = (uint8_t)(kProtocolVersion & 0xFF);
	msg[6] = (uint8_t)((kProtocolVersion >> 8) & 0xFF);
	memcpy(msg + 7, sLocal.exe, 32);
	memcpy(msg + 39, sLocal.cfg, 32);
	memcpy(msg + 71, sLocal.boot, 32);
	msg[103] = (uint8_t)(sLocal.seed & 0xFF);
	msg[104] = (uint8_t)((sLocal.seed >> 8) & 0xFF);
	msg[105] = (uint8_t)((sLocal.seed >> 16) & 0xFF);
	msg[106] = (uint8_t)((sLocal.seed >> 24) & 0xFF);
	msg[107] = refuseField;
	if (sCfg.isHost) {
		if (sHaveRemote)
			sSock->send_to(pc_netplay_transport::kChannelHandshake, msg, sizeof(msg), sRemoteIp,
			               sRemotePort);
	} else {
		sSock->send_payload(pc_netplay_transport::kChannelHandshake, msg, sizeof(msg));
	}
}

bool parse_hello_msg(const uint8_t* p, size_t len, uint8_t* type, uint16_t* proto, Hello* h,
                     uint8_t* refuseField)
{
	if (p == nullptr || len != kHsLen) return false;
	if (p[0] != (uint8_t)kHsMagic[0] || p[1] != (uint8_t)kHsMagic[1] || p[2] != (uint8_t)kHsMagic[2]
	    || p[3] != (uint8_t)kHsMagic[3])
		return false;
	*type        = p[4];
	*proto       = (uint16_t)(p[5] | ((uint16_t)p[6] << 8));
	memcpy(h->exe, p + 7, 32);
	memcpy(h->cfg, p + 39, 32);
	memcpy(h->boot, p + 71, 32);
	h->seed      = (uint32_t)p[103] | ((uint32_t)p[104] << 8) | ((uint32_t)p[105] << 16)
	         | ((uint32_t)p[106] << 24);
	*refuseField = p[107];
	return true;
}

void refuse_and_exit(uint8_t field)
{
	printf("[netplay] handshake refused: %s\n", field_name(field));
	fflush(stdout);
	// Best effort: tell the peer why (5 quick sends; the channel is
	// unreliable by design under the lossy test wrapper).
	for (int i = 0; i < 5; ++i) send_hello_msg(kHsRefuse, field);
	fflush(stdout);
	std::exit(4);
}

// Returns true once the GekkoNet session may start.
bool handshake_pump()
{
	const double now = now_ms();
	if (sHsStartMs == 0) {
		sHsStartMs     = now;
		sHsLastSendMs  = 0;
		sRunStartMs    = now;
		sNextTurnMs    = now;
	}
	const double timeoutMs = (double)read_unsigned_env("PIKMIN_NETPLAY_HANDSHAKE_TIMEOUT_MS", 30000);
	if (now - sHsStartMs > timeoutMs) {
		printf("[netplay] handshake timeout after %.0f ms\n", now - sHsStartMs);
		fflush(stdout);
		std::exit(4);
	}
	if (now - sHsLastSendMs >= 100) {
		send_hello_msg(kHsHello, 0);
		// Re-send our ack so a lost ack cannot stall the peer.
		if (sSentAck) send_hello_msg(kHsAck, 0);
		sHsLastSendMs = now;
	}
	std::vector<pc_netplay_transport::UdpSocket::Datagram> grams;
	if (sLink != nullptr) grams = sLink->drain_handshake();
	for (auto& g : grams) {
		uint8_t type   = 0;
		uint16_t proto = 0;
		Hello h;
		uint8_t refuse = 0;
		memset(&h, 0, sizeof(h));
		if (!parse_hello_msg(g.payload.data(), g.payload.size(), &type, &proto, &h, &refuse))
			continue; // wrong magic/length: ignore
		if (type == kHsRefuse) {
			printf("[netplay] handshake refused: %s\n",
			       field_name(refuse == 0 ? 99 : refuse));
			fflush(stdout);
			std::exit(4);
		}
		if (sCfg.isHost && !sHaveRemote) {
			// Learn the joiner's endpoint from its first hello.
			sRemoteIp   = g.fromIpHostOrder;
			sRemotePort = g.fromPort;
			sHaveRemote = true;
			sSock->set_peer(sRemoteIp, sRemotePort);
			printf("[netplay] handshake: peer %u.%u.%u.%u:%u\n", (sRemoteIp >> 24) & 0xFF,
			       (sRemoteIp >> 16) & 0xFF, (sRemoteIp >> 8) & 0xFF, sRemoteIp & 0xFF,
			       (unsigned)sRemotePort);
			fflush(stdout);
		}
		if (type == kHsHello || type == kHsAck) {
			// Compare in field order; the first mismatch refuses.
			// (Ack echoes the sender's own values, which matched ours
			// when it sent the ack, so re-checking is harmless.)
			if (proto != kProtocolVersion) refuse_and_exit(kFieldProto);
			if (memcmp(h.exe, sLocal.exe, 32) != 0) refuse_and_exit(kFieldExe);
			if (memcmp(h.cfg, sLocal.cfg, 32) != 0) refuse_and_exit(kFieldConfig);
			if (memcmp(h.boot, sLocal.boot, 32) != 0) refuse_and_exit(kFieldBootstrap);
			if (h.seed != sLocal.seed) refuse_and_exit(kFieldSeed);
			if (type == kHsHello && !sSentAck) {
				send_hello_msg(kHsAck, 0);
				sSentAck = true;
			}
			if (type == kHsAck) sGotAck = true;
		}
	}
	return sSentAck && sGotAck;
}

// M1: answer late/duplicate handshake traffic once the GekkoNet session is
// up. A peer that got our Hello+Ack in one drain sends its single Ack and
// moves on; if that Ack is lost, the other peer keeps sending Hello until
// its 30 s timeout while this peer sits in a session with no remote (the
// disconnect timeout only applies after a connection exists). Draining here
// and re-acking keeps one lost datagram from hanging the session. The lossy
// test wrapper covers only channel 0x02, so handshake loss never showed in
// the pair runs; this path is exercised by the handshake logic itself.
void answer_handshake_in_session()
{
	if (sLink == nullptr || sSock == nullptr) return;
	std::vector<pc_netplay_transport::UdpSocket::Datagram> grams = sLink->drain_handshake();
	for (auto& g : grams) {
		uint8_t type   = 0;
		uint16_t proto = 0;
		Hello h;
		uint8_t refuse = 0;
		memset(&h, 0, sizeof(h));
		if (!parse_hello_msg(g.payload.data(), g.payload.size(), &type, &proto, &h, &refuse))
			continue;
		if (type == kHsRefuse) continue; // session already agreed; ignore
		if (type != kHsHello && type != kHsAck) continue;
		// Only answer the known peer (host learns it during the handshake;
		// the joiner always talks to its configured host).
		if (sCfg.isHost && sHaveRemote
		    && (g.fromIpHostOrder != sRemoteIp || g.fromPort != sRemotePort))
			continue;
		send_hello_msg(kHsAck, 0);
	}
}

void start_gekko_session()
{
	sGekko = nullptr;
	if (!gekko_create(&sGekko, GekkoGameSession) || sGekko == nullptr) {
		printf("[netplay] gekko_create failed\n");
		fflush(stdout);
		std::exit(1);
	}
	// Lossy test wrapper only when impairments are configured; otherwise
	// the link adapter feeds GekkoNet directly.
	pc_netplay_transport::LossyParams lp;
	lp.latencyMs      = read_double_env("PIKMIN_NETPLAY_TEST_LATENCY_MS", 0.0);
	lp.jitterMs       = read_double_env("PIKMIN_NETPLAY_TEST_JITTER_MS", 0.0);
	lp.lossPct        = read_double_env("PIKMIN_NETPLAY_TEST_LOSS_PCT", 0.0);
	lp.reorderPct     = read_double_env("PIKMIN_NETPLAY_TEST_REORDER_PCT", 0.0);
	lp.reorderExtraMs = read_double_env("PIKMIN_NETPLAY_TEST_REORDER_MS", 0.0);
	lp.seed           = read_u32_env("PIKMIN_NETPLAY_TEST_SEED", 0);
	if (lp.latencyMs < 0) lp.latencyMs = 0;
	if (lp.jitterMs < 0) lp.jitterMs = 0;
	if (lp.lossPct < 0) lp.lossPct = 0;
	if (lp.lossPct > 100) lp.lossPct = 100;
	const bool lossy = lp.latencyMs > 0 || lp.jitterMs > 0 || lp.lossPct > 0
	                || (lp.reorderPct > 0 && lp.reorderExtraMs > 0);
	if (lossy) {
		sLossy   = new pc_netplay_transport::LossyLink(sLink->adapter(), lp);
		sAdapter = sLossy->adapter();
		printf("[netplay] lossy adapter: latency=%.1fms jitter=%.1fms loss=%.1f%% seed=%u\n",
		       lp.latencyMs, lp.jitterMs, lp.lossPct, lp.seed);
	} else {
		sAdapter = sLink->adapter();
	}
	fflush(stdout);
	GekkoConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.num_players             = 2;
	cfg.max_spectators          = 0;
	cfg.input_prediction_window = 0; // lockstep (window 0)
	cfg.spectator_delay         = 0;
	cfg.input_size              = 16;
	cfg.state_size              = 8; // 8-byte handle {frame}
	cfg.limited_saving          = false;
	cfg.desync_detection        = true;
	cfg.check_distance          = 7;
	gekko_start(sGekko, &cfg);
	// NOTE: the adapter must be set AFTER gekko_start: Init() clears it.
	gekko_net_adapter_set(sGekko, sAdapter);
	// Role-ordered actors so both peers map handle 0 = host/P1 and
	// handle 1 = joiner/P2: host adds local then remote; joiner adds
	// remote then local.
	GekkoNetAddress raddr;
	if (sCfg.isHost) {
		sLocalHandle = gekko_add_actor(sGekko, GekkoLocalPlayer, nullptr);
		sRemoteAddrBlob[0] = (uint8_t)((sRemoteIp >> 24) & 0xFF);
		sRemoteAddrBlob[1] = (uint8_t)((sRemoteIp >> 16) & 0xFF);
		sRemoteAddrBlob[2] = (uint8_t)((sRemoteIp >> 8) & 0xFF);
		sRemoteAddrBlob[3] = (uint8_t)(sRemoteIp & 0xFF);
		sRemoteAddrBlob[4] = (uint8_t)((sRemotePort >> 8) & 0xFF);
		sRemoteAddrBlob[5] = (uint8_t)(sRemotePort & 0xFF);
		raddr.data         = sRemoteAddrBlob;
		raddr.size         = 6;
		(void)gekko_add_actor(sGekko, GekkoRemotePlayer, &raddr);
	} else {
		sRemoteAddrBlob[0] = (uint8_t)((sCfg.joinIp >> 24) & 0xFF);
		sRemoteAddrBlob[1] = (uint8_t)((sCfg.joinIp >> 16) & 0xFF);
		sRemoteAddrBlob[2] = (uint8_t)((sCfg.joinIp >> 8) & 0xFF);
		sRemoteAddrBlob[3] = (uint8_t)(sCfg.joinIp & 0xFF);
		sRemoteAddrBlob[4] = (uint8_t)((sCfg.joinPort >> 8) & 0xFF);
		sRemoteAddrBlob[5] = (uint8_t)(sCfg.joinPort & 0xFF);
		raddr.data         = sRemoteAddrBlob;
		raddr.size         = 6;
		(void)gekko_add_actor(sGekko, GekkoRemotePlayer, &raddr);
		sLocalHandle = gekko_add_actor(sGekko, GekkoLocalPlayer, nullptr);
	}
	gekko_set_local_delay(sGekko, sLocalHandle, (unsigned char)sCfg.localDelay);
	gekko_set_disconnect_timeout(sGekko, 5000);
	pc_state_hash_set_netplay_capture(true);
	load_scripted_file();
	printf("[netplay] session started: role=%s localHandle=%d delay=%u seed=%u\n",
	       sCfg.isHost ? "host/P1" : "joiner/P2", sLocalHandle, sCfg.localDelay, sCfg.seed);
	printf("[netplay] exe=%s\n", sExeHexStr.c_str());
	printf("[netplay] config=%s\n", sCfgHexStr.c_str());
	printf("[netplay] bootstrap=%s\n", sBootHexStr.c_str());
	fflush(stdout);
	sSessionStartMs = now_ms();
	sPhase          = kSession;
}

void handle_session_events()
{
	if (sGekko == nullptr) return;
	int n                 = 0;
	GekkoSessionEvent** ev = gekko_session_events(sGekko, &n);
	if (ev == nullptr || n <= 0) return;
	for (int i = 0; i < n; ++i) {
		if (ev[i] == nullptr) continue;
		switch (ev[i]->type) {
		case GekkoPlayerConnected:
			printf("[netplay] connected: handle=%d\n", ev[i]->data.connected.handle);
			break;
		case GekkoSessionStarted:
			printf("[netplay] gekko session started\n");
			sGekkoStarted = true;
			break;
		case GekkoPlayerDisconnected:
			printf("[netplay] disconnected: handle=%d\n", ev[i]->data.disconnected.handle);
			fflush(stdout);
			pc_state_hash_flush();
			stop_session();
			sPhase = kDone;
			request_quit();
			return; // session is gone: stop processing this batch
		case GekkoDesyncDetected: {
			// M3: dump the desynced frame's sub-hashes from the ring, not
			// the latest tick's. GekkoNet frame F maps to hash tick F+1.
			const int frame = ev[i]->data.desynced.frame;
			const uint64_t wantTick = frame >= 0 ? (uint64_t)frame + 1 : 0;
			const HashEntry* e      = hash_ring_find(wantTick);
			uint64_t total = 0, subs[6] = { 0, 0, 0, 0, 0, 0 }, tick = 0;
			bool ringHit = false;
			if (e != nullptr) {
				total   = e->total;
				tick    = e->tick;
				ringHit = true;
				for (int k = 0; k < 6; ++k) subs[k] = e->subs[k];
			} else {
				pc_state_hash_current(&total, subs, &tick);
			}
			printf("[netplay] desync detected: frame=%d local=%08x remote=%08x handle=%d\n",
			       ev[i]->data.desynced.frame, ev[i]->data.desynced.local_checksum,
			       ev[i]->data.desynced.remote_checksum, ev[i]->data.desynced.remote_handle);
			printf("[netplay] desync subs at tick=%llu%s: total=%016llx (fold %08x) "
			       "navi=%016llx piki=%016llx teki=%016llx item=%016llx world=%016llx "
			       "rng=%016llx\n",
			       (unsigned long long)tick, ringHit ? "" : " (ring miss: latest)",
			       (unsigned long long)total, fold_hash64(total),
			       (unsigned long long)subs[0], (unsigned long long)subs[1],
			       (unsigned long long)subs[2], (unsigned long long)subs[3],
			       (unsigned long long)subs[4], (unsigned long long)subs[5]);
			fflush(stdout);
			pc_state_hash_flush();
			stop_session();
			sPhase = kDone;
			fflush(stdout);
			std::exit(5);
			break;
		}
		default:
			break;
		}
	}
	fflush(stdout);
}

// Runs one tick body per Advance event, in event order. Returns the number
// of Advances processed (0 while waiting for remote input).
int handle_game_events(System* sys, BaseApp* app)
{
	int count           = 0;
	GekkoGameEvent** ev = gekko_update_session(sGekko, &count);
	if (ev == nullptr || count <= 0) return 0;
	int advances = 0;
	for (int i = 0; i < count; ++i) {
		// The exit/disconnect handlers below stop the session mid-batch;
		// never touch it again afterwards.
		if (sPhase != kSession || sGekko == nullptr) break;
		if (ev[i] == nullptr) continue;
		switch (ev[i]->type) {
		case GekkoAdvanceEvent: {
			GekkoGameEvent* e = ev[i];
			if (e->data.adv.input_len != 32 || e->data.adv.inputs == nullptr) {
				printf("[netplay] bad advance: input_len=%u\n", e->data.adv.input_len);
				fflush(stdout);
				std::abort();
			}
			PcNetplayInput p0, p1;
			if (!pc_netplay_input_decode(e->data.adv.inputs, 16, p0)
			    || !pc_netplay_input_decode(e->data.adv.inputs + 16, 16, p1)) {
				printf("[netplay] advance decode failed\n");
				fflush(stdout);
				std::abort();
			}
			inject_input(0, p0);
			inject_input(1, p1);
			inject_neutral_pad(2);
			inject_neutral_pad(3);
			// Exactly one tick: the same per-tick sequence the normal
			// path runs (M4). Jac_Gsync drives the per-frame audio event
			// timers + gameplay-audio unpause; OSCheckActiveThreads is the
			// normal path's liveness check. Both are per-Advance (not per
			// loop turn), so they stay deterministic. The det profile note
			// mirrors the normal path's 600-tick report; input_log_tick_end
			// is a no-op with no record/replay but keeps recording
			// unsupported-but-harmless instead of silently skipped.
			Jac_Gsync();
			(void)OSCheckActiveThreads();
			sys->updateSysClock();
			pc_netplay_on_tick_begin();
			app->idle();
			pc_netplay_det_profile_note_tick();
			pc_input_log_tick_end();
			pc_state_hash_tick_end();
			{
				uint64_t total = 0, subs[6] = { 0, 0, 0, 0, 0, 0 }, tick = 0;
				if (pc_state_hash_current(&total, subs, &tick) && tick > 0)
					hash_ring_store(tick, total, subs);
			}
			++sSessionTicks;
			++advances;
			++sAdvances;
			if (sCfg.exitAfter > 0 && sSessionTicks >= sCfg.exitAfter) {
				const double nowW = now_ms();
				const double wallS =
				    (sSessionStartMs > 0) ? (nowW - sSessionStartMs) / 1000.0 : 0.0;
				const double tps = wallS > 0 ? (double)sAdvances / wallS : 0.0;
				const double stallPct =
				    wallS > 0 ? 100.0 * sStallMs / (wallS * 1000.0) : 0.0;
				printf("[netplay] exit after %llu ticks\n", (unsigned long long)sSessionTicks);
				printf("[netplay] script records consumed: %llu/%llu\n",
				       (unsigned long long)sScriptIdx, (unsigned long long)sScriptTicks);
				printf("[netplay] wall=%.1fs tps=%.1f stall=%.1f%% (wall-clock)\n", wallS,
				       tps, stallPct);
				fflush(stdout);
				pc_state_hash_flush();
				stop_session();
				sPhase = kDone;
				request_quit();
				break; // stop processing this batch; the session is gone
			}
			break;
		}
		case GekkoSaveEvent: {
			GekkoGameEvent* e = ev[i];
			uint64_t total = 0, subs[6] = { 0, 0, 0, 0, 0, 0 }, tick = 0;
			pc_state_hash_current(&total, subs, &tick);
			if (e->data.save.state != nullptr && e->data.save.state_len != nullptr
			    && *e->data.save.state_len >= 8) {
				uint64_t frame = (uint64_t)e->data.save.frame;
				for (int b = 0; b < 8; ++b)
					e->data.save.state[b] = (uint8_t)((frame >> (b * 8)) & 0xFF);
				*e->data.save.state_len = 8;
			}
			if (e->data.save.checksum != nullptr) *e->data.save.checksum = fold_hash64(total);
			++sSaves;
			break;
		}
		case GekkoLoadEvent:
			// Window 0 never rolls back; a Load means the library's
			// lockstep contract broke.
			printf("[netplay] unexpected load event at frame=%d (window 0 forbids loads)\n",
			       ev[i]->data.load.frame);
			fflush(stdout);
			std::abort();
			break;
		default:
			break;
		}
	}
	if (sPhase == kSession && sSessionTicks > 0 && sSessionTicks % 300 == 0 && advances > 0) {
		const double now = now_ms();
		const double secs = (now - sRunStartMs) / 1000.0;
		const double tps  = secs > 0 ? (double)sAdvances / secs : 0.0;
		// M5: wall-clock stall % (time in turns with no Advance over
		// session wall time). The turn-counter ratio is kept in the log
		// for continuity but is NOT the stall metric under UNTHROTTLED.
		const double wallS =
		    (sSessionStartMs > 0) ? (now - sSessionStartMs) / 1000.0 : 0.0;
		const double wallStallPct =
		    wallS > 0 ? 100.0 * sStallMs / (wallS * 1000.0) : 0.0;
		const double turnStallPct =
		    (sAdvances + sStalls) > 0 ? 100.0 * (double)sStalls / (double)(sAdvances + sStalls) : 0.0;
		printf("[netplay] tick=%llu adv=%llu stalls=%llu (turn %.1f%%, wall %.1f%%) "
		       "tps=%.1f ahead=%.2f saves=%llu submitted=%llu\n",
		       (unsigned long long)sSessionTicks, (unsigned long long)sAdvances,
		       (unsigned long long)sStalls, turnStallPct, wallStallPct, tps,
		       gekko_frames_ahead(sGekko), (unsigned long long)sSaves,
		       (unsigned long long)sSubmitted);
		fflush(stdout);
	}
	return advances;
}

} // namespace

void pc_netplay_session_notify_argv(int argc, char** argv)
{
	sArgc = argc;
	sArgv = argv;
}

bool pc_netplay_session_active(void)
{
	if (!sInitialised) {
		parse_config();
		sInitialised = true;
	}
	return sCfg.active;
}

bool pc_netplay_session_drive(System* sys, BaseApp* app)
{
	if (!pc_netplay_session_active()) return false; // switch off: normal path
	if (sys == nullptr || app == nullptr) return false;

	if (!sForcedModes) {
		sForcedModes = true;
		// Netplay forces deterministic mode and co-op on (brief item 5).
		pc_netplay_det_force_on();
		PcCoopSwitch sw = pc_coop_switch_parse(sArgc, sArgv);
		sw.coop         = true;
		pc_coop_switch_apply(sw);
		compute_local_hello();
		printf("[netplay] mode=%s delay=%u seed=%u\n", sCfg.isHost ? "host" : "join",
		       sCfg.localDelay, sCfg.seed);
		fflush(stdout);
		// Transport up before the handshake pump runs.
		sSock = new pc_netplay_transport::UdpSocket();
		if (sCfg.isHost) {
			if (!sSock->bind(sCfg.hostPort)) {
				printf("[netplay] bind port %u failed\n", (unsigned)sCfg.hostPort);
				fflush(stdout);
				std::exit(1);
			}
		} else {
			if (!sSock->bind(0)) {
				printf("[netplay] bind ephemeral failed\n");
				fflush(stdout);
				std::exit(1);
			}
			sSock->set_peer(sCfg.joinIp, sCfg.joinPort);
		}
		sLink = new pc_netplay_transport::GekkoLink(sSock);
	}

	if (pc_window_should_close()) {
		// Relinquish the loop so System::run breaks at its should_close
		// check (before any further tick) and shuts down normally.
		if (sPhase != kDone) {
			pc_state_hash_flush();
			stop_session();
			sPhase = kDone;
		}
		return false;
	}

	if (sPhase == kDone) {
		// Drain the quit event requested above (nothing else polls while
		// the driver owns the loop), then relinquish so the loop breaks.
		pc_window_poll_events(nullptr);
		if (pc_window_should_close()) return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return true;
	}

	if (sPhase == kHandshake) {
		// Keep the window responsive while waiting for the peer.
		// m12: mControllerMgr.update() already pumps the window via
		// PADRead -> pc_window_poll_events; a second poll here consumed
		// edge latches twice per turn, so only poll once.
		sys->mControllerMgr.update();
		if (handshake_pump()) start_gekko_session();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return true;
	}

	// kSession.
	const bool unthrottled = pc_netplay_unthrottled();
	const double turnStartMs = now_ms();
	// 1. Sample the local pad (pumps SDL via PADRead).
	sys->mControllerMgr.update();
	// 2-3. Build + submit the local input, at most one per Advance (B2).
	// GekkoNet accepts exactly one local input per frame; extra submits
	// for the same frame are silently dropped (InputBuffer::AddInput),
	// which used to burn a script record / pad sample per stall turn and
	// left both peers on neutral input for >98% of the pair runs. The
	// transport test's fed == adv gate is the model: submit only when the
	// session will accept one. Before SessionStarted nothing is submitted.
	int advances = 0;
	if (sGekko != nullptr) {
		// M1: keep answering late handshake traffic while in session.
		answer_handshake_in_session();
		if (sGekkoStarted && sSubmitted == sAdvances) {
			// B1: fresh capture so the local yaw follows the live camera
			// instead of freezing at the first injected value.
			pc_input_log_capture_yaw_fresh();
			PcNetplayInput local = build_local_input();
			uint8_t wire[16];
			pc_netplay_input_encode(local, wire);
			gekko_add_local_input(sGekko, sLocalHandle, wire);
			++sSubmitted;
		}
		// 4-5. Advance + per-tick block.
		advances = handle_game_events(sys, app);
		// 8. Session events.
		handle_session_events();
		if (sPhase != kSession) return true; // stopped inside the handlers
	}
	if (advances == 0) {
		++sStalls;
		// M5: wall-clock stall accounting (fraction of session wall time
		// spent in turns with no Advance).
		sStallMs += now_ms() - turnStartMs;
		// 9. Waiting: no tick. The turn above already pumped the network
		// (update_session) and window events (PADRead poll).
	}
	// Pacing: real-time 30 Hz ticks; unthrottled runs as fast as the
	// session allows (tests).
	if (!unthrottled) {
		// m2: the old ahead > 6 skip never fired (|ahead| <= 2.5 in the
		// logs). Slow down proportionally when ahead instead: sleep a
		// share of the frame budget per ahead-frame past 0.75.
		const float aheadNow = (sGekko != nullptr) ? gekko_frames_ahead(sGekko) : 0.0f;
		if (aheadNow > 0.75f) {
			const double extraMs = (double)(aheadNow - 0.75f) * (1000.0 / 30.0) * 0.5;
			if (extraMs > 0.0)
				std::this_thread::sleep_for(
				    std::chrono::duration<double, std::milli>(extraMs));
		}
		const double now = now_ms();
		if (sNextTurnMs == 0) sNextTurnMs = now + 1000.0 / 30.0;
		if (now < sNextTurnMs) {
			std::this_thread::sleep_for(
			    std::chrono::duration<double, std::milli>(sNextTurnMs - now));
			sNextTurnMs += 1000.0 / 30.0;
		} else {
			sNextTurnMs = 0; // overrun: no catch-up spiral
		}
	}
	return true;
}
