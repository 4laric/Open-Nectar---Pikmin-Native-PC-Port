// Netplay launch lane (issue #887): host-run test for the launcher's pure
// pieces (pc_netplay_launch_util.h, header-only, no SDL/game).
//
// Covers: the SESSION re-stamp against the root M4c helper's rules (m2), the
// bootstrap acceptance rules including the P2 sidecar refusal (M3) and the
// u16 size cap (m1), the per-run/per-peer layout and the randomizer's
// campaign-dir derivation (M1), and connection-code classification (m3).

#include "netplay/pc_netplay_launch_util.h"

#include <cstdio>
#include <string>

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

const std::string kTok(64, 'b');
const std::string kTok2(64, 'c');

std::string boot(const std::string& session)
{
	return "PIKMIN_RANDOMIZER 5\n" + session + "FINGERPRINT " + std::string(64, 'f')
	     + "\nPROFILE foh-day2\nCATALOG gameplay-checks-v5\nPLACEMENT identity-v1\nGOAL 25\n"
	       "DAYS repeat-day29-v1\nCOLOR red\nSTARTING_FLARLIC 10\nEND\n";
}

} // namespace

int main()
{
	using namespace pc_netplay_launch_util;

	// 1. Re-stamp, M4c rules: exactly one SESSION line, must change, lines
	//    joined with "\n" plus a final "\n", every other byte kept.
	{
		std::string out, err;
		const std::string in = boot("SESSION " + kTok + "\n");
		CHECK(restamp_session(in, kTok2, &out, &err), "one SESSION line re-stamps");
		CHECK(out == boot("SESSION " + kTok2 + "\n"), "only the SESSION line changes");
		CHECK(!restamp_session(in, kTok, &out, &err) && err == "bootstrap SESSION line unchanged",
		      "same token refused (M4c: SESSION line unchanged)");
		CHECK(!restamp_session(boot(""), kTok2, &out, &err) && err == "bootstrap has no single SESSION line",
		      "no SESSION line refused");
		CHECK(!restamp_session(boot("SESSION " + kTok + "\nSESSION " + kTok + "\n"), kTok2, &out, &err),
		      "two SESSION lines refused");
		CHECK(!restamp_session("", kTok2, &out, &err), "empty bootstrap refused");
		CHECK(!restamp_session(in, "XYZ", &out, &err) && err == "invalid peer token", "bad token refused");
		CHECK(!restamp_session(in, std::string(64, 'B'), &out, &err), "uppercase token refused (M4c _is_hex)");
		// "SESSION" needs its space: "SESSIONX" is not a SESSION line.
		CHECK(!restamp_session(boot("SESSIONX " + kTok + "\n"), kTok2, &out, &err),
		      "SESSIONX is not a SESSION line");
		// CRLF input: the SESSION line is replaced whole (its \r goes, as in
		// M4c); every other line keeps its \r.
		const std::string crlf = "PIKMIN_RANDOMIZER 5\r\nSESSION " + kTok + "\r\nEND\r\n";
		CHECK(restamp_session(crlf, kTok2, &out, &err)
		          && out == "PIKMIN_RANDOMIZER 5\r\nSESSION " + kTok2 + "\nEND\r\n",
		      "CRLF: only the SESSION line loses its CR");
		// No final newline: M4c appends one.
		CHECK(restamp_session("PIKMIN_RANDOMIZER 5\nSESSION " + kTok + "\nEND", kTok2, &out, &err)
		          && out == "PIKMIN_RANDOMIZER 5\nSESSION " + kTok2 + "\nEND\n",
		      "missing final newline is added (M4c join)");
		// Blank lines in the middle are kept.
		CHECK(restamp_session("A\n\nSESSION " + kTok + "\n\nEND\n", kTok2, &out, &err)
		          && out == "A\n\nSESSION " + kTok2 + "\n\nEND\n",
		      "inner blank lines kept");
	}

	// 2. Bootstrap acceptance.
	{
		std::string err;
		bool p2 = false;
		CHECK(validate_bootstrap(boot("SESSION " + kTok + "\n"), &err, &p2) && !p2, "stock bootstrap accepted");
		CHECK(!validate_bootstrap("", &err, &p2), "empty refused");
		CHECK(!validate_bootstrap("hello\nSESSION " + kTok + "\n", &err, &p2), "missing header refused");
		CHECK(!validate_bootstrap(boot(""), &err, &p2), "no SESSION refused");
		std::string nul = boot("SESSION " + kTok + "\n");
		nul[5]          = '\0';
		CHECK(!validate_bootstrap(nul, &err, &p2), "NUL bytes refused");
		// The real 918-byte P2 seeds carry ENEMY_P2 (+ optional P2_PROXY_TIER).
		const std::string withP2 = "PIKMIN_RANDOMIZER 9\nSESSION " + kTok
		                         + "\nENEMIES 0\nENEMY_P2 1 abc 1 5465461 76\nEND\n";
		CHECK(!validate_bootstrap(withP2, &err, &p2) && p2, "ENEMY_P2 refused as the P2 refusal (M3)");
		const std::string withTier = "PIKMIN_RANDOMIZER 9\nSESSION " + kTok + "\nP2_PROXY_TIER 1\nEND\n";
		CHECK(!validate_bootstrap(withTier, &err, &p2) && p2, "P2_PROXY_TIER refused as P2");
		const std::string crlfP2 = "PIKMIN_RANDOMIZER 9\r\nSESSION " + kTok + "\r\nENEMY_P2 1\r\nEND\r\n";
		CHECK(!validate_bootstrap(crlfP2, &err, &p2) && p2, "ENEMY_P2 on a CRLF line refused");
		// m1: the u16 cap, 65535 accepted, 65536 refused.
		std::string big = boot("SESSION " + kTok + "\n");
		big.resize(kMaxBootstrapBytes, ' ');
		CHECK(validate_bootstrap(big, &err, &p2), "65535-byte bootstrap accepted");
		big.push_back(' ');
		CHECK(!validate_bootstrap(big, &err, &p2) && !p2, "65536-byte bootstrap refused");
		static_assert(kMaxBootstrapBytes == 0xFFFF, "cap equals the u16 field range");
	}

	// 3. Layout: private campaign dir per run and per peer (M1).
	{
		const RunLayout host = run_layout("C:/game/netplay/run-20260928-120000-host-pid10", kTok);
		const RunLayout join = run_layout("C:/game/netplay/run-20260928-120000-join-pid11", kTok2);
		const RunLayout next = run_layout("C:/game/netplay/run-20260928-121500-host-pid12", kTok);
		CHECK(host.bootstrapPath == "C:/game/netplay/run-20260928-120000-host-pid10/session/runs/" + kTok
		                              + "/bootstrap.txt",
		      "bootstrap two levels inside the session dir");
		CHECK(derived_campaign_dir(host.bootstrapPath) == host.campaignDir,
		      "randomizer derivation lands on the run's own campaign dir");
		CHECK(host.campaignDir == "C:/game/netplay/run-20260928-120000-host-pid10/session/campaign",
		      "campaign dir inside the run dir");
		CHECK(derived_campaign_dir(join.bootstrapPath) != derived_campaign_dir(host.bootstrapPath),
		      "host and joiner never share a campaign dir");
		CHECK(derived_campaign_dir(next.bootstrapPath) != derived_campaign_dir(host.bootstrapPath),
		      "consecutive sessions never share a campaign dir");
		CHECK(derived_campaign_dir("C:/game/netplay/run-x/bootstrap.txt") == "C:/game/campaign",
		      "the old one-level layout derived the shared <exe dir>/campaign (the M1 bug)");
		CHECK(host.saveDir == "C:/game/netplay/run-20260928-120000-host-pid10/save", "save dir inside the run dir");
	}

	// 4. Code classification (m3).
	{
		CHECK(classify_code("") == kCodeEmpty, "empty");
		CHECK(classify_code("NPIX2-abc") == kCodeOfferV2, "v2 offer");
		CHECK(classify_code("NPIX1-abc") == kCodeV1, "v1 offer or answer");
		CHECK(classify_code("hello") == kCodeGarbage, "garbage");
		CHECK(classify_code("npix2-abc") == kCodeGarbage, "prefix is case-sensitive");
	}

	std::printf("pc_netplay_launch_test: %s (%d checks, %d failures)\n", sFailures == 0 ? "PASS" : "FAIL",
	            sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
