// Netplay M5c lane C (issue #887): host-run test for the continue/recovery
// pure pieces (pc_netplay_continue.h, header-only, no SDL/game).
//
// Covers: run folder names and newest-first order, launch.txt values, the
// bootstrap FINGERPRINT, checkpoint names and the integrity rules against a
// checkpoint built exactly like pc_randomizer.cpp write_campaign_checkpoint,
// the campaign record (confirmed generations, days, abandoned saves, legacy
// runs without a record) and which generation --continue picks (never a
// half-saved day), and the final recovery message per end kind and role.

#include "netplay/pc_netplay_continue.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int sFailures = 0;
int sChecks   = 0;

void check(bool ok, const char* what, int line)
{
	++sChecks;
	if (!ok) {
		++sFailures;
		std::printf("FAIL line %d: %s\n", line, what);
	}
}
#define CHECK(ok, what) check((ok), (what), __LINE__)

using namespace pc_netplay_continue;

// pc_randomizer.cpp write_campaign_checkpoint(), restated.
std::string make_checkpoint(const std::string& fp, unsigned long long gen, int usedCount, char fill)
{
	std::string meta = "PIKMIN_CAMPAIGN_1 " + fp + " " + std::to_string(gen);
	for (int i = 0; i < usedCount; ++i) meta += " " + std::to_string(i);
	std::string block(kCheckpointBlock, fill);
	block[0] = 'x';
	block[kCheckpointBlock - 1] = '\n'; // a newline inside the block must not confuse the header parse
	const uint64_t hash = fnv1a64(meta + "\n" + block);
	return meta + " " + std::to_string(hash) + "\n" + block;
}

bool contains(const std::vector<std::string>& lines, const std::string& needle)
{
	for (const std::string& l : lines) {
		if (l.find(needle) != std::string::npos) return true;
	}
	return false;
}

} // namespace

int main()
{
	// 1. Run folder names.
	{
		RunName a, b, c, d;
		CHECK(parse_run_name("run-20260929-101500-host-pid1234", &a) && a.host && a.pid == 1234 && a.seq == 0
		          && a.stamp == "20260929-101500",
		      "host run name");
		CHECK(parse_run_name("run-20260929-101500-join-pid99-3", &b) && !b.host && b.pid == 99 && b.seq == 3,
		      "joiner run name with an exclusive-create retry");
		CHECK(!parse_run_name("run-2026092-101500-host-pid1", nullptr), "short date refused");
		CHECK(!parse_run_name("run-20260929-101500-peer-pid1", nullptr), "unknown role refused");
		CHECK(!parse_run_name("run-20260929-101500-host-pid", nullptr), "missing pid refused");
		CHECK(!parse_run_name("run-20260929-101500-host-pid1-x", nullptr), "bad retry suffix refused");
		CHECK(!parse_run_name("sidecar-set-aside-1", nullptr), "other folders refused");
		CHECK(parse_run_name("run-20260930-000001-host-pid1", &c) && run_newer(c, a), "a later stamp is newer");
		CHECK(parse_run_name("run-20260929-101500-host-pid1-1", &d) && run_newer(d, a), "a retry of the same second is newer");
		CHECK(!run_newer(a, a), "newer is strict");
		std::vector<RunName> v = { a, c, d };
		std::sort(v.begin(), v.end(), run_newer);
		CHECK(v[0].stamp == "20260930-000001" && v[1].seq == 1 && v[2].seq == 0, "newest first");
	}

	// 2. launch.txt and the bootstrap fingerprint.
	{
		const std::string launch = "role host\r\ntoken abc\nbootstrap C:/a b/boot.txt\nseed 7\n";
		CHECK(launch_value(launch, "role") == "host", "role (CRLF stripped)");
		CHECK(launch_value(launch, "bootstrap") == "C:/a b/boot.txt", "a value keeps its spaces");
		CHECK(launch_value(launch, "seed") == "7", "seed");
		CHECK(launch_value(launch, "see").empty(), "a key prefix is not a key");
		CHECK(launch_value(launch, "campaign").empty(), "absent key");
		const std::string boot = "PIKMIN_RANDOMIZER 5\nSESSION aa\nFINGERPRINT 0123abcd\nPROFILE foh-day2\nEND\n";
		CHECK(bootstrap_fingerprint(boot) == "0123abcd", "fingerprint");
		CHECK(bootstrap_fingerprint("PIKMIN_RANDOMIZER 5\nEND\n").empty(), "no fingerprint");
	}

	// 3. Checkpoint names and integrity.
	{
		unsigned long long g = 0;
		CHECK(checkpoint_gen_from_name("00000000000000000007.sav", &g) && g == 7, "gen from name");
		CHECK(checkpoint_name(7) == "00000000000000000007.sav", "name from gen");
		CHECK(!checkpoint_gen_from_name("7.sav", nullptr), "short name");
		CHECK(!checkpoint_gen_from_name("00000000000000000007.sav.pending", nullptr), "pending is not a checkpoint");
		CHECK(!checkpoint_gen_from_name("00000000000000000007.sav.unconfirmed", nullptr), "unconfirmed is not a checkpoint");
		const std::string fp = std::string(64, 'f');
		const std::string ok = make_checkpoint(fp, 2, 3, 'a');
		CHECK(check_checkpoint(ok, fp, 2) == CkptCheck::Ok, "a checkpoint written like the randomizer's passes");
		CHECK(check_checkpoint(make_checkpoint(fp, 2, 7, 'q'), fp, 2) == CkptCheck::Ok, "7 used counts (schema 5)");
		CHECK(check_checkpoint(ok, std::string(64, 'e'), 2) == CkptCheck::Fingerprint, "another seed");
		CHECK(check_checkpoint(ok, fp, 3) == CkptCheck::Generation, "gen mismatch with the name");
		std::string cut = ok;
		cut.pop_back();
		CHECK(check_checkpoint(cut, fp, 2) == CkptCheck::Size, "truncated");
		std::string flip = ok;
		flip[flip.size() - 100] ^= 1;
		CHECK(check_checkpoint(flip, fp, 2) == CkptCheck::Hash, "one flipped block byte");
		CHECK(check_checkpoint("garbage", fp, 2) == CkptCheck::BadHeader, "garbage");
		CHECK(check_checkpoint("PIKMIN_CAMPAIGN_1 x\n" + std::string(kCheckpointBlock, 'a'), "x", 2)
		          == CkptCheck::BadHeader,
		      "too few header fields");
	}

	// 4. The campaign record and the pick.
	{
		const std::string rec = record_header() + record_line_carried(1, 3, "C:/games/netplay/run-1 x")
		                      + record_line_start(1, true) + record_line_day(1, 3)
		                      + record_line_saved(2, 58000, 3) + record_line_day(2, 4)
		                      + record_line_abandoned(3, 6) + record_line_end("save-timeout", 6, 90000);
		const Record r = parse_record(rec);
		CHECK(r.present && r.confirmedMax == 2, "confirmed max is the newest agreed save");
		CHECK(r.dayOf.at(1) == 3 && r.dayOf.at(2) == 4, "days per generation");
		CHECK(r.dayEnded.at(2) == 3, "day ended by the save");
		CHECK(r.carriedFrom == "C:/games/netplay/run-1 x", "carried-from path keeps spaces");
		CHECK(r.abandoned.size() == 1 && r.abandoned[0] == 3, "abandoned save");
		CHECK(r.endKind == "save-timeout" && r.endCode == 6, "end line");
		CHECK(pick_generation({ 1, 2, 3 }, r) == 2, "an abandoned (half-saved) day is never picked");
		CHECK(pick_generation({ 1 }, r) == 1, "the newest valid confirmed checkpoint");
		CHECK(pick_generation({}, r) == 0, "nothing valid");
		const Record fresh = parse_record(record_header() + record_line_start(0, true));
		CHECK(fresh.present && fresh.confirmedMax == 0, "a new campaign confirms nothing");
		CHECK(pick_generation({ 1 }, fresh) == 0, "a checkpoint written but never agreed is not continued");
		const Record legacy = parse_record("", false);
		CHECK(pick_generation({ 1, 4, 2 }, legacy) == 4, "a run without a record: every valid checkpoint counts");
		const Record junk = parse_record("saved gen=x\nday gen=1\nstart\n# comment gen=9\n");
		CHECK(junk.confirmedMax == 0 && junk.dayOf.empty(), "malformed lines are ignored");
		CHECK(record_field("day gen=1 day_ended=5 day=4", "day") == "4", "day= is not day_ended=");
	}

	// 5. The final message.
	{
		EndInfo e;
		e.kind = EndKind::Desync;
		e.host = true;
		e.frame = 30012;
		e.gen = 1;
		e.day = 3;
		e.exe = "nectar.exe";
		e.extraArgs = "--netplay-input keyboard";
		std::vector<std::string> l = recovery_lines(e);
		CHECK(contains(l, "DESYNC") && contains(l, "at frame 30012"), "what happened");
		CHECK(contains(l, "Last saved day: day 3"), "the last saved day");
		CHECK(contains(l, "  or: .\\nectar.exe --netplay-host-ice --continue --netplay-input keyboard"),
		      "the exact command, PowerShell-ready (.\\ prefix)");
		CHECK(contains(l, "  .\\host.bat --continue"), "the .bat route with --continue");
		CHECK(contains(l, "PowerShell or Command Prompt"), "which consoles the commands are for");
		e.host = false;
		l = recovery_lines(e);
		CHECK(contains(l, "the host runs .\\host.bat --continue (or .\\nectar.exe --netplay-host-ice --continue)") &&
		          contains(l, "You join as usual"),
		      "joiner wording");
		e.kind = EndKind::SaveTimeout;
		e.gen = 0;
		e.day = 0;
		l = recovery_lines(e);
		CHECK(contains(l, "SAVE NOT AGREED") && contains(l, "Nothing is saved yet") && contains(l, "new campaign"),
		      "no saved day: a new campaign");
		CHECK(!contains(l, "--continue"), "no continue command without a saved day");
		CHECK(contains(l, "the host runs .\\host.bat and you join as before") && !contains(l, "--netplay-input"),
		      "joiner: the host's command, not this peer's switches");
		e.host = true;
		l = recovery_lines(e);
		CHECK(contains(l, "run .\\host.bat (or .\\nectar.exe --netplay-host-ice --netplay-input keyboard)"),
		      "host: its own command with its switches");
		CHECK(local_command("nectar.exe") == ".\\nectar.exe", "plain exe name: .\\ prefix");
		CHECK(local_command("nectar (2).exe") == "& '.\\nectar (2).exe'", "spaces: PowerShell call operator");
		CHECK(local_command("it's.exe") == "& '.\\it''s.exe'", "a quote is doubled");
		// The banner font has no backslash: PowerShell's '/' form, labelled.
		CHECK(banner_line("  .\\host.bat --continue") == "  ./host.bat --continue", "banner: ./ form");
		CHECK(banner_line("run this in the game's folder (PowerShell or Command Prompt):") ==
		          "run this in the game's folder (PowerShell; the console window has the Command Prompt form):",
		      "banner: says the / form is PowerShell's");
		for (const std::string& ln : recovery_lines(e)) {
			CHECK(banner_line(ln).find('\\') == std::string::npos, "banner: no backslash left");
		}
		e.kind = EndKind::PeerQuit;
		e.gen = 2;
		e.day = 0;
		e.dayEnded = 3;
		e.host = true;
		l = recovery_lines(e);
		CHECK(contains(l, "OTHER PLAYER LEFT") && contains(l, "the end of day 3"), "day unknown: the day it ended");
		e.launcher = false;
		l = recovery_lines(e);
		CHECK(contains(l, "same switches") && !contains(l, "host.bat"), "low-level switches");
		CHECK(std::string(end_kind_name(EndKind::Disconnect)) == "disconnect", "kind names");
	}

	std::printf("pc_netplay_continue_test: %s (%d checks, %d failures)\n", sFailures == 0 ? "PASS" : "FAIL",
	            sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
