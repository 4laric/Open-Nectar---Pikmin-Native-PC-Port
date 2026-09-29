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
//      while the menu is open). Every turn's physical sample is folded into
//      the button-OR / latest-sticks accumulator (B2 residual fix).
//   2. Build the local input (accumulator take, or the next scripted
//      record).
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
// Pacing: one network turn per System::run loop iteration. While a netplay
// session runs, the session owns the single 30 Hz schedule (the presentation
// limiter in pc_window.cpp stays off there, otherwise two stacked pacers cap
// throttled pairs at ~21 tps). Drift-free deadline: next += 1000/30 ms per
// Advance, with bounded catch-up (snap forward when more than ~5 slots
// behind, at most 2 slots per turn so hiccups replay over several turns).
// gekko_frames_ahead() stretches the deadline proportionally and small when
// ahead past 0.75 (0.05 slot per frame, capped at 10% of a slot), so an ahead
// peer converges. Stall turns (no Advance) never spend a whole slot: they
// pump the network and wait ~1 ms, so the 30 Hz budget is spent on ticks, not
// waits. Short waits use one shared high-resolution waitable timer (the
// timer carries the bulk; a slot wait spins at most its last 1 ms and poll
// waits never spin), so throttled pairs hold 30 Hz and a lobby wait idles
// without pinning a core. While the session runs, the process opts out of
// timer-resolution power throttling (its timer-resolution requests are
// always honoured); at session end control returns to the system.
// PIKMIN_NETPLAY_UNTHROTTLED=1 runs as fast as the session allows (tests).
// Stall % is the wall-clock share of no-Advance turns (the slot wait lives on
// advance turns and is never charged as stall); the logs also give effective
// tps plus the 1 - tps/30 slot-loss fraction (n2). Local delay is
// PIKMIN_NETPLAY_DELAY frames (default 2) or "auto" (nonce-matched median
// handshake RTT, ceil((RTT/2)/33.333ms - 0.05)+1 with a 0.05-slot boundary
// tolerance, clamped 1..8; delay is per-peer, not hashed). Disconnect timeout
// is PIKMIN_NETPLAY_DISCONNECT_MS (default 15000) (N3). Trade-off: a real peer
// loss takes ~15 s to detect (GekkoNet wall-clock idle timer); use 5000 only
// for tests. Long blocking loads (M4 gap-fix lane S, issue #885): the load
// guard (pc_netplay_loadguard.h) keeps a loading peer talking with a
// main-thread keep-alive network poll between the steps of the long loops
// (TEV program creation, DVD reads, the day-end save's I/O), and raises the
// timeout to 60 s from each stage load until 30 frames later on both peers (a
// deterministic window, no message needed); a peer that dies inside that
// window is detected after the window's timeout instead. The window also
// changes the old N3 negative control: PIKMIN_NETPLAY_DISCONNECT_MS=5000 with
// a 9 s PIKMIN_NETPLAY_TEST_LOAD_DELAY_MS no longer disconnects (the load
// runs under the 60 s window timeout); that control now needs
// PIKMIN_NETPLAY_LOAD_GUARD=0 (or PIKMIN_NETPLAY_LOAD_WINDOW=0).
// Handshake wire: stable 7-byte header prefix (magic 4 + type 1 + proto LE16)
// parsed before the full length check, so a protocol mismatch refuses fast
// with code 4 instead of a 30 s timeout;
// PIKMIN_NETPLAY_TEST_PROTOCOL_VERSION overrides the local version and
// PIKMIN_NETPLAY_TEST_HANDSHAKE_LEN=108 sends the v1 length to exercise the
// cross-length refuse path. The refuse field is fixed at offset 107 in every
// version. M4 lane B2 (issue #885): protocol v3 (252 bytes, layout in
// pc_netplay_transfer.h) adds the checkpoint / card / P2 digests; the test
// length hook also accepts 116 (the v2 length). A new kTransfer phase between
// the handshake and GekkoNet moves the host's checkpoint and P2 sidecars to
// the joiner over the bulk channel, and pc_netplay_save_barrier agrees the
// day-end save outcome inside the save tick (bulk channel only).

#include "netplay/pc_netplay_session.h"

#include "netplay/pc_netplay_adaptive.h"
#include "netplay/pc_netplay_det.h"
#include "netplay/pc_netplay_gekko_input.h"
#include "netplay/pc_netplay_ice.h"
#include "netplay/pc_netplay_input_sel.h"
#include "netplay/pc_netplay_launch.h"
#include "netplay/pc_netplay_loadguard.h"
#include "netplay/pc_netplay_pad.h"
#include "netplay/pc_netplay_present.h"
#include "netplay/pc_netplay_randstate.h"
#include "netplay/pc_netplay_transfer.h"
#include "netplay/pc_netplay_udp.h"
#include "netplay/pc_input_log.h"
#include "netplay/pc_state_hash.h"
#include "netplay/pc_coop_switch.h"
#include "pc_coop.h"
#include "pc_coop_policy.h"
#include "pc_window.h"
#include "settings/pc_settings.h"

#include "gekkonet.h"

#include "system.h"
#include "BaseApp.h"
#include "Controller.h"
#include "Dolphin/pad.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <synchapi.h>
#endif

// pc_state_hash additions (M3): capture the last tick's hashes even when no
// hash log file is open, so Save events and desync dumps have checksums.
// Declared here to avoid a header dependency cycle; defined in
// pc_state_hash.cpp. Inert unless pc_state_hash_set_netplay_capture(true).
void pc_state_hash_set_netplay_capture(bool on);
bool pc_state_hash_current(uint64_t* total, uint64_t subs[7], uint64_t* tick);
// Netplay M4 lane A randomizer hooks (issue #885). Strong-defined by
// pc_randomizer.cpp (linked into every game build); null here only in
// engine-free harnesses, where each use is guarded. The session never
// touches randomizer state except through these two functions, and only at
// the deterministic points below (submit embed on the host, apply at tick
// start on both peers).
#if defined(__GNUC__)
__attribute__((weak)) bool pc_randomizer_apply_net_state(const pc_randstate::PcRandState& st);
__attribute__((weak)) bool pc_randomizer_get_net_state(pc_randstate::PcRandState* out);
__attribute__((weak)) bool pc_randomizer_enabled(void);
__attribute__((weak)) bool pc_randomizer_force_net_publish(void);
// M4 lane B1 (issue #885): outbox flush, host link liveness, RESUME
// snapshot, client mirror ledger and the sim-affecting TEST knob. Same weak
// pattern: strong in pc_randomizer.cpp, null only in engine-free harnesses.
__attribute__((weak)) void pc_randomizer_outbox_flush(uint32_t frame);
__attribute__((weak)) bool pc_randomizer_link_live(void);
__attribute__((weak)) bool pc_randomizer_resume_snapshot(pc_randstate::PcRandState* out);
__attribute__((weak)) void pc_randomizer_mirror_ledger_receive(const uint8_t* data, size_t len,
                                                               uint32_t frame);
__attribute__((weak)) bool pc_randomizer_test_deathlink_as_ordinary(void);
// M4 lane B2 (issue #885): checkpoint info for the Hello, the campaign dir
// for the card digest and the transfer, the joiner's adoption, and the P2
// bridge switch (sidecar / overlay digests). Same weak pattern.
__attribute__((weak)) bool pc_randomizer_checkpoint_info(uint64_t* gen, uint8_t sha[32]);
__attribute__((weak)) const char* pc_randomizer_campaign_dir(void);
__attribute__((weak)) bool pc_randomizer_adopt_checkpoint(void);
__attribute__((weak)) bool pc_randomizer_p2_bridge(void);
// B2 fix round 1 (C2): the barrier is abandoned (exit 5 or 6); the client
// retracts the checkpoint it wrote for this save before the process exits.
__attribute__((weak)) void pc_randomizer_netplay_barrier_abandoned(void);
#else
bool pc_randomizer_apply_net_state(const pc_randstate::PcRandState& st);
bool pc_randomizer_get_net_state(pc_randstate::PcRandState* out);
bool pc_randomizer_enabled(void);
bool pc_randomizer_force_net_publish(void);
void pc_randomizer_outbox_flush(uint32_t frame);
bool pc_randomizer_link_live(void);
bool pc_randomizer_resume_snapshot(pc_randstate::PcRandState* out);
void pc_randomizer_mirror_ledger_receive(const uint8_t* data, size_t len, uint32_t frame);
bool pc_randomizer_test_deathlink_as_ordinary(void);
bool pc_randomizer_checkpoint_info(uint64_t* gen, uint8_t sha[32]);
const char* pc_randomizer_campaign_dir(void);
bool pc_randomizer_adopt_checkpoint(void);
bool pc_randomizer_p2_bridge(void);
void pc_randomizer_netplay_barrier_abandoned(void);
#endif
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
	kTransfer, // M4 lane B2: checkpoint / P2 sidecar transfer before the GekkoNet session
	kSession,
	kDone,     // clean end requested: keep owning the loop until the process quits
};

struct Config {
	bool isHost = false;
	bool active = false;
	uint16_t hostPort = 0;      // --netplay-host <port>
	uint32_t joinIp = 0;        // --netplay-join <ip:port>
	uint16_t joinPort = 0;
	bool iceMode = false;       // M5a: --netplay-ice-host / --netplay-ice-join
	std::string iceJoinCode;    // --netplay-ice-join <offer-code-or-@file>
	unsigned localDelay = 2;    // PIKMIN_NETPLAY_DELAY (numeric)
	bool delayAuto = false;     // PIKMIN_NETPLAY_DELAY=auto (fix round 2)
	uint32_t seed = 0;          // PIKMIN_NETPLAY_SEED (launcher joiner: the offer's)
	std::string bootstrapPath;  // --randomizer-seed <file> or bootstrap.txt
	std::string localInputFile; // PIKMIN_NETPLAY_LOCAL_INPUT_FILE
	uint64_t exitAfter = 0;     // PIKMIN_NETPLAY_EXIT_AFTER_TICKS (0 = run)
	// Launch lane (issue #887): one command per role, no environment
	// variables. The launcher's own state lives in PcNetplayLaunch
	// (pc_netplay_launch.h), resolved before engine init.
	bool launcherMode = false;  // --netplay-host-ice / --netplay-join-ice
	std::string inputSpec;      // --netplay-input / PIKMIN_NETPLAY_INPUT
	int inputKind = 0;          // pc_netplay_input_sel::Kind (0 = auto)
	int inputGamepad = 0;       // gamepad index for kind == gamepad
};

int sArgc = 0;
char** sArgv = nullptr;
Config sCfg;
Phase sPhase = kIdle;
bool sInitialised = false;

// Handshake constants.
// Polish (issue #880): stable 7-byte header prefix across versions (magic 4 +
// type 1 + proto LE16 at the same offsets), parsed before the full length
// check so a protocol mismatch refuses fast with code 4 instead of a 30 s
// timeout. v1 is 108 bytes (no nonce); v2 adds the 8-byte nonce echo.
// M4 lane B2 (issue #885): v3 (252 bytes) adds the checkpoint generation and
// digest, the card digest and the P2 sidecar / overlay digests; the layout,
// the codec and the refuse fields live in pc_netplay_transfer.h. The refuse
// field stays at offset 107, so v1/v2 peers still refuse `protocol` fast.
using pc_netplay_xfer::kHsMagic;
constexpr uint16_t kProtocolVersion = pc_netplay_xfer::kProtocolV3;
constexpr size_t kHsHeaderLen = pc_netplay_xfer::kHsHeaderLen; // 7, stable across versions
constexpr size_t kHsLenV1 = pc_netplay_xfer::kHsLenV1;         // 108
constexpr size_t kHsLenV2 = pc_netplay_xfer::kHsLenV2;         // 116
constexpr uint8_t kHsHello  = pc_netplay_xfer::kHsHello;
constexpr uint8_t kHsAck    = pc_netplay_xfer::kHsAck;
constexpr uint8_t kHsRefuse = pc_netplay_xfer::kHsRefuse;
// Refuse field ids (logged as names).
constexpr uint8_t kFieldProto = pc_netplay_xfer::kFieldProto;
constexpr uint8_t kFieldExe = pc_netplay_xfer::kFieldExe;
constexpr uint8_t kFieldConfig = pc_netplay_xfer::kFieldConfig;
constexpr uint8_t kFieldBootstrap = pc_netplay_xfer::kFieldBootstrap;
constexpr uint8_t kFieldSeed = pc_netplay_xfer::kFieldSeed;
constexpr uint8_t kFieldCheckpoint = pc_netplay_xfer::kFieldCheckpoint; // 6
constexpr uint8_t kFieldSidecars = pc_netplay_xfer::kFieldSidecars;     // 7
constexpr uint8_t kFieldP2Assets = pc_netplay_xfer::kFieldP2Assets;     // 8
constexpr size_t kHsLen = pc_netplay_xfer::kHsLenV3; // 252

const char* field_name(uint8_t f) { return pc_netplay_xfer::field_name(f); }

// Polish protocol override (test only): PIKMIN_NETPLAY_TEST_PROTOCOL_VERSION
// replaces the local wire version for sends and for the mismatch check, so a
// mixed-version pair can be exercised without rebuilding.
uint16_t local_protocol_version()
{
	static bool init = false;
	static uint16_t v = kProtocolVersion;
	if (!init) {
		init = true;
		if (const char* e = std::getenv("PIKMIN_NETPLAY_TEST_PROTOCOL_VERSION")) {
			char* end = nullptr;
			unsigned long n = strtoul(e, &end, 10);
			if (end != e && *end == '\0' && n >= 1 && n <= 0xFFFF) v = (uint16_t)n;
		}
	}
	return v;
}

// Stable header parse: magic + type + proto, valid for any version length.
// Lets a protocol mismatch refuse fast instead of timing out on length.
bool parse_hello_header(const uint8_t* p, size_t len, uint8_t* type, uint16_t* proto)
{
	return pc_netplay_xfer::decode_hello_header(p, len, type, proto);
}

// v3 Hello (pc_netplay_transfer.h): exe/config/bootstrap/seed/nonce plus
// ckptGen/ckptSha/cardSha/sidecarSha/p2AssetsSha.
using Hello = pc_netplay_xfer::Hello;

Hello sLocal;
// M4 lane B2: the peer's Hello/Ack fields once the handshake accepted them
// (the transfer phase and the decision log read them).
Hello sRemote;
bool sHaveRemoteHello = false;
uint8_t sExeHex[128] = { 0 };
std::string sExeHexStr;
std::string sCfgHexStr;
std::string sBootHexStr;

// Transport / session objects (owned by the session TU, created at handshake).
pc_netplay_transport::UdpSocket* sSock = nullptr;
pc_netplay_transport::GekkoLink* sLink = nullptr;
// M5a ICE transport (issue #887): exactly one of the UDP pair (sSock/sLink)
// or the ICE pair (sIce/sIceLink) is ever non-null. The session talks to
// whichever is up through hs_send()/hs_drain() below, so the handshake and
// GekkoNet flows are unchanged.
pc_netplay_ice::IceSocket* sIce = nullptr;
pc_netplay_ice::IceLink* sIceLink = nullptr;
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
// Fix round 3 (DELAY=auto): nonce-matched RTT measurement. Every Hello send
// gets a fresh 8-byte nonce with its send timestamp recorded; each received
// Ack echoes the Hello nonce it answers, so the sample is now - send[nonce]
// for the matching send, not the latest resend. Samples are median-filtered
// (at least 5 required when delay=auto) instead of the old minimum, which
// biased down under periodic resends.
uint64_t sHsNextNonce = 1;
uint64_t sHsLastHelloNonce = 0; // last Hello nonce received (for Ack echo)
std::vector<std::pair<uint64_t, double>> sHsSendTimes; // nonce -> send ms
std::vector<double> sHsSamples;                        // RTT samples, ms
double sHsRttMs = -1; // median at resolve time (-1 = none yet)
// Fix round 3 (M1 test): drop handshake-phase Acks when
// PIKMIN_NETPLAY_TEST_DROP_FINAL_ACK=1. The dropping peer still sets
// sSentAck and enters the session on the peer's Ack, while the peer never
// gets an Ack and must recover via in-session Hello answers.
bool sDropFinalAckInit = false;
bool sDropFinalAck = false;
// N3 test hook: PIKMIN_NETPLAY_TEST_LOAD_DELAY_MS sleeps once inside the
// next stage load on that peer only (see pc_netplay_on_stage_load).
bool sLoadDelayDone = false;
// M4 gap-fix lane S (issue #885): load guard (pc_netplay_loadguard.h has the
// design). sInAdvance is true only while app->idle() runs inside an Advance,
// the only place the keep-alive may pump. Per-tick figures feed the long-tick
// log line; session totals feed the stop_session summary. The long loops also
// run on other threads (the audio / system DVD threads, the loading-screen
// thread), so the keep-alive acts only on the session's own thread: the
// thread id is written before the first tick, and sInAdvance (atomic) is set
// only by that thread, so another thread that sees it true also sees the id.
std::atomic<bool> sInAdvance{ false };
std::thread::id sLgMainThread;
bool sLgConfigured = false;
bool sLgGuard = true;        // PIKMIN_NETPLAY_LOAD_GUARD (fix round 1: also the save barrier's wait)
bool sLgKeepAlive = true;
bool sLgSpeculative = false; // the tick being executed is a rollback / runahead re-run (MN7)
pc_netplay_loadguard::KeepAliveGate sLgGate;
pc_netplay_loadguard::LoadWindow sLgWindow;
pc_netplay_loadguard::StallPlan sLgStall;
bool sLgStallDone = false;
uint32_t sLgStageLoads = 0;  // stage loads inside session ticks
uint32_t sLgTickFrame = 0;   // frame of the tick being executed
double sLgTickStartMs = 0;   // wall time the current tick's app->idle() began
double sLgLastPumpMs = 0;    // last network pump inside this tick (or its start)
double sLgTickMaxGapMs = 0;  // longest stretch without a pump inside this tick
uint64_t sLgTickPumps = 0;   // keep-alive pumps inside this tick
uint64_t sLgPumps = 0;       // keep-alive pumps, whole session
uint64_t sLgSitePumps[pc_netplay_loadguard::kSiteCount] = {}; // ... by call site
uint64_t sLgWindows = 0;     // load windows opened
uint64_t sLgLongTicks = 0;   // ticks longer than kLongTickMs
double sLgLongestTickMs = 0; // longest tick of the session
double sLgWorstGapMs = 0;    // longest unpumped stretch inside any long tick
double sLgWindowOpenMs = 0;  // wall time the current window opened
uint32_t sLgWindowFirst = 0; // frame of the stage load that opened it
bool sLgSummaryDone = false;
// Launch lane test hook (B3 evidence): PIKMIN_NETPLAY_TEST_F1_CYCLE_TICK=<n>
// opens and closes the F1 menu once, right after tick n, which runs the
// settings save path mid-session.
uint64_t sF1CycleTick = 0;
bool sF1CycleDone = false;

// Run stats.
uint64_t sSessionTicks = 0;
uint64_t sAdvances = 0;
uint64_t sStalls = 0;
uint64_t sSaves = 0;
// B2: number of local inputs actually submitted to GekkoNet. A submit is
// only accepted when it targets the session's current frame (InputBuffer
// drops non-sequential frames), so the driver submits at most one input per
// Advance (sSubmitted == sAdvances once started) and consumes one script
// record / pad sample per submit. M5c lane B: a count of local inputs added
// (a delay increase adds several in one turn); the gate is sNextLand below.
uint64_t sSubmitted = 0;
// M5c lane B (issue #887): the frame the next local input lands on
// (GekkoNet's local last-received frame + 1; pc_netplay_adaptive.h has the
// transition arithmetic). A submit is due when sNextLand == sAdvances +
// sCfg.localDelay, which is exactly the old sSubmitted == sAdvances gate
// while the delay never changes. The B1 HOLD gate, the RESUME catch-up and
// the host's hold-frame check read it instead of sSubmitted + localDelay.
uint64_t sNextLand = 0;
double sRunStartMs = 0;
double sNextTurnMs = 0;
bool sAheadLogged = false;
// M5: wall-clock stall accounting. sStallMs accumulates the wall time of
// loop turns that produced no Advance (advance turns own the slot wait, so
// the pacing sleep is never charged as stall; fix m1). The charge runs from
// turn start to turn end.
// (The old sStalls turn counter is kept for the log line, but under
// UNTHROTTLED the loop spins ~1500 turns per advance, so the turn ratio is
// not a wall-clock stall.)
double sStallMs = 0;
double sSessionStartMs = 0;
// B2 residual (fix round 2): physical-pad accumulator between submits.
// Every kSession turn folds its pad sample into this (buttons OR, sticks/
// yaw latest); each submit takes the merged input. Scripted file inputs
// bypass it (one record per submit).
PcNetplayAccum sPadAccum;
// M3: ring of per-tick hashes so a desync report can dump the desynced
// frame's sub-hashes, not the latest tick's. GekkoNet frame F maps to hash
// tick F+1 (ticks are 1-based, frames 0-based). 256 deep: well past the
// check_distance-7 health lag, even at 100 ms latency.
struct HashEntry {
	bool valid = false;
	uint64_t tick = 0;
	uint64_t total = 0;
	uint64_t subs[7] = { 0, 0, 0, 0, 0, 0, 0 };
};
constexpr size_t kHashRing = 256;
HashEntry sHashRing[kHashRing];

void hash_ring_store(uint64_t tick, uint64_t total, const uint64_t subs[7])
{
	HashEntry& e = sHashRing[tick % kHashRing];
	e.valid      = true;
	e.tick       = tick;
	e.total      = total;
	for (int i = 0; i < 7; ++i) e.subs[i] = subs[i];
}

const HashEntry* hash_ring_find(uint64_t tick)
{
	const HashEntry& e = sHashRing[tick % kHashRing];
	return (e.valid && e.tick == tick) ? &e : nullptr;
}

// ---- M4 lane A randomizer external-state stream (issue #885) ----
//
// Kept in clearly separated functions (per the brief) to minimise merge
// conflicts with the polish lane (pacing/sleep/handshake ownership).
//
// Deterministic rule (documented per the brief): the host is the only peer
// that ever sets HAS_CHUNK. It emits each published generation's 16
// fragments on 16 consecutive host submits (a mid-transfer publish waits in
// a one-deep queue and starts at the next fragment 0, so generations are
// never mixed). Every Advance delivers the same host input (p0) on both
// peers, so both reassemblers complete generation g in the same Advance
// frame F, and both apply it at the start of the tick for frame F+1,
// before inject_input() and app->idle(). Duplicate, stale (gen <= applied)
// or incomplete fragments are no-ops. The reassembly buffer is fed
// identically on both peers, so both hold identical copies.
//
// Session start: the host publishes gen 1 at boot (stream-mode update) and
// start_gekko_session forces a publish whatever the stamp says, so the
// first full snapshot rides the first 16 submits. Until it applies, both
// peers run neutral inputs on identical pristine state (randstate_gate_
// neutral); the gate lifts on the same frame on both peers. Link-liveness
// HOLD on AP drops stays lane B work.
double now_ms(); // defined below (wall-clock milliseconds)
bool sRandStream = false; // cached: session active && env gate on
pc_randstate::Reassembler sRandReasm;
uint8_t sRandWire[pc_randstate::kStateBytes] = {};
bool sRandHaveSnapshot = false; // host published at least one snapshot
size_t sRandNextFrag = 0;       // next fragment index to embed (0..kFragCount)
// M1 fix: a publish that lands mid-transfer no longer restarts the cursor
// (which mixed two generations into one CRC-failing buffer on both peers).
// The newer full snapshot waits in this one-deep queue and swaps in when
// the in-flight generation's last fragment is submitted. Coalescing is safe
// because snapshots are full state; at most one waits.
uint8_t sRandQueuedWire[pc_randstate::kStateBytes] = {};
bool sRandHaveQueued = false;
pc_netplay_bulk::BulkChannel sBulk; // M4a bulk 0x03 endpoint (lane B queues)

// ---- M4 lane B1 synchronized HOLD/RESUME + mirror ledger (issue #885) ----
//
// The host's randomizer link (pc_randomizer_link_live: state.txt readable,
// ready=1, rewritten within 3 s; always live in launcher sessions) going
// down makes the host set kFlagsHold on exactly one submitted input. That
// input's frame H is the hold frame on both peers (p0 is the host input on
// both). Each peer keeps submitting until its next local input would land
// on frame H+kHoldLeadFrames (the frame of a submit is its index plus the
// local delay: GekkoNet InputBuffer::AddLocalInput stores the input of
// current frame c at c + delay, and the sSubmitted == sAdvances gate makes
// c == sSubmitted), so both peers advance through H+11 and then produce no
// Advance. While held every turn still answers handshakes, pumps bulk,
// updates GekkoNet (its 500 ms NetworkHealth packets keep both disconnect
// timers fed) and handles session events. When the link is live again and
// the freeze point is reached, the host sends bulk kBulkRandFull
// {u32 resumeFrame = H+12, 64-byte snapshot with a fresh gen} and resumes
// submitting; the client resumes submitting only once it holds that
// snapshot, so frame H+12 cannot advance anywhere before it. Both apply it
// at the tick start of H+12 (before inject_input), drop any pending
// fragment generation <= gen and mark gen applied. kHoldLeadFrames must
// exceed the maximum local delay (8): when a peer learns H (Advance H) it
// has submitted at most frame H + delay <= H+8, so no input past H+11 can
// exist yet on either peer.
constexpr uint32_t kHoldLeadFrames = 12;
bool sHoldAtFirstInput = false; // host: state.txt missing at session start
bool sHoldRequested = false;    // host: flagged input submitted, Advance H not yet seen
uint64_t sHoldExpectFrame = 0;  // host: submit index + delay of the flagged input
bool sHolding = false;          // both: Advance H seen, RESUME not yet applied
uint32_t sHoldFrame = 0;
double sHoldStartMs = 0;
bool sHeldLogged = false;
bool sResumeHave = false;       // host: RESUME sent; client: RESUME received
uint32_t sResumeFrame = 0;
pc_randstate::PcRandState sResumeState;
bool sResumePrepared = false;   // host: snapshot built, bulk queue was full
uint8_t sResumePayload[4 + pc_randstate::kStateBytes] = {};
double sHoldMs = 0;             // wall time frozen (held at -> resume), all holds
double sHoldBeatMs = 0;         // last frozen heartbeat log
double sFrozenStartMs = 0;      // wall time of this hold's "held at"
uint64_t sHoldTurns = 0;
uint64_t sHoldsDone = 0;
uint32_t sLastAdvanceFrame = 0; // frame of the last completed Advance
uint32_t sCurAdvanceFrame = 0;  // frame of the Advance being executed (outbox stamps)
// Host: kBulkMirrorLedger payloads waiting for room in the 4-deep bulk queue.
std::vector<std::vector<uint8_t>> sLedgerOut;
constexpr size_t kLedgerOutMax = 256;
// B2 fix round 1 (X6): kBulkMirrorLedger messages the host queued this
// session (carried in its SaveResult) and the client applied, so the client
// writes a day's SAVE_RESULT mirror line only after that day's ledger lines.
uint32_t sLedgerQueued = 0;
uint32_t sLedgerApplied = 0;
// B2 fix round 1 (C15): wall time the Advance being executed started, so the
// barrier can log how long this peer's own save I/O kept GekkoNet silent.
double sCurAdvanceStartMs = 0;

void hold_reset()
{
	sHoldAtFirstInput = false;
	sHoldRequested = false;
	sHoldExpectFrame = 0;
	sHolding = false;
	sHoldFrame = 0;
	sHoldStartMs = 0;
	sHeldLogged = false;
	sResumeHave = false;
	sResumeFrame = 0;
	sResumeState = pc_randstate::PcRandState();
	sResumePrepared = false;
	sHoldMs = 0;
	sHoldBeatMs = 0;
	sHoldTurns = 0;
	sHoldsDone = 0;
	sLastAdvanceFrame = 0;
	sCurAdvanceFrame = 0;
	// Fix round 1 (B1-C10): no ledger message from an earlier session state
	// may reach this session's peer. (A process runs one netplay session:
	// kHandshake is entered only from parse_config, and kSession only ever
	// goes to kDone, so this is defensive.)
	sLedgerOut.clear();
}

bool randstate_env_on()
{
	const char* e = std::getenv("PIKMIN_NETPLAY_RANDSTATE_STREAM");
	if (e == nullptr || *e == '\0') return true; // enabled by default
	return !(e[0] == '0' && e[1] == '\0');       // ...=0 disables (negative control)
}

bool randstate_stream_on() { return sCfg.active && sRandStream; }

// Host input-build step: embed the next pending snapshot fragment into the
// local input's spare bytes. Runs after build_local_input(), before encode.
void randstate_embed_on_submit(PcNetplayInput& local)
{
	if (!randstate_stream_on() || !sCfg.isHost) return;
	// A finished transfer picks up the queued generation, if any, before
	// embedding (M1 fix: generations go out consecutively, never mixed).
	if (sRandNextFrag >= pc_randstate::kFragCount) {
		if (!sRandHaveQueued) return;
		memcpy(sRandWire, sRandQueuedWire, sizeof(sRandWire));
		sRandHaveQueued = false;
		sRandNextFrag = 0;
	}
	if (!sRandHaveSnapshot || sRandNextFrag >= pc_randstate::kFragCount) return;
	const uint8_t idx = (uint8_t)sRandNextFrag;
	local.flags |= pc_netplay_gekko::kFlagsRandChunk;
	if (idx + 1 == pc_randstate::kFragCount) local.flags |= pc_netplay_gekko::kFlagsRandLast;
	local.fragSeq = pc_randstate::frag_seq_make(idx);
	for (size_t i = 0; i < pc_randstate::kFragBytes; ++i)
		local.fragData[i] = sRandWire[idx * pc_randstate::kFragBytes + i];
	++sRandNextFrag;
}

// Tick-start step (both peers): apply a completed snapshot before the
// sim runs. Must run before inject_input() / app->idle() for this frame.
void randstate_apply_before_tick(int frame)
{
	if (!randstate_stream_on()) return;
	// B1 RESUME: the bulk snapshot applies at the tick start of H+12 on
	// both peers, before any fragment generation it supersedes.
	if (sHolding && (uint32_t)frame == sHoldFrame + kHoldLeadFrames) {
		if (!sResumeHave || sResumeFrame != (uint32_t)frame) {
			printf("[netplay] resume: frame=%d reached without its RESUME snapshot (have=%d for=%u)\n",
			       frame, (int)sResumeHave, sResumeFrame);
			fflush(stdout);
			std::abort();
		}
		const uint32_t gen = sResumeState.gen;
		const bool dropped = sRandReasm.discard_pending_upto(gen);
		bool ok = false;
		if (pc_randomizer_apply_net_state != nullptr) ok = pc_randomizer_apply_net_state(sResumeState);
		sRandReasm.mark_applied(gen);
		if (ok) printf("[netplay] randstate gen=%u applied at frame=%d\n", gen, frame);
		else printf("[netplay] randstate gen=%u dropped: apply rejected at frame=%d\n", gen, frame);
		if (dropped) printf("[netplay] resume superseded a pending fragment generation\n");
		const double resumeMs = now_ms();
		// held_ms: wall time from this peer's "hold at" (Advance H) to the
		// RESUME apply. The frozen part (held at -> resume) is exact wall
		// time and is what the stall / tps / slot-loss figures exclude.
		if (sHeldLogged) sHoldMs += resumeMs - sFrozenStartMs;
		printf("[netplay] resume at frame=%d gen=%u held_ms=%.0f\n", frame, gen, resumeMs - sHoldStartMs);
		fflush(stdout);
		sHolding = false;
		sHeldLogged = false;
		sResumeHave = false;
		++sHoldsDone;
	}
	if (!sRandReasm.has_pending()) return;
	if (frame < (int)sRandReasm.pending_frame()) return; // not yet (unreachable; defensive)
	pc_randstate::PcRandState st;
	if (!sRandReasm.take_pending(st)) return;
	const uint32_t gen = st.gen;
	bool ok = false;
	if (pc_randomizer_apply_net_state != nullptr) ok = pc_randomizer_apply_net_state(st);
	sRandReasm.mark_applied(gen);
	if (ok) {
		printf("[netplay] randstate gen=%u applied at frame=%d\n", gen, frame);
	} else {
		// Visible instead of silent: a validation drop kills the stream
		// without this line (every later snapshot carries the same bits).
		printf("[netplay] randstate gen=%u dropped: apply rejected at frame=%d\n", gen,
		       frame);
	}
	fflush(stdout);
}

// Per-Advance step (both peers): feed the host input's fragment, if any,
// into the reassembler. p0 is the host input on both peers (role-ordered
// actors). Runs after randstate_apply_before_tick() so a completion always
// arms the *next* frame.
void randstate_feed_advance(const PcNetplayInput& hostInput, int frame)
{
	if (!randstate_stream_on()) return;
	const bool has = (hostInput.flags & pc_netplay_gekko::kFlagsRandChunk) != 0;
	const bool last = (hostInput.flags & pc_netplay_gekko::kFlagsRandLast) != 0;
	sRandReasm.feed(has, hostInput.fragSeq, hostInput.fragData, last, (uint32_t)frame);
}

// M5 fix: pre-snapshot neutral gate. Until the first snapshot applies,
// both peers run neutral inputs (no gameplay) on identical pristine state.
// The gate lifts on the same frame on both peers (same p0 stream implies
// same completion and apply frames), so hashes stay identical throughout.
// Without the stream, or without the randomizer, there is nothing to wait
// for. Pre-apply ticks still hash, still pump the network, and still feed
// the reassembler above, so gen 1 (which rides the first submits) always
// arrives.
bool randstate_gate_neutral()
{
	if (!randstate_stream_on()) return false;
	if (pc_randomizer_enabled == nullptr) return false;
	if (!pc_randomizer_enabled()) return false;
	return sRandReasm.applied_gen() == 0;
}

// B1: Advance-start hold detection (both peers, after the tick-start apply
// so a RESUME at H+12 is complete before a later flag is looked at).
void hold_on_advance_begin(const PcNetplayInput& hostInput, int frame)
{
	if (!randstate_stream_on()) return;
	if ((hostInput.flags & pc_netplay_gekko::kFlagsHold) == 0) return;
	if (sHolding) {
		printf("[netplay] hold flag at frame=%d ignored: hold in progress\n", frame);
		fflush(stdout);
		return;
	}
	// Fix round 1 (B1-C2): a RESUME in hand must be for this hold's H+12.
	// The strict acceptance in hold_client_on_randfull already refuses any
	// other; this keeps a stale one from ever pre-arming the submit gate.
	if (sResumeHave && sResumeFrame != (uint32_t)frame + kHoldLeadFrames) {
		printf("[netplay] hold at frame=%d: stale RESUME for frame=%u discarded\n", frame, sResumeFrame);
		sResumeHave = false;
	}
	sHolding = true;
	sHoldFrame = (uint32_t)frame;
	sHoldStartMs = now_ms();
	sHeldLogged = false;
	printf("[netplay] hold at frame=%d freeze-after=%u\n", frame, sHoldFrame + kHoldLeadFrames - 1);
	if (sCfg.isHost) {
		// Verify the frame arithmetic against GekkoNet: the flagged input
		// landed on the frame recorded when it was added (sNextLand; submit
		// index k plus delay d while the delay never changed).
		printf("[netplay] hold frame check: host flagged submit frame=%llu, gekko frame=%d (%s)\n",
		       (unsigned long long)sHoldExpectFrame, frame,
		       sHoldExpectFrame == (uint64_t)frame ? "match" : "MISMATCH");
		sHoldRequested = false;
	}
	fflush(stdout);
}

// B1: after the Advance for `frame` completed (tick ran, outbox flushed).
void hold_after_advance(int frame)
{
	if (sHolding && !sHeldLogged && (uint32_t)frame == sHoldFrame + kHoldLeadFrames - 1) {
		sHeldLogged = true;
		sFrozenStartMs = now_ms();
		printf("[netplay] held at frame=%d\n", frame);
		fflush(stdout);
	}
}

// B1: frozen = no further Advance can happen until RESUME (H+11 is done).
bool hold_frozen() { return sHolding && sAdvances >= (uint64_t)sHoldFrame + kHoldLeadFrames; }

// B1 submit gate: keep submitting while the next local input lands on a
// frame <= H+11; then stop until the RESUME snapshot is in hand.
// M5c lane B: "the next local input" is sNextLand, the frame it lands on
// whatever delay changes came before (the adaptive delay never changes the
// delay while a hold is in progress, and never above 8, so the kHoldLeadFrames
// argument above is unchanged).
bool hold_blocks_submit()
{
	if (!sHolding) return false;
	const uint64_t next = sNextLand;
	if (next < (uint64_t)sHoldFrame + kHoldLeadFrames) return false;
	return !sResumeHave;
}

// B1 RESUME catch-up. A frozen peer submitted through frame H+11 and then
// advanced to current frame H+12, so sSubmitted + delay == sAdvances: its
// next ordinary submit would land on H+12+delay, and GekkoNet's InputBuffer
// drops any non-sequential local input, so frame H+12 could never get this
// peer's input (a deadlock). The catch-up submit sets the local delay to 0,
// adds the input (it lands on the current frame H+12), then restores the
// delay: InputBuffer::SetDelay, growing from 0, appends `delay` copies of
// that input on H+13..H+12+delay. Those copies are ordinary local inputs
// (sent to the remote like any other), so both peers see the same inputs.
// Afterwards the ordinary gate resumes at sAdvances == H+13, whose submit
// lands on H+13+delay. A peer that got the RESUME snapshot before it
// stopped (sSubmitted == sAdvances) simply keeps using the ordinary gate.
// M5c lane B: "sSubmitted + delay" is sNextLand (the frozen peer's next input
// would land on H+12, the current frame); after the catch-up sNextLand is
// H+13+delay, so the ordinary gate again opens at sAdvances == H+13.
bool hold_resume_catchup_due()
{
	if (!sHolding || !sResumeHave || sCfg.localDelay == 0) return false;
	const uint64_t resumeFrame = (uint64_t)sHoldFrame + kHoldLeadFrames;
	return sAdvances == resumeFrame && sNextLand == resumeFrame;
}

// B1 host: flag exactly one submitted input when the link goes down (or on
// the first input when state.txt was missing at session start). Stream mode
// only; never in launcher sessions (their static state is always live).
void hold_host_maybe_flag(PcNetplayInput& local)
{
	if (!randstate_stream_on() || !sCfg.isHost || sCfg.launcherMode) return;
	if (sHolding || sHoldRequested) return;
	if (pc_randomizer_enabled == nullptr || !pc_randomizer_enabled()) return;
	bool down = sHoldAtFirstInput;
	if (!down && pc_randomizer_link_live != nullptr && !pc_randomizer_link_live()) down = true;
	if (!down) return;
	sHoldAtFirstInput = false;
	local.flags |= pc_netplay_gekko::kFlagsHold;
	sHoldRequested = true;
	sHoldExpectFrame = sNextLand; // M5c lane B: the frame this input lands on
	printf("[netplay] hold requested: host link down; HOLD flag on submit=%llu (frame %llu)\n",
	       (unsigned long long)sSubmitted, (unsigned long long)sHoldExpectFrame);
	fflush(stdout);
}

// B1 host: once frozen and the link is live again, send the RESUME snapshot.
// A full 4-deep bulk queue keeps the prepared payload for the next turn (the
// generation is bumped once).
void hold_host_try_resume()
{
	if (!sCfg.isHost || !sHolding || sResumeHave) return;
	if (!hold_frozen()) return;
	if (!sResumePrepared) {
		if (pc_randomizer_link_live == nullptr || !pc_randomizer_link_live()) return;
		pc_randstate::PcRandState st;
		if (pc_randomizer_resume_snapshot == nullptr || !pc_randomizer_resume_snapshot(&st)) return;
		const uint32_t rf = sHoldFrame + kHoldLeadFrames;
		sResumePayload[0] = (uint8_t)(rf & 0xFF);
		sResumePayload[1] = (uint8_t)((rf >> 8) & 0xFF);
		sResumePayload[2] = (uint8_t)((rf >> 16) & 0xFF);
		sResumePayload[3] = (uint8_t)((rf >> 24) & 0xFF);
		pc_randstate::encode(st, sResumePayload + 4);
		sResumeState = st;
		sResumeFrame = rf;
		sResumePrepared = true;
		// An older snapshot still waiting for the fragment stream is
		// superseded by this one (it would only arrive stale).
		sRandHaveQueued = false;
	}
	if (!sBulk.send(pc_netplay_bulk::kBulkRandFull, sResumePayload, sizeof(sResumePayload))) return;
	sResumePrepared = false;
	sResumeHave = true;
	printf("[netplay] resume: host link live, RESUME gen=%u for frame=%u sent\n", sResumeState.gen,
	       sResumeFrame);
	fflush(stdout);
}

// B1 client: a completed kBulkRandFull message.
void hold_client_on_randfull(const std::vector<uint8_t>& data)
{
	if (sCfg.isHost) return;
	if (data.size() != sizeof(sResumePayload)) {
		printf("[netplay] RESUME snapshot malformed (len=%llu); dropped\n", (unsigned long long)data.size());
		fflush(stdout);
		return;
	}
	const uint32_t rf = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16)
	    | ((uint32_t)data[3] << 24);
	// Fix round 1 (B1-C2 / R11): accept a RESUME only for the hold in
	// progress. The host sends it only once frozen at H+11, which needs this
	// client's inputs through H+11, submitted after its Advance H+3 or later,
	// so a legitimate RESUME always finds sHolding set with this sHoldFrame.
	// Anything else (a late duplicate completion of an earlier RESUME that
	// outlived the channel's 16-id dedupe window) is dropped, so it can
	// never pre-arm the submit gate for the next hold.
	if (!sHolding || rf != sHoldFrame + kHoldLeadFrames || sResumeHave) {
		printf("[netplay] RESUME for frame=%u dropped: not the hold in progress (holding=%d hold frame=%u have=%d)\n",
		       rf, (int)sHolding, sHoldFrame, (int)sResumeHave);
		fflush(stdout);
		return;
	}
	pc_randstate::PcRandState st;
	if (!pc_randstate::decode(data.data() + 4, data.size() - 4, st)) {
		printf("[netplay] RESUME snapshot for frame=%u failed to decode; dropped\n", rf);
		fflush(stdout);
		return;
	}
	sResumeState = st;
	sResumeFrame = rf;
	sResumeHave = true;
	printf("[netplay] resume: RESUME gen=%u for frame=%u received\n", st.gen, rf);
	fflush(stdout);
}

// ---- M4 lane B2: bulk message types, inbox, outbox (issue #885) ----
// One table for the wave: 0x10 kBulkRandFull (lane A / B1 RESUME), 0x11
// kBulkSaveResult (host -> client), 0x12 kBulkCheckpoint (host -> joiner),
// 0x13 kBulkSaveAck (client -> host), 0x14 kBulkMirrorLedger (B1), 0x15
// kBulkTransferDone (joiner -> host), 0x16 kBulkSidecars (host -> joiner).
// Payload layouts: pc_netplay_transfer.h. Delivery is unordered, so B2
// phases are sequenced by message type and content: completed B2 messages
// wait in a small bounded inbox until the transfer phase or the save barrier
// takes the one it is waiting for.
constexpr uint8_t kBulkSaveResult   = pc_netplay_bulk::kBulkSaveResult; // 0x11
constexpr uint8_t kBulkCheckpoint   = pc_netplay_bulk::kBulkCheckpoint; // 0x12
constexpr uint8_t kBulkSaveAck      = 0x13;
constexpr uint8_t kBulkTransferDone = 0x15;
constexpr uint8_t kBulkSidecars     = 0x16;
struct B2Msg {
	uint8_t type = 0;
	std::vector<uint8_t> data;
};
std::vector<B2Msg> sB2Inbox;
constexpr size_t kB2InboxMax = 64;
// Outgoing B2 messages beyond the 4-deep bulk send queue (FIFO).
std::vector<B2Msg> sB2Out;

bool b2_type(uint8_t t)
{
	return t == kBulkSaveResult || t == kBulkCheckpoint || t == kBulkSaveAck || t == kBulkTransferDone
	    || t == kBulkSidecars;
}

// B2 fix round 1 (C4/E2): the test impairment knobs that LossyLink applies to
// GekkoNet (PIKMIN_NETPLAY_TEST_LATENCY_MS / _JITTER_MS / _LOSS_PCT, seeded by
// _SEED) also apply to received bulk (0x03) datagrams, so the save barrier and
// the transfer phase run over the same impaired link. Receive side on each
// peer (like the handshake channel), so each direction is impaired once.
// PIKMIN_NETPLAY_TEST_BULK_DROP_SAVE=<n> additionally drops the first n
// received SAVE_RESULT / SAVE_ACK data fragments, forcing a retransmit inside
// the barrier. Test only: with none of the knobs set nothing is drawn or held.
double read_double_env(const char* name, double fallback); // defined below
uint32_t read_u32_env(const char* name, uint32_t fallback); // defined below
unsigned read_unsigned_env(const char* name, unsigned fallback); // defined below
struct BulkImpair {
	bool init = false;
	bool on = false;
	double latMs = 0, jitMs = 0, lossPct = 0;
	uint32_t rng = 1;
	unsigned dropSave = 0;
	uint64_t received = 0, dropped = 0, delayed = 0, droppedSave = 0;
	struct Held {
		double atMs = 0;
		std::vector<uint8_t> bytes;
	};
	std::vector<Held> held;
};
BulkImpair sBulkImp;

double bulk_impair_draw()
{
	uint32_t x = sBulkImp.rng; // xorshift32
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	sBulkImp.rng = x;
	return (double)(x >> 8) / 16777216.0;
}

void bulk_impair_init()
{
	BulkImpair& b = sBulkImp;
	b.init = true;
	b.latMs = read_double_env("PIKMIN_NETPLAY_TEST_LATENCY_MS", 0.0);
	b.jitMs = read_double_env("PIKMIN_NETPLAY_TEST_JITTER_MS", 0.0);
	b.lossPct = read_double_env("PIKMIN_NETPLAY_TEST_LOSS_PCT", 0.0);
	if (b.latMs < 0) b.latMs = 0;
	if (b.jitMs < 0) b.jitMs = 0;
	if (b.lossPct < 0) b.lossPct = 0;
	if (b.lossPct > 100) b.lossPct = 100;
	b.on = b.latMs > 0 || b.jitMs > 0 || b.lossPct > 0;
	b.rng = (read_u32_env("PIKMIN_NETPLAY_TEST_SEED", 0) ^ 0xB0F1C3A5u) + (sCfg.isHost ? 1u : 2u);
	if (b.rng == 0) b.rng = 1;
	b.dropSave = read_unsigned_env("PIKMIN_NETPLAY_TEST_BULK_DROP_SAVE", 0);
	if (b.on || b.dropSave > 0) {
		printf("[netplay] bulk impairment (test): latency=%.1fms jitter=%.1fms loss=%.1f%% drop_save=%u\n", b.latMs,
		       b.jitMs, b.lossPct, b.dropSave);
		fflush(stdout);
	}
}

// One received bulk payload: delivered now, held for its delay, or dropped.
void bulk_impair_in(std::vector<uint8_t>&& p)
{
	BulkImpair& b = sBulkImp;
	if (!b.init) bulk_impair_init();
	++b.received;
	if (b.dropSave > 0 && !p.empty() && (p[0] == kBulkSaveResult || p[0] == kBulkSaveAck)) {
		--b.dropSave;
		++b.droppedSave;
		printf("[netplay] test: dropped a received bulk %s fragment (%u more to drop)\n",
		       p[0] == kBulkSaveResult ? "SAVE_RESULT" : "SAVE_ACK", b.dropSave);
		fflush(stdout);
		return;
	}
	if (!b.on) {
		sBulk.on_receive(p.data(), p.size(), now_ms());
		return;
	}
	if (b.lossPct > 0 && bulk_impair_draw() * 100.0 < b.lossPct) {
		++b.dropped;
		return;
	}
	double d = b.latMs;
	if (b.jitMs > 0) d += bulk_impair_draw() * b.jitMs;
	if (d <= 0) {
		sBulk.on_receive(p.data(), p.size(), now_ms());
		return;
	}
	++b.delayed;
	BulkImpair::Held h;
	h.atMs = now_ms() + d;
	h.bytes = std::move(p);
	b.held.push_back(std::move(h));
	if (b.held.size() > 512) b.held.erase(b.held.begin(), b.held.begin() + (long)(b.held.size() - 512));
}

// Delivers the held payloads that are due, in hold order.
void bulk_impair_release()
{
	BulkImpair& b = sBulkImp;
	if (b.held.empty()) return;
	const double now = now_ms();
	for (size_t i = 0; i < b.held.size();) {
		if (b.held[i].atMs <= now) {
			sBulk.on_receive(b.held[i].bytes.data(), b.held[i].bytes.size(), now);
			b.held.erase(b.held.begin() + (long)i);
		} else {
			++i;
		}
	}
}

// Per-turn bulk pump (both peers): drain channel 0x03 into the endpoint,
// send due frags/acks. Message delivery order is NOT guaranteed: each
// message completes independently when all its fragments arrive (see
// pc_netplay_udp.h); B2 sequences SaveResult/Checkpoint itself (above).
// M4 lane B2: serves whichever transport is up (UDP sLink/sSock or ICE
// sIceLink/sIce, the start_gekko_session pattern), and runs before GekkoNet
// exists (the transfer phase) as well as inside a save tick (the barrier).
void bulk_pump()
{
	if (!sCfg.active) return;
	if (sIceLink == nullptr && (sLink == nullptr || sSock == nullptr)) return;
	if (sIceLink != nullptr) {
		// ICE is connected 1:1 (the sender fields are the fixed placeholder),
		// so every bulk datagram is the session peer's.
		for (pc_netplay_ice::IceSocket::Datagram& g : sIceLink->drain_bulk())
			if (!g.payload.empty()) bulk_impair_in(std::move(g.payload));
	} else {
		// m2 fix: accept bulk only from the session peer, so an off-path
		// sender cannot inject or ACK bulk messages. The host learns the
		// joiner's endpoint from its first hello; the joiner dials the host.
		const uint32_t peerIp = sCfg.isHost ? sRemoteIp : sCfg.joinIp;
		const uint16_t peerPort = sCfg.isHost ? sRemotePort : sCfg.joinPort;
		const bool peerKnown = sCfg.isHost ? sHaveRemote : (peerPort != 0);
		std::vector<pc_netplay_transport::UdpSocket::Datagram> grams = sLink->drain_bulk();
		for (auto& g : grams) {
			if (peerKnown && (g.fromIpHostOrder != peerIp || g.fromPort != peerPort)) continue;
			if (!g.payload.empty()) bulk_impair_in(std::move(g.payload));
		}
	}
	bulk_impair_release(); // B2 fix round 1: impaired payloads that are due
	sBulk.sweep(now_ms()); // expire abandoned partial reassemblies
	std::vector<pc_netplay_bulk::BulkChannel::Message> complete = sBulk.poll_complete();
	for (auto& m : complete) {
		printf("[netplay] bulk msg complete: type=0x%02x len=%llu\n", m.type,
		       (unsigned long long)m.data.size());
		fflush(stdout);
		// B1 consumers (client side; the host never receives these types).
		if (m.type == pc_netplay_bulk::kBulkRandFull) hold_client_on_randfull(m.data);
		else if (m.type == pc_netplay_bulk::kBulkMirrorLedger && !sCfg.isHost
		         && pc_randomizer_mirror_ledger_receive != nullptr) {
			pc_randomizer_mirror_ledger_receive(m.data.data(), m.data.size(), sLastAdvanceFrame);
			++sLedgerApplied; // B2 fix round 1 (X6): the barrier waits for the host's count
		} else if (b2_type(m.type)) {
			// B2: kept for the transfer phase / the save barrier.
			if (sB2Inbox.size() >= kB2InboxMax) {
				printf("[netplay] bulk: B2 inbox full; type=0x%02x dropped\n", m.type);
				fflush(stdout);
			} else {
				B2Msg b;
				b.type = m.type;
				b.data = std::move(m.data);
				sB2Inbox.push_back(std::move(b));
			}
		}
	}
	// B2: queued transfer messages go out first, as bulk queue room frees.
	while (!sB2Out.empty() && sBulk.can_send()) {
		const B2Msg& d = sB2Out.front();
		if (!sBulk.send(d.type, d.data.data(), d.data.size())) break;
		sB2Out.erase(sB2Out.begin());
	}
	// B1 host: queued mirror-ledger messages go out as bulk queue room frees.
	while (sCfg.isHost && !sLedgerOut.empty() && sBulk.can_send()) {
		const std::vector<uint8_t>& d = sLedgerOut.front();
		if (!sBulk.send(pc_netplay_bulk::kBulkMirrorLedger, d.data(), d.size())) break;
		sLedgerOut.erase(sLedgerOut.begin());
	}
	std::vector<std::vector<uint8_t>> out = sBulk.poll_outgoing(now_ms());
	for (auto& d : out) {
		if (d.empty()) continue;
		if (sIce != nullptr) sIce->send_payload(pc_netplay_transport::kChannelBulk, d.data(), d.size());
		else sSock->send_payload(pc_netplay_transport::kChannelBulk, d.data(), d.size());
	}
}

// B2: takes the first inbox message of `type`, if any.
bool b2_take(uint8_t type, std::vector<uint8_t>* out)
{
	for (size_t i = 0; i < sB2Inbox.size(); ++i) {
		if (sB2Inbox[i].type != type) continue;
		*out = std::move(sB2Inbox[i].data);
		sB2Inbox.erase(sB2Inbox.begin() + (long)i);
		return true;
	}
	return false;
}

// B2: true when every bulk message this peer queued has been acknowledged.
bool bulk_all_acked()
{
	return sB2Out.empty() && sBulk.send_acked_frags() == sBulk.send_total_frags();
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

// Polish fix round (review B1/M3/m8, fix2 n1): one shared high-resolution
// timer setup. Hidden (TEST_BACKGROUND) processes on Windows 11 do not get
// the timeBeginPeriod(1) granularity they ask for (sleep_for(1ms) sleeps
// ~15.6ms), which quantised both the handshake RTT samples and the 30 Hz
// slot wait. The bulk of every short wait therefore runs on a waitable timer
// created with CREATE_WAITABLE_TIMER_HIGH_RESOLUTION (declared in
// synchapi.h). Slot waits spin only a tail of at most 1 ms; poll waits
// (handshake, stall turns) block on the timer and never spin (fix3 R2-2/R2-3).
// The timer-resolution power-throttling opt-out
// (SetProcessInformation / PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION,
// resolved at runtime because its declaration needs _WIN32_WINNT >= 0x0602
// while this build defaults to 0x0601) is held only while a netplay session
// runs. If the high-resolution flag is unsupported (Windows before 1803) the
// plain waitable timer is used with timeBeginPeriod(1) held until exit
// (R2-4); if no waitable timer can be created at all, the timeBeginPeriod
// path keeps the old single-sample coarse/fine classification.
#ifdef _WIN32
static bool sHiresTimerReady = false;
static HANDLE sHiresTimer = NULL; // waitable timer for the bulk of short waits
static bool sHiresTimerHighRes = false; // created with the high-resolution flag
static bool sHiresPeriodBegun = false; // fallback path owns a timeBeginPeriod
static bool sHiresCoarse = false; // fallback path only: Sleep(1) quantised
static void hires_timer_teardown()
{
	if (sHiresTimer != NULL) {
		CloseHandle(sHiresTimer);
		sHiresTimer = NULL;
	}
	if (sHiresPeriodBegun) {
		sHiresPeriodBegun = false;
		timeEndPeriod(1);
	}
}
static void ensure_hires_timer()
{
	if (sHiresTimerReady) return;
	sHiresTimerReady = true;
	HANDLE timer =
	    CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	if (timer != NULL) {
		sHiresTimer = timer;
		sHiresTimerHighRes = true;
	} else {
		// Fix3 R2-4: plain-timer fallback still raises the timer resolution, so
		// pre-1803 Windows does not quantise slot sleeps to 15.6 ms.
		// Paired with timeEndPeriod in hires_timer_teardown at exit.
		timer = CreateWaitableTimerW(NULL, FALSE, NULL);
		if (timer != NULL) {
			sHiresTimer = timer;
			sHiresTimerHighRes = false;
			timeBeginPeriod(1);
			sHiresPeriodBegun = true;
		}
	}
	if (sHiresTimer != NULL) {
		printf("[netplay] timer path: %swaitable timer%s\n",
		       sHiresTimerHighRes ? "high-resolution " : "plain ",
		       sHiresTimerHighRes ? "" : " + timeBeginPeriod(1)");
	} else {
		timeBeginPeriod(1);
		sHiresPeriodBegun = true;
		const double t0 = now_ms();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		sHiresCoarse = (now_ms() - t0) > 5.0;
		printf("[netplay] timer path: timeBeginPeriod fallback (waitable timer unavailable; %s)\n",
		       sHiresCoarse ? "coarse: spin waits" : "fine: sleep+spin tail");
	}
	fflush(stdout);
	std::atexit(hires_timer_teardown);
}
#else
static inline void ensure_hires_timer() {}
#endif

// Waits ms. Everything except the last spinTailMs blocks; the tail spins so a
// slot deadline lands precisely (fix3 R2-3: at most 1 ms, and the caller adds
// no spin of its own). Poll waits (handshake, stall turns) pass
// spinTailMs = 0 and block for the whole wait, never spinning (R2-2): the
// high-resolution timer's ~0.5 ms precision is ample for a 1 ms poll.
static void sleep_hires_ms(double ms, double spinTailMs = 1.0)
{
	if (ms <= 0) return;
	if (spinTailMs < 0) spinTailMs = 0;
	if (spinTailMs > 1.0) spinTailMs = 1.0;
	ensure_hires_timer();
	const double start = now_ms();
	const double blockMs = ms - spinTailMs;
#ifdef _WIN32
	if (sHiresTimer != NULL) {
		// Waitable-timer path (high-resolution, or plain + timeBeginPeriod).
		if (blockMs > 0.05) {
			LARGE_INTEGER due;
			due.QuadPart = -(LONGLONG)(blockMs * 10000.0); // ms to 100 ns units
			if (SetWaitableTimer(sHiresTimer, &due, 0, NULL, NULL, FALSE)) {
				WaitForSingleObject(sHiresTimer, INFINITE);
			} else {
				std::this_thread::sleep_for(
				    std::chrono::duration<double, std::milli>(blockMs));
			}
		}
	} else if (!sHiresCoarse || spinTailMs == 0) {
		// timeBeginPeriod fallback (waitable timer creation failed). On a
		// coarse timer a precise slot wait still spins (Sleep would round
		// up to 15.6 ms), but a poll wait always blocks: a late poll only
		// costs latency, a spinning one costs a core.
		if (blockMs > 0.05)
			std::this_thread::sleep_for(
			    std::chrono::duration<double, std::milli>(blockMs));
	}
#else
	if (blockMs > 0.05)
		std::this_thread::sleep_for(
		    std::chrono::duration<double, std::milli>(blockMs));
#endif
	if (spinTailMs == 0) return;
	const double deadline = start + ms;
	while (now_ms() < deadline - 0.3) std::this_thread::yield();
	while (now_ms() < deadline) {
	}
}

// Fix3 R2-1: timer-resolution power-throttling opt-out, held only while a
// netplay session runs. "Always honour timer resolution requests" is
// ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION with
// StateMask = 0; handing control back to the system is ControlMask = 0 with
// StateMask = 0. (Setting StateMask to the flag instead means "always
// ignore" timer resolution requests, which is the opposite of this opt-out.)
// The constants and PROCESS_POWER_THROTTLING_STATE are declared
// unconditionally, but the SetProcessInformation function
// declaration needs _WIN32_WINNT >= 0x0602 while this build defaults to
// 0x0601, so resolve it at runtime via GetProcAddress: on Windows 8+ it is
// in kernel32, elsewhere the session simply runs without the opt-out.
static void netplay_timer_power_opt(bool on)
{
#ifdef _WIN32
	static bool resolved = false;
	typedef BOOL(WINAPI* SetProcessInformationFn)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
	static SetProcessInformationFn fn = nullptr;
	static bool active = false;
	static bool unavailableLogged = false;
	if (!resolved) {
		resolved = true;
		HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
		if (kernel != nullptr)
			fn = (SetProcessInformationFn)GetProcAddress(kernel, "SetProcessInformation");
	}
	if (fn == nullptr) {
		if (!unavailableLogged) {
			unavailableLogged = true;
			printf("[netplay] power-throttling opt-out: unavailable "
			       "(SetProcessInformation not found)\n");
			fflush(stdout);
		}
		return;
	}
	if (on == active) return;
	PROCESS_POWER_THROTTLING_STATE state;
	state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
	if (on) {
		// Always honour timer resolution requests while the session
		// runs (Control = flag, State = 0).
		state.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
		state.StateMask = 0;
	} else {
		// Hand timer-throttling control back to the system.
		state.ControlMask = 0;
		state.StateMask = 0;
	}
	if (fn(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state))) {
		active = on;
		printf("[netplay] power-throttling opt-out: %s\n",
		       on ? "timer resolution requests always honoured while session runs"
		          : "released (timer-resolution throttling back under system control)");
	} else {
		printf("[netplay] power-throttling opt-out: failed (%lu)\n",
		       (unsigned long)GetLastError());
	}
	fflush(stdout);
#else
	(void)on;
#endif
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
//   netplaySeed, netplayDelay, protocolVersion.
//   randStream (M4 external-state stream gate: stream vs legacy polling).
//   testDeathlinkAsOrdinary (M4 B1 test knob).
//   coopEvents (gapfix C: FNV-1a 64 of the PIKMIN_NETPLAY_TEST_COOP_EVENTS
//   file, 0 when the knob is unset or not honoured; 16 hex digits).
// Deliberately EXCLUDED (local-only, documented in the handoff): windowWidth
// and windowHeight (launch lane: presentation-only, they stay local, so a
// joiner with a different window size still joins; was: pre-M2b both peers
// had to render the same view), display/render settings (shadows, gamma,
// resolution scale, vsync), audio settings, key/gamepad bindings, gyro
// calibration (sensitivity/invert/bias), touch settings, photo-mode state
// (no sim getter; local overlay), VS rules/mode (co-op sessions only),
// language.
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
	// Launch lane: windowWidth/windowHeight are presentation-only and stay
	// local (see the EXCLUDED list above); they are never hashed.
	addi("netplaySeed", (long long)sCfg.seed);
	// Fix round 3 (asymmetric delay): GekkoNet local delay is per actor, so
	// peers may legally use different delays (e.g. 2 and 4). The delay is
	// therefore NOT part of the handshake hash; only the seed (shared) and
	// the protocol version are. (Previously the numeric delay was hashed,
	// which refused asymmetric pairs on config.)
	addi("protocolVersion", (long long)kProtocolVersion);
	// M4 fix round 1 (m1): the external-state stream gate is per-peer config
	// that changes the sim (stream vs legacy file polling). A pair with the
	// gate set differently on each side would desync silently; hashing it
	// makes the handshake refuse instead.
	addi("randStream", sRandStream ? 1 : 0);
	// M4 lane B1: PIKMIN_NETPLAY_TEST_DEATHLINK_AS_ORDINARY changes sim state
	// (induced DeathLink kills count as ordinary deaths; deathsReported is
	// hashed), so a pair with the knob set on one side refuses on config.
	addi("testDeathlinkAsOrdinary",
	     (pc_randomizer_test_deathlink_as_ordinary != nullptr
	      && pc_randomizer_test_deathlink_as_ordinary()) ? 1 : 0);
	// Gapfix C (#885): PIKMIN_NETPLAY_TEST_COOP_EVENTS scripts captain HP and
	// downs into the co-op policy (sim state). The FNV-1a of the file's bytes
	// (0 without the knob) is hashed next to randStream, so a pair whose
	// peers load different event files, or only one of them, refuses on
	// config instead of desyncing. The perturb knob
	// (PIKMIN_NETPLAY_TEST_COOP_PERTURB) is deliberately NOT hashed: it
	// exists to prove that a one-sided co-op state change is caught as a
	// desync by the state hash.
	snprintf(fbuf, sizeof(fbuf), "coopEvents=%016llx;", (unsigned long long)pc_coop_events_config_hash());
	s += fbuf;
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

// ---- launch lane (issue #887): one command per role, no env vars ----
//
// Everything the session needs from the launcher is resolved before engine
// init by pc_netplay_launch_preinit (pc_netplay_launch.cpp): the private run
// dir, the run bootstrap (fed to the game as --randomizer-seed), the save
// dir, the joiner's adopted settings and seed, the input device. Only the
// ICE code exchange is left for here.

void stop_session(); // defined below with the other session lifecycle hooks

// Host bundle bootstrap: the run bootstrap bytes, bounded by the offer's u16
// length field (preinit already refused anything longer).
std::string read_bootstrap_bytes()
{
	std::string bytes;
	FILE* f = fopen(sCfg.bootstrapPath.c_str(), "rb");
	if (f == nullptr) return bytes;
	char chunk[8192];
	while (bytes.size() <= pc_netplay_ice::kMaxBundleBootBytes) {
		const size_t n = fread(chunk, 1, sizeof(chunk), f);
		if (n > 0) bytes.append(chunk, n);
		if (n < sizeof(chunk)) break;
	}
	fclose(f);
	return bytes;
}

// Prints the code (the one-line format the pair tools grep for), copies it
// to the clipboard (not in --netplay-test-hidden), writes it atomically to
// --netplay-code-out / PIKMIN_NETPLAY_ICE_CODE_OUT, and keeps a copy of this
// peer's own code in its run dir.
void launcher_emit_code(const char* kind, const std::string& code)
{
	const PcNetplayLaunch& L = pc_netplay_launch_setup();
	printf("[netplay] ice %s code:\n%s\n", kind, code.c_str());
	fflush(stdout);
	if (!L.testHidden) {
		if (SDL_SetClipboardText(code.c_str()) == 0) {
			printf("[netplay] launch: %s code copied to the clipboard (%llu chars)\n", kind,
			       (unsigned long long)code.size());
		} else {
			printf("[netplay] launch: clipboard copy failed (%s); copy the code above\n", SDL_GetError());
		}
		fflush(stdout);
	}
	const char* out = nullptr;
	if (!L.codeOut.empty()) out = L.codeOut.c_str();
	else out = getenv_nonempty("PIKMIN_NETPLAY_ICE_CODE_OUT");
	std::string err;
	if (out != nullptr) {
		if (!pc_netplay_ice::ice_write_code_file(out, code, &err)) {
			printf("[netplay] cannot write code file %s: %s\n", out, err.c_str());
			fflush(stdout);
			stop_session();
			std::exit(1);
		}
		printf("[netplay] launch: %s code written to %s\n", kind, out);
		fflush(stdout);
	}
	if (!L.runDir.empty()) pc_netplay_ice::ice_write_code_file(L.runDir + "/" + kind + ".txt", code, &err);
}

// Answer wait: --netplay-answer-in / PIKMIN_NETPLAY_ICE_ANSWER_IN file poll,
// else stdin (paste + Enter, re-prompting after a bad paste). Stdin is the
// most robust human option: it works in any console and headless, and needs
// no key handling in the (possibly hidden) game window.
bool launcher_wait_answer(const std::function<void()>& pump, std::string* answerOut,
                          std::string* err)
{
	const PcNetplayLaunch& L = pc_netplay_launch_setup();
	const char* in           = nullptr;
	if (!L.answerIn.empty()) in = L.answerIn.c_str();
	else in = getenv_nonempty("PIKMIN_NETPLAY_ICE_ANSWER_IN");
	if (in != nullptr) {
		printf("[netplay] launch: waiting for the answer code in %s\n", in);
		fflush(stdout);
		return pc_netplay_ice::ice_poll_answer_file(in, pc_netplay_ice::ice_connect_timeout_ms(), pump,
		                                            answerOut, err);
	}
	return pc_netplay_ice::ice_read_answer_stdin(pump, answerOut, err);
}

// Input ownership: maps --netplay-input onto the pc_window device
// assignment (sPlayerDevice / sKeyboardOwner / resolvePlayerPads) and the
// ownership filter. The accumulator then samples the session role's slot.
void launcher_apply_input()
{
	using namespace pc_netplay_input_sel;
	if (sCfg.inputKind == (int)kInputAuto) {
		if (!sCfg.inputSpec.empty()) {
			printf("[netplay] input: auto (today's behaviour)\n");
			fflush(stdout);
		}
		return;
	}
	const Kind kind = (Kind)sCfg.inputKind;
	bool ignoreKb = false, ignorePad = false;
	filter_for_selection(kind, &ignoreKb, &ignorePad);
	pc_window_set_netplay_input_filter(ignoreKb, ignorePad);
	if (kind == kInputKeyboard) {
		pc_window_input_assign(sLocalRole, PC_INPUT_DEV_KEYBOARD, -1);
		printf("[netplay] input: keyboard and mouse feed %s (every gamepad ignored; input only "
		       "while this window has focus)\n",
		       sLocalRole == 0 ? "host/P1" : "joiner/P2");
	} else {
		pc_window_set_netplay_gamepad(sLocalRole, sCfg.inputGamepad);
		printf("[netplay] input: gamepad #%d feeds %s (every key ignored; the pad keeps working "
		       "while another window has focus)\n",
		       sCfg.inputGamepad, sLocalRole == 0 ? "host/P1" : "joiner/P2");
		if (pc_window_num_gamepads() <= sCfg.inputGamepad)
			printf("[netplay] input: gamepad #%d is not connected yet; %s stays neutral until it "
			       "is plugged in\n",
			       sCfg.inputGamepad, sLocalRole == 0 ? "host/P1" : "joiner/P2");
	}
	fflush(stdout);
}

// One-command host flow: v2 bundle offer (print + clipboard + file), then
// the answer (file or stdin), then connect.
bool launcher_host_flow(const pc_netplay_ice::IceNetConfig& nic, const std::function<void()>& pump,
                        std::string* err)
{
	sIce = new pc_netplay_ice::IceSocket();
	pc_netplay_ice::SessionBundle bundle;
	bundle.seed           = sCfg.seed;
	bundle.configText     = build_config_string();
	bundle.bootstrapBytes = read_bootstrap_bytes();
	std::string offer;
	if (!sIce->host_create_offer_v2(nic, bundle, &offer, err, pump)) return false;
	printf("[netplay] ice offer bundle: seed=%u config=%lluB bootstrap=%lluB\n", bundle.seed,
	       (unsigned long long)bundle.configText.size(), (unsigned long long)bundle.bootstrapBytes.size());
	fflush(stdout);
	launcher_emit_code("offer", offer);
	std::string answer;
	if (!launcher_wait_answer(pump, &answer, err)) return false;
	if (!sIce->host_apply_answer(answer, err)) return false;
	double completedMs = -1;
	if (!sIce->wait_connected(pc_netplay_ice::ice_connect_timeout_ms(), &completedMs, err, pump))
		return false;
	printf("[netplay] ice transport ready (connect %.0fms)\n", completedMs);
	fflush(stdout);
	return true;
}

// One-command joiner flow: the offer text preinit read (once) and adopted,
// answer print + clipboard + file, then connect.
bool launcher_joiner_flow(const pc_netplay_ice::IceNetConfig& nic, const std::function<void()>& pump,
                          std::string* err)
{
	sIce = new pc_netplay_ice::IceSocket();
	std::string answer;
	if (!sIce->join_create_answer(nic, pc_netplay_launch_setup().offerCode, &answer, err, pump, nullptr))
		return false;
	launcher_emit_code("answer", answer);
	double completedMs = -1;
	if (!sIce->wait_connected(pc_netplay_ice::ice_connect_timeout_ms(), &completedMs, err, pump))
		return false;
	printf("[netplay] ice transport ready (connect %.0fms)\n", completedMs);
	fflush(stdout);
	return true;
}

// m13: the low-level ICE switches take each side's own settings, bootstrap
// and seed, so a mismatch there refuses; the likeliest cause is typing
// --netplay-ice-host/--netplay-ice-join for the launcher's
// --netplay-host-ice/--netplay-join-ice.
void b2_refusal_view(uint8_t field); // B2 fix round 1 (X7/E5), defined below

void print_refusal_hint(uint8_t field)
{
	if (!sCfg.iceMode || sCfg.launcherMode) return;
	if (field != kFieldConfig && field != kFieldBootstrap && field != kFieldSeed) return;
	printf("[netplay] hint: --netplay-ice-host/--netplay-ice-join are the low-level switches: each "
	       "side uses its own settings, bootstrap and seed. For one-command play use "
	       "--netplay-host-ice / --netplay-join-ice (the offer carries the session setup).\n");
	fflush(stdout);
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

bool script_via_accum()
{
	// Fix round 3 item 5: PIKMIN_NETPLAY_TEST_SCRIPT_VIA_ACCUM=1 feeds
	// scripted records through the same PcNetplayAccum path live pads use.
	static bool init = false;
	static bool on  = false;
	if (!init) {
		init = true;
		if (const char* e = std::getenv("PIKMIN_NETPLAY_TEST_SCRIPT_VIA_ACCUM"))
			on = (e[0] == '1' && e[1] == '\0');
	}
	return on;
}

PcNetplayInput build_local_input()
{
	// Scripted input for tests (brief item 8): pad-0 records (+ yaw) from
	// the file, one per local input submission, instead of the pad.
	if (sScriptActive) {
		if (script_via_accum()) {
			// Item 5: route the record through the live-pad accumulator
			// (add then take) so the driver wiring is exercised at
			// runtime. The every-turn physical fold in accum_add_current
			// still runs (neutral in hidden runs), and the scripted
			// record overwrites sticks/yaw (latest-wins) and ORs buttons
			// onto neutral, so the submitted input is exactly the scripted
			// record: deterministic 1:1, identical hashes to direct mode.
			PcNetplayInput rec = scripted_record(sScriptIdx++);
			if (local_ui_open()) {
				sPadAccum.reset();
				return pc_netplay_input_neutral();
			}
			sPadAccum.add_input(rec);
			PcNetplayInput out = sPadAccum.take();
			if (sScriptIdx == 1) {
				printf("[netplay] script via accum: records feed PcNetplayAccum\n");
				fflush(stdout);
			}
			return out;
		}
		PcNetplayInput in = scripted_record(sScriptIdx++);
		if (local_ui_open()) in = pc_netplay_input_neutral();
		return in;
	}
	// Physical pad: the merged accumulator (buttons OR'd across every turn
	// since the last submit, sticks/yaw latest). The driver folds the
	// current sample in every turn via accum_add_current(), so a tap that
	// starts and ends between two submit turns is never lost.
	if (local_ui_open()) {
		// While the F1 menu is open the local input is neutral, and the
		// latch is cleared so buttons held before opening do not leak
		// into the sim after it closes.
		sPadAccum.reset();
		return pc_netplay_input_neutral();
	}
	return sPadAccum.take();
}

// B2 residual (fix round 2): fold the current physical pad sample into the
// accumulator. Called on EVERY kSession turn (submit or stall), so button
// presses shorter than the submit interval still reach the next submit.
// The fresh yaw capture keeps the submitted yaw following the live camera
// (B1); the synced values still win in the sim because inject_input()
// rewrites the slots before every app->idle().
// Diagnostic (PIKMIN_NETPLAY_INPUT_TRACE=1): last inputs injected per pad.
PcNetplayInput sTraceInjected[2] = {};

void accum_add_current()
{
	// Fix round 3 item 5: in script-via-accum mode the every-turn physical
	// fold still runs (neutral in hidden runs), so the driver wiring has
	// runtime coverage; the scripted record is merged on top at submit time
	// (see build_local_input) and wins (latest/OR-onto-neutral).
	if (sScriptActive && !script_via_accum()) return; // direct script path
	// B1: fresh capture so the local yaw follows the live camera instead
	// of freezing at the first injected value.
	pc_input_log_capture_yaw_fresh();
	PADStatus* pads = pc_netplay_pad_status();
	// Launch lane (issue #887): input ownership. auto keeps today's
	// behaviour (slot 0 on both peers); an explicit --netplay-input samples
	// the session role's slot, which the device assignment routes the
	// selected device to. Either way the local player is fed through the
	// PcNetplayAccum path below.
	const int padIdx =
	    pc_netplay_input_sel::local_pad_index(sLocalRole,
	                                          (pc_netplay_input_sel::Kind)sCfg.inputKind);
	PADStatus s = pads[padIdx]; // post-PADRead sample (F1 consume applied)
	{
		// Diagnostic (PIKMIN_NETPLAY_INPUT_TRACE=1): once a second, or when the
		// sampled slot changes, log where a local pad value got to: device
		// routing and raw SDL state, both pad slots after PADRead, the slot
		// sampled for the local input, and the last injected inputs. Log-only.
		static int sTrace = -1;
		static unsigned sTraceCalls = 0;
		static PADStatus sTraceLast = {};
		if (sTrace < 0) {
			const char* e = std::getenv("PIKMIN_NETPLAY_INPUT_TRACE");
			sTrace = (e != nullptr && e[0] == '1') ? 1 : 0;
		}
		if (sTrace == 1) {
			const bool changed = s.button != sTraceLast.button || s.stickX != sTraceLast.stickX
			                     || s.stickY != sTraceLast.stickY;
			if (changed || (sTraceCalls % 30) == 0) {
				char dev[512];
				pc_window_netplay_input_trace(dev, (int)sizeof(dev));
				printf("[netplay] input-trace call=%u adv=%llu role=%d kind=%d slot=%d "
				       "pad0=%04x/%d,%d pad1=%04x/%d,%d sampled=%04x/%d,%d inj0=%04x/%d,%d inj1=%04x/%d,%d %s\n",
				       sTraceCalls, (unsigned long long)sAdvances, sLocalRole, (int)sCfg.inputKind, padIdx,
				       pads[0].button, pads[0].stickX, pads[0].stickY, pads[1].button, pads[1].stickX,
				       pads[1].stickY, s.button, s.stickX, s.stickY, sTraceInjected[0].buttons,
				       sTraceInjected[0].stickX, sTraceInjected[0].stickY, sTraceInjected[1].buttons,
				       sTraceInjected[1].stickX, sTraceInjected[1].stickY, dev);
				fflush(stdout);
			}
			sTraceLast = s;
			++sTraceCalls;
		}
	}
	uint16_t yaw = 0;
	if (pc_input_log_yaw_valid(sLocalRole)) yaw = pc_input_log_yaw_raw(sLocalRole);
	sPadAccum.add(s.button, s.stickX, s.stickY, s.substickX, s.substickY,
	              s.triggerLeft, s.triggerRight, yaw);
}

void inject_input(int pad, const PcNetplayInput& in)
{
	if (pad == 0 || pad == 1) sTraceInjected[pad] = in;
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

// ---- M5c lane B: session stats and adaptive input delay (issue #887) ----
//
// Policy and arithmetic: pc_netplay_adaptive.h. Wall-clock only: nothing
// here reads or writes sim state, and the only GekkoNet calls are the local
// delay setters and gekko_network_stats. Each peer reports its own counted
// stalls to the other every 250 ms (kHsAdvice on the handshake channel,
// unreliable, cumulative) and adapts its own delay to the stalls the other
// reports (the remote waits when this peer's inputs arrive late), less this
// peer's own slow ticks. A delay change moves which frame this peer's next
// inputs land on; every frame still gets exactly one local input, built in
// frame order (one scripted record per frame), and both peers advance a
// frame only with its confirmed inputs, so both peers run identical inputs
// and a scripted pair replays a fixed-delay run's per-frame inputs exactly
// (record F - d0 on frame F).
//
//   PIKMIN_NETPLAY_ADAPTIVE_DELAY  unset/1: on in real-time sessions (off in
//                                  unthrottled test runs, where every turn
//                                  that finds no input is a "stall");
//                                  0: off (the delay stays where it started);
//                                  force: on even when unthrottled (tests)
//   PIKMIN_NETPLAY_TEST_DELAY_SCHEDULE=<frame>:<delay>[,...]  test only:
//                                  forced changes at those frames (the
//                                  controller is off; works unthrottled)
//   PIKMIN_NETPLAY_STALL_TRACE=1   diagnostic: one line per stall event and
//                                  per tick of 50 ms or more
//
// Freeze rules (no change at all, evidence still gathered): a B1 HOLD in
// progress or requested (the hold arithmetic keys on the delay), a lane S load
// window (its stalls are load time), a shrink still in progress, and the
// first kAdaptiveWarmupFrames (the two schedules settle after the first
// load). The delay stays in 1..8: B1's kHoldLeadFrames (12) and lane S's
// load-window close frame both assume a delay of at most 8.
constexpr uint64_t kAdaptiveWarmupFrames = 150;
constexpr double kRttSampleMs = 500.0; // GekkoNet's NetworkHealth period
constexpr double kAdviceMs = 250.0;    // advice period (pc_netplay_adaptive::Policy::reportPeriodMs)
constexpr uint8_t kHsAdvice = 0x40;    // handshake-channel type: stable header + Advice payload
pc_netplay_adaptive::SessionStats sStats;
pc_netplay_adaptive::DelayController sDelayCtl;
bool sAdaptiveConfigured = false;
bool sAdaptive = false;         // the controller may change the delay
const char* sAdaptiveWhy = "";  // why it is on/off (log)
std::vector<pc_netplay_adaptive::ScheduleStep> sDelaySched;
size_t sDelaySchedIdx = 0;
unsigned sDelayStart = 0;       // d0, the session's first delay
unsigned sDelayLow = 0;
unsigned sDelayHigh = 0;
uint64_t sDelayUps = 0;
uint64_t sDelayDowns = 0;
std::string sDelayTimeline;     // "+<s>s@<frame>:<delay> ..."
std::string sPendingWhy;        // reason of a growth submitted this turn
bool sStallOpen = false;
double sStallStartMs = 0;
double sStallDurMs = 0;
bool sStallExcluded = false;
double sLastFrameEndMs = 0;     // end of the last Advance (its frame was presented)
double sRttNextMs = 0;
bool sStatsFinalDone = false;
// Counted local stalls (what this peer reports) and the advice exchange.
double sLateMs = 0;
uint32_t sLateEvents = 0;
uint32_t sAdviceSeq = 0;
double sAdviceNextMs = 0;
pc_netplay_adaptive::AdviceReceiver sAdviceIn;
double sPeerLateMs = 0; // lateness the peer reported (this peer's inputs late there)
double sSelfOverrunMs = 0; // own input lateness vs the 30 Hz schedule, outside load windows
double sNextDueMs = 0;     // when the next session turn is due to start (pacing schedule)
bool sHaveDue = false;
bool sPrevTurnStalled = true;
double sOwnLagPrev = 0;
float sStallAhead = 0;     // gekko_frames_ahead() when the open stall began (trace)
int sStallTrace = -1;      // PIKMIN_NETPLAY_STALL_TRACE=1: one line per stall event / slow tick

bool stall_trace()
{
	if (sStallTrace < 0) {
		const char* e = std::getenv("PIKMIN_NETPLAY_STALL_TRACE");
		sStallTrace = (e != nullptr && e[0] == '1' && e[1] == '\0') ? 1 : 0;
	}
	return sStallTrace == 1;
}

double session_s(double nowMs) { return sSessionStartMs > 0 ? (nowMs - sSessionStartMs) / 1000.0 : 0.0; }

void adaptive_timeline_add(double nowMs, uint64_t frame, unsigned delay)
{
	char cell[64];
	snprintf(cell, sizeof(cell), "%s+%.1fs@%llu:%u", sDelayTimeline.empty() ? "" : " ", session_s(nowMs),
	         (unsigned long long)frame, delay);
	sDelayTimeline += cell;
	if (delay < sDelayLow) sDelayLow = delay;
	if (delay > sDelayHigh) sDelayHigh = delay;
}

// Session start (after the delay, including DELAY=auto, is final).
void adaptive_configure()
{
	sAdaptiveConfigured = true;
	sStats.reset();
	sDelayStart = sCfg.localDelay;
	sDelayLow = sDelayHigh = sCfg.localDelay;
	sDelayUps = sDelayDowns = 0;
	sDelayTimeline.clear();
	sStallOpen = false;
	sLastFrameEndMs = 0;
	sRttNextMs = 0;
	sStatsFinalDone = false;
	sLateMs = 0;
	sLateEvents = 0;
	sAdviceSeq = 0;
	sAdviceNextMs = 0;
	sAdviceIn.reset();
	sPeerLateMs = 0;
	sSelfOverrunMs = 0;
	sNextDueMs = 0;
	sHaveDue = false;
	sPrevTurnStalled = true;
	sOwnLagPrev = 0;
	sNextLand = sCfg.localDelay; // the first add fills frames 0..d-1 with GekkoNet's empty input
	pc_netplay_adaptive::Policy pol;
	sDelayCtl.configure(pol);
	sDelayCtl.start(now_ms());
	sDelaySched.clear();
	sDelaySchedIdx = 0;
	std::string err;
	const char* sched = getenv_nonempty("PIKMIN_NETPLAY_TEST_DELAY_SCHEDULE");
	if (sched != nullptr && !pc_netplay_adaptive::parse_schedule(sched, pol.minDelay, pol.maxDelay, &sDelaySched, &err)) {
		printf("[netplay] adaptive delay: test schedule ignored (%s)\n", err.c_str());
		sDelaySched.clear();
	}
	const char* mode = getenv_nonempty("PIKMIN_NETPLAY_ADAPTIVE_DELAY");
	const bool off = mode != nullptr && (std::strcmp(mode, "0") == 0 || std::strcmp(mode, "off") == 0);
	const bool force = mode != nullptr && std::strcmp(mode, "force") == 0;
	if (!sDelaySched.empty()) {
		sAdaptive = false;
		sAdaptiveWhy = "off (test schedule drives the delay)";
	} else if (off) {
		sAdaptive = false;
		sAdaptiveWhy = "off (PIKMIN_NETPLAY_ADAPTIVE_DELAY=0)";
	} else if (pc_netplay_unthrottled() && !force) {
		sAdaptive = false;
		sAdaptiveWhy = "off (unthrottled test run)";
	} else {
		sAdaptive = true;
		sAdaptiveWhy = force ? "on (forced)" : "on";
	}
	printf("[netplay] adaptive delay: %s start=%u range=%u..%u up=%.0fms stall in %.1fs "
	       "down=%.0fs clean+rtt schedule=%llu steps\n",
	       sAdaptiveWhy, sCfg.localDelay, pol.minDelay, pol.maxDelay, pol.upStallMs, pol.upWindowMs / 1000.0,
	       pol.downHoldMs / 1000.0, (unsigned long long)sDelaySched.size());
	fflush(stdout);
}

void adaptive_log_change(unsigned from, unsigned to, const std::string& why)
{
	const double now = now_ms();
	if (to > from) ++sDelayUps;
	else ++sDelayDowns;
	adaptive_timeline_add(now, sAdvances, to);
	printf("[netplay] delay change: %u -> %u at frame=%llu next-land=%llu t=%.1fs (%s)\n", from, to,
	       (unsigned long long)sAdvances, (unsigned long long)sNextLand, session_s(now), why.c_str());
	fflush(stdout);
}

// Why no change may start now (nullptr: free to change).
const char* adaptive_frozen()
{
	if (sHolding || sHoldRequested) return "hold";
	if (sLgWindow.is_open()) return "load window";
	if (!pc_netplay_adaptive::submit_due(sNextLand, sAdvances, sCfg.localDelay)) return "transition";
	if (sAdvances < kAdaptiveWarmupFrames) return "warm-up";
	return nullptr;
}

// Target delay for this submit turn (== the current delay: nothing to do).
// Called only on a turn whose submit is due.
unsigned adaptive_target()
{
	const unsigned cur = sCfg.localDelay;
	sPendingWhy.clear();
	if (!sDelaySched.empty()) {
		if (sDelaySchedIdx >= sDelaySched.size()) return cur;
		const pc_netplay_adaptive::ScheduleStep& st = sDelaySched[sDelaySchedIdx];
		if (sAdvances < st.frame) return cur;
		// A scheduled step waits out a hold / load window / shrink.
		if (sHolding || sHoldRequested || sLgWindow.is_open()) return cur;
		++sDelaySchedIdx;
		char buf[64];
		snprintf(buf, sizeof(buf), "test schedule step %llu for frame %llu", (unsigned long long)sDelaySchedIdx,
		         (unsigned long long)st.frame);
		sPendingWhy = buf;
		return st.delay;
	}
	if (!sAdaptive) return cur;
	const char* frozen = adaptive_frozen();
	pc_netplay_adaptive::Decision d = sDelayCtl.decide(now_ms(), cur, frozen != nullptr);
	if (!d.changed) {
		if (!d.reason.empty()) {
			printf("[netplay] delay hold at %u: %s\n", cur, d.reason.c_str());
			fflush(stdout);
		}
		return cur;
	}
	sPendingWhy = d.reason;
	return d.target;
}

// A shrink: GekkoNet touches no input; the next (cur - target) turns submit
// nothing, until sNextLand == sAdvances + target.
void adaptive_shrink(unsigned target)
{
	const unsigned from = sCfg.localDelay;
	gekko_set_local_delay_nofill(sGekko, sLocalHandle, (unsigned char)target);
	sCfg.localDelay = target;
	adaptive_log_change(from, target, sPendingWhy);
}

// The submit of this turn: n == 1 normally; n == 1 + k grows the delay by k
// (see pc_netplay_adaptive.h, SubmitGate). Each input lands on sNextLand.
// Scripted inputs consume one record per frame; a live pad repeats this
// turn's sample on the k extra frames (a held state, never a release plus a
// second press).
void submit_local_inputs(unsigned n)
{
	const unsigned from = sCfg.localDelay;
	PcNetplayInput firstPad;
	for (unsigned j = 0; j < n; ++j) {
		if (j > 0) gekko_set_local_delay_nofill(sGekko, sLocalHandle, (unsigned char)(from + j));
		PcNetplayInput local = (j == 0 || sScriptActive) ? build_local_input() : firstPad;
		if (j == 0) firstPad = local;
		const uint64_t land = sNextLand;
		// M4a: the host embeds the next snapshot fragment here (the
		// joiner never sets chunk bits). Input-build ownership stays
		// in this function; pacing/handshake below are untouched.
		randstate_embed_on_submit(local);
		// B1: the host flags exactly one input when its link is down.
		hold_host_maybe_flag(local);
		uint8_t wire[16];
		pc_netplay_input_encode(local, wire);
		gekko_add_local_input(sGekko, sLocalHandle, wire);
		++sSubmitted;
		++sNextLand;
		if (sHolding && land == (uint64_t)sHoldFrame + kHoldLeadFrames - 1) {
			printf("[netplay] hold: last pre-hold input frame=%llu (submit=%llu delay=%u)\n",
			       (unsigned long long)land, (unsigned long long)(sSubmitted - 1), from + j);
			fflush(stdout);
		}
	}
	if (n > 1) {
		sCfg.localDelay = from + n - 1;
		adaptive_log_change(from, sCfg.localDelay, sPendingWhy);
	}
}

// A completed Advance: one presented frame. `spansHold` drops the interval
// that contains a frozen B1 hold (held time is reported as held=).
void adaptive_note_frame(bool spansHold)
{
	const double now = now_ms();
	if (sLastFrameEndMs > 0 && !spansHold) sStats.add_frame(now - sLastFrameEndMs);
	sLastFrameEndMs = now;
}

void adaptive_close_stall()
{
	if (!sStallOpen) return;
	sStallOpen = false;
	pc_netplay_adaptive::StallEvent e;
	e.startMs = sStallStartMs;
	e.durMs = sStallDurMs;
	e.excluded = sStallExcluded;
	sStats.add_stall(e);
	// Reported to the peer (its inputs were late here), never fed to this
	// peer's own controller.
	const bool counted = pc_netplay_adaptive::stall_counted(e, sDelayCtl.policy().hitchMs);
	if (counted) {
		sLateMs += e.durMs;
		++sLateEvents;
	}
	if (stall_trace()) {
		printf("[netplay] stall-trace: stall frame=%llu t=%.3fs dur=%.1fms excluded=%d counted=%d ahead=%.2f delay=%u\n",
		       (unsigned long long)sAdvances, session_s(e.startMs), e.durMs, (int)e.excluded, (int)counted,
		       sStallAhead, sCfg.localDelay);
		fflush(stdout);
	}
}

// Own lateness. This peer produces its next input at the start of each turn,
// and the 30 Hz schedule says when that turn is due (sNextDueMs, set by the
// pacing block of the previous advance turn). A turn that starts late
// because of this peer (a long tick, a late wake-up of a loaded machine)
// delays its input by that much, and the peer will report the wait. The lag
// only counts where it grows (a catch-up run of turns after one long tick
// shrinks it again), never after a stall turn (then the lag is the peer's
// doing: the baseline resets), and not inside a load window (nobody reports
// those stalls). The controller subtracts it from the reported lateness.
void adaptive_note_turn_start(double turnStartMs)
{
	if (!sAdaptiveConfigured || !sGekkoStarted || sAdvances == 0 || pc_netplay_unthrottled()) return;
	const double lag = (sHaveDue && turnStartMs > sNextDueMs) ? turnStartMs - sNextDueMs : 0.0;
	if (sPrevTurnStalled || !sHaveDue) {
		sOwnLagPrev = lag;
		return;
	}
	const double grew = lag - sOwnLagPrev;
	sOwnLagPrev = lag;
	if (grew <= 0 || sLgWindow.is_open()) return;
	sSelfOverrunMs += grew;
	sDelayCtl.add_self_overrun(turnStartMs, grew);
	if (stall_trace() && grew >= 20.0) {
		printf("[netplay] stall-trace: own lag frame=%llu t=%.3fs +%.1fms (behind schedule %.1fms)\n",
		       (unsigned long long)sAdvances, session_s(turnStartMs), grew, lag);
		fflush(stdout);
	}
}

// End of a session turn: the pacing's next due time (advance turns only).
void adaptive_note_turn_end(bool advanced, bool haveDue, double nextDueMs)
{
	sPrevTurnStalled = !advanced;
	if (advanced && haveDue) {
		sNextDueMs = nextDueMs;
		sHaveDue = true;
	}
}

// After each Advance's tick (trace only).
void adaptive_note_tick(double tickMs)
{
	const double now = now_ms();
	if (stall_trace() && tickMs >= 50.0) {
		printf("[netplay] stall-trace: slow tick frame=%llu t=%.3fs tick=%.1fms window=%d\n",
		       (unsigned long long)sAdvances, session_s(now), tickMs, (int)sLgWindow.is_open());
		fflush(stdout);
	}
}

void hs_send(const uint8_t* msg, size_t len); // defined with the handshake below

// Every kAdviceMs once GekkoNet started: this peer's cumulative counted
// stalls, delay and RTT p50 on the handshake channel (unreliable: the totals
// are cumulative, so a lost datagram loses nothing but time).
void adaptive_send_advice(double nowMs)
{
	if (nowMs < sAdviceNextMs) return;
	sAdviceNextMs = nowMs + kAdviceMs;
	pc_netplay_adaptive::Advice a;
	a.seq = ++sAdviceSeq;
	a.lateMs = (uint32_t)(sLateMs + 0.5);
	a.lateEvents = sLateEvents;
	a.delay = (uint8_t)sCfg.localDelay;
	const double p50 = sStats.rtt_percentile(50);
	a.rttP50 = p50 < 0 ? 0xFFFF : (uint16_t)(p50 > 65534 ? 65534 : p50);
	a.flags = sAdaptive ? 1 : 0;
	uint8_t msg[kHsHeaderLen + pc_netplay_adaptive::kAdvicePayload];
	memcpy(msg, pc_netplay_xfer::kHsMagic, 4);
	msg[4] = kHsAdvice;
	const uint16_t proto = local_protocol_version();
	msg[5] = (uint8_t)(proto & 0xFF);
	msg[6] = (uint8_t)((proto >> 8) & 0xFF);
	pc_netplay_adaptive::advice_encode(a, msg + kHsHeaderLen);
	hs_send(msg, sizeof(msg));
}

// A kHsAdvice datagram from the peer (answer_handshake_in_session).
void adaptive_on_advice(const uint8_t* payload, size_t len)
{
	if (!sAdaptiveConfigured) return;
	pc_netplay_adaptive::Advice a;
	if (!pc_netplay_adaptive::advice_decode(payload, len, &a)) return;
	double delta = 0;
	uint32_t events = 0;
	if (!sAdviceIn.take(a, &delta, &events)) return; // stale or duplicate
	const double now = now_ms();
	sDelayCtl.add_report(now);
	if (delta > 0) {
		sPeerLateMs += delta;
		sDelayCtl.add_lateness(now, delta, events);
	}
}

// A session turn without an Advance (not a frozen hold) that took durMs.
void adaptive_note_stall_turn(double startMs, double durMs)
{
	if (sAdvances == 0) return; // before the first Advance: session start, not a stall
	if (!sStallOpen) {
		sStallOpen = true;
		sStallStartMs = startMs;
		sStallDurMs = 0;
		// Never reported: a load window (load time, lane S), and the first
		// frames, while the two 30 Hz schedules settle after the first load.
		sStallExcluded = sLgWindow.is_open() || sAdvances < kAdaptiveWarmupFrames;
		sStallAhead = sGekko != nullptr ? gekko_frames_ahead(sGekko) : 0.0f;
	}
	sStallDurMs += durMs;
}

// Every session turn: RTT sampling.
void adaptive_poll(double nowMs)
{
	if (sGekko == nullptr || !sGekkoStarted) return;
	adaptive_send_advice(nowMs);
	if (nowMs < sRttNextMs) return;
	sRttNextMs = nowMs + kRttSampleMs;
	GekkoNetworkStats st;
	memset(&st, 0, sizeof(st));
	gekko_network_stats(sGekko, 1 - sLocalHandle, &st);
	if (st.avg_ping <= 0.0f && st.last_ping == 0) return; // no sample yet
	sStats.add_rtt((double)st.last_ping);
	pc_netplay_adaptive::RttSample s;
	s.atMs = nowMs;
	s.rttMs = (double)st.last_ping;
	sDelayCtl.add_rtt(s);
}

void adaptive_stats_line(const char* tag)
{
	const double now = now_ms();
	uint64_t n10 = 0;
	double ms10 = 0;
	sStats.recent(now, 10000.0, &n10, &ms10);
	const pc_netplay_adaptive::FrameTimeHist& f = sStats.frames();
	printf("[netplay] %s: t=%.1fs frame=%llu delay=%u (start %u, range %u..%u, up %llu down %llu) "
	       "stalls=%llu total=%.0fms max=%.0fms excluded=%llu last10s=%llu/%.0fms reported=%.0fms "
	       "own-lag=%.0fms peer: delay=%d late=%.0fms reports=%llu "
	       "rtt last=%.0f p50=%.0f p95=%.0f jitter=%.1f samples=%llu "
	       "frames=%llu p50=%.1f p95=%.1f p99=%.1f max=%.1f >50ms=%llu >100ms=%llu\n",
	       tag, session_s(now), (unsigned long long)sAdvances, sCfg.localDelay, sDelayStart, sDelayLow, sDelayHigh,
	       (unsigned long long)sDelayUps, (unsigned long long)sDelayDowns, (unsigned long long)sStats.stall_count(),
	       sStats.stall_total_ms(), sStats.stall_max_ms(), (unsigned long long)sStats.stall_excluded(),
	       (unsigned long long)n10, ms10, sLateMs, sSelfOverrunMs, sAdviceIn.have() ? (int)sAdviceIn.last().delay : -1, sPeerLateMs,
	       (unsigned long long)sAdviceIn.reports(), sStats.rtt_last(), sStats.rtt_percentile(50), sStats.rtt_percentile(95),
	       sStats.rtt_jitter(), (unsigned long long)sStats.rtt_samples(), (unsigned long long)f.count(),
	       f.percentile(50), f.percentile(95), f.percentile(99), f.max_ms(), (unsigned long long)f.at_least(50.0),
	       (unsigned long long)f.at_least(100.0));
	fflush(stdout);
}

// Once per session, from stop_session (exit-after, disconnect, desync,
// window close): the final figures, the delay timeline and the histogram.
// "excluded" stalls (load window, first frames) are in every total but never
// reported to the peer; "reported" is what this peer told the peer.
void adaptive_final_stats()
{
	if (!sAdaptiveConfigured || sStatsFinalDone || sSessionStartMs <= 0) return;
	sStatsFinalDone = true;
	adaptive_close_stall();
	adaptive_stats_line("stats final");
	printf("[netplay] delay timeline: +0.0s@0:%u%s%s\n", sDelayStart, sDelayTimeline.empty() ? "" : " ",
	       sDelayTimeline.c_str());
	printf("[netplay] frame-time histogram (ms): %s\n", sStats.frames().summary().c_str());
	fflush(stdout);
}

void loadguard_summary(); // M4 gap-fix lane S, defined with the load guard below

void stop_session()
{
	adaptive_final_stats(); // M5c lane B: once per session; no-op unless the session started
	loadguard_summary(); // lane S: once per session; no-op unless the session started
	sInAdvance = false;
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
	// M5a ICE teardown (mirrors the UDP pair above; the link dies first so
	// no trampoline can touch the socket during close).
	delete sIceLink;
	sIceLink = nullptr;
	if (sIce != nullptr) {
		sIce->close();
		delete sIce;
		sIce = nullptr;
	}
	// Fix3 R2-1: the session is over, hand timer-resolution throttling back
	// to the system (ControlMask = StateMask = 0).
	netplay_timer_power_opt(false);
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
	// M5a ICE switches (issue #887): --netplay-ice-host is a bare flag (the
	// host has no UDP port to bind); --netplay-ice-join takes the offer code
	// or @file. Env equivalents: PIKMIN_NETPLAY_ICE_HOST=1,
	// PIKMIN_NETPLAY_ICE_JOIN=<code-or-@file>.
	auto argv_present = [](const char* flag) {
		for (int i = 1; i < sArgc; ++i) {
			if (sArgv[i] != nullptr && std::strcmp(sArgv[i], flag) == 0) return true;
		}
		return false;
	};
	const bool iceHostCli = argv_present("--netplay-ice-host");
	const char* iceJoinCli = argv_value(sArgc, sArgv, "--netplay-ice-join");
	const char* iceHostEnv = getenv_nonempty("PIKMIN_NETPLAY_ICE_HOST");
	const char* iceJoinEnv = getenv_nonempty("PIKMIN_NETPLAY_ICE_JOIN");
	const bool iceHostVal = iceHostCli || (iceHostEnv != nullptr && iceHostEnv[0] == '1');
	const char* iceJoinVal = iceJoinCli != nullptr ? iceJoinCli : iceJoinEnv;
	if ((hostVal != nullptr || joinVal != nullptr) && (iceHostVal || iceJoinVal != nullptr)) {
		printf("[netplay] --netplay-host/--netplay-join and --netplay-ice-host/--netplay-ice-join are exclusive\n");
		fflush(stdout);
		std::exit(2);
	}
	if (iceHostVal && iceJoinVal != nullptr) {
		printf("[netplay] --netplay-ice-host and --netplay-ice-join are exclusive\n");
		fflush(stdout);
		std::exit(2);
	}
	// Launch lane (issue #887): --netplay-host-ice / --netplay-join-ice were
	// resolved before engine init (pc_netplay_launch_preinit), which also
	// refused every conflicting switch. The run bootstrap reaches the session
	// as the injected --randomizer-seed below, like any hand-passed seed.
	const PcNetplayLaunch& launch = pc_netplay_launch_setup();
	if (launch.active) {
		sCfg.isHost       = launch.isHost;
		sCfg.active       = true;
		sCfg.iceMode      = true;
		sCfg.launcherMode = true;
		sCfg.exitAfter    = launch.testTicks;
	}
	if (iceHostVal || iceJoinVal != nullptr) {
		sCfg.isHost   = iceHostVal;
		sCfg.active   = true;
		sCfg.iceMode  = true;
		if (iceJoinVal != nullptr) sCfg.iceJoinCode = iceJoinVal;
	}
	if (sCfg.iceMode) {
		// ICE mode parsed above; the UDP chain is skipped.
	} else if (hostVal != nullptr && joinVal != nullptr) {
		printf("[netplay] --netplay-host and --netplay-join are exclusive\n");
		fflush(stdout);
		std::exit(2);
	} else if (hostVal != nullptr) {
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
	// Fix round 3: PIKMIN_NETPLAY_DELAY=auto measures the nonce-matched
	// median handshake RTT and picks ceil((RTT/2) / 33.3 ms - 0.05) + 1
	// (clamped 1..8) after the handshake. Numeric delays are per-peer (GekkoNet delay
	// is per actor) and are NOT part of the config hash, so asymmetric
	// pairs (e.g. 2 and 4) start without refusing.
	if (const char* d = getenv_nonempty("PIKMIN_NETPLAY_DELAY")) {
		if (std::strcmp(d, "auto") == 0) {
			sCfg.delayAuto  = true;
			sCfg.localDelay = 2;
		} else {
			sCfg.localDelay = read_unsigned_env("PIKMIN_NETPLAY_DELAY", 2);
			if (sCfg.localDelay > 8) sCfg.localDelay = 8;
		}
	} else if (sCfg.launcherMode) {
		// Launch lane: DELAY=auto is the default (humans never set it).
		// An explicit PIKMIN_NETPLAY_DELAY still wins (test surface).
		sCfg.delayAuto  = true;
		sCfg.localDelay = 2;
	} else {
		sCfg.localDelay = 2;
	}
	// Launcher joiner: the offer's seed (preinit also exported it as
	// PIKMIN_NETPLAY_SEED, which the det reseed reads).
	sCfg.seed = launch.active ? launch.seed : read_u32_env("PIKMIN_NETPLAY_SEED", 0);
	const char* bootCli = argv_value(sArgc, sArgv, "--randomizer-seed");
	sCfg.bootstrapPath  = bootCli != nullptr ? bootCli : "bootstrap.txt";
	// --netplay-input keyboard|gamepad[:N]|auto (CLI wins, PIKMIN_NETPLAY_INPUT
	// is the test-surface fallback), validated before engine init by
	// pc_netplay_launch_preinit (so a gamepad peer's background hint is set
	// before SDL_Init, m6) for every netplay mode.
	sCfg.inputSpec    = launch.inputSpec;
	sCfg.inputKind    = launch.inputKind;
	sCfg.inputGamepad = launch.inputIndex;
	const char* lif     = getenv_nonempty("PIKMIN_NETPLAY_LOCAL_INPUT_FILE");
	if (lif != nullptr) sCfg.localInputFile = lif;
	// CLI first, env second (CLI wins): --netplay-test-ticks already set
	// exitAfter above; the env only fills it in when no CLI value was given.
	uint64_t exitAfter = sCfg.exitAfter;
	if (exitAfter == 0) {
		if (const char* ea = getenv_nonempty("PIKMIN_NETPLAY_EXIT_AFTER_TICKS")) {
			char* end     = nullptr;
			unsigned long n = strtoul(ea, &end, 10);
			if (end != ea && *end == '\0' && n > 0) exitAfter = n;
		}
	}
	sCfg.exitAfter = exitAfter;
	sLocalRole    = sCfg.isHost ? 0 : 1;
	// Each peer presents its own captain full screen (M2b): host P1, joiner P2.
	pc_netplay_present_set_local_player_default(sLocalRole);
	// M4a: cache the external-state stream gate (env default on; =0 is the
	// negative control that restores legacy file polling on both peers).
	sRandStream = randstate_env_on();
	sPhase        = kHandshake;
}

// ---- M4 lane B2: checkpoint / card / P2 sidecar and overlay digests ----
// (issue #885). All reads are bounded; nothing here touches sim state.
namespace fs = std::filesystem;
using pc_netplay_xfer::File;

std::string hex16(const uint8_t* sha) { return to_hex(sha, 8); }

bool b2_p2_mode()
{
	return pc_randomizer_enabled != nullptr && pc_randomizer_enabled() && pc_randomizer_p2_bridge != nullptr
	    && pc_randomizer_p2_bridge();
}

std::string b2_campaign_dir()
{
	if (pc_randomizer_campaign_dir == nullptr) return std::string();
	const char* d = pc_randomizer_campaign_dir();
	return d != nullptr ? std::string(d) : std::string();
}

// Reads a whole regular file of at most `cap` bytes. False when missing,
// unreadable or larger than the cap.
bool read_small_file(const fs::path& path, size_t cap, std::string* out)
{
	std::error_code ec;
	if (!fs::is_regular_file(path, ec)) return false;
	const uintmax_t size = fs::file_size(path, ec);
	if (ec || size > cap) return false;
	FILE* f = fopen(path.string().c_str(), "rb");
	if (f == nullptr) return false;
	out->assign((size_t)size, '\0');
	const size_t got = size > 0 ? fread(&(*out)[0], 1, (size_t)size, f) : 0;
	fclose(f);
	return got == (size_t)size;
}

// The card files of this peer's campaign (names relative to card0/, sorted).
// A file that cannot be read makes the set "unreadable" (err set).
bool read_card_files(std::vector<File>* out, std::string* err)
{
	out->clear();
	const std::string dir = b2_campaign_dir();
	if (dir.empty()) return true;
	const fs::path card0 = fs::path(dir) / "card" / "card0";
	std::error_code ec;
	if (!fs::is_directory(card0, ec)) return true;
	for (const auto& entry : fs::directory_iterator(card0, ec)) {
		if (!entry.is_regular_file(ec)) continue;
		File f;
		f.name = entry.path().filename().string();
		if (!read_small_file(entry.path(), pc_netplay_xfer::kMaxBundleFileBytes, &f.bytes)) {
			*err = "cannot read card file " + f.name;
			return false;
		}
		out->push_back(std::move(f));
		if (out->size() > pc_netplay_xfer::kMaxBundleFiles) {
			*err = "too many card files";
			return false;
		}
	}
	pc_netplay_xfer::sort_files(*out);
	return true;
}

// P2 sidecar set: the regular files directly in `dir` whose names match
// ^(p2|sarai)-[a-z0-9-]+\.txt$, sorted, each and in total within the bounds.
bool read_sidecar_set(const fs::path& dir, std::vector<File>* out, std::string* err)
{
	out->clear();
	std::error_code ec;
	size_t total = 0;
	for (const auto& entry : fs::directory_iterator(dir, ec)) {
		const std::string name = entry.path().filename().string();
		if (!pc_netplay_xfer::sidecar_name_ok(name) || !entry.is_regular_file(ec)) continue;
		File f;
		f.name = name;
		if (!read_small_file(entry.path(), pc_netplay_xfer::kMaxBundleFileBytes, &f.bytes)) {
			*err = "sidecar " + name + " is unreadable or larger than " +
			       std::to_string(pc_netplay_xfer::kMaxBundleFileBytes) + " bytes";
			return false;
		}
		total += f.bytes.size();
		out->push_back(std::move(f));
	}
	if (ec) {
		*err = "cannot list " + dir.string();
		return false;
	}
	if (total > pc_netplay_xfer::kMaxSidecarTotal) {
		*err = "sidecar set is larger than 2 MiB";
		return false;
	}
	if (out->size() > pc_netplay_xfer::kMaxBundleFiles) {
		*err = "too many sidecar files";
		return false;
	}
	pc_netplay_xfer::sort_files(*out);
	return true;
}

// SHA-256 of a file named by a (possibly non-ASCII) path.
bool sha_path(const fs::path& p, uint8_t out[32], uint64_t* size)
{
#ifdef _WIN32
	FILE* f = _wfopen(p.c_str(), L"rb");
#else
	FILE* f = fopen(p.c_str(), "rb");
#endif
	if (f == nullptr) return false;
	pc_netplay_sha::Sha256 s;
	static uint8_t chunk[65536];
	uint64_t total = 0;
	bool bad = false;
	while (true) {
		const size_t n = fread(chunk, 1, sizeof(chunk), f);
		if (n > 0) s.update(chunk, n);
		total += n;
		if (n < sizeof(chunk)) {
			bad = ferror(f) != 0;
			break;
		}
	}
	fclose(f);
	if (bad) return false;
	s.final(out);
	*size = total;
	return true;
}

// P2 overlay digest (B2 fix round 1, X1/C10). Scope, all cwd-relative:
//   assets/dataDir/courses/pikmin2room/**  the P2 room models;
//   assets/dataDir/stages/**               the stage .ini files and the .gen
//                                          generators (the overlay rewrites
//                                          stages/stage1/*.gen);
//   assets/config.ini, assets/.pikmin-assets and assets/p2-*.txt.
// Junctions and symlinks are followed and paths are lexical (relative to
// assets/, '/'-separated, UTF-8), so an overlay that is a full copy, a
// junction, or a copy with junctioned subfolders gives the same digest for
// the same content. Sorted paths plus each file's SHA-256; the content never
// travels, each peer brings its own overlay.
const char* kP2AssetsScope = "assets/dataDir/courses/pikmin2room/**, assets/dataDir/stages/**, assets/config.ini, "
                             "assets/.pikmin-assets, assets/p2-*.txt";
bool p2_assets_digest(uint8_t out[32], size_t* files, uint64_t* bytes, std::string* err)
{
	std::vector<pc_netplay_xfer::AssetEntry> entries;
	*files = 0;
	*bytes = 0;
	std::error_code ec;
	const fs::path assets = "assets";
	auto add = [&](const fs::path& p) -> bool {
		pc_netplay_xfer::AssetEntry e;
		e.path = p.lexically_relative(assets).generic_u8string();
		if (e.path.empty() || e.path.compare(0, 2, "..") == 0) {
			*err = "cannot relativise " + p.generic_u8string();
			return false;
		}
		uint64_t size = 0;
		if (!sha_path(p, e.sha, &size)) {
			*err = "cannot read " + p.generic_u8string();
			return false;
		}
		*bytes += size;
		entries.push_back(e);
		return true;
	};
	auto walk = [&](const fs::path& root) -> bool {
		if (!fs::is_directory(root, ec)) return true;
		for (fs::recursive_directory_iterator it(root, fs::directory_options::follow_directory_symlink, ec), end;
		     it != end && !ec; it.increment(ec)) {
			std::error_code ec2;
			if (it->is_regular_file(ec2) && !add(it->path())) return false;
		}
		if (ec) {
			*err = "cannot walk " + root.generic_u8string() + ": " + ec.message();
			return false;
		}
		return true;
	};
	if (!walk(assets / "dataDir" / "courses" / "pikmin2room")) return false;
	if (!walk(assets / "dataDir" / "stages")) return false;
	if (fs::is_directory(assets, ec)) {
		for (fs::directory_iterator it(assets, ec), end; it != end && !ec; it.increment(ec)) {
			const std::string name = it->path().filename().generic_u8string();
			std::error_code ec2;
			const bool wanted = name == "config.ini" || name == ".pikmin-assets"
			                 || (name.size() > 7 && name.compare(0, 3, "p2-") == 0
			                     && name.compare(name.size() - 4, 4, ".txt") == 0);
			if (wanted && it->is_regular_file(ec2) && !add(it->path())) return false;
		}
		if (ec) {
			*err = "cannot list assets/: " + ec.message();
			return false;
		}
	}
	*files = entries.size();
	pc_netplay_xfer::assets_digest(entries, out);
	return true;
}

// Fills the v3 Hello's checkpoint, card and P2 fields (B2). Called once,
// from compute_local_hello, before the transport comes up.
void compute_local_hello_b2()
{
	uint64_t gen = 0;
	if (pc_randomizer_checkpoint_info != nullptr) pc_randomizer_checkpoint_info(&gen, sLocal.ckptSha);
	sLocal.ckptGen = gen;
	std::vector<File> card;
	std::string err;
	if (!read_card_files(&card, &err)) {
		printf("[netplay] campaign card: %s; refusing to start\n", err.c_str());
		fflush(stdout);
		std::exit(2);
	}
	pc_netplay_xfer::card_digest(card, sLocal.cardSha);
	printf("[netplay] local checkpoint: gen=%llu sha=%s card=%s (%llu card files)\n", (unsigned long long)gen,
	       hex16(sLocal.ckptSha).c_str(), hex16(sLocal.cardSha).c_str(), (unsigned long long)card.size());
	for (const File& f : card)
		if (!pc_netplay_xfer::card_file_name_ok(f.name))
			printf("[netplay] campaign card: warning: '%s' in card0/ is not a card file this game writes (it is a "
			       "card entry for the game, and it cannot be transferred)\n",
			       f.name.c_str());
	if (b2_p2_mode()) {
		std::vector<File> side;
		if (!read_sidecar_set(fs::current_path(), &side, &err)) {
			printf("[netplay] p2 sidecars: %s; refusing to start\n", err.c_str());
			fflush(stdout);
			std::exit(2);
		}
		pc_netplay_xfer::sidecar_digest(side, sLocal.sidecarSha);
		uint64_t sideBytes = 0;
		for (const File& f : side) sideBytes += f.bytes.size();
		size_t assetFiles = 0;
		uint64_t assetBytes = 0;
		const double t0 = now_ms();
		if (!p2_assets_digest(sLocal.p2AssetsSha, &assetFiles, &assetBytes, &err)) {
			printf("[netplay] p2 overlay: %s; refusing to start\n", err.c_str());
			fflush(stdout);
			std::exit(2);
		}
		printf("[netplay] p2 sidecars: %llu files, %llu B in %s, sidecarSha=%s\n", (unsigned long long)side.size(),
		       (unsigned long long)sideBytes, fs::current_path().string().c_str(),
		       to_hex(sLocal.sidecarSha, 32).c_str());
		printf("[netplay] p2 overlay: %llu files, %llu B (%s), p2AssetsSha=%s (%.0f ms)\n",
		       (unsigned long long)assetFiles, (unsigned long long)assetBytes, kP2AssetsScope,
		       to_hex(sLocal.p2AssetsSha, 32).c_str(), now_ms() - t0);
		if (assetFiles == 0)
			printf("[netplay] p2 overlay: warning: no P2 overlay files under assets/ (the room models are missing)\n");
	}
	fflush(stdout);
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
	if (sCfg.launcherMode) {
		// The exact sim settings this peer runs the session with (the joiner's
		// are the host's, adopted before init); the handshake hashes this text.
		printf("[netplay] launch: session config %s\n", cfg.c_str());
		fflush(stdout);
	}
	// Bootstrap hash with the SESSION token line removed.
	sha_text(read_bootstrap_stripped(), sLocal.boot);
	sBootHexStr  = to_hex(sLocal.boot, 32);
	sLocal.seed  = sCfg.seed;
	compute_local_hello_b2();
}

// M5a transport-selection hook (issue #887): the session creates either the
// UDP socket/adapter or the ICE agent/adapter, and everything below talks
// through these two helpers. hs_send routes one handshake datagram;
// hs_drain collects arrived handshake datagrams as UdpSocket::Datagram
// (the ICE grams convert field-for-field; over ICE the sender fields are
// the fixed 127.0.0.1:1 placeholder).
void hs_send(const uint8_t* msg, size_t len)
{
	if (sIce != nullptr) {
		sIce->send_payload(pc_netplay_transport::kChannelHandshake, msg, len);
		return;
	}
	if (sSock == nullptr) return;
	if (sCfg.isHost) {
		if (sHaveRemote)
			sSock->send_to(pc_netplay_transport::kChannelHandshake, msg, len, sRemoteIp,
			               sRemotePort);
	} else {
		sSock->send_payload(pc_netplay_transport::kChannelHandshake, msg, len);
	}
}

std::vector<pc_netplay_transport::UdpSocket::Datagram> hs_drain()
{
	if (sIceLink != nullptr) {
		std::vector<pc_netplay_transport::UdpSocket::Datagram> out;
		for (pc_netplay_ice::IceSocket::Datagram& g : sIceLink->drain_handshake()) {
			pc_netplay_transport::UdpSocket::Datagram d;
			d.channel         = g.channel;
			d.payload         = g.payload;
			d.fromIpHostOrder = g.fromIpHostOrder;
			d.fromPort        = g.fromPort;
			out.push_back(std::move(d));
		}
		return out;
	}
	if (sLink != nullptr) return sLink->drain_handshake();
	return std::vector<pc_netplay_transport::UdpSocket::Datagram>();
}

void send_hello_msg(uint8_t type, uint8_t refuseField, uint64_t nonce)
{
	uint8_t msg[kHsLen];
	Hello h = sLocal;
	h.nonce = nonce;
	pc_netplay_xfer::encode_hello(type, local_protocol_version(), h, refuseField, msg);
	// Test hook: PIKMIN_NETPLAY_TEST_HANDSHAKE_LEN=108 truncates the frame to
	// the v1 length so the 108-byte cross-version refuse path is exercised,
	// not just the version field (m2). Any other value sends kHsLen.
	size_t wireLen = sizeof(msg);
	{
		static int sHsLenInit = 0;
		static size_t sHsLen  = 0;
		if (!sHsLenInit) {
			sHsLenInit = 1;
			sHsLen     = sizeof(msg);
			if (const char* e = std::getenv("PIKMIN_NETPLAY_TEST_HANDSHAKE_LEN")) {
				char* end         = nullptr;
				unsigned long n = strtoul(e, &end, 10);
				// B2: 116 sends the v2 length (a v2-shaped frame at v3).
				if (end != e && *end == '\0' && (n == kHsLenV1 || n == kHsLenV2)) sHsLen = (size_t)n;
			}
		}
		wireLen = sHsLen;
	}
	// Fix round 3 (M1 test): PIKMIN_NETPLAY_TEST_DROP_FINAL_ACK=1 drops
	// handshake-phase Acks (the Ack(s) sent just before entering the
	// session), reporting success so the peer must recover via in-session
	// Hello answers. In-session answers (sPhase == kSession) are never
	// dropped.
	if (type == kHsAck && sPhase == kHandshake) {
		if (!sDropFinalAckInit) {
			sDropFinalAckInit = true;
			if (const char* e = std::getenv("PIKMIN_NETPLAY_TEST_DROP_FINAL_ACK"))
				sDropFinalAck = (e[0] == '1' && e[1] == '\0');
		}
		if (sDropFinalAck) {
			printf("[netplay] test: dropped final handshake Ack (echo=%llu)\n",
			       (unsigned long long)nonce);
			fflush(stdout);
			return;
		}
	}
	hs_send(msg, wireLen);
}

bool parse_hello_msg(const uint8_t* p, size_t len, uint8_t* type, uint16_t* proto, Hello* h,
                     uint8_t* refuseField)
{
	return pc_netplay_xfer::decode_hello(p, len, type, proto, h, refuseField);
}

void refuse_and_exit(uint8_t field)
{
	printf("[netplay] handshake refused: %s\n", field_name(field));
	fflush(stdout);
	print_refusal_hint(field);
	// Best effort: tell the peer why (5 quick sends; the channel is
	// unreliable by design under the lossy test wrapper).
	for (int i = 0; i < 5; ++i) send_hello_msg(kHsRefuse, field, 0);
	fflush(stdout);
	// m12: stop the session first so the libjuice agent thread is joined
	// before exit (its callbacks can otherwise printf during teardown).
	stop_session();
	std::exit(4);
}

// ---- M4 lane B2: handshake decisions and the transfer phase (issue #885) ----
void start_gekko_session(); // defined below

const Hello& b2_host_hello() { return sCfg.isHost ? sLocal : sRemote; }
const Hello& b2_join_hello() { return sCfg.isHost ? sRemote : sLocal; }

void log_checkpoint_line(pc_netplay_xfer::CkptAction action, const char* why)
{
	const Hello& H = b2_host_hello();
	const Hello& J = b2_join_hello();
	printf("[netplay] checkpoint: host gen=%llu sha=%s joiner gen=%llu sha=%s action=%s\n",
	       (unsigned long long)H.ckptGen, hex16(H.ckptSha).c_str(), (unsigned long long)J.ckptGen,
	       hex16(J.ckptSha).c_str(), pc_netplay_xfer::action_name(action));
	printf("[netplay] checkpoint: host card=%s joiner card=%s (%s)\n", hex16(H.cardSha).c_str(),
	       hex16(J.cardSha).c_str(), why != nullptr ? why : "-");
	fflush(stdout);
}

// B2 fix round 1 (X7/E5): the peer that RECEIVES a checkpoint, sidecars or
// p2assets refusal logs its own view before exiting: the decision line when
// it already holds the other peer's Hello (the same table both sides
// evaluate), otherwise its local digests.
void b2_refusal_view(uint8_t field)
{
	if (field != kFieldCheckpoint && field != kFieldSidecars && field != kFieldP2Assets) return;
	if (sHaveRemoteHello) {
		const char* why = nullptr;
		const pc_netplay_xfer::CkptAction action = pc_netplay_xfer::decide_checkpoint(b2_host_hello(),
		                                                                              b2_join_hello(), &why);
		log_checkpoint_line(action, why);
	} else {
		printf("[netplay] checkpoint: %s gen=%llu sha=%s card=%s (the refusal came before the %s's Hello)\n",
		       sCfg.isHost ? "host" : "joiner", (unsigned long long)sLocal.ckptGen, hex16(sLocal.ckptSha).c_str(),
		       hex16(sLocal.cardSha).c_str(), sCfg.isHost ? "joiner" : "host");
	}
	if (field != kFieldCheckpoint)
		printf("[netplay] p2 digests: local sidecarSha=%s p2AssetsSha=%s\n", to_hex(sLocal.sidecarSha, 32).c_str(),
		       to_hex(sLocal.p2AssetsSha, 32).c_str());
	fflush(stdout);
}

// Called for every accepted Hello/Ack (after the M3 field checks): refuses
// a checkpoint the table cannot reconcile and an overlay mismatch. A sidecar
// mismatch is a sidecar transfer in every mode (B2 fix round 1, C13).
void handshake_check_b2(const Hello& h)
{
	sRemote          = h;
	sHaveRemoteHello = true;
	const Hello& H   = b2_host_hello();
	const Hello& J   = b2_join_hello();
	const char* why  = nullptr;
	const pc_netplay_xfer::CkptAction action = pc_netplay_xfer::decide_checkpoint(H, J, &why);
	if (action == pc_netplay_xfer::CkptAction::Refuse) {
		log_checkpoint_line(action, why);
		refuse_and_exit(kFieldCheckpoint);
	}
	if (memcmp(H.p2AssetsSha, J.p2AssetsSha, 32) != 0) {
		printf("[netplay] p2 overlay mismatch: host p2AssetsSha=%s joiner p2AssetsSha=%s (each player needs the same "
		       "P2 assets overlay)\n",
		       to_hex(H.p2AssetsSha, 32).c_str(), to_hex(J.p2AssetsSha, 32).c_str());
		fflush(stdout);
		refuse_and_exit(kFieldP2Assets);
	}
}

// Transfer phase state.
pc_netplay_xfer::CkptAction sXferAction = pc_netplay_xfer::CkptAction::None;
bool sXferNeedCkpt = false;
bool sXferNeedSidecars = false;
double sXferStartMs = 0;
constexpr double kXferTimeoutMs = 60000.0;
pc_netplay_xfer::BundleCollector sXferCkpt;
pc_netplay_xfer::BundleCollector sXferSide;
bool sXferCkptDone = false;
bool sXferSideDone = false;
bool sXferDoneSent = false;
size_t sXferCkptFiles = 0, sXferSideFiles = 0;

// Atomic write: <tmpDir>/.xfer-<name>.tmp, flushed and synced, then renamed
// onto the target. B2 fix round 1 (C9): the temporary lives in tmpDir (the
// campaign root for card files), never inside card0/, where the card stub
// would list a leftover as a card entry. Callers move a differing existing
// target aside first (write_keeping_old).
bool atomic_write(const fs::path& path, const std::string& bytes, const fs::path& tmpDir, std::string* err)
{
	std::error_code ec;
	fs::create_directories(path.parent_path(), ec);
	const fs::path tmp = tmpDir / (".xfer-" + path.filename().string() + ".tmp");
	FILE* f = fopen(tmp.string().c_str(), "wb");
	if (f == nullptr) {
		*err = "cannot create " + tmp.string();
		return false;
	}
	bool ok = (bytes.empty() || fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size()) && fflush(f) == 0;
#ifdef _WIN32
	ok = ok && _commit(_fileno(f)) == 0;
#endif
	ok = (fclose(f) == 0) && ok;
	if (!ok) {
		fs::remove(tmp, ec);
		*err = "cannot write " + tmp.string();
		return false;
	}
#ifdef _WIN32
	if (!MoveFileExA(tmp.string().c_str(), path.string().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		fs::remove(tmp, ec);
		*err = "cannot rename onto " + path.string();
		return false;
	}
#else
	fs::rename(tmp, path, ec);
	if (ec) {
		*err = "cannot rename onto " + path.string();
		return false;
	}
#endif
	return true;
}

// Host: the checkpoint bundle (newest .sav + card files) as bulk messages.
bool host_checkpoint_messages(std::vector<std::vector<uint8_t>>* msgs, size_t* files, size_t* bytes, std::string* err)
{
	const Hello& H = sLocal;
	std::vector<File> set;
	const fs::path dir = b2_campaign_dir();
	if (H.ckptGen != 0) {
		char name[32];
		snprintf(name, sizeof(name), "%020llu.sav", (unsigned long long)H.ckptGen);
		File f;
		f.name = name;
		if (!read_small_file(dir / name, pc_netplay_xfer::kMaxBundleFileBytes, &f.bytes)) {
			*err = std::string("cannot read ") + name;
			return false;
		}
		uint8_t sha[32];
		pc_netplay_sha::sha256(f.bytes.data(), f.bytes.size(), sha);
		if (memcmp(sha, H.ckptSha, 32) != 0) {
			*err = std::string(name) + " changed since the handshake";
			return false;
		}
		set.push_back(std::move(f));
	}
	std::vector<File> card;
	if (!read_card_files(&card, err)) return false;
	uint8_t cs[32];
	pc_netplay_xfer::card_digest(card, cs);
	if (memcmp(cs, H.cardSha, 32) != 0) {
		*err = "card files changed since the handshake";
		return false;
	}
	// B2 fix round 1 (C9): only the card files the stub writes can travel; name
	// anything else so the host can move it out instead of guessing.
	for (const File& f : card) {
		if (!pc_netplay_xfer::card_file_name_ok(f.name)) {
			*err = "the host's card folder " + (dir / "card" / "card0").string() + " holds '" + f.name +
			       "', which is not a card file this game writes; move it out of that folder and start again";
			return false;
		}
	}
	for (File& f : card) set.push_back(File{ "card/card0/" + f.name, std::move(f.bytes) });
	pc_netplay_xfer::sort_files(set);
	*files = set.size();
	*bytes = 0;
	for (const File& f : set) *bytes += f.bytes.size();
	return pc_netplay_xfer::encode_bundle(pc_netplay_xfer::BundleKind::Checkpoint, set, msgs, err);
}

// Host: the P2 sidecar set from this peer's working directory.
bool host_sidecar_messages(std::vector<std::vector<uint8_t>>* msgs, size_t* files, size_t* bytes, std::string* err)
{
	std::vector<File> side;
	if (!read_sidecar_set(fs::current_path(), &side, err)) return false;
	uint8_t d[32];
	pc_netplay_xfer::sidecar_digest(side, d);
	if (memcmp(d, sLocal.sidecarSha, 32) != 0) {
		*err = "sidecar files changed since the handshake";
		return false;
	}
	*files = side.size();
	*bytes = 0;
	for (const File& f : side) *bytes += f.bytes.size();
	// TEST ONLY (netplay builds): PIKMIN_NETPLAY_TEST_TAMPER_SIDECARS=1 flips
	// one byte of the first sidecar after hashing, so the joiner's digest
	// check must refuse `sidecars` (negative control).
	if (const char* t = getenv_nonempty("PIKMIN_NETPLAY_TEST_TAMPER_SIDECARS")) {
		if (t[0] == '1' && t[1] == '\0' && !side.empty() && !side[0].bytes.empty()) {
			side[0].bytes[0] = (char)(side[0].bytes[0] ^ 0x20);
			printf("[netplay] test: tampering sidecar %s in transit\n", side[0].name.c_str());
		}
	}
	return pc_netplay_xfer::encode_bundle(pc_netplay_xfer::BundleKind::Sidecars, side, msgs, err);
}

void transfer_fail(uint8_t field, const std::string& why)
{
	printf("[netplay] transfer failed: %s\n", why.c_str());
	fflush(stdout);
	refuse_and_exit(field);
}

// Handshake success: the decision (identical on both peers), then either
// the GekkoNet session right away or the transfer phase first.
void on_handshake_done()
{
	// B2 fix round 1 (C14): the decision below is over the remote Hello; a
	// path that got here without one must not decide over zeros.
	if (!sHaveRemoteHello) {
		printf("[netplay] handshake: completed without the peer's Hello (internal error); refusing\n");
		fflush(stdout);
		refuse_and_exit(kFieldCheckpoint);
	}
	// The bulk endpoint starts fresh here, once per session, before any B2
	// message (it used to reset in start_gekko_session; the transfer phase
	// now runs in between, so msgIds must not restart after it).
	sBulk.reset();
	sB2Inbox.clear();
	sB2Out.clear();
	sLedgerQueued = 0;
	sLedgerApplied = 0;
	const Hello& H = b2_host_hello();
	const Hello& J = b2_join_hello();
	const char* why = nullptr;
	sXferAction = pc_netplay_xfer::decide_checkpoint(H, J, &why);
	sXferNeedCkpt = sXferAction == pc_netplay_xfer::CkptAction::Transfer;
	// B2 fix round 1 (C13): sidecars travel in every mode, not only the
	// launcher's (the host's P2 receipt ledgers match the set too).
	sXferNeedSidecars = pc_netplay_xfer::sidecars_needed(H, J);
	log_checkpoint_line(sXferAction, why);
	if (!pc_netplay_sha::is_zero(H.sidecarSha, 32) || !pc_netplay_sha::is_zero(H.p2AssetsSha, 32)) {
		printf("[netplay] p2 digests: host sidecarSha=%s p2AssetsSha=%s\n", to_hex(H.sidecarSha, 32).c_str(),
		       to_hex(H.p2AssetsSha, 32).c_str());
		printf("[netplay] p2 digests: joiner sidecarSha=%s p2AssetsSha=%s\n", to_hex(J.sidecarSha, 32).c_str(),
		       to_hex(J.p2AssetsSha, 32).c_str());
		fflush(stdout);
	}
	if (!sXferNeedCkpt && !sXferNeedSidecars) {
		start_gekko_session();
		return;
	}
	sXferStartMs = now_ms();
	sXferCkpt = pc_netplay_xfer::BundleCollector();
	sXferSide = pc_netplay_xfer::BundleCollector();
	sXferCkptDone = sXferSideDone = sXferDoneSent = false;
	sPhase = kTransfer;
	if (sCfg.isHost) {
		std::string err;
		if (sXferNeedCkpt) {
			std::vector<std::vector<uint8_t>> msgs;
			size_t files = 0, bytes = 0;
			if (!host_checkpoint_messages(&msgs, &files, &bytes, &err)) transfer_fail(kFieldCheckpoint, err);
			for (auto& m : msgs) sB2Out.push_back(B2Msg{ kBulkCheckpoint, std::move(m) });
			printf("[netplay] transfer: sending checkpoint gen=%llu (%llu files, %llu B, %llu messages)\n",
			       (unsigned long long)H.ckptGen, (unsigned long long)files, (unsigned long long)bytes,
			       (unsigned long long)msgs.size());
		}
		if (sXferNeedSidecars) {
			std::vector<std::vector<uint8_t>> msgs;
			size_t files = 0, bytes = 0;
			if (!host_sidecar_messages(&msgs, &files, &bytes, &err)) transfer_fail(kFieldSidecars, err);
			for (auto& m : msgs) sB2Out.push_back(B2Msg{ kBulkSidecars, std::move(m) });
			printf("[netplay] transfer: sending %llu sidecar files (%llu B, %llu messages)\n",
			       (unsigned long long)files, (unsigned long long)bytes, (unsigned long long)msgs.size());
		}
	} else {
		printf("[netplay] transfer: waiting for the host's %s%s%s\n", sXferNeedCkpt ? "checkpoint" : "",
		       sXferNeedCkpt && sXferNeedSidecars ? " and " : "", sXferNeedSidecars ? "sidecars" : "");
	}
	fflush(stdout);
}

// Transfer phase: handshake stragglers get their Ack; a Refuse ends the
// run (exit 4) like during the handshake.
void transfer_handshake_pump()
{
	for (auto& g : hs_drain()) {
		uint8_t hdrType = 0;
		uint16_t hdrProto = 0;
		if (!parse_hello_header(g.payload.data(), g.payload.size(), &hdrType, &hdrProto)) continue;
		if (hdrProto != local_protocol_version()) continue;
		uint8_t type = 0, refuse = 0;
		uint16_t proto = 0;
		Hello h;
		if (!parse_hello_msg(g.payload.data(), g.payload.size(), &type, &proto, &h, &refuse)) continue;
		if (sCfg.isHost && sHaveRemote && sIceLink == nullptr
		    && (g.fromIpHostOrder != sRemoteIp || g.fromPort != sRemotePort))
			continue;
		if (type == kHsRefuse) {
			printf("[netplay] handshake refused: %s\n", field_name(refuse == 0 ? 99 : refuse));
			b2_refusal_view(refuse);
			fflush(stdout);
			stop_session();
			std::exit(4);
		}
		if (type == kHsHello) send_hello_msg(kHsAck, 0, h.nonce);
	}
}

long long unix_secs()
{
	return (long long)std::chrono::duration_cast<std::chrono::seconds>(
	           std::chrono::system_clock::now().time_since_epoch())
	    .count();
}

// Moves `from` into `asideDir` (created on demand), never deleting; a name
// already taken there gets a -<n> suffix. False (err set) on any failure.
bool move_aside(const fs::path& from, const fs::path& asideDir, std::string* err)
{
	std::error_code ec;
	fs::create_directories(asideDir, ec);
	if (ec) {
		*err = "cannot create " + asideDir.string() + ": " + ec.message();
		return false;
	}
	fs::path to = asideDir / from.filename();
	for (int n = 1; fs::exists(to, ec) && n < 1000; ++n)
		to = asideDir / (from.filename().string() + "-" + std::to_string(n));
	fs::rename(from, to, ec);
	if (ec) {
		*err = "cannot move " + from.string() + " to " + to.string() + ": " + ec.message();
		return false;
	}
	return true;
}

// B2 fix round 1 (X10): writes `bytes` to `target` atomically, but an
// existing target with other bytes is moved into asideDir first instead of
// being overwritten in place. Identical bytes are left untouched.
bool write_keeping_old(const fs::path& target, const std::string& bytes, const fs::path& asideDir,
                       const fs::path& tmpDir, bool* movedAside, std::string* err)
{
	*movedAside = false;
	std::error_code ec;
	if (fs::exists(target, ec)) {
		std::string old;
		if (read_small_file(target, bytes.size(), &old) && old == bytes) return true;
		if (!move_aside(target, asideDir, err)) return false;
		*movedAside = true;
	}
	return atomic_write(target, bytes, tmpDir, err);
}

// Joiner: writes the verified checkpoint bundle into its own campaign/ and
// adopts it. Card files the host does not have, and existing files with other
// bytes, are moved aside (never deleted), so card0/ then holds exactly the
// host's set; fix round 1 (C8) checks that on disk before adopting.
void joiner_adopt_checkpoint(const std::vector<File>& files)
{
	const fs::path dir = b2_campaign_dir();
	if (dir.empty()) transfer_fail(kFieldCheckpoint, "no campaign directory (randomizer off)");
	const Hello& H = b2_host_hello();
	const fs::path card0 = dir / "card" / "card0";
	const long long secs = unix_secs();
	const fs::path cardAside = dir / ("card-set-aside-" + std::to_string(secs));
	const fs::path ckptAside = dir / ("checkpoint-set-aside-" + std::to_string(secs));
	std::error_code ec;
	std::string err;
	std::vector<std::string> keep;
	for (const File& f : files)
		if (f.name.compare(0, 11, "card/card0/") == 0) keep.push_back(f.name.substr(11));
	// Collect first, then move: the folder is never changed while listed.
	std::vector<fs::path> extra;
	if (fs::is_directory(card0, ec)) {
		for (fs::directory_iterator it(card0, ec), end; !ec && it != end; it.increment(ec)) {
			std::error_code ec2;
			if (!it->is_regular_file(ec2)) continue;
			const std::string name = it->path().filename().string();
			if (std::find(keep.begin(), keep.end(), name) == keep.end()) extra.push_back(it->path());
		}
		if (ec) transfer_fail(kFieldCheckpoint, "cannot list " + card0.string() + ": " + ec.message());
	}
	for (const fs::path& p : extra) {
		if (!move_aside(p, cardAside, &err)) transfer_fail(kFieldCheckpoint, err);
		printf("[netplay] transfer: card file %s is not the host's; moved to %s\n", p.filename().string().c_str(),
		       cardAside.string().c_str());
	}
	size_t bytes = 0;
	for (const File& f : files) {
		const bool isCard = f.name.compare(0, 11, "card/card0/") == 0;
		const fs::path& aside = isCard ? cardAside : ckptAside;
		bool moved = false;
		if (!write_keeping_old(dir / fs::path(f.name), f.bytes, aside, dir, &moved, &err))
			transfer_fail(kFieldCheckpoint, err);
		if (moved)
			printf("[netplay] transfer: the previous %s had other bytes; moved to %s\n", f.name.c_str(),
			       aside.string().c_str());
		bytes += f.bytes.size();
	}
	// On disk now: exactly the host's card set and checkpoint.
	std::vector<File> card;
	if (!read_card_files(&card, &err)) transfer_fail(kFieldCheckpoint, "after writing the host's files: " + err);
	uint8_t cs[32];
	pc_netplay_xfer::card_digest(card, cs);
	if (memcmp(cs, H.cardSha, 32) != 0)
		transfer_fail(kFieldCheckpoint, "after writing the host's files the card folder digest is " + to_hex(cs, 32) +
		                                    ", not the host's " + to_hex(H.cardSha, 32));
	if (H.ckptGen != 0) {
		char name[32];
		snprintf(name, sizeof(name), "%020llu.sav", (unsigned long long)H.ckptGen);
		std::string sav;
		uint8_t ss[32];
		if (!read_small_file(dir / name, pc_netplay_xfer::kMaxBundleFileBytes, &sav))
			transfer_fail(kFieldCheckpoint, std::string("cannot read back ") + name);
		pc_netplay_sha::sha256(sav.data(), sav.size(), ss);
		if (memcmp(ss, H.ckptSha, 32) != 0)
			transfer_fail(kFieldCheckpoint, std::string(name) + " on disk does not match the host's digest");
	}
	printf("[netplay] transfer: on-disk check: card=%s (%llu files)%s\n", hex16(cs).c_str(),
	       (unsigned long long)card.size(), H.ckptGen != 0 ? ", checkpoint sha matches" : "");
	bool resumed = false;
	if (pc_randomizer_adopt_checkpoint != nullptr) resumed = pc_randomizer_adopt_checkpoint();
	printf("[netplay] checkpoint adopted gen=%llu sha=%s card=%s files=%llu (%llu B) resumed=%d in %.0f ms\n",
	       (unsigned long long)H.ckptGen, hex16(H.ckptSha).c_str(), hex16(H.cardSha).c_str(),
	       (unsigned long long)files.size(), (unsigned long long)bytes, (int)resumed, now_ms() - sXferStartMs);
	fflush(stdout);
	sXferCkptFiles = files.size();
}

// Joiner: writes the host's sidecar set into the working directory. Its own
// sidecar-named files that the host lacks, and same-named files with other
// bytes, are moved to sidecar-set-aside-<secs>/ (never deleted); fix round 1
// (C13) then re-reads the set from disk and requires the host's digest.
void joiner_write_sidecars(const std::vector<File>& files)
{
	const fs::path cwd = fs::current_path();
	const fs::path aside = cwd / ("sidecar-set-aside-" + std::to_string(unix_secs()));
	std::string err;
	std::vector<fs::path> extra;
	std::error_code ec;
	for (fs::directory_iterator it(cwd, ec), end; !ec && it != end; it.increment(ec)) {
		const std::string name = it->path().filename().string();
		std::error_code ec2;
		if (!pc_netplay_xfer::sidecar_name_ok(name) || !it->is_regular_file(ec2)) continue;
		bool hostHas = false;
		for (const File& f : files) hostHas = hostHas || f.name == name;
		if (!hostHas) extra.push_back(it->path());
	}
	if (ec) transfer_fail(kFieldSidecars, "cannot list " + cwd.string() + ": " + ec.message());
	for (const fs::path& p : extra) {
		if (!move_aside(p, aside, &err)) transfer_fail(kFieldSidecars, err);
		printf("[netplay] transfer: sidecar %s is not the host's; moved to %s\n", p.filename().string().c_str(),
		       aside.string().c_str());
	}
	size_t bytes = 0;
	for (const File& f : files) {
		bool moved = false;
		if (!write_keeping_old(cwd / f.name, f.bytes, aside, cwd, &moved, &err)) transfer_fail(kFieldSidecars, err);
		if (moved)
			printf("[netplay] transfer: the previous %s had other bytes; moved to %s\n", f.name.c_str(),
			       aside.string().c_str());
		bytes += f.bytes.size();
	}
	std::vector<File> side;
	if (!read_sidecar_set(cwd, &side, &err)) transfer_fail(kFieldSidecars, "after writing the host's sidecars: " + err);
	uint8_t ds[32];
	pc_netplay_xfer::sidecar_digest(side, ds);
	if (memcmp(ds, b2_host_hello().sidecarSha, 32) != 0)
		transfer_fail(kFieldSidecars, "after writing the host's sidecars the folder digest is " + to_hex(ds, 32));
	printf("[netplay] sidecars received: %llu files (%llu B) written to %s, sidecarSha=%s in %.0f ms\n",
	       (unsigned long long)files.size(), (unsigned long long)bytes, fs::current_path().string().c_str(),
	       to_hex(b2_host_hello().sidecarSha, 32).c_str(), now_ms() - sXferStartMs);
	fflush(stdout);
	sXferSideFiles = files.size();
}

void transfer_pump()
{
	transfer_handshake_pump();
	bulk_pump();
	const double elapsed = now_ms() - sXferStartMs;
	std::vector<uint8_t> d;
	std::string err;
	if (sCfg.isHost) {
		if (b2_take(kBulkTransferDone, &d)) {
			pc_netplay_xfer::TransferDone done;
			const Hello& H = sLocal;
			if (!pc_netplay_xfer::decode_transfer_done(d.data(), d.size(), &done))
				transfer_fail(sXferNeedCkpt ? kFieldCheckpoint : kFieldSidecars, "malformed transfer-done message");
			if (sXferNeedCkpt && (!(done.flags & pc_netplay_xfer::kDoneCheckpoint) || done.gen != H.ckptGen
			                      || memcmp(done.ckptSha, H.ckptSha, 32) != 0))
				transfer_fail(kFieldCheckpoint, "the joiner did not adopt this checkpoint");
			if (sXferNeedSidecars && (!(done.flags & pc_netplay_xfer::kDoneSidecars)
			                          || memcmp(done.sidecarSha, H.sidecarSha, 32) != 0))
				transfer_fail(kFieldSidecars, "the joiner did not write these sidecars");
			printf("[netplay] transfer: joiner done (checkpoint=%d sidecars=%d) in %.0f ms\n",
			       (int)((done.flags & pc_netplay_xfer::kDoneCheckpoint) != 0),
			       (int)((done.flags & pc_netplay_xfer::kDoneSidecars) != 0), elapsed);
			fflush(stdout);
			start_gekko_session();
			return;
		}
		if (elapsed > kXferTimeoutMs)
			transfer_fail(sXferNeedCkpt ? kFieldCheckpoint : kFieldSidecars,
			              "no transfer-done from the joiner within 60 s");
		return;
	}
	while (b2_take(kBulkCheckpoint, &d)) {
		pc_netplay_xfer::BundleMsg m;
		if (!pc_netplay_xfer::decode_bundle(pc_netplay_xfer::BundleKind::Checkpoint, d.data(), d.size(), &m, &err)
		    || !sXferCkpt.add(pc_netplay_xfer::BundleKind::Checkpoint, m, &err))
			transfer_fail(kFieldCheckpoint, "checkpoint bundle: " + err);
	}
	while (b2_take(kBulkSidecars, &d)) {
		pc_netplay_xfer::BundleMsg m;
		if (!pc_netplay_xfer::decode_bundle(pc_netplay_xfer::BundleKind::Sidecars, d.data(), d.size(), &m, &err)
		    || !sXferSide.add(pc_netplay_xfer::BundleKind::Sidecars, m, &err))
			transfer_fail(kFieldSidecars, "sidecar bundle: " + err);
	}
	if (sXferNeedCkpt && !sXferCkptDone && sXferCkpt.complete()) {
		const std::vector<File> files = sXferCkpt.files();
		if (!pc_netplay_xfer::verify_checkpoint_set(files, b2_host_hello(), &err)) transfer_fail(kFieldCheckpoint, err);
		joiner_adopt_checkpoint(files);
		sXferCkptDone = true;
	}
	if (sXferNeedSidecars && !sXferSideDone && sXferSide.complete()) {
		const std::vector<File> files = sXferSide.files();
		uint8_t digest[32];
		pc_netplay_xfer::sidecar_digest(files, digest);
		if (memcmp(digest, b2_host_hello().sidecarSha, 32) != 0)
			transfer_fail(kFieldSidecars, "received sidecars do not match the host's sidecarSha (got " +
			                                  to_hex(digest, 32) + ")");
		joiner_write_sidecars(files);
		sXferSideDone = true;
	}
	const bool ready = (!sXferNeedCkpt || sXferCkptDone) && (!sXferNeedSidecars || sXferSideDone);
	if (ready && !sXferDoneSent) {
		pc_netplay_xfer::TransferDone done;
		const Hello& H = b2_host_hello();
		if (sXferNeedCkpt) {
			done.flags |= pc_netplay_xfer::kDoneCheckpoint;
			done.gen = H.ckptGen;
			memcpy(done.ckptSha, H.ckptSha, 32);
		}
		if (sXferNeedSidecars) {
			done.flags |= pc_netplay_xfer::kDoneSidecars;
			memcpy(done.sidecarSha, H.sidecarSha, 32);
		}
		sB2Out.push_back(B2Msg{ kBulkTransferDone, pc_netplay_xfer::encode_transfer_done(done) });
		sXferDoneSent = true;
	}
	// Start only once the host holds our transfer-done (every B2 message we
	// queued is acknowledged), so the host starts its session too.
	if (sXferDoneSent && bulk_all_acked()) {
		printf("[netplay] transfer: done in %.0f ms; starting the session\n", elapsed);
		fflush(stdout);
		start_gekko_session();
		return;
	}
	if (elapsed > kXferTimeoutMs)
		transfer_fail(sXferNeedCkpt && !sXferCkptDone ? kFieldCheckpoint : kFieldSidecars,
		              "the host's files did not all arrive within 60 s");
}

double handshake_rtt_median()
{
	if (sHsSamples.empty()) return -1;
	std::vector<double> v = sHsSamples;
	std::sort(v.begin(), v.end());
	return v[v.size() / 2];
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
		printf("[netplay] handshake timeout after %.0f ms (sentAck=%d gotAck=%d rttSamples=%llu)\n",
		       now - sHsStartMs, (int)sSentAck, (int)sGotAck,
		       (unsigned long long)sHsSamples.size());
		fflush(stdout);
		// m12: join the ICE thread before exit.
		stop_session();
		std::exit(4);
	}
	if (now - sHsLastSendMs >= 100) {
		const uint64_t nonce = sHsNextNonce++;
		send_hello_msg(kHsHello, 0, nonce);
		sHsSendTimes.emplace_back(nonce, now);
		if (sHsSendTimes.size() > 64)
			sHsSendTimes.erase(sHsSendTimes.begin(),
			                   sHsSendTimes.begin() + (sHsSendTimes.size() - 64));
		// Polish: periodic Ack resends echo nonce 0 (ignored for RTT).
		// Real RTT samples come only from immediate Hello answers, which
		// echo the Hello nonce. Resends exist only so a lost Ack cannot
		// stall the peer.
		if (sSentAck) send_hello_msg(kHsAck, 0, 0);
		sHsLastSendMs = now;
	}
	std::vector<pc_netplay_transport::UdpSocket::Datagram> grams = hs_drain();
	for (auto& g : grams) {
		uint8_t hdrType = 0;
		uint16_t hdrProto = 0;
		if (!parse_hello_header(g.payload.data(), g.payload.size(), &hdrType, &hdrProto))
			continue; // wrong magic: ignore
		if (sCfg.isHost && !sHaveRemote) {
			// Learn the joiner's endpoint from its first header-valid
			// datagram (before any refusal, so a refuse can go back).
			// Over ICE this branch never runs (the agent is connected
			// 1:1, so sHaveRemote is preset before the pump starts).
			sRemoteIp   = g.fromIpHostOrder;
			sRemotePort = g.fromPort;
			sHaveRemote = true;
			sSock->set_peer(sRemoteIp, sRemotePort);
			printf("[netplay] handshake: peer %u.%u.%u.%u:%u\n", (sRemoteIp >> 24) & 0xFF,
			       (sRemoteIp >> 16) & 0xFF, (sRemoteIp >> 8) & 0xFF, sRemoteIp & 0xFF,
			       (unsigned)sRemotePort);
			fflush(stdout);
		}
		// Stable header prefix first: a protocol mismatch refuses fast
		// with code 4 even across length changes (v1 108 B vs v2 116 B),
		// instead of timing out after 30 s.
		if (hdrProto != local_protocol_version()) {
			// A refuse notice itself is honoured even cross-version when
			// its header parses; otherwise report the local mismatch.
			// The refuse field is fixed at offset 107 in every version
			// (v1 len 108, v2 len 116), so it reads the same either way.
			if (hdrType == kHsRefuse && g.payload.size() >= kHsHeaderLen + 101) {
				uint8_t rf = g.payload[107];
				printf("[netplay] handshake refused: %s\n",
				       field_name(rf == 0 ? 99 : rf));
				fflush(stdout);
				// m12: join the ICE thread before exit.
				stop_session();
				std::exit(4);
			}
			refuse_and_exit(kFieldProto);
		}
		uint8_t type   = 0;
		uint16_t proto = 0;
		Hello h;
		uint8_t refuse = 0;
		if (!parse_hello_msg(g.payload.data(), g.payload.size(), &type, &proto, &h, &refuse))
			continue; // same-proto length mismatch: ignore
		if (type == kHsRefuse) {
			printf("[netplay] handshake refused: %s\n",
			       field_name(refuse == 0 ? 99 : refuse));
			b2_refusal_view(refuse);
			fflush(stdout);
			print_refusal_hint(refuse);
			// m12: join the ICE thread before exit.
			stop_session();
			std::exit(4);
		}
		if (type == kHsHello || type == kHsAck) {
			// Compare in field order; the first mismatch refuses.
			// (Ack echoes the sender's own values, which matched ours
			// when it sent the ack, so re-checking is harmless. The nonce
			// is excluded: it differs per send by design.)
			if (proto != local_protocol_version()) refuse_and_exit(kFieldProto);
			if (memcmp(h.exe, sLocal.exe, 32) != 0) refuse_and_exit(kFieldExe);
			if (memcmp(h.cfg, sLocal.cfg, 32) != 0) refuse_and_exit(kFieldConfig);
			if (memcmp(h.boot, sLocal.boot, 32) != 0) refuse_and_exit(kFieldBootstrap);
			if (h.seed != sLocal.seed) refuse_and_exit(kFieldSeed);
			// M4 lane B2 (plan section 6b): both peers evaluate the same
			// decision table over (host Hello, joiner Hello).
			handshake_check_b2(h);
			if (type == kHsHello) {
				sHsLastHelloNonce = h.nonce;
				// Polish: ack EVERY Hello immediately, echoing its nonce.
				// The old once-only Ack biased the RTT upward by up to
				// ~100 ms (later samples measured the peer's resend phase,
				// not the wire), picking 1-2 frames too many.
				send_hello_msg(kHsAck, 0, h.nonce);
				sSentAck = true;
			}
			if (type == kHsAck) {
				sGotAck = true;
				// Nonce-matched RTT sample; nonce 0 is the periodic
				// resend and is ignored for RTT.
				if (h.nonce == 0) continue;
				for (auto& st : sHsSendTimes) {
					if (st.first == h.nonce) {
						const double sample = now - st.second;
						if (sample >= 0 && sample < 60000)
							sHsSamples.push_back(sample);
						break;
					}
				}
			}
		}
	}
	// Fix round 3: with DELAY=auto the session starts only after at least 5
	// RTT samples, so the median is meaningful. The 100 ms resend gives ~5
	// Hellos in 500 ms; loss only delays this, it cannot deadlock it
	// (handshake timeout still applies).
	if (sCfg.delayAuto && sSentAck && sGotAck && sHsSamples.size() < 5) return false;
	return sSentAck && sGotAck;
}

// M1: answer late/duplicate handshake traffic once the GekkoNet session is
// up. A peer that got our Hello+Ack in one drain sends its single Ack and
// moves on; if that Ack is lost, the other peer keeps sending Hello until
// its 30 s timeout while this peer sits in a session with no remote (the
// disconnect timeout only applies after a connection exists). Draining here
// and re-acking keeps one lost datagram from hanging the session.
// n1 (fix round 2): answer only Hellos. Answering an incoming Ack with an
// Ack made Acks bounce between the peers for the whole session (each side's
// final Hello+Ack burst seeded about two such loops of 116-byte datagrams).
// A lost Ack still recovers: the peer still in handshake re-sends Hello
// every 100 ms, and every Hello here gets an Ack.
// Fix round 3 item 3: each in-session answer is logged (the Ack-drop test
// greps for it to prove the M1 path ran).
void answer_handshake_in_session()
{
	if (sIceLink == nullptr && (sLink == nullptr || sSock == nullptr)) return;
	std::vector<pc_netplay_transport::UdpSocket::Datagram> grams = hs_drain();
	for (auto& g : grams) {
		uint8_t hdrType = 0;
		uint16_t hdrProto = 0;
		if (!parse_hello_header(g.payload.data(), g.payload.size(), &hdrType, &hdrProto))
			continue;
		// Session already agreed: ignore cross-version strays, never refuse.
		if (hdrProto != local_protocol_version()) continue;
		// M5c lane B: the peer's stall advice (adaptive delay).
		if (hdrType == kHsAdvice) {
			adaptive_on_advice(g.payload.data() + kHsHeaderLen, g.payload.size() - kHsHeaderLen);
			continue;
		}
		uint8_t type   = 0;
		uint16_t proto = 0;
		Hello h;
		uint8_t refuse = 0;
		if (!parse_hello_msg(g.payload.data(), g.payload.size(), &type, &proto, &h, &refuse))
			continue;
		if (type == kHsRefuse) continue; // session already agreed; ignore
		if (type != kHsHello) continue;  // n1: never answer an Ack with an Ack
		// Only answer the known peer (host learns it during the handshake;
		// the joiner always talks to its configured host). Over ICE the
		// sender fields are the fixed 127.0.0.1:1 placeholder, which equals
		// the preset remote, so this filter already passes there.
		if (sCfg.isHost && sHaveRemote
		    && (g.fromIpHostOrder != sRemoteIp || g.fromPort != sRemotePort))
			continue;
		send_hello_msg(kHsAck, 0, h.nonce);
		printf("[netplay] answered in-session Hello with Ack (echo=%llu)\n",
		       (unsigned long long)h.nonce);
		fflush(stdout);
	}
}

// ---- M4 gap-fix lane S: load guard (issue #885) ----
// Design and GekkoNet evidence: pc_netplay_loadguard.h. Switches, read once
// at session start (netplay sessions only):
//   PIKMIN_NETPLAY_LOAD_GUARD=0        (a) and (b) off, and the day-end save
//                                      barrier back to B2's fixed 10 s: the
//                                      pre-fix behaviour, for the runs that
//                                      show the test bites
//   PIKMIN_NETPLAY_LOAD_KEEPALIVE=0    (a) off (the keep-alive pump)
//   PIKMIN_NETPLAY_LOAD_WINDOW=0       (b) off (the load-window extension)
//   PIKMIN_NETPLAY_LOAD_DISCONNECT_MS  timeout inside a load window (default
//                                      60000; never below the normal one), and
//                                      the save barrier's wait for a peer that
//                                      is still connected (never below 10 s)
// plus the PIKMIN_NETPLAY_TEST_STALL_* injector (pc_netplay_loadguard.h).
void loadguard_configure(unsigned normalMs)
{
	using namespace pc_netplay_loadguard;
	const bool guard  = read_unsigned_env("PIKMIN_NETPLAY_LOAD_GUARD", 1) != 0;
	sLgGuard          = guard;
	sLgKeepAlive      = guard && read_unsigned_env("PIKMIN_NETPLAY_LOAD_KEEPALIVE", 1) != 0;
	const bool window = guard && read_unsigned_env("PIKMIN_NETPLAY_LOAD_WINDOW", 1) != 0;
	sLgWindow.configure(window, normalMs,
	                    read_unsigned_env("PIKMIN_NETPLAY_LOAD_DISCONNECT_MS", kDefaultLoadDisconnectMs));
	sLgGate.reset();
	sLgStallDone = false;
	sLgStageLoads = 0;
	sLgPumps = sLgWindows = sLgLongTicks = 0;
	for (uint64_t& n : sLgSitePumps) n = 0;
	sLgSpeculative = false;
	sLgLongestTickMs = sLgWorstGapMs = 0;
	sLgSummaryDone = false;
	sInAdvance = false;
	sLgMainThread = std::this_thread::get_id(); // the session driver's thread
	std::string err;
	if (!parse_stall_plan(getenv_nonempty("PIKMIN_NETPLAY_TEST_STALL_MS"), getenv_nonempty("PIKMIN_NETPLAY_TEST_STALL_AT"),
	                      getenv_nonempty("PIKMIN_NETPLAY_TEST_STALL_ROLE"),
	                      getenv_nonempty("PIKMIN_NETPLAY_TEST_STALL_SLICE_MS"), &sLgStall, &err))
		printf("[netplay] test stall: ignored (%s)\n", err.c_str());
	sLgConfigured = true;
	const char* ka = sLgKeepAlive ? "on (network poll at most every 50 ms inside a tick)" : "off";
	if (window)
		printf("[netplay] load guard: keep-alive=%s load window=on (disconnect timeout %u ms from a stage load until "
		       "%u frames later, %u ms otherwise)\n",
		       ka, sLgWindow.load_ms(), (unsigned)kLoadWindowFrames, normalMs);
	else
		printf("[netplay] load guard: keep-alive=%s load window=off (disconnect timeout %u ms throughout)\n", ka,
		       normalMs);
	printf("[netplay] load guard: day-end save barrier waits up to %u ms for a connected peer%s\n",
	       barrier_deadline_ms(guard, sLgWindow.load_ms()),
	       guard ? " (a peer GekkoNet reports disconnected ends it earlier)" : " (B2 fixed wait)");
	if (sLgStall.enabled) {
		const char* at = sLgStall.at == StallAt::Load ? "load" : sLgStall.at == StallAt::Shader ? "shader" : "tick";
		printf("[netplay] test stall: armed at=%s#%llu role=%s total=%ums slice=%ums; this peer %s\n", at,
		       (unsigned long long)sLgStall.index, stall_role_name(sLgStall.roles), sLgStall.totalMs,
		       sLgStall.sliceMs, stall_applies_to(sLgStall, sCfg.isHost) ? "stalls" : "does not stall");
	}
	fflush(stdout);
}

// One keep-alive network pump inside a tick: GekkoNet's network layer only
// (receive, ack, resend unacked inputs, NetworkHealth, idle-timer check; no
// Advance, no game or session event consumed), as the B2 save barrier does
// inside its save tick. The bulk channel is left to the next loop turn: it
// has no idle timer (it resends until acked) and its consumers run between
// ticks. A disconnect found here is queued as a session event and handled
// right after this tick (handle_session_events).
void loadguard_pump(double now, int site)
{
	if (sGekko != nullptr) gekko_network_poll(sGekko);
	if (site >= 0 && site < pc_netplay_loadguard::kSiteCount) ++sLgSitePumps[site];
	const double gap = now - sLgLastPumpMs;
	if (gap > sLgTickMaxGapMs) sLgTickMaxGapMs = gap;
	sLgLastPumpMs = now;
	sLgGate.pumped(now);
	++sLgTickPumps;
	++sLgPumps;
}

// A network poll made inside a tick by other code (the B2 save barrier's
// wait loop): counted as a pump for the long-tick figures and the gate.
void loadguard_note_external_poll()
{
	if (!sInAdvance) return;
	const double now = now_ms();
	const double gap = now - sLgLastPumpMs;
	if (gap > sLgTickMaxGapMs) sLgTickMaxGapMs = gap;
	sLgLastPumpMs = now;
	sLgGate.pumped(now);
}

// Keep-alive entry (the public pc_netplay_load_keepalive and the stall
// slices). Inert outside an Advance tick, and with (a) switched off.
void loadguard_keepalive(int site)
{
	if (!sInAdvance || !sLgKeepAlive || sPhase != kSession || sGekko == nullptr) return;
	if (std::this_thread::get_id() != sLgMainThread) return; // never from another thread
	const double now = now_ms();
	if (!sLgGate.due(now)) return;
	loadguard_pump(now, site);
}

// Test stall (wall clock only): slices separated by the keep-alive entry the
// real long loops call.
void loadguard_run_stall(const char* where)
{
	using namespace pc_netplay_loadguard;
	sLgStallDone = true;
	printf("[netplay] test stall: begin role=%s at=%s frame=%u total=%ums slice=%ums keep-alive=%s\n",
	       sCfg.isHost ? "host" : "join", where, sLgTickFrame, sLgStall.totalMs, sLgStall.sliceMs,
	       sLgKeepAlive ? "on" : "off");
	fflush(stdout);
	const double t0 = now_ms();
	for (unsigned i = 0;; ++i) {
		const unsigned ms = stall_slice_ms(sLgStall, i);
		if (ms == 0) break;
		if (i > 0) loadguard_keepalive(kSiteStall);
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
	printf("[netplay] test stall: end after %.0f ms\n", now_ms() - t0);
	fflush(stdout);
}

// Called with the stall site; runs the stall once when the plan targets it.
void loadguard_maybe_stall(pc_netplay_loadguard::StallAt at, uint64_t index, const char* where)
{
	if (sLgStallDone || !sInAdvance || sLgSpeculative || !stall_applies_to(sLgStall, sCfg.isHost)) return;
	if (sLgStall.at != at || sLgStall.index != index) return;
	loadguard_run_stall(where);
}

// Around app->idle() inside an Advance. `speculative`: a rolling-back or
// running-ahead re-run (never in lockstep), which keeps the keep-alive but
// neither opens, extends nor closes a window nor fires a test stall (MN7).
void loadguard_tick_begin(uint32_t frame, bool speculative)
{
	sLgSpeculative  = speculative;
	sInAdvance      = true;
	sLgTickFrame    = frame;
	sLgTickStartMs  = now_ms();
	sLgLastPumpMs   = sLgTickStartMs;
	sLgTickMaxGapMs = 0;
	sLgTickPumps    = 0;
	// tick:<n> is the hash tick (GekkoNet frame F is hash tick F+1).
	loadguard_maybe_stall(pc_netplay_loadguard::StallAt::Tick, (uint64_t)frame + 1, "tick");
}

void loadguard_tick_end()
{
	sInAdvance       = false;
	const double now = now_ms();
	const double gap = now - sLgLastPumpMs;
	if (gap > sLgTickMaxGapMs) sLgTickMaxGapMs = gap;
	const double tickMs = now - sLgTickStartMs;
	if (tickMs > sLgLongestTickMs) sLgLongestTickMs = tickMs;
	if (tickMs > pc_netplay_loadguard::kLongTickMs) {
		++sLgLongTicks;
		if (sLgTickMaxGapMs > sLgWorstGapMs) sLgWorstGapMs = sLgTickMaxGapMs;
		printf("[netplay] long tick: frame=%u took %.0f ms; keep-alive pumps=%llu, longest stretch without a network "
		       "poll %.0f ms (load window %s)\n",
		       sLgTickFrame, tickMs, (unsigned long long)sLgTickPumps, sLgTickMaxGapMs,
		       sLgWindow.is_open() ? "open" : "closed");
		fflush(stdout);
	}
}

// After each completed Advance: close the load window once frame F+30 ran
// (first executions only: a resimulated frame never closes it).
void loadguard_after_advance(uint32_t frame, bool speculative)
{
	if (speculative || !sLgWindow.after_advance(frame)) return;
	if (sGekko != nullptr) gekko_set_disconnect_timeout(sGekko, sLgWindow.normal_ms());
	printf("[netplay] load window: closed at frame=%u (opened at frame=%u, last stage load at frame=%u, %.0f ms "
	       "open); disconnect timeout %u ms\n",
	       frame, sLgWindowFirst, sLgWindow.open_frame(), now_ms() - sLgWindowOpenMs, sLgWindow.normal_ms());
	fflush(stdout);
}

// Stage load inside a session tick (GameFlow::softReset -> the N3 hook).
void loadguard_on_stage_load()
{
	if (!sInAdvance || sPhase != kSession || sGekko == nullptr) return;
	if (sLgSpeculative) {
		// A resimulated stage load (rollback only): keep talking, but the
		// window, the load count and load:<n> stay keyed to first executions.
		loadguard_keepalive(pc_netplay_loadguard::kSiteStageLoad);
		return;
	}
	++sLgStageLoads;
	const bool wasOpen = sLgWindow.is_open();
	if (sLgWindow.open(sLgTickFrame)) {
		gekko_set_disconnect_timeout(sGekko, sLgWindow.load_ms());
		sLgWindowOpenMs = now_ms();
		sLgWindowFirst  = sLgTickFrame;
		++sLgWindows;
		printf("[netplay] load window: open at frame=%u (stage load %u); disconnect timeout %u ms until frame=%u\n",
		       sLgTickFrame, sLgStageLoads, sLgWindow.load_ms(), sLgWindow.close_frame());
		fflush(stdout);
	} else if (wasOpen) {
		printf("[netplay] load window: stage load %u at frame=%u extends it to frame=%u\n", sLgStageLoads,
		       sLgTickFrame, sLgWindow.close_frame());
		fflush(stdout);
	}
	loadguard_keepalive(pc_netplay_loadguard::kSiteStageLoad);
	loadguard_maybe_stall(pc_netplay_loadguard::StallAt::Load, sLgStageLoads, "load");
}

// Session totals, once (stop_session runs on every exit path).
void loadguard_summary()
{
	if (!sLgConfigured || sLgSummaryDone) return;
	sLgSummaryDone = true;
	using pc_netplay_loadguard::site_name;
	printf("[netplay] load guard summary: keep-alive pumps=%llu (%s=%llu %s=%llu %s=%llu %s=%llu %s=%llu) load "
	       "windows=%llu long ticks=%llu longest tick=%.0f ms longest stretch without a network poll in a long "
	       "tick=%.0f ms\n",
	       (unsigned long long)sLgPumps, site_name(1), (unsigned long long)sLgSitePumps[1], site_name(2),
	       (unsigned long long)sLgSitePumps[2], site_name(3), (unsigned long long)sLgSitePumps[3], site_name(4),
	       (unsigned long long)sLgSitePumps[4], site_name(5), (unsigned long long)sLgSitePumps[5],
	       (unsigned long long)sLgWindows, (unsigned long long)sLgLongTicks, sLgLongestTickMs, sLgWorstGapMs);
	fflush(stdout);
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
	// M5a: the base adapter is whichever transport is up (UDP or ICE); the
	// lossy test wrapper and GekkoNet see no difference.
	GekkoNetAdapter* baseAdapter = (sIceLink != nullptr) ? sIceLink->adapter() : sLink->adapter();
	if (lossy) {
		sLossy   = new pc_netplay_transport::LossyLink(baseAdapter, lp);
		sAdapter = sLossy->adapter();
		printf("[netplay] lossy adapter: latency=%.1fms jitter=%.1fms loss=%.1f%% seed=%u\n",
		       lp.latencyMs, lp.jitterMs, lp.lossPct, lp.seed);
	} else {
		sAdapter = baseAdapter;
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
	// N3: sessions must survive synchronous stage loads inside app->idle(),
	// which block the loop turn's GekkoNet pumping. The load is synchronous
	// and deterministic (both peers load the same stage at the same tick), so
	// no input exchange is needed mid-load; the only failure is the idle
	// timeout firing. The normal timeout is PIKMIN_NETPLAY_DISCONNECT_MS
	// (default 15000). M4 gap-fix lane S (issue #885): a 15 s timeout alone
	// failed on a ~16 s TEV specialisation under load (integ-m4 section 3.4),
	// so the load guard (loadguard_configure above; pc_netplay_loadguard.h)
	// adds (a) a main-thread keep-alive network poll that the long loops call
	// between their steps (same thread, no Advance, so no second thread and
	// no load refactor), and (b) a deterministic load-window extension of this
	// timeout from each stage load to 30 frames later.
	unsigned disconnectMs = read_unsigned_env("PIKMIN_NETPLAY_DISCONNECT_MS", 15000);
	if (disconnectMs < 1000) disconnectMs = 1000;
	gekko_set_disconnect_timeout(sGekko, disconnectMs);
	printf("[netplay] disconnect timeout: %ums\n", disconnectMs);
	loadguard_configure(disconnectMs);
	pc_state_hash_set_netplay_capture(true);
	load_scripted_file();
	// Polish DELAY=auto: nonce-matched median RTT (at least 5 samples; the
	// handshake gate above guarantees it). Full speed needs delay >=
	// ceil(one-way latency / (1000/30) ms), plus one frame of margin (one
	// local submit per Advance). Clamped to 1..8. The delay is not part of
	// the config hash, so asymmetric links pick per-peer values without
	// refusing. Formula: ceil((RTT/2)/33.333 - 0.05)+1: the 0.05-slot
	// tolerance keeps the exact-boundary rows (100/200 ms one-way sit
	// exactly on ceil steps) from flipping one frame high on a few ms of
	// measurement overhead. It never lowers a pick by more than the margin
	// the +1 already adds.
	if (sCfg.delayAuto) {
		unsigned autoDelay = 2;
		double oneWayMs    = 0;
		double rttMed      = handshake_rtt_median();
		sHsRttMs           = rttMed;
		if (rttMed >= 0) {
			oneWayMs = rttMed / 2.0;
			const double slots = oneWayMs / (1000.0 / 30.0);
			double adj         = slots - 0.05;
			if (adj < 0) adj = 0;
			unsigned d = (unsigned)adj + 1; // ceil(adj) + 1 margin
			if ((double)(unsigned)adj < adj) ++d; // exact ceil, no <cmath>
			if (d < 1) d = 1;
			if (d > 8) d = 8;
			autoDelay = d;
		}
		sCfg.localDelay = autoDelay;
		gekko_set_local_delay(sGekko, sLocalHandle, (unsigned char)sCfg.localDelay);
		printf("[netplay] auto delay: rtt=%.1fms (median of %llu) one-way=%.1fms delay=%u\n",
		       rttMed, (unsigned long long)sHsSamples.size(), oneWayMs, autoDelay);
		// Per-sample evidence (review B1): the sorted RTT samples behind
		// the median, so quantization or impairment bias stays visible.
		{
			std::vector<double> sorted = sHsSamples;
			std::sort(sorted.begin(), sorted.end());
			std::string line = "[netplay] auto delay samples:";
			char cell[32];
			for (size_t i = 0; i < sorted.size() && i < 16; ++i) {
				snprintf(cell, sizeof(cell), " %.1f", sorted[i]);
				line += cell;
			}
			printf("%s\n", line.c_str());
		}
	}
	printf("[netplay] session started: role=%s localHandle=%d delay=%u seed=%u\n",
	       sCfg.isHost ? "host/P1" : "joiner/P2", sLocalHandle, sCfg.localDelay, sCfg.seed);
	// M5c lane B: the delay is final here (numeric or DELAY=auto); the submit
	// gate starts at it and the stats / controller start fresh.
	adaptive_configure();
	printf("[netplay] exe=%s\n", sExeHexStr.c_str());
	printf("[netplay] config=%s\n", sCfgHexStr.c_str());
	printf("[netplay] bootstrap=%s\n", sBootHexStr.c_str());
	fflush(stdout);
	// M4 fix round 1: fresh session state. The receiver starts empty (the
	// boot-published host snapshot in sRandWire is kept: it rides the first
	// submits). The bulk endpoint resets so a previous session's msgIds and
	// partials cannot collide with this one's. The host then forces a
	// publish (M5): whatever the stamp says, gen 1 is queued before the
	// first submit.
	sRandReasm.reset();
	// M4 lane B2: sBulk is reset once per session at handshake success
	// (on_handshake_done), before the transfer phase, not here: msgIds must
	// not restart between the transfer and the session.
	hold_reset();
	bool hostState = true;
	if (pc_randomizer_force_net_publish != nullptr) hostState = pc_randomizer_force_net_publish();
	// M4 lane B1 (lane A recheck item 3): a host whose state.txt cannot be
	// read at session start HOLDs from its first input instead of running
	// the neutral gate indefinitely without a word. The RESUME snapshot is
	// then gen 1 and the neutral gate stays up until it applies.
	if (!hostState && randstate_stream_on() && sCfg.isHost && !sCfg.launcherMode
	    && pc_randomizer_enabled != nullptr && pc_randomizer_enabled()) {
		printf("[netplay] hold: host state.txt missing at session start\n");
		fflush(stdout);
		sHoldAtFirstInput = true;
	}
	sSessionStartMs = now_ms();
	// Polish pacing: the drift-free deadline starts here (not at handshake
	// start), so the first session turn never takes the overrun path.
	sNextTurnMs = sSessionStartMs + 1000.0 / 30.0;
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
			uint64_t total = 0, subs[7] = { 0, 0, 0, 0, 0, 0, 0 }, tick = 0;
			bool ringHit = false;
			if (e != nullptr) {
				total   = e->total;
				tick    = e->tick;
				ringHit = true;
				for (int k = 0; k < 7; ++k) subs[k] = e->subs[k];
			} else {
				pc_state_hash_current(&total, subs, &tick);
			}
			printf("[netplay] desync detected: frame=%d local=%08x remote=%08x handle=%d\n",
			       ev[i]->data.desynced.frame, ev[i]->data.desynced.local_checksum,
			       ev[i]->data.desynced.remote_checksum, ev[i]->data.desynced.remote_handle);
			printf("[netplay] desync subs at tick=%llu%s: total=%016llx (fold %08x) "
			       "navi=%016llx piki=%016llx teki=%016llx item=%016llx world=%016llx "
			       "rng=%016llx rand=%016llx\n",
			       (unsigned long long)tick, ringHit ? "" : " (ring miss: latest)",
			       (unsigned long long)total, fold_hash64(total),
			       (unsigned long long)subs[0], (unsigned long long)subs[1],
			       (unsigned long long)subs[2], (unsigned long long)subs[3],
			       (unsigned long long)subs[4], (unsigned long long)subs[5],
			       (unsigned long long)subs[6]);
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
			// M4a: same-tick apply pair. A snapshot that completed in
			// frame F arms frame F+1; apply it now, before the sim runs,
			// then feed this frame's host fragment (arming F+1 at the
			// earliest). Both peers execute the identical sequence.
			sCurAdvanceFrame = (uint32_t)e->data.adv.frame; // B1: outbox entry frame
			sCurAdvanceStartMs = now_ms();                  // B2 fix round 1 (C15): barrier I/O log
			const uint64_t holdsBefore = sHoldsDone;        // M5c lane B: frame-time excludes a frozen hold
			randstate_apply_before_tick(e->data.adv.frame);
			// B1: HOLD flag in the host input (after the RESUME apply above).
			hold_on_advance_begin(p0, e->data.adv.frame);
			randstate_feed_advance(p0, e->data.adv.frame);
			if (randstate_gate_neutral()) {
				// M5: pre-snapshot neutral ticks (identical on both peers).
				inject_neutral_pad(0);
				inject_neutral_pad(1);
			} else {
				inject_input(0, p0);
				inject_input(1, p1);
			}
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
			loadguard_tick_begin((uint32_t)e->data.adv.frame, // lane S: keep-alive may pump inside
			                     e->data.adv.rolling_back || e->data.adv.running_ahead);
			app->idle();
			loadguard_tick_end();
			adaptive_note_tick(now_ms() - sLgTickStartMs); // M5c lane B: slow-tick trace
			pc_netplay_det_profile_note_tick();
			pc_input_log_tick_end();
			pc_state_hash_tick_end();
			{
				uint64_t total = 0, subs[7] = { 0, 0, 0, 0, 0, 0, 0 }, tick = 0;
				if (pc_state_hash_current(&total, subs, &tick) && tick > 0)
					hash_ring_store(tick, total, subs);
			}
			// M4 lane B1: confirmed-frame outbox flush, once per Advance,
			// after the state hash and before the exit-after check so the
			// last tick's entries land before exit. Host: journals; client:
			// mirror-events.txt. No-op outside outbox mode.
			if (pc_randomizer_outbox_flush != nullptr)
				pc_randomizer_outbox_flush((uint32_t)e->data.adv.frame);
			sLastAdvanceFrame = (uint32_t)e->data.adv.frame;
			hold_after_advance(e->data.adv.frame);
			loadguard_after_advance((uint32_t)e->data.adv.frame,
			                        e->data.adv.rolling_back || e->data.adv.running_ahead);
			++sSessionTicks;
			++advances;
			++sAdvances;
			adaptive_note_frame(sHoldsDone != holdsBefore); // M5c lane B: presented-frame time
			if (sCfg.exitAfter > 0 && sSessionTicks >= sCfg.exitAfter) {
				const double nowW = now_ms();
				// B1: frozen HOLD time is excluded from the stall %, tps and
				// slot-loss figures and reported separately as held=.
				double wallS =
				    (sSessionStartMs > 0) ? (nowW - sSessionStartMs - sHoldMs) / 1000.0 : 0.0;
				if (wallS < 0) wallS = 0;
				const double tps = wallS > 0 ? (double)sAdvances / wallS : 0.0;
				const double stallPct =
				    wallS > 0 ? 100.0 * sStallMs / (wallS * 1000.0) : 0.0;
				// n2: effective tps plus the fraction of 30 Hz slots lost,
				// so throttled runs cannot hide behind a sleep-free stall %.
				const double slotLossPct =
				    tps >= 30.0 ? 0.0 : 100.0 * (1.0 - tps / 30.0);
				printf("[netplay] exit after %llu ticks\n", (unsigned long long)sSessionTicks);
				printf("[netplay] script records consumed: %llu/%llu\n",
				       (unsigned long long)sScriptIdx, (unsigned long long)sScriptTicks);
				printf("[netplay] wall=%.1fs tps=%.1f stall=%.1f%% (wall-clock no-Advance turns; slot wait excluded) slot-loss=%.1f%% vs 30Hz held=%.0fms holds=%llu\n",
				       wallS, tps, stallPct, slotLossPct, sHoldMs, (unsigned long long)sHoldsDone);
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
			uint64_t total = 0, subs[7] = { 0, 0, 0, 0, 0, 0, 0 }, tick = 0;
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
		// n2: effective tps is Advances over SESSION wall time (the old
		// code divided by the handshake-start clock, deflating tps), and
		// the slot-loss fraction 1 - tps/30 is reported alongside the
		// wall-clock stall % (which now includes the pacing sleep).
		double wallS =
		    (sSessionStartMs > 0) ? (now - sSessionStartMs - sHoldMs) / 1000.0 : 0.0; // B1: hold excluded
		if (wallS < 0) wallS = 0;
		const double tps = wallS > 0 ? (double)sAdvances / wallS : 0.0;
		const double wallStallPct =
		    wallS > 0 ? 100.0 * sStallMs / (wallS * 1000.0) : 0.0;
		const double slotLossPct = tps >= 30.0 ? 0.0 : 100.0 * (1.0 - tps / 30.0);
		const double turnStallPct =
		    (sAdvances + sStalls) > 0 ? 100.0 * (double)sStalls / (double)(sAdvances + sStalls) : 0.0;
		printf("[netplay] tick=%llu adv=%llu stalls=%llu (turn %.1f%%, wall %.1f%%) "
		       "tps=%.1f (slot loss %.1f%% vs 30Hz) ahead=%.2f saves=%llu submitted=%llu held=%.0fms\n",
		       (unsigned long long)sSessionTicks, (unsigned long long)sAdvances,
		       (unsigned long long)sStalls, turnStallPct, wallStallPct, tps, slotLossPct,
		       gekko_frames_ahead(sGekko), (unsigned long long)sSaves,
		       (unsigned long long)sSubmitted, sHoldMs);
		fflush(stdout);
		adaptive_stats_line("stats"); // M5c lane B
	}
	return advances;
}

} // namespace

// ---- Netplay M4 lane A public hooks (issue #885) ----
// Read by pc_randomizer.cpp through weak references (null-checked there),
// so the default build and engine-free harnesses are unaffected.
bool pc_netplay_randstate_stream_enabled(void) { return sCfg.active && sRandStream; }
bool pc_netplay_is_host(void) { return sCfg.isHost; }

// M5c lane B (issue #887): live figures for a HUD. Wall-clock state only.
bool pc_netplay_live_stats(PcNetplayLiveStats* out)
{
	if (out == nullptr || sPhase != kSession || !sAdaptiveConfigured) return false;
	uint64_t n10 = 0;
	double ms10 = 0;
	const double now = now_ms();
	sStats.recent(now, 10000.0, &n10, &ms10);
	out->delay = sCfg.localDelay;
	out->adaptive = sAdaptive ? 1 : 0;
	out->rttLastMs = (float)sStats.rtt_last();
	out->rttP50Ms = (float)sStats.rtt_percentile(50);
	out->jitterMs = (float)sStats.rtt_jitter();
	out->stallsLast10s = (unsigned)n10;
	out->stallMsLast10s = (float)ms10;
	out->stallCount = (unsigned long long)sStats.stall_count();
	out->stallTotalMs = (float)sStats.stall_total_ms();
	out->stallOpen = sStallOpen ? 1 : 0;
	return true;
}
// Host I/O side publish: encode the snapshot and queue its 16 fragments for
// the next host submits. A publish that lands mid-transfer waits in a
// one-deep queue (M1 fix) instead of restarting the cursor; the receiver
// resets on fragment 0, so both peers stay identical (see Reassembler).
void pc_netplay_randstate_publish(const pc_randstate::PcRandState& st)
{
	if (!sCfg.isHost) return; // only the host publishes
	if (sRandHaveSnapshot && sRandNextFrag > 0 && sRandNextFrag < pc_randstate::kFragCount) {
		pc_randstate::encode(st, sRandQueuedWire);
		sRandHaveQueued = true;
		// B1 (lane A recheck item 1): make the one-deep queue visible, so a
		// pair can prove the queued generation still applies on both peers.
		printf("[netplay] randstate gen=%u queued behind in-flight fragment %u/%u\n", st.gen,
		       (unsigned)sRandNextFrag, (unsigned)pc_randstate::kFragCount);
		fflush(stdout);
		return;
	}
	pc_randstate::encode(st, sRandWire);
	sRandHaveSnapshot = true;
	sRandNextFrag = 0;
	// B1 (lane A recheck item 2): a direct write supersedes anything still
	// queued, which is older; without this the stale queued snapshot went
	// out after this one and cost 16 submits for a receiver-side no-op.
	sRandHaveQueued = false;
	printf("[netplay] randstate gen=%u published (host)\n", st.gen);
	fflush(stdout);
}

// ---- Netplay M4 lane B1 public hooks (issue #885) ----
// True while a synchronized HOLD is requested (host) or in progress: the
// host I/O side then leaves publishing to the RESUME snapshot.
bool pc_netplay_hold_active(void) { return sCfg.active && (sHolding || sHoldRequested); }
// Fix round 1 (R9): the frame of the Advance being executed, stamped on each
// outbox entry at push time so its mirror line carries the event's frame
// whichever flush writes it.
uint32_t pc_netplay_current_frame(void) { return sCurAdvanceFrame; }
// Host: queue one kBulkMirrorLedger payload (sent by bulk_pump as the 4-deep
// bulk queue frees). Bounded; the mirror is never sim state, so an overflow
// is logged and dropped rather than fatal.
void pc_netplay_mirror_ledger_send(const uint8_t* data, size_t len)
{
	if (!sCfg.active || !sCfg.isHost || data == nullptr || len == 0) return;
	if (sLedgerOut.size() >= kLedgerOutMax) {
		printf("[netplay] mirror ledger: send queue full; message dropped\n");
		fflush(stdout);
		return;
	}
	sLedgerOut.emplace_back(data, data + len);
	++sLedgerQueued; // B2 fix round 1 (X6): carried in the host's SaveResult
}

// ---- Netplay M4 lane B2: day-end SAVE_RESULT barrier (issue #885) ----
// Called by pc_randomizer_save_campaign_netplay inside the day-end save tick
// (one Advance, the same frame on both peers), after this peer wrote (or
// failed to write) its checkpoint. The host sends kBulkSaveResult, the client
// kBulkSaveAck, both {u32 frame, u8 ok, u64 gen, savSha[32], cardSha[32],
// u32 ledgerCount} (cardSha: SHA-256 of the 0x8000 game-file block just
// written; ledgerCount: the host's kBulkMirrorLedger messages queued this
// session, 0 from the client). Each side then waits, 1 ms between polls,
// until it holds the peer's message for this frame (the client also until it
// applied that many ledger messages, fix round 1 X6), like a synchronous
// stage load blocks (the N3 note in start_gekko_session). No Advance can
// happen inside a tick; B2 fix round 1 (X5/C15) still polls GekkoNet's
// network layer (gekko_network_poll: receive, ack, resend the unacked
// inputs, health; no Advance, no events consumed), so a lost input datagram
// is resent and neither peer's 15 s GekkoNet disconnect runs while both are
// in the barrier. The agreed outcome is the host's ok. Both roles apply the
// same checks to the same pair of results (pc_netplay_xfer::barrier_verdict,
// fix round 1 C1/C6):
//   * no peer message (or ledger) within 10 s of entering the barrier:
//     `[netplay] save barrier timeout`, exit 6 (today's abandoned day);
//     M4 gap-fix lane S fix round 1 (MJ1): with the load guard on (the
//     default), the peer may still be in a long tick of its own up to `delay`
//     frames back (a TEV burst at sunset, a cold disk, AV scanning its save),
//     kept alive by the keep-alive poll, so a fixed 10 s was the tightest
//     liveness timer in the session. The barrier now lets GekkoNet decide
//     peer death: it abandons the day (exit 6, the same retraction) as soon
//     as GekkoNet reports the peer disconnected (its idle timer: 15 s, or the
//     load timeout inside a load window), and otherwise waits up to
//     pc_netplay_loadguard::barrier_deadline_ms (the load timeout, 60 s by
//     default), which only ends the wait for a peer that keeps polling but
//     never arrives. PIKMIN_NETPLAY_LOAD_GUARD=0 keeps the fixed 10 s;
//   * a different frame, generation or game-file block, or both checkpoints
//     written with different digests: a desync, exit 5.
// Before either exit the client retracts the checkpoint it wrote for this
// save (fix round 1 C2), and a desync first lingers (<= 2 s) until this
// peer's own message is acknowledged, so both peers see the mismatch.
// Returns false (and *hostOk = localOk) outside a running session.
bool pc_netplay_save_barrier(uint32_t frame, bool localOk, unsigned long long gen, const uint8_t* sav, size_t savLen,
                             const uint8_t* block, size_t blockLen, bool* hostOk, char hostSavHex[65])
{
	if (hostOk != nullptr) *hostOk = localOk;
	if (hostSavHex != nullptr) hostSavHex[0] = '\0';
	if (!sCfg.active || sPhase != kSession) return false;
	const double t0 = now_ms();
	const double ioMs = sCurAdvanceStartMs > 0 ? t0 - sCurAdvanceStartMs : 0.0;
	// Lane S fix round 1 (MJ1): how long to wait for a peer GekkoNet still
	// reports connected; with the guard on, a GekkoNet disconnect ends it.
	const bool liveByGekko = sLgGuard;
	const unsigned deadlineMs = pc_netplay_loadguard::barrier_deadline_ms(sLgGuard, sLgWindow.load_ms());
	pc_netplay_xfer::SaveResult mine;
	mine.frame = frame;
	mine.ok    = localOk ? 1 : 0;
	mine.gen   = gen;
	mine.ledgerCount = sCfg.isHost ? sLedgerQueued : 0;
	if (localOk && sav != nullptr && savLen > 0) pc_netplay_sha::sha256(sav, savLen, mine.savSha);
	if (block != nullptr && blockLen > 0) pc_netplay_sha::sha256(block, blockLen, mine.cardSha);
	// TEST ONLY (netplay builds): PIKMIN_NETPLAY_TEST_BARRIER_CORRUPT=sav|block
	// flips one bit of this peer's own checkpoint / game-file block digest, as
	// a real one-peer desync would, so a pair proves that BOTH peers exit 5.
	if (const char* t = getenv_nonempty("PIKMIN_NETPLAY_TEST_BARRIER_CORRUPT")) {
		const bool s = strcmp(t, "sav") == 0, b = strcmp(t, "block") == 0;
		if (s) mine.savSha[0] ^= 1;
		if (b) mine.cardSha[0] ^= 1;
		if (s || b) printf("[netplay] test: this peer's %s digest altered at the barrier\n", t);
	}
	const uint8_t sendType = sCfg.isHost ? kBulkSaveResult : kBulkSaveAck;
	const uint8_t waitType = sCfg.isHost ? kBulkSaveAck : kBulkSaveResult;
	sB2Out.push_back(B2Msg{ sendType, pc_netplay_xfer::encode_save_result(mine) });
	printf("[netplay] save barrier enter: frame=%u gen=%llu local_ok=%d; this tick's save I/O before the barrier "
	       "took %.0f ms; waits up to %u ms%s\n",
	       frame, gen, (int)mine.ok, ioMs, deadlineMs,
	       liveByGekko ? " while GekkoNet reports the peer connected" : "");
	fflush(stdout);
	auto pump = [&]() {
		bulk_pump();
		if (sGekko != nullptr) gekko_network_poll(sGekko);
		loadguard_note_external_poll(); // lane S: long-tick figures count this wait's polls
	};
	auto die = [&](int code) {
		if (code == 5) {
			// Let the peer see our message too, so it reaches the same verdict.
			const double l0 = now_ms();
			while (!bulk_all_acked() && now_ms() - l0 < 2000.0) {
				pump();
				sleep_hires_ms(1.0, 0.0);
			}
		}
		if (pc_randomizer_netplay_barrier_abandoned != nullptr) pc_randomizer_netplay_barrier_abandoned();
		printf("[netplay] save barrier abandoned: exit %d after %.0f ms\n", code, now_ms() - t0);
		fflush(stdout);
		pc_state_hash_flush();
		stop_session();
		sPhase = kDone;
		std::exit(code);
	};
	pc_netplay_xfer::SaveResult peer;
	bool have = false;
	bool ledgerWaitLogged = false;
	while (true) {
		pump();
		std::vector<uint8_t> d;
		while (!have && b2_take(waitType, &d)) {
			pc_netplay_xfer::SaveResult r;
			if (!pc_netplay_xfer::decode_save_result(d.data(), d.size(), &r)) {
				printf("[netplay] save barrier: malformed peer message (len=%llu) dropped\n",
				       (unsigned long long)d.size());
				continue;
			}
			if (r.frame < frame) {
				printf("[netplay] save barrier: stale peer message for frame=%u dropped\n", r.frame);
				continue;
			}
			if (r.frame != frame) {
				printf("[netplay] save barrier: frame mismatch local=%u peer=%u (desync)\n", frame, r.frame);
				die(5);
			}
			peer = r;
			have = true;
		}
		// The client writes SAVE_RESULT only after the host's ledger lines of
		// this session have been applied (they were queued before the
		// SaveResult, but bulk delivery is unordered).
		const bool ledgerDone = sCfg.isHost || !have || sLedgerApplied >= peer.ledgerCount;
		if (have && !ledgerDone && !ledgerWaitLogged) {
			printf("[netplay] save barrier: waiting for host ledger messages (%u of %u applied)\n", sLedgerApplied,
			       peer.ledgerCount);
			ledgerWaitLogged = true;
		}
		if (have && ledgerDone) break;
		const char* missing = !have ? (sCfg.isHost ? "SAVE_ACK" : "SAVE_RESULT") : "ledger messages";
		if (liveByGekko && sGekko != nullptr) {
			// The polls above run GekkoNet's idle check; a peer it dropped
			// (or that sent Disconnect) is queued as a session event, which
			// this does not consume (handle_session_events would, after the
			// tick, but this exits first).
			int n = 0;
			GekkoSessionEvent** ev = gekko_session_events(sGekko, &n);
			for (int i = 0; ev != nullptr && i < n; ++i) {
				if (ev[i] == nullptr || ev[i]->type != GekkoPlayerDisconnected) continue;
				printf("[netplay] disconnected: handle=%d (inside the day-end save barrier)\n",
				       ev[i]->data.disconnected.handle);
				printf("[netplay] save barrier timeout (frame=%u gen=%llu, no %s: the peer disconnected after "
				       "%.0f ms in the barrier; GekkoNet timeout %u ms)\n",
				       frame, gen, missing, now_ms() - t0,
				       sLgWindow.is_open() ? sLgWindow.load_ms() : sLgWindow.normal_ms());
				die(6);
			}
		}
		if (now_ms() - t0 > (double)deadlineMs) {
			printf("[netplay] save barrier timeout (frame=%u gen=%llu, no %s from the peer within %.0f s)\n", frame,
			       gen, missing, deadlineMs / 1000.0);
			die(6);
		}
		sleep_hires_ms(1.0, 0.0);
	}
	pump(); // our message (and the peer's ack) go out now
	const double waitMs = now_ms() - t0;
	const pc_netplay_xfer::SaveResult& h = sCfg.isHost ? mine : peer;
	switch (pc_netplay_xfer::barrier_verdict(mine, peer)) {
	case pc_netplay_xfer::BarrierVerdict::GenMismatch:
		printf("[netplay] save barrier: generation mismatch local=%llu peer=%llu (desync)\n",
		       (unsigned long long)mine.gen, (unsigned long long)peer.gen);
		die(5);
		break;
	case pc_netplay_xfer::BarrierVerdict::BlockMismatch:
		printf("[netplay] save barrier: game-file block mismatch local=%s peer=%s (desync)\n",
		       to_hex(mine.cardSha, 32).c_str(), to_hex(peer.cardSha, 32).c_str());
		die(5);
		break;
	case pc_netplay_xfer::BarrierVerdict::DigestMismatch: {
		// Both digests, named by role, so the two peers print the same line.
		const pc_netplay_xfer::SaveResult& j = sCfg.isHost ? peer : mine;
		printf("[netplay] save barrier: checkpoint digest mismatch host=%s joiner=%s (desync)\n",
		       to_hex(h.savSha, 32).c_str(), to_hex(j.savSha, 32).c_str());
		die(5);
		break;
	}
	case pc_netplay_xfer::BarrierVerdict::Agree:
		break;
	}
	printf("[netplay] save barrier frame=%u gen=%llu host_ok=%d local_ok=%d sav=%s\n", frame, gen, (int)h.ok,
	       (int)mine.ok, hex16(h.savSha).c_str());
	printf("[netplay] save barrier wait: %.0f ms (bulk pumped, GekkoNet network-polled, no Advance) card block=%s "
	       "peer=%s ledger=%u/%u bulk resends=%llu bulk rx impaired: dropped=%llu delayed=%llu save-dropped=%llu\n",
	       waitMs, hex16(mine.cardSha).c_str(), hex16(peer.cardSha).c_str(),
	       sCfg.isHost ? sLedgerQueued : sLedgerApplied, sCfg.isHost ? sLedgerQueued : peer.ledgerCount,
	       (unsigned long long)sBulk.resend_count(), (unsigned long long)sBulkImp.dropped,
	       (unsigned long long)sBulkImp.delayed, (unsigned long long)sBulkImp.droppedSave);
	fflush(stdout);
	if (hostOk != nullptr) *hostOk = h.ok != 0;
	if (hostSavHex != nullptr) {
		const std::string hx = to_hex(h.savSha, 32);
		memcpy(hostSavHex, hx.c_str(), 65);
	}
	return true;
}

// B2 fix round 1 (C12, C3): a local card outcome that cannot be brought back
// in line with the agreed one ends the session as a desync (exit 5) instead
// of letting this peer's sim branch alone.
void pc_netplay_abort_desync(const char* why)
{
	printf("[netplay] desync: %s\n", why != nullptr ? why : "(unspecified)");
	fflush(stdout);
	if (!sCfg.active) std::exit(5);
	pc_state_hash_flush();
	stop_session();
	sPhase = kDone;
	std::exit(5);
}

// Launch lane self-test (pc_netplay_launch_selftest.cpp): the exact config
// text the handshake hashes. Valid until the next call.
const char* pc_netplay_session_config_text(void)
{
	static std::string text;
	text = build_config_string();
	return text.c_str();
}

void pc_netplay_session_notify_argv(int argc, char** argv)
{
	sArgc = argc;
	sArgv = argv;
	// M4 fix (round 1): pc_main now calls this BEFORE pc_bbft_init, so the
	// boot-time pc_randomizer_init->update already sees the real argv and
	// the client takes the stream-only path from the first poll. No re-arm
	// is needed; a second call (if any) just refreshes the stored argv.
}

// N3 test hook + long-load survival note. Called (weakly) from
// GameFlow::softReset on section changes, i.e. inside the synchronous stage
// load that runs within app->idle() during one Advance tick. When
// PIKMIN_NETPLAY_TEST_LOAD_DELAY_MS=<n> is set, sleeps n ms exactly once
// (the next stage load on that peer only) so the pair test can prove the
// session survives a load longer than the old 5 s timeout. The sleep is
// wall-clock only: it blocks GekkoNet pumping exactly like a real slow disk,
// without touching sim state, RNG or hashes. No-op without a netplay switch.
// M4 gap-fix lane S (issue #885): inside a session tick this hook first opens
// the load window (b), pumps the keep-alive (a) and runs a load-targeted test
// stall (PIKMIN_NETPLAY_TEST_STALL_*); the legacy single sleep above stays a
// plain un-pumped block, so it exercises the load window alone.
void pc_netplay_on_stage_load(void)
{
	if (!sInitialised || !sCfg.active) return;
	loadguard_on_stage_load();
	if (sLoadDelayDone) return;
	const char* e = std::getenv("PIKMIN_NETPLAY_TEST_LOAD_DELAY_MS");
	if (e == nullptr || *e == '\0') return;
	char* end         = nullptr;
	unsigned long n = strtoul(e, &end, 10);
	if (end == e || *end != '\0' || n == 0) return;
	sLoadDelayDone = true;
	printf("[netplay] test load delay: sleeping %lums inside stage load\n", n);
	fflush(stdout);
	std::this_thread::sleep_for(std::chrono::milliseconds(n));
	printf("[netplay] test load delay: done\n");
	fflush(stdout);
}

// M4 gap-fix lane S (issue #885): keep-alive entry for the long main-thread
// loops (pc_netplay_loadguard.h, part (a)). Called weakly between the steps
// of a long operation: kSiteShader after each specialised TEV program is
// created (pc_gfx.cpp), kSiteDvd on DVDOpen/DVDRead (dvd_stubs.cpp), kSiteSave
// while the day-end save waits for its card I/O (memoryCard.cpp waitPolling,
// cardutil.cpp CardUtilIdleWhileBusy) and around its checkpoint write
// (pc_randomizer.cpp; fix round 1, MJ1). Inert
// unless a netplay session tick is executing: then, at most every 50 ms, one
// GekkoNet network poll (no Advance, no sim state). The shader site is also
// where PIKMIN_NETPLAY_TEST_STALL_AT=shader stalls. Null in default builds.
void pc_netplay_load_keepalive(int site)
{
	if (!sInAdvance) return; // outside a session tick (and every non-netplay run)
	if (std::this_thread::get_id() != sLgMainThread) return; // DVD/audio/loading threads
	if (site == pc_netplay_loadguard::kSiteShader)
		loadguard_maybe_stall(pc_netplay_loadguard::StallAt::Shader, 1, "shader");
	loadguard_keepalive(site);
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
		// Launch lane: the co-op switch just reset the captains; the joiner
		// takes the host's again (they are part of the adopted block).
		pc_netplay_launch_apply_captains();
		launcher_apply_input();
		if (const char* f1 = getenv_nonempty("PIKMIN_NETPLAY_TEST_F1_CYCLE_TICK")) {
			char* end          = nullptr;
			unsigned long long n = strtoull(f1, &end, 10);
			if (end != f1 && *end == '\0' && n > 0) sF1CycleTick = n;
		}
		compute_local_hello();
		printf("[netplay] mode=%s delay=%u seed=%u\n", sCfg.isHost ? "host" : "join",
		       sCfg.localDelay, sCfg.seed);
		fflush(stdout);
		// Transport up before the handshake pump runs. M5a hook (issue #887):
		// ICE mode runs the copy-paste signalling exchange over libjuice and
		// connects the agent; UDP mode binds the socket as before. Exactly
		// one pair comes up; the handshake and session flows after this are
		// transport-agnostic (hs_send/hs_drain + the link adapter).
		if (sCfg.iceMode) {
			pc_netplay_ice::IceNetConfig nic = pc_netplay_ice::ice_net_config_from_env();
			printf("[netplay] transport=ice role=%s stun=%s turn=%s turnOnly=%d\n",
			       sCfg.isHost ? "host" : "join",
			       nic.stun.empty() ? "none" : nic.stun.front().host.c_str(),
			       nic.turn.empty() ? "none" : nic.turn.front().host.c_str(),
			       (int)nic.turnOnly);
			fflush(stdout);
			auto pump = [&]() { sys->mControllerMgr.update(); };
			std::string err;
			bool ok = false;
			// Launch lane: one-command flows (v2 bundle offer, clipboard/file
			// code exchange) instead of the env-driven M5a exchange. The
			// low-level switches and env vars below keep working untouched.
			if (sCfg.launcherMode && sCfg.isHost) ok = launcher_host_flow(nic, pump, &err);
			else if (sCfg.launcherMode) ok = launcher_joiner_flow(nic, pump, &err);
			else {
				sIce = new pc_netplay_ice::IceSocket();
				if (sCfg.isHost) ok = pc_netplay_ice::ice_host_session(nic, pump, sIce, &err);
				else ok = pc_netplay_ice::ice_join_session(nic, sCfg.iceJoinCode, pump, sIce, &err);
			}
			if (!ok) {
				printf("[netplay] ice setup failed: %s\n", err.c_str());
				fflush(stdout);
				// m12: join the libjuice thread before exit.
				stop_session();
				std::exit(1);
			}
			// The agent is connected 1:1: no endpoint learning. GekkoNet
			// still routes actors by an address blob, so both sides use
			// the conventional 127.0.0.1:1 placeholder: it matches the
			// fixed blob IceLink synthesises on receive (the content is
			// otherwise ignored over ICE).
			sHaveRemote   = true;
			sRemoteIp     = 0x7F000001;
			sRemotePort   = 1;
			sCfg.joinIp   = 0x7F000001;
			sCfg.joinPort = 1;
			sIceLink      = new pc_netplay_ice::IceLink(sIce);
		} else {
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
		// Fix3 R2-1: always honour this process's timer-resolution requests
		// while the session runs (released in stop_session), on either
		// transport.
		netplay_timer_power_opt(true);
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
		// Polish: poll every ~1 ms (not 5 ms) so the nonce-matched RTT
		// measures the wire, not the poll phase (else +30-50 ms bias picks
		// one frame too many at 50/100/200 ms one-way). The wait is the
		// hires timer wait below, not Sleep(1): hidden processes do not
		// get 1 ms Sleep granularity (B1), so a plain sleep quantises
		// every sample to the 15.6 ms tick.
		sys->mControllerMgr.update();
		// M4 lane B2: handshake success goes through the checkpoint decision
		// (and, when needed, the transfer phase) before the GekkoNet session.
		if (handshake_pump()) on_handshake_done();
		// Fix3 R2-2: the 1 ms poll blocks on the waitable timer (spin tail
		// 0), so a host waiting with no joiner idles instead of pinning a
		// core.
		sleep_hires_ms(1.0, 0.0);
		return true;
	}

	if (sPhase == kTransfer) {
		// M4 lane B2: checkpoint / P2 sidecar transfer. Only the handshake
		// and bulk channels are pumped (GekkoNet does not exist yet); the
		// window stays responsive through PADRead's poll.
		sys->mControllerMgr.update();
		transfer_pump();
		if (sPhase == kTransfer) sleep_hires_ms(1.0, 0.0);
		return true;
	}

	// kSession.
	const bool unthrottled = pc_netplay_unthrottled();
	const double turnStartMs = now_ms();
	adaptive_note_turn_start(turnStartMs); // M5c lane B: own input lateness
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
		// B1: once frozen and live again, the host queues the RESUME
		// snapshot (before bulk_pump so it goes out this turn).
		hold_host_try_resume();
		// M4a: pump the bulk 0x03 channel (acks now, lane-B messages later).
		bulk_pump();
		// B2 residual: fold this turn's physical sample into the
		// accumulator on every turn (submit or stall), so a tap between
		// two submit turns still reaches the next submit.
		accum_add_current();
		if (sGekkoStarted && hold_resume_catchup_due()) {
			// B1: RESUME catch-up submit (see hold_resume_catchup_due). No
			// snapshot fragment and no HOLD flag ride this input: its
			// delay copies would only repeat them. Fix round 1 (B1-C7):
			// it is neutral, so SetDelay's d copies cannot hold a live
			// press for d+1 frames, and it consumes no script record and
			// leaves the pad accumulator latch for the next real submit.
			PcNetplayInput local = pc_netplay_input_neutral();
			uint8_t wire[16];
			pc_netplay_input_encode(local, wire);
			const uint64_t landing = sAdvances; // == H+12
			gekko_set_local_delay(sGekko, sLocalHandle, 0);
			gekko_add_local_input(sGekko, sLocalHandle, wire);
			gekko_set_local_delay(sGekko, sLocalHandle, (unsigned char)sCfg.localDelay);
			++sSubmitted;
			// M5c lane B: H+12 plus the delay copies on H+13..H+12+delay.
			sNextLand = landing + 1 + sCfg.localDelay;
			printf("[netplay] resume: catch-up input for frame=%llu at delay 0, delay %u restored "
			       "(frames %llu..%llu repeat it)\n",
			       (unsigned long long)landing, sCfg.localDelay, (unsigned long long)(landing + 1),
			       (unsigned long long)(landing + sCfg.localDelay));
			fflush(stdout);
		} else if (sGekkoStarted && !hold_blocks_submit()) {
			// M5c lane B: the submit is due when the next local input lands
			// on sAdvances + delay (the old sSubmitted == sAdvances gate while
			// the delay stays put). On a due turn the adaptive delay (or the
			// test schedule) may shrink the delay, which skips this turn's
			// submit and the next ones until due again, or grow it, which
			// adds one input per new frame now (submit_local_inputs).
			unsigned grow = 0;
			if (pc_netplay_adaptive::submit_due(sNextLand, sAdvances, sCfg.localDelay)) {
				const unsigned target = adaptive_target();
				if (target < sCfg.localDelay) adaptive_shrink(target);
				else grow = target - sCfg.localDelay;
			}
			if (pc_netplay_adaptive::submit_due(sNextLand, sAdvances, sCfg.localDelay))
				submit_local_inputs(1 + grow);
		}
		// 4-5. Advance + per-tick block.
		advances = handle_game_events(sys, app);
		// 8. Session events.
		handle_session_events();
		if (sPhase != kSession) return true; // stopped inside the handlers
		if (sF1CycleTick > 0 && !sF1CycleDone && sSessionTicks >= sF1CycleTick) {
			// Test hook: the F1 open/close save path, between two ticks.
			sF1CycleDone = true;
			printf("[netplay] test hook: F1 open/close after tick %llu\n",
			       (unsigned long long)sSessionTicks);
			fflush(stdout);
			pc_settings_test_f1_cycle();
		}
	}
	// Pacing: the session owns the single 30 Hz schedule (polish item 1).
	// Drift-free deadline: next += 1000/30 ms per Advance, with bounded
	// catch-up (snap forward when more than ~5 slots behind, so an overrun
	// never costs more than a slot and never spirals; at most 2 slots of
	// catch-up per turn, m5, so a hiccup replays over several turns instead
	// of one burst + one long pause). Stall turns never spend a whole slot:
	// they pump and sleep ~1 ms. The frames-ahead correction is applied to
	// the schedule itself (M1): sleeping after the deadline only shifts one
	// turn's phase and never converges, so the ahead peer stretches its own
	// deadline by a small proportional share instead.
	double nextDueMs = 0; // M5c lane B: see adaptive_note_turn_start
	bool haveDue = false;
	if (!unthrottled) {
		constexpr double kSlotMs = 1000.0 / 30.0;
		constexpr double kMaxCatchupSlots = 5.0;
		constexpr double kMaxAdvanceSlots = 2.0; // m5: burst cap per turn
		if (advances > 0) {
			// Proportional, small frames-ahead slow-down on the schedule:
			// 0.05 slot per ahead-frame past 0.75, capped at 10% of a slot
			// (~3.3 ms). Persistent (added to the deadline), so a peer that
			// stays ahead converges instead of re-phasing one turn (M1).
			const float aheadNow =
			    (sGekko != nullptr) ? gekko_frames_ahead(sGekko) : 0.0f;
			double extraMs = 0.0;
			if (aheadNow > 0.75f) {
				extraMs = (double)(aheadNow - 0.75f) * kSlotMs * 0.05;
				const double cap = kSlotMs * 0.10;
				if (extraMs > cap) extraMs = cap;
			}
			double effAdv = (double)advances;
			if (effAdv > kMaxAdvanceSlots) effAdv = kMaxAdvanceSlots;
			double now = now_ms();
			if (sNextTurnMs == 0) sNextTurnMs = now + kSlotMs;
			nextDueMs = sNextTurnMs; // M5c lane B: the next turn is due here (a snap moves it to now)
			haveDue = true;
			if (now < sNextTurnMs) {
				// Fix3 R2-3: one call; the timer carries the bulk and
				// sleep_hires_ms owns the whole spin tail (at most 1 ms per
				// slot, no caller-side spin).
				const double wait = sNextTurnMs - now;
				if (wait > 0.05) sleep_hires_ms(wait);
				sNextTurnMs += kSlotMs * effAdv + extraMs;
			} else {
				// Overrun: bounded catch-up. Far behind (long load inside
				// an Advance) snaps to now + slot; a small overrun keeps
				// the deadline so the average stays drift-free.
				const double behind = now - sNextTurnMs;
				if (behind > kMaxCatchupSlots * kSlotMs) {
					sNextTurnMs = now + kSlotMs;
					nextDueMs = now;
				} else {
					sNextTurnMs += kSlotMs * effAdv + extraMs;
				}
			}
		} else {
			// Stall turn: keep pumping without spending a whole slot.
			// The network was already pumped above; a ~1 ms timer wait
			// avoids a busy spin while keeping poll latency far under a
			// slot (plain Sleep(1) is ~15.6 ms in hidden runs, B1).
			// Fix3 R2-2: spin tail 0, so stall turns block on the timer.
			sleep_hires_ms(1.0, 0.0);
		}
	}
	if (advances == 0 && hold_frozen()) {
		// B1: a frozen HOLD turn slept like a stall turn above, but its
		// wall time is held time, not stall: it is never charged to the
		// stall figures. The frozen wall time itself (held at -> resume) is
		// accumulated exactly into sHoldMs at the RESUME apply and excluded
		// from the stall %, tps and slot-loss figures (reported as held=).
		++sHoldTurns;
		const double holdNow = now_ms();
		// Heartbeat every 5 s of freeze: proves the held peer keeps pumping.
		if (holdNow - sHoldBeatMs >= 5000.0) {
			sHoldBeatMs = holdNow;
			printf("[netplay] holding: frozen after frame=%u for %.0f ms (turns=%llu resume=%d)\n",
			       sHoldFrame + kHoldLeadFrames - 1, holdNow - sHoldStartMs,
			       (unsigned long long)sHoldTurns, (int)sResumeHave);
			fflush(stdout);
		}
	} else if (advances == 0) {
		++sStalls;
		// M5/n2 + fix m1: wall-clock stall accounting is the wall time of
		// turns with no Advance (advance turns own the slot wait, so it is
		// never charged as stall). The charge runs from turn start to turn
		// end; report it next to tps and the 1 - tps/30 slot-loss fraction.
		const double turnMs = now_ms() - turnStartMs;
		sStallMs += turnMs;
		// M5c lane B: consecutive stall turns form one stall event.
		adaptive_note_stall_turn(turnStartMs, turnMs);
		// 9. Waiting: no tick. The turn above already pumped the network
		// (update_session) and window events (PADRead poll).
	}
	if (advances > 0) adaptive_close_stall(); // M5c lane B: the stall (if any) ended
	adaptive_note_turn_end(advances > 0, haveDue, nextDueMs);
	adaptive_poll(now_ms());                  // M5c lane B: RTT sample every 500 ms
	return true;
}
