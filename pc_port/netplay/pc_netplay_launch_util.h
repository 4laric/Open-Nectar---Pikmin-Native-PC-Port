#pragma once
// Netplay launch lane (issue #887): engine-free pieces of the one-command
// launcher (pc_netplay_launch.cpp). Header-only and pure (no SDL, no game,
// no file I/O), so the host-run test (pc_netplay_launch_test) links nothing
// else.
//
//   restamp_session        SESSION re-stamp, exactly like the root M4c helper
//                          randomizer/netplay_mirror.py
//                          restamp_bootstrap_for_peer()
//   validate_bootstrap     what the launcher accepts as a session bootstrap
//                          (bounded, one SESSION line, no P2 sidecar seeds)
//   run_layout             the per-run, per-peer directory layout, chosen so
//                          the randomizer's derived campaign dir
//                          (bootstrap dir -> parent -> parent -> "campaign")
//                          lands inside the run dir
//   classify_code          which connection code the user pasted

#include <cstddef>
#include <cstdint>
#include <string>

namespace pc_netplay_launch_util {

// The v2 offer carries the bootstrap length in a u16 field (see
// pc_netplay_ice.h); the launcher refuses anything longer before it is
// read in full.
constexpr size_t kMaxBootstrapBytes = 65535;

// M4c `_is_hex(text, len)` for a token: lowercase hex only, 8..128 chars.
inline bool is_token(const std::string& t)
{
	if (t.size() < 8 || t.size() > 128) return false;
	for (char c : t) {
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
	}
	return true;
}

// Re-stamps the SESSION line exactly like the root M4c helper
// restamp_bootstrap_for_peer(bootstrap_text, peer_token):
//   * the text is split on "\n" and one trailing empty element is dropped;
//   * every line starting with "SESSION " becomes "SESSION <token>" (the
//     whole line is replaced, so a CRLF SESSION line loses its "\r"; every
//     other line keeps its bytes, "\r" included);
//   * exactly one SESSION line must exist, else refuse;
//   * the stamped text must differ from the input, else refuse;
//   * the result is the lines joined with "\n" plus a final "\n".
// Returns false with a one-line reason in *err.
inline bool restamp_session(const std::string& text, const std::string& token, std::string* out,
                            std::string* err)
{
	auto fail = [&](const char* why) {
		if (err != nullptr) *err = why;
		return false;
	};
	if (!is_token(token)) return fail("invalid peer token");
	std::string result;
	result.reserve(text.size() + token.size() + 16);
	size_t sessions = 0;
	bool changed    = false;
	size_t pos      = 0;
	const size_t n  = text.size();
	// Number of "\n"-separated elements is (count of '\n') + 1; the last one
	// is dropped when empty (text ends with "\n" or is empty).
	while (pos <= n) {
		const size_t eol = text.find('\n', pos);
		const size_t end = (eol == std::string::npos) ? n : eol;
		const bool last  = (eol == std::string::npos);
		if (last && end == pos) break; // trailing empty element dropped
		const std::string line = text.substr(pos, end - pos);
		if (line.compare(0, 8, "SESSION ") == 0) {
			++sessions;
			const std::string stamped = "SESSION " + token;
			if (stamped != line) changed = true;
			result += stamped;
		} else {
			result += line;
		}
		result += '\n';
		if (last) break;
		pos = eol + 1;
	}
	if (sessions != 1) return fail("bootstrap has no single SESSION line");
	if (!changed) return fail("bootstrap SESSION line unchanged");
	*out = result;
	return true;
}

// True when a line of the bootstrap needs the P2 enemy bridge sidecars:
// ENEMY_P2 (and its P2_PROXY_TIER continuation). Those seeds read per-run
// files from the working directory (p2-*-actors/bank/profile.txt, an
// assets/ overlay, p2-binding-receipt.json, several hundred KB in total)
// that a connection code cannot carry and the handshake does not hash.
inline bool line_needs_p2_sidecars(const std::string& line)
{
	return line.compare(0, 8, "ENEMY_P2") == 0 || line.compare(0, 3, "P2_") == 0;
}

// What the launcher accepts as a session bootstrap, before the randomizer's
// own parser sees it:
//   * non-empty and at most kMaxBootstrapBytes;
//   * no NUL bytes;
//   * starts with "PIKMIN_RANDOMIZER ";
//   * exactly one "SESSION " line (the re-stamp rule);
//   * no P2 enemy-bridge lines (refused: sidecars cannot travel).
// Returns false with a one-line reason; *p2 is set when the refusal is the
// P2 one (so the caller can print the longer explanation).
inline bool validate_bootstrap(const std::string& text, std::string* err, bool* p2 = nullptr)
{
	if (p2 != nullptr) *p2 = false;
	auto fail = [&](const std::string& why) {
		if (err != nullptr) *err = why;
		return false;
	};
	if (text.empty()) return fail("bootstrap is empty");
	if (text.size() > kMaxBootstrapBytes)
		return fail("bootstrap is larger than " + std::to_string(kMaxBootstrapBytes) + " bytes");
	if (text.find('\0') != std::string::npos) return fail("bootstrap contains NUL bytes");
	if (text.compare(0, 18, "PIKMIN_RANDOMIZER ") != 0)
		return fail("not a randomizer bootstrap (missing PIKMIN_RANDOMIZER header)");
	size_t sessions = 0;
	size_t pos      = 0;
	while (pos < text.size()) {
		size_t eol = text.find('\n', pos);
		if (eol == std::string::npos) eol = text.size();
		std::string line = text.substr(pos, eol - pos);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.compare(0, 8, "SESSION ") == 0) ++sessions;
		if (line_needs_p2_sidecars(line)) {
			if (p2 != nullptr) *p2 = true;
			return fail("bootstrap uses P2 enemies (" + line.substr(0, line.find(' ')) + ")");
		}
		pos = eol + 1;
	}
	if (sessions != 1) return fail("bootstrap has no single SESSION line");
	return true;
}

// Per-run, per-peer layout under the run dir R (absolute):
//   R/session/runs/<token>/bootstrap.txt   --randomizer-seed target
//   R/session/campaign/                    derived campaign dir (card,
//                                          day-end checkpoints)
//   R/save/                                NECTAR_SAVE_DIR (shader cache,
//                                          non-randomizer card fallback)
// The randomizer derives campaign = bootstrap dir .. .. / "campaign"
// (pc_randomizer.cpp), so R/session/campaign is private to this run and
// this peer as long as R is.
struct RunLayout {
	std::string bootstrapDir;
	std::string bootstrapPath;
	std::string campaignDir;
	std::string saveDir;
};

inline RunLayout run_layout(const std::string& runDir, const std::string& token)
{
	RunLayout l;
	l.bootstrapDir  = runDir + "/session/runs/" + token;
	l.bootstrapPath = l.bootstrapDir + "/bootstrap.txt";
	l.campaignDir   = runDir + "/session/campaign";
	l.saveDir       = runDir + "/save";
	return l;
}

// The randomizer's derivation, restated over '/'-separated strings for the
// test: parent(parent(dirname(bootstrapPath))) + "/campaign".
inline std::string derived_campaign_dir(const std::string& bootstrapPath)
{
	std::string p = bootstrapPath;
	for (int i = 0; i < 3; ++i) {
		const size_t slash = p.find_last_of("/\\");
		if (slash == std::string::npos) return std::string();
		p.erase(slash);
	}
	return p + "/campaign";
}

enum CodeKind {
	kCodeEmpty,
	kCodeOfferV2, // "NPIX2-": the launcher's bundle offer
	kCodeV1,      // "NPIX1-": an M5a offer or any answer (decode tells which)
	kCodeGarbage, // anything else
};

inline CodeKind classify_code(const std::string& trimmed)
{
	if (trimmed.empty()) return kCodeEmpty;
	if (trimmed.compare(0, 6, "NPIX2-") == 0) return kCodeOfferV2;
	if (trimmed.compare(0, 6, "NPIX1-") == 0) return kCodeV1;
	return kCodeGarbage;
}

} // namespace pc_netplay_launch_util
