#pragma once
// Netplay M5c lane C (issue #887): recovery v1, "continue the campaign".
// Engine-free and header-only (no SDL, no game, no file I/O), so the
// host-run test (pc_netplay_continue_test) links nothing else. The launcher
// (pc_netplay_launch.cpp) does the directory walk and the copies; the session
// (pc_netplay_session.cpp) writes the campaign record and prints the final
// recovery message. Everything here is pure string work.
//
// Pieces:
//   parse_run_name / run_newer   the launcher's run folder names
//                                (run-YYYYMMDD-HHMMSS-host|join-pid<N>[-k]),
//                                newest first
//   launch_value                 one "key value" line of <run>/launch.txt
//   bootstrap_fingerprint        the FINGERPRINT a checkpoint must carry
//   checkpoint_gen_from_name     "<20 digits>.sav" -> generation
//   check_checkpoint             the loadCampaignCheckpoint integrity rules
//                                that need no bootstrap flags: header
//                                fingerprint and generation, the 32 KiB block
//                                and the FNV-1a 64 hash over them
//   parse_record / pick_generation
//                                <run>/campaign-record.txt, the lines the
//                                session writes, and which checkpoint a
//                                --continue may use (only a day both games
//                                agreed on: never a half-saved day)
//   record_line_*                the record's line formats
//   recovery_lines               the final console message on a session end
//
// Campaign record (<run>/campaign-record.txt, one event per line, appended;
// written in launcher mode by both roles; never read by the game itself):
//   carried gen=G day=D from=<run folder>   --continue copied checkpoint G
//                                           (day D, 0 = unknown) into this
//                                           run before the session started
//   start gen=G                             the session started on checkpoint
//                                           G (0 = a new campaign), after the
//                                           handshake agreed it on both sides
//   saved gen=G frame=F day_ended=D         a day-end save both games agreed
//                                           on (the save barrier's verdict)
//   day gen=G day=D                         checkpoint G plays on from day D
//                                           (the first day start after it)
//   abandoned gen=G exit=C                  a day-end save that was NOT
//                                           agreed (exit 5 desync / 6
//                                           timeout): never continued from
//   end kind=K code=C frame=F               how the session ended
// Confirmed generations are the ones named by carried/start/saved. A run
// folder without a record (an older build) counts every valid checkpoint as
// confirmed, which is what that build's resume already did.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace pc_netplay_continue {

// ---- run folder names ----

struct RunName {
	std::string stamp; // "YYYYMMDD-HHMMSS"
	bool host = false;
	unsigned long pid = 0;
	unsigned seq = 0; // the "-k" suffix of an exclusive-create retry (0 = none)
};

inline bool all_digits(const std::string& s, size_t from, size_t count)
{
	if (from + count > s.size()) return false;
	for (size_t i = from; i < from + count; ++i) {
		if (s[i] < '0' || s[i] > '9') return false;
	}
	return true;
}

// run-YYYYMMDD-HHMMSS-host-pid123 or ...-join-pid123, optionally "-<k>".
inline bool parse_run_name(const std::string& name, RunName* out)
{
	// "run-" + 8 digits + "-" + 6 digits = 19 chars.
	if (name.size() < 19 || name.compare(0, 4, "run-") != 0) return false;
	if (!all_digits(name, 4, 8) || name[12] != '-' || !all_digits(name, 13, 6)) return false;
	RunName r;
	r.stamp = name.substr(4, 15);
	size_t at = 19;
	if (name.compare(at, 6, "-host-") == 0) r.host = true;
	else if (name.compare(at, 6, "-join-") == 0) r.host = false;
	else return false;
	at += 6;
	if (name.compare(at, 3, "pid") != 0) return false;
	at += 3;
	const size_t pidStart = at;
	while (at < name.size() && name[at] >= '0' && name[at] <= '9') ++at;
	if (at == pidStart || at - pidStart > 10) return false;
	r.pid = std::stoul(name.substr(pidStart, at - pidStart));
	if (at < name.size()) {
		if (name[at] != '-') return false;
		++at;
		const size_t seqStart = at;
		while (at < name.size() && name[at] >= '0' && name[at] <= '9') ++at;
		if (at == seqStart || at != name.size() || at - seqStart > 3) return false;
		r.seq = (unsigned)std::stoul(name.substr(seqStart, at - seqStart));
	}
	if (out != nullptr) *out = r;
	return true;
}

// Newest first: later stamp, then the later exclusive-create retry, then the
// higher pid (two sessions started in the same second: a deterministic tie
// break, not a claim about which is newer).
inline bool run_newer(const RunName& a, const RunName& b)
{
	if (a.stamp != b.stamp) return a.stamp > b.stamp;
	if (a.seq != b.seq) return a.seq > b.seq;
	return a.pid > b.pid;
}

// ---- small text readers ----

// The value of the first "key value" line of launch.txt ("" when absent). The
// value is the rest of the line (paths may hold spaces), without a trailing CR.
inline std::string launch_value(const std::string& text, const std::string& key)
{
	size_t pos = 0;
	while (pos < text.size()) {
		size_t eol = text.find('\n', pos);
		if (eol == std::string::npos) eol = text.size();
		std::string line = text.substr(pos, eol - pos);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.size() > key.size() && line.compare(0, key.size(), key) == 0 && line[key.size()] == ' ')
			return line.substr(key.size() + 1);
		pos = eol + 1;
	}
	return std::string();
}

// The bootstrap's FINGERPRINT token ("" when absent). The randomizer reads
// the file as whitespace-separated tokens; the launcher's bootstraps carry it
// on its own line, and so does every seed the generator writes.
inline std::string bootstrap_fingerprint(const std::string& text)
{
	auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; };
	size_t i = 0;
	const size_t n = text.size();
	bool next = false;
	while (i < n) {
		while (i < n && ws(text[i])) ++i;
		const size_t start = i;
		while (i < n && !ws(text[i])) ++i;
		if (i == start) break;
		const std::string tok = text.substr(start, i - start);
		if (next) return tok;
		next = tok == "FINGERPRINT";
	}
	return std::string();
}

// "<20 digits>.sav" -> generation. Exactly the randomizer's naming rule.
inline bool checkpoint_gen_from_name(const std::string& name, unsigned long long* gen)
{
	if (name.size() != 24 || name.compare(20, 4, ".sav") != 0 || !all_digits(name, 0, 20)) return false;
	if (gen != nullptr) *gen = std::stoull(name.substr(0, 20));
	return true;
}

inline std::string checkpoint_name(unsigned long long gen)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%020llu.sav", gen);
	return buf;
}

// ---- checkpoint integrity ----

constexpr size_t kCheckpointBlock = 32768;

enum class CkptCheck { Ok, BadHeader, Fingerprint, Generation, Size, Hash };

inline const char* ckpt_check_name(CkptCheck c)
{
	switch (c) {
	case CkptCheck::Ok: return "ok";
	case CkptCheck::BadHeader: return "bad header";
	case CkptCheck::Fingerprint: return "another seed's checkpoint";
	case CkptCheck::Generation: return "generation does not match the file name";
	case CkptCheck::Size: return "wrong size";
	case CkptCheck::Hash: return "damaged (hash mismatch)";
	}
	return "?";
}

inline uint64_t fnv1a64(const std::string& bytes)
{
	uint64_t hash = 14695981039346656037ULL;
	for (unsigned char byte : bytes) {
		hash ^= byte;
		hash *= 1099511628211ULL;
	}
	return hash;
}

// pc_randomizer.cpp write_campaign_checkpoint():
//   "<magic> <fingerprint> <gen> <used...> <hash>\n" + 32768-byte block,
//   hash = FNV-1a 64 over "<header without ' <hash>'>\n<block>".
// The used-count list length depends on the bootstrap's flags; the hash does
// not, so every rule here holds without them (the randomizer re-checks the
// rest when it loads the copy).
inline CkptCheck check_checkpoint(const std::string& bytes, const std::string& fingerprint,
                                  unsigned long long gen)
{
	const size_t eol = bytes.find('\n');
	if (eol == std::string::npos || eol == 0 || eol > 512) return CkptCheck::BadHeader;
	const std::string header = bytes.substr(0, eol);
	if (header.compare(0, 16, "PIKMIN_CAMPAIGN_") != 0) return CkptCheck::BadHeader;
	std::vector<std::string> tok;
	size_t p = 0;
	while (p < header.size()) {
		while (p < header.size() && header[p] == ' ') ++p;
		const size_t s = p;
		while (p < header.size() && header[p] != ' ') ++p;
		if (p > s) tok.push_back(header.substr(s, p - s));
	}
	if (tok.size() < 5) return CkptCheck::BadHeader; // magic fp gen >=1 used hash
	if (tok[1] != fingerprint) return CkptCheck::Fingerprint;
	for (char c : tok[2]) {
		if (c < '0' || c > '9') return CkptCheck::BadHeader;
	}
	if (tok[2].empty() || tok[2].size() > 20 || std::stoull(tok[2]) != gen) return CkptCheck::Generation;
	if (bytes.size() != eol + 1 + kCheckpointBlock) return CkptCheck::Size;
	const std::string& hashTok = tok.back();
	for (char c : hashTok) {
		if (c < '0' || c > '9') return CkptCheck::BadHeader;
	}
	if (hashTok.empty() || hashTok.size() > 20) return CkptCheck::BadHeader;
	const size_t cut = header.rfind(' ');
	const std::string signedPart = header.substr(0, cut) + "\n" + bytes.substr(eol + 1);
	if (std::to_string(fnv1a64(signedPart)) != hashTok) return CkptCheck::Hash;
	return CkptCheck::Ok;
}

// ---- campaign record ----

struct Record {
	bool present = false;               // the file exists (a record-writing build made the run)
	unsigned long long confirmedMax = 0; // newest agreed generation (carried/start/saved)
	std::map<unsigned long long, int> dayOf; // generation -> day it plays on from
	std::map<unsigned long long, int> dayEnded; // generation -> day whose end it saved
	std::vector<unsigned long long> abandoned;
	std::string carriedFrom;            // the run a --continue copied from
	std::string endKind;                // last "end kind=" value
	int endCode = -1;
};

// "key=value" from one record line ("" when absent).
inline std::string record_field(const std::string& line, const std::string& key)
{
	const std::string k = " " + key + "=";
	const size_t at = (" " + line).find(k);
	if (at == std::string::npos) return std::string();
	const size_t v = at + k.size() - 1; // index in `line`
	size_t e = v;
	if (key == "from") return line.substr(v); // a path: the rest of the line
	while (e < line.size() && line[e] != ' ') ++e;
	return line.substr(v, e - v);
}

inline bool record_u64(const std::string& line, const std::string& key, unsigned long long* out)
{
	const std::string v = record_field(line, key);
	if (v.empty() || v.size() > 20) return false;
	for (char c : v) {
		if (c < '0' || c > '9') return false;
	}
	*out = std::stoull(v);
	return true;
}

inline Record parse_record(const std::string& text, bool present = true)
{
	Record r;
	r.present = present;
	size_t pos = 0;
	while (pos < text.size()) {
		size_t eol = text.find('\n', pos);
		if (eol == std::string::npos) eol = text.size();
		std::string line = text.substr(pos, eol - pos);
		pos = eol + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty() || line[0] == '#') continue;
		const size_t sp = line.find(' ');
		const std::string kind = line.substr(0, sp);
		unsigned long long gen = 0, day = 0, code = 0;
		const bool haveGen = record_u64(line, "gen", &gen);
		if (kind == "carried" || kind == "start" || kind == "saved") {
			if (!haveGen) continue;
			if (gen > r.confirmedMax) r.confirmedMax = gen;
			if (kind == "carried") {
				r.carriedFrom = record_field(line, "from");
				if (record_u64(line, "day", &day) && day > 0) r.dayOf[gen] = (int)day;
			}
			if (kind == "saved" && record_u64(line, "day_ended", &day) && day > 0) r.dayEnded[gen] = (int)day;
		} else if (kind == "day") {
			if (haveGen && record_u64(line, "day", &day) && day > 0) r.dayOf[gen] = (int)day;
		} else if (kind == "abandoned") {
			if (haveGen) r.abandoned.push_back(gen);
		} else if (kind == "end") {
			r.endKind = record_field(line, "kind");
			if (record_u64(line, "code", &code)) r.endCode = (int)code;
		}
	}
	return r;
}

// The checkpoint a --continue may use: the newest valid generation that the
// record confirms (every valid one when the run has no record). 0 = none.
inline unsigned long long pick_generation(const std::vector<unsigned long long>& validGens, const Record& rec)
{
	unsigned long long best = 0;
	for (unsigned long long g : validGens) {
		if (g == 0) continue;
		if (rec.present && g > rec.confirmedMax) continue; // never a half-saved day
		if (g > best) best = g;
	}
	return best;
}

inline std::string record_header()
{
	return "# netplay campaign record (nectar.exe, issue #887): which day-end saves both games agreed on.\n"
	       "# Read by --netplay-host-ice --continue; never read by the game itself.\n";
}
inline std::string record_line_carried(unsigned long long gen, int day, const std::string& from)
{
	return "carried gen=" + std::to_string(gen) + " day=" + std::to_string(day > 0 ? day : 0) + " from=" + from + "\n";
}
inline std::string record_line_start(unsigned long long gen, bool host)
{
	return "start gen=" + std::to_string(gen) + " role=" + (host ? "host" : "join") + "\n";
}
inline std::string record_line_saved(unsigned long long gen, uint32_t frame, int dayEnded)
{
	return "saved gen=" + std::to_string(gen) + " frame=" + std::to_string(frame) + " day_ended=" +
	       std::to_string(dayEnded > 0 ? dayEnded : 0) + "\n";
}
inline std::string record_line_day(unsigned long long gen, int day)
{
	return "day gen=" + std::to_string(gen) + " day=" + std::to_string(day) + "\n";
}
inline std::string record_line_abandoned(unsigned long long gen, int exitCode)
{
	return "abandoned gen=" + std::to_string(gen) + " exit=" + std::to_string(exitCode) + "\n";
}
inline std::string record_line_end(const char* kind, int code, uint64_t frame)
{
	return std::string("end kind=") + kind + " code=" + std::to_string(code) + " frame=" + std::to_string(frame) + "\n";
}

// ---- the final recovery message ----

enum class EndKind {
	Desync,        // GekkoNet checksum mismatch (exit 5)
	SaveDesync,    // the day-end save differed between the games (exit 5)
	SaveTimeout,   // the day-end save was not agreed in time (exit 6)
	Disconnect,    // no data from the other game for the disconnect timeout
	PeerQuit,      // the other game left (its quit notice, or it closed)
	LocalQuit,     // this player closed the game
};

inline const char* end_kind_name(EndKind k)
{
	switch (k) {
	case EndKind::Desync: return "desync";
	case EndKind::SaveDesync: return "save-desync";
	case EndKind::SaveTimeout: return "save-timeout";
	case EndKind::Disconnect: return "disconnect";
	case EndKind::PeerQuit: return "peer-quit";
	case EndKind::LocalQuit: return "quit";
	}
	return "?";
}

struct EndInfo {
	EndKind kind = EndKind::Disconnect;
	bool host = true;
	bool launcher = true;      // --netplay-host-ice / --netplay-join-ice (else the low-level switches)
	uint64_t frame = 0;        // Desync: the frame whose checksums differed; else the last frame advanced
	unsigned long long gen = 0; // newest checkpoint both games agreed on (0 = none)
	int day = 0;               // the day that checkpoint plays on from (0 = unknown)
	int dayEnded = 0;          // the day whose end it saved (0 = unknown)
	std::string exe;           // this exe's file name, for the command line
	std::string extraArgs;     // switches to repeat (for example "--netplay-input keyboard")
};

// What happened, in one line, per kind.
inline std::string end_headline(const EndInfo& e)
{
	switch (e.kind) {
	case EndKind::Desync:
		return "DESYNC: the two games stopped agreeing about the game at frame " + std::to_string(e.frame) +
		       ", so the session stopped.";
	case EndKind::SaveDesync:
		return "DESYNC AT THE DAY-END SAVE: the two games saved different days, so that save does not count.";
	case EndKind::SaveTimeout:
		return "SAVE NOT AGREED: the other game did not finish the day-end save with this one, so that day is "
		       "not saved.";
	case EndKind::Disconnect:
		return "CONNECTION LOST: no data from the other game for too long (it may have crashed or lost its "
		       "network).";
	case EndKind::PeerQuit:
		return "THE OTHER PLAYER LEFT: their game closed the session.";
	case EndKind::LocalQuit:
		return "You left the session.";
	}
	return "The session ended.";
}

// The saved-day line.
inline std::string saved_day_line(const EndInfo& e)
{
	if (e.gen == 0)
		return "Nothing is saved yet: no day of this campaign ended with a save both games agreed on.";
	std::string s = "Last saved day: ";
	if (e.day > 0) s += "day " + std::to_string(e.day) + " (the campaign continues from the start of day " +
		                     std::to_string(e.day) + ")";
	else if (e.dayEnded > 0) s += "the end of day " + std::to_string(e.dayEnded) + " (the campaign continues "
		                                                                               "on the next day)";
	else s += "campaign checkpoint " + std::to_string(e.gen);
	s += "; checkpoint " + std::to_string(e.gen) + ".";
	return s;
}

// A program in the game's folder, as typed in a console opened there. The
// ".\" prefix is what PowerShell needs (it does not run programs from the
// current folder by bare name), and Command Prompt accepts it too. A name
// with anything but letters, digits, '.', '_' or '-' is quoted for
// PowerShell with its call operator: & '.\my game.exe'.
inline std::string local_command(const std::string& file)
{
	bool plain = !file.empty();
	for (char c : file) {
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
		                c == '_' || c == '-';
		if (!ok) plain = false;
	}
	if (plain) return ".\\" + file;
	std::string quoted;
	for (char c : file) {
		quoted += c;
		if (c == '\'') quoted += '\''; // PowerShell doubles a quote inside '...'
	}
	return "& '.\\" + quoted + "'";
}

// One final-message line as the end banner shows it. The banner draws with
// the game's own font, which has no backslash glyph (0x5C comes out as
// another letter: ".\host.bat" read ",Ahost.bat" in a capture), so the banner
// shows each command in its PowerShell form with '/' (./host.bat), which
// Command Prompt does not take, and says so; the console message keeps the
// ".\" form that both consoles take.
inline std::string banner_line(std::string l)
{
	const std::string both = "(PowerShell or Command Prompt)";
	const size_t at = l.find(both);
	if (at != std::string::npos) l.replace(at, both.size(), "(PowerShell; the console window has the Command Prompt form)");
	for (char& c : l) {
		if (c == '\\') c = '/';
	}
	return l;
}

// Every line of the final message, without the "[netplay] " prefix. The
// commands are typed in a console opened in the game's folder (PowerShell
// or Command Prompt: see local_command).
inline std::vector<std::string> recovery_lines(const EndInfo& e)
{
	std::vector<std::string> out;
	out.push_back("==== netplay session ended ====");
	out.push_back(end_headline(e));
	out.push_back(saved_day_line(e));
	const std::string exe   = local_command(e.exe.empty() ? std::string("nectar.exe") : e.exe);
	const std::string extra = e.extraArgs.empty() ? std::string() : " " + e.extraArgs;
	if (!e.launcher) {
		out.push_back("To carry on, start both games again with the same switches; the host's last saved day is "
		              "sent to the joiner at the handshake.");
		return out;
	}
	if (e.gen == 0) {
		if (e.host)
			out.push_back("To play again: run .\\host.bat (or " + exe + " --netplay-host-ice" + extra +
			              ") in the game's folder and your partner joins as before; that starts a new campaign.");
		else
			out.push_back("To play again: the host runs .\\host.bat and you join as before (.\\join.bat); that "
			              "starts a new campaign.");
		return out;
	}
	if (e.host) {
		out.push_back("To carry on from that day, run this in the game's folder (PowerShell or Command Prompt):");
		out.push_back("  .\\host.bat --continue");
		out.push_back("  or: " + exe + " --netplay-host-ice --continue" + extra);
		out.push_back("Your partner joins as usual (.\\join.bat); your saved day is sent to them automatically.");
	} else {
		out.push_back("To carry on from that day: the host runs .\\host.bat --continue (or " + exe +
		              " --netplay-host-ice --continue) in the game's folder.");
		out.push_back("You join as usual (.\\join.bat); the host's saved day is sent to you automatically.");
	}
	return out;
}

} // namespace pc_netplay_continue
