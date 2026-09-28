// Netplay launch lane (issue #887): the one-command launcher's pre-init
// stage. See pc_netplay_launch.h for the order of events and the contract.

#include "netplay/pc_netplay_launch.h"

#include "netplay/pc_netplay_ice.h"
#include "netplay/pc_netplay_input_sel.h"
#include "netplay/pc_netplay_launch_util.h"
#include "settings/pc_settings.h"
#include "pc_coop.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

using namespace pc_netplay_launch_util;

PcNetplayLaunch sSetup;
// Rewritten argv (original + --randomizer-seed <path>), kept alive for the
// whole process because every later consumer holds the pointer.
std::vector<std::string> sArgStore;
std::vector<char*> sArgv;

[[noreturn]] void die(const char* fmt, ...)
{
	char buf[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	printf("[netplay] launch: %s\n", buf);
	fflush(stdout);
	std::exit(2);
}

const char* getenv_nonempty(const char* name)
{
	const char* v = std::getenv(name);
	return (v != nullptr && *v != '\0') ? v : nullptr;
}

bool argv_present(int argc, char** argv, const char* flag)
{
	for (int i = 1; i < argc; ++i) {
		if (argv[i] != nullptr && std::strcmp(argv[i], flag) == 0) return true;
	}
	return false;
}

// Value after `flag`; dies when the flag is the last argument.
const char* argv_value(int argc, char** argv, const char* flag)
{
	for (int i = 1; i < argc; ++i) {
		if (argv[i] == nullptr || std::strcmp(argv[i], flag) != 0) continue;
		if (i + 1 >= argc || argv[i + 1] == nullptr) die("%s needs a value", flag);
		return argv[i + 1];
	}
	return nullptr;
}

void set_env(const char* name, const std::string& value)
{
#ifdef _WIN32
	_putenv_s(name, value.c_str());
#else
	setenv(name, value.c_str(), 1);
#endif
}

unsigned current_pid()
{
#ifdef _WIN32
	return (unsigned)GetCurrentProcessId();
#else
	return (unsigned)getpid();
#endif
}

// 64 lowercase hex chars. random_device mixed with the clock and the pid:
// a per-run identity token, not a secret.
std::string fresh_token()
{
	std::random_device rd;
	const uint64_t clock = (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::seed_seq seq{ rd(), rd(), rd(), rd(), (unsigned)(clock & 0xFFFFFFFFu), (unsigned)(clock >> 32),
		               current_pid() };
	std::mt19937_64 gen(seq);
	std::string tok;
	char cell[17];
	for (int i = 0; i < 4; ++i) {
		snprintf(cell, sizeof(cell), "%016llx", (unsigned long long)gen());
		tok += cell;
	}
	return tok;
}

std::string forward_slashes(std::string p)
{
	for (char& c : p) {
		if (c == '\\') c = '/';
	}
	return p;
}

std::string exe_dir()
{
#ifdef _WIN32
	char path[4096];
	DWORD n = GetModuleFileNameA(nullptr, path, sizeof(path));
	if (n == 0 || n >= sizeof(path)) return std::string();
	std::string p = forward_slashes(std::string(path, n));
#else
	char path[4096];
	ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (n <= 0) return std::string();
	std::string p(path, (size_t)n);
#endif
	const size_t slash = p.find_last_of('/');
	return slash == std::string::npos ? std::string() : p.substr(0, slash);
}

// Creates one directory. Returns 1 created, 0 already exists, -1 failed.
int make_dir(const std::string& path)
{
#ifdef _WIN32
	if (CreateDirectoryA(path.c_str(), nullptr)) return 1;
	return GetLastError() == ERROR_ALREADY_EXISTS ? 0 : -1;
#else
	if (mkdir(path.c_str(), 0755) == 0) return 1;
	return errno == EEXIST ? 0 : -1;
#endif
}

bool make_dirs(const std::string& path)
{
	for (size_t i = 1; i <= path.size(); ++i) {
		if (i == path.size() || path[i] == '/') {
			const std::string part = path.substr(0, i);
			if (part.size() == 2 && part[1] == ':') continue; // drive root
			if (make_dir(part) < 0) return false;
		}
	}
	return true;
}

bool write_file(const std::string& path, const std::string& data)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (f == nullptr) return false;
	bool ok = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();
	ok      = (fclose(f) == 0) && ok;
	return ok;
}

// Reads at most cap + 1 bytes, so an oversized file is refused without
// being read in full (m1). Returns -1 when the file cannot be opened.
long read_file_bounded(const std::string& path, size_t cap, std::string* out)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (f == nullptr) return -1;
	out->clear();
	char chunk[8192];
	while (out->size() <= cap) {
		const size_t want = std::min(sizeof(chunk), cap + 1 - out->size());
		const size_t n    = fread(chunk, 1, want, f);
		if (n > 0) out->append(chunk, n);
		if (n < want) break;
	}
	fclose(f);
	return (long)out->size();
}

bool dir_writable(const std::string& dir)
{
	const std::string probe = dir + "/.nectar-write-test";
	if (!write_file(probe, "1")) return false;
	remove(probe.c_str());
	return true;
}

// The run_pair.py profile (foh-day2, identity placements, 25-repair goal,
// red start, 10 Flarlic), with a fresh FINGERPRINT per session so every
// launcher session is its own campaign. Its SESSION line is a placeholder:
// like any --bootstrap file it is re-stamped with the run token after.
std::string default_bootstrap()
{
	const std::string fingerprint = fresh_token();
	return "PIKMIN_RANDOMIZER 5\nSESSION " + fresh_token() + "\nFINGERPRINT " + fingerprint
	     + "\nPROFILE foh-day2\nCATALOG gameplay-checks-v5\nPLACEMENT identity-v1\nGOAL 25\n"
	       "DAYS repeat-day29-v1\nCOLOR red\nSTARTING_FLARLIC 10\nEND\n";
}

#ifdef _WIN32
// @clipboard before SDL exists: the Win32 clipboard, CF_UNICODETEXT, as
// UTF-8 (codes are ASCII; anything else fails the decode with a reason).
bool read_clipboard(std::string* out, std::string* err)
{
	bool opened = false;
	for (int attempt = 0; attempt < 10 && !opened; ++attempt) {
		opened = OpenClipboard(nullptr) != 0;
		if (!opened) Sleep(50); // another process may hold it briefly
	}
	if (!opened) {
		*err = "cannot open the clipboard";
		return false;
	}
	bool ok = false;
	HANDLE h = GetClipboardData(CF_UNICODETEXT);
	if (h != nullptr) {
		const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(h));
		if (w != nullptr) {
			const size_t cap = pc_netplay_ice::kMaxOfferV2Chars + 4096;
			size_t wlen      = 0;
			while (w[wlen] != L'\0' && wlen <= cap) ++wlen;
			if (wlen > cap) {
				*err = "clipboard text is too large to be a connection code";
			} else {
				const int n = WideCharToMultiByte(CP_UTF8, 0, w, (int)wlen, nullptr, 0, nullptr, nullptr);
				std::string s((size_t)(n > 0 ? n : 0), '\0');
				if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, (int)wlen, &s[0], n, nullptr, nullptr);
				*out = s;
				ok   = true;
			}
			GlobalUnlock(h);
		}
	} else {
		*err = "the clipboard holds no text";
	}
	CloseClipboard();
	return ok;
}
#endif

std::string trim(const std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
	while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
	return s.substr(b, e - b);
}

// Offer text from a literal, @file, or @clipboard. Read exactly once (m12:
// the adopted bundle and the SDP always come from the same text).
std::string read_offer_arg(const std::string& arg)
{
	std::string code, err;
	if (trim(arg) == "@clipboard") {
#ifdef _WIN32
		if (!read_clipboard(&code, &err)) die("cannot read the offer code from the clipboard: %s", err.c_str());
#else
		die("@clipboard needs Windows here; pass the code itself or @file");
#endif
		code = trim(code);
		if (code.empty()) die("the clipboard is empty; copy the host's offer code first");
		return code;
	}
	if (!pc_netplay_ice::ice_read_code_arg(arg, &code, &err)) die("cannot read the offer code: %s", err.c_str());
	return code;
}

void parse_captains(const std::string& block)
{
	auto value = [&](const char* key, int* out) {
		const std::string k = std::string(";") + key + "=";
		const size_t at     = block.find(k);
		if (at == std::string::npos) return false;
		char* end    = nullptr;
		const char* v = block.c_str() + at + k.size();
		long n        = strtol(v, &end, 10);
		if (end == v || *end != ';') return false;
		n %= PC_CAPTAIN_COUNT;
		if (n < 0) n += PC_CAPTAIN_COUNT;
		*out = (int)n;
		return true;
	};
	int p1 = 0, p2 = 1;
	if (value("captainP1", &p1) && value("captainP2", &p2)) {
		sSetup.haveCaptains = true;
		sSetup.captainP1    = p1;
		sSetup.captainP2    = p2;
	}
}

const char* kP2Refusal =
    "this seed uses P2 enemies (ENEMY_P2). P2 seeds read per-run sidecar files from the "
    "working directory (p2-*-actors/bank/profile.txt, an assets/ overlay and "
    "p2-binding-receipt.json, several hundred KB) that a connection code cannot carry and the "
    "handshake does not check, so the two players would silently run different enemies. "
    "Netplay sessions support seeds without ENEMY_P2 (or the default bootstrap: leave out "
    "--bootstrap).";

} // namespace

const PcNetplayLaunch& pc_netplay_launch_setup(void) { return sSetup; }

bool pc_netplay_launch_wants_local_state(void) { return sSetup.active; }

void pc_netplay_launch_preinit(int* argcp, char*** argvp)
{
	const int argc = *argcp;
	char** argv    = *argvp;

	// --netplay-input works with every netplay mode; resolve it first so a
	// gamepad peer's background-joystick hint is set before SDL_Init (m6).
	{
		const char* inputCli = argv_value(argc, argv, "--netplay-input");
		const char* inputEnv = getenv_nonempty("PIKMIN_NETPLAY_INPUT");
		const char* spec     = inputCli != nullptr ? inputCli : inputEnv;
		if (spec != nullptr) {
			pc_netplay_input_sel::Kind kind = pc_netplay_input_sel::kInputAuto;
			int index                       = 0;
			if (!pc_netplay_input_sel::parse_input_spec(spec, &kind, &index))
				die("bad --netplay-input %s (want keyboard|gamepad[:N]|auto)", spec);
			sSetup.inputSpec  = spec;
			sSetup.inputKind  = (int)kind;
			sSetup.inputIndex = index;
			if (kind == pc_netplay_input_sel::kInputGamepad) {
				// Keep reading the pad while the other window has focus.
				SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
			}
		}
	}

	const bool hostIce   = argv_present(argc, argv, "--netplay-host-ice");
	const bool joinIce   = argv_present(argc, argv, "--netplay-join-ice");
	const char* launcherOnly[] = { "--bootstrap", "--netplay-code-out", "--netplay-answer-in",
		                           "--netplay-test-hidden", "--netplay-test-ticks" };
	if (!hostIce && !joinIce) {
		for (const char* flag : launcherOnly) {
			if (argv_present(argc, argv, flag))
				die("%s needs --netplay-host-ice or --netplay-join-ice", flag);
		}
		return;
	}
	if (hostIce && joinIce) die("--netplay-host-ice and --netplay-join-ice are exclusive");

	// Launcher mode is exclusive with the low-level switches (they stay the
	// test surface) and with every other way to pick a game mode: the run
	// bootstrap is the launcher's, so a hand-passed seed would silently
	// differ from the bundle the joiner receives.
	{
		const char* legacy[] = { "--netplay-host", "--netplay-join", "--netplay-ice-host",
			                     "--netplay-ice-join" };
		for (const char* flag : legacy) {
			if (argv_present(argc, argv, flag))
				die("--netplay-host-ice/--netplay-join-ice are exclusive with "
				    "--netplay-host/--netplay-join/--netplay-ice-host/--netplay-ice-join (%s given)",
				    flag);
		}
		const char* iceHostEnv = getenv_nonempty("PIKMIN_NETPLAY_ICE_HOST");
		if (getenv_nonempty("PIKMIN_NETPLAY_HOST") != nullptr || getenv_nonempty("PIKMIN_NETPLAY_JOIN") != nullptr
		    || (iceHostEnv != nullptr && iceHostEnv[0] == '1') || getenv_nonempty("PIKMIN_NETPLAY_ICE_JOIN") != nullptr)
			die("--netplay-host-ice/--netplay-join-ice are exclusive with the PIKMIN_NETPLAY_HOST/JOIN/"
			    "ICE_HOST/ICE_JOIN environment variables; unset them");
		const char* modes[] = { "--randomizer-seed", "--bbft-port", "--experimental-pikmin2-room",
			                    "--experimental-challenge-level", "--experimental-challenge-stage" };
		for (const char* flag : modes) {
			if (argv_present(argc, argv, flag))
				die("%s cannot be combined with --netplay-host-ice/--netplay-join-ice%s", flag,
				    std::strcmp(flag, "--randomizer-seed") == 0
				        ? " (the host passes a seed with --bootstrap <file>; the joiner gets it from the offer)"
				        : "");
		}
		if (getenv_nonempty("BBFT_PORT") != nullptr)
			die("BBFT_PORT is set: BBFT sessions cannot be combined with --netplay-host-ice/--netplay-join-ice");
	}

	sSetup.active = true;
	sSetup.isHost = hostIce;
	if (!hostIce) {
		if (argv_present(argc, argv, "--bootstrap"))
			die("--bootstrap is host-only: the joiner gets the bootstrap from the offer code");
		if (argv_present(argc, argv, "--netplay-answer-in"))
			die("--netplay-answer-in is host-only (the joiner prints its answer)");
	}
	if (const char* v = argv_value(argc, argv, "--netplay-code-out")) sSetup.codeOut = v;
	if (const char* v = argv_value(argc, argv, "--netplay-answer-in")) sSetup.answerIn = v;
	sSetup.testHidden = argv_present(argc, argv, "--netplay-test-hidden");
	if (const char* tt = argv_value(argc, argv, "--netplay-test-ticks")) {
		char* end       = nullptr;
		unsigned long n = strtoul(tt, &end, 10);
		if (end == tt || *end != '\0' || n == 0) die("bad --netplay-test-ticks %s", tt);
		sSetup.testTicks = n;
	}
	if (sSetup.testHidden) {
		// Hidden, private test runs: the randomizer creates its window hidden
		// under PIKMIN_RANDOMIZER_TEST_BACKGROUND=1, and audio stays silent.
		set_env("PIKMIN_RANDOMIZER_TEST_BACKGROUND", "1");
		if (getenv_nonempty("SDL_AUDIODRIVER") == nullptr) set_env("SDL_AUDIODRIVER", "dummy");
	}

	sSetup.token = fresh_token();

	// ---- the session bootstrap (and, for the joiner, the whole bundle) ----
	std::string boot;
	if (sSetup.isHost) {
		const char* bootCli = argv_value(argc, argv, "--bootstrap");
		if (bootCli != nullptr) {
			const long n = read_file_bounded(bootCli, kMaxBootstrapBytes, &boot);
			if (n < 0) die("cannot read --bootstrap %s", bootCli);
			if ((size_t)n > kMaxBootstrapBytes)
				die("--bootstrap %s is larger than %u bytes (the offer code's limit)", bootCli,
				    (unsigned)kMaxBootstrapBytes);
			sSetup.bootstrapSource = bootCli;
		} else {
			boot                   = default_bootstrap();
			sSetup.bootstrapSource = "default";
		}
		const char* seedEnv = getenv_nonempty("PIKMIN_NETPLAY_SEED");
		if (seedEnv != nullptr) {
			char* end               = nullptr;
			const unsigned long long v = strtoull(seedEnv, &end, 0);
			if (end != seedEnv && *end == '\0' && v <= 0xFFFFFFFFull) sSetup.seed = (uint32_t)v;
		}
	} else {
		const char* joinArg = argv_value(argc, argv, "--netplay-join-ice");
		sSetup.offerCode    = read_offer_arg(joinArg);
		std::string err;
		switch (classify_code(sSetup.offerCode)) {
		case kCodeEmpty:
			die("the offer code is empty; copy the host's whole offer line");
		case kCodeGarbage:
			die("that is not a connection code (the host's offer starts with NPIX2-); copy the "
			    "host's whole offer line");
		case kCodeV1: {
			bool isOffer = false;
			std::string sdp;
			if (!pc_netplay_ice::ice_decode_code(sSetup.offerCode, &isOffer, &sdp, &err))
				die("%s", err.c_str());
			if (!isOffer)
				die("that is an answer code (NPIX1- answer): it goes to the host, not to "
				    "--netplay-join-ice; ask the host for the offer (NPIX2-...)");
			die("that offer (NPIX1-) comes from an older build without the session bundle; the "
			    "one-command joiner needs the NPIX2- offer that --netplay-host-ice prints (same "
			    "build on both sides). For an old host, use --netplay-ice-join with matching local "
			    "bootstrap and settings files instead");
		}
		case kCodeOfferV2:
			break;
		}
		std::string sdp;
		pc_netplay_ice::SessionBundle bundle;
		if (!pc_netplay_ice::ice_decode_offer_v2(sSetup.offerCode, &sdp, &bundle, &err)) die("%s", err.c_str());
		if (bundle.bootstrapBytes.empty())
			die("the offer carries no bootstrap (host build too old?); both sides need the same build");
		if (bundle.configText.compare(0, 13, "m3-config-v1;") != 0)
			die("the offer's settings block is not m3-config-v1; both sides need the same build");
		boot                   = bundle.bootstrapBytes;
		sSetup.bootstrapSource = "offer bundle";
		sSetup.seed            = bundle.seed;
		sSetup.configBlock     = bundle.configText;
		parse_captains(sSetup.configBlock);
	}
	{
		std::string err;
		bool p2 = false;
		if (!validate_bootstrap(boot, &err, &p2)) {
			if (p2) die("refusing %s: %s", sSetup.isHost ? "--bootstrap" : "the offer's bootstrap", kP2Refusal);
			die("refusing %s: %s", sSetup.isHost ? "--bootstrap" : "the offer's bootstrap", err.c_str());
		}
	}
	std::string stamped;
	{
		std::string err;
		if (!restamp_session(boot, sSetup.token, &stamped, &err))
			die("cannot re-stamp the bootstrap SESSION line: %s", err.c_str());
	}

	// ---- this run's private dir (per run and per peer) ----
	std::string base = exe_dir();
	base             = base.empty() ? std::string("netplay") : base + "/netplay";
	if (!make_dirs(base) || !dir_writable(base)) {
		const char* local = getenv_nonempty("LOCALAPPDATA");
		const std::string fallback = local != nullptr ? forward_slashes(local) + "/Nectar/netplay" : std::string();
		if (fallback.empty() || !make_dirs(fallback) || !dir_writable(fallback))
			die("cannot create a run dir: %s is not writable%s%s", base.c_str(),
			    fallback.empty() ? "" : " and neither is ", fallback.c_str());
		printf("[netplay] launch: %s is not writable; run dirs go to %s\n", base.c_str(), fallback.c_str());
		base = fallback;
	}
	{
		const time_t now = time(nullptr);
		struct tm lt;
#ifdef _WIN32
		localtime_s(&lt, &now);
#else
		localtime_r(&now, &lt);
#endif
		char stamp[128];
		snprintf(stamp, sizeof(stamp), "run-%04d%02d%02d-%02d%02d%02d-%s-pid%u", lt.tm_year + 1900,
		         lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, sSetup.isHost ? "host" : "join",
		         current_pid());
		// Exclusive create: a run dir is never reused, so no stale hello.txt,
		// checks.txt or campaign checkpoint can meet a new session.
		for (int n = 0; n < 100 && sSetup.runDir.empty(); ++n) {
			const std::string dir = base + "/" + stamp + (n == 0 ? std::string() : "-" + std::to_string(n));
			const int r           = make_dir(dir);
			if (r == 1) sSetup.runDir = dir;
			else if (r < 0) die("cannot create run dir %s", dir.c_str());
		}
		if (sSetup.runDir.empty()) die("cannot create a fresh run dir under %s", base.c_str());
	}
	const RunLayout layout = run_layout(sSetup.runDir, sSetup.token);
	sSetup.bootstrapPath   = layout.bootstrapPath;
	sSetup.campaignDir     = layout.campaignDir;
	sSetup.saveDir         = layout.saveDir;
	if (!make_dirs(layout.bootstrapDir) || !make_dirs(layout.saveDir))
		die("cannot create the run layout under %s", sSetup.runDir.c_str());
	if (!write_file(layout.bootstrapPath, stamped)) die("cannot write %s", layout.bootstrapPath.c_str());

	// ---- process environment for the engine ----
	// B2: the card/save root is resolved at boot (CARDInit), so the private
	// save dir is set here, before anything can resolve it. With the
	// randomizer active the card itself lives in the derived campaign dir;
	// NECTAR_SAVE_DIR then holds the shader cache (and the card fallback).
	if (const char* prev = getenv_nonempty("NECTAR_SAVE_DIR"))
		printf("[netplay] launch: NECTAR_SAVE_DIR was %s; this session uses its private save dir\n", prev);
	set_env("NECTAR_SAVE_DIR", layout.saveDir);
	if (!sSetup.isHost) {
		// The deterministic reseed (pc_netplay_det) reads the netplay seed
		// from the environment at every stage load: the joiner takes the
		// host's.
		set_env("PIKMIN_NETPLAY_SEED", std::to_string(sSetup.seed));
	}

	// ---- records (never read back by the game) ----
	{
		std::string conf;
		if (read_file_bounded("pikmin_settings.conf", 1u << 20, &conf) >= 0)
			write_file(sSetup.runDir + "/settings-at-start.conf", conf);
		std::string rec;
		rec += std::string("role ") + (sSetup.isHost ? "host" : "join") + "\n";
		rec += "token " + sSetup.token + "\n";
		rec += "bootstrap " + layout.bootstrapPath + "\n";
		rec += "bootstrap_source " + sSetup.bootstrapSource + "\n";
		rec += "campaign " + layout.campaignDir + "\n";
		rec += "save " + layout.saveDir + "\n";
		rec += "seed " + std::to_string(sSetup.seed) + "\n";
		rec += "input " + (sSetup.inputSpec.empty() ? std::string("auto") : sSetup.inputSpec) + "\n";
		write_file(sSetup.runDir + "/launch.txt", rec);
	}

	// ---- argv: feed the run bootstrap through the ordinary seed path ----
	sArgStore.clear();
	for (int i = 0; i < argc; ++i) sArgStore.push_back(argv[i] != nullptr ? argv[i] : "");
	sArgStore.push_back("--randomizer-seed");
	sArgStore.push_back(layout.bootstrapPath);
	sArgv.clear();
	for (std::string& a : sArgStore) sArgv.push_back(&a[0]);
	sArgv.push_back(nullptr);
	*argcp = (int)sArgStore.size();
	*argvp = sArgv.data();

	printf("[netplay] launch: role=%s run dir %s\n", sSetup.isHost ? "host" : "join", sSetup.runDir.c_str());
	printf("[netplay] launch: bootstrap %s (%lluB from %s, SESSION %.8s...)\n", layout.bootstrapPath.c_str(),
	       (unsigned long long)stamped.size(), sSetup.bootstrapSource.c_str(), sSetup.token.c_str());
	printf("[netplay] launch: campaign dir %s (private to this run and this peer)\n", layout.campaignDir.c_str());
	printf("[netplay] launch: NECTAR_SAVE_DIR=%s\n", layout.saveDir.c_str());
	if (!sSetup.isHost)
		printf("[netplay] launch: offer bundle seed=%u config=%lluB bootstrap=%lluB\n", sSetup.seed,
		       (unsigned long long)sSetup.configBlock.size(), (unsigned long long)boot.size());
	fflush(stdout);
}

void pc_netplay_launch_post_settings(void)
{
	if (!sSetup.active) return;
	// Both roles: sim-relevant settings are locked for the session and the
	// settings file keeps each player's own values (B3). The joiner adopts
	// the host's block here, before anything reads a sim setting.
	pc_settings_session_begin(sSetup.isHost ? nullptr : sSetup.configBlock.c_str());
}

void pc_netplay_launch_apply_captains(void)
{
	if (!sSetup.active || sSetup.isHost || !sSetup.haveCaptains) return;
	pc_coop_set_captain(0, sSetup.captainP1);
	pc_coop_set_captain(1, sSetup.captainP2);
}
