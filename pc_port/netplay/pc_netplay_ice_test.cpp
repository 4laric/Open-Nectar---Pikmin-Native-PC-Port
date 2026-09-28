// Netplay M5a host-run ICE test (issue #887).
//
// Two libjuice agents in one process connect with host candidates only
// (no STUN/TURN, so no external network is required). m4: libjuice excludes
// 127.x candidates unless JUICE_ENABLE_LOCALHOST_ADDRESS=1, so the selected
// pair is normally a LAN host candidate (for example 192.168.x), not
// loopback. The test therefore needs a live non-loopback IPv4 interface and
// may fail offline or on CI without one:
//   1. connection-code encode/decode round-trips, and garbage is rejected;
//   2. the copy-paste flow (offer -> answer -> apply) reaches COMPLETED on
//      both agents;
//   3. a 64 KiB transfer through the channel interface (1-byte channel
//      prefix, kChannelGekko) arrives intact, plus a handshake-channel
//      ping-pong.
// Engine-free: links juice + pc_netplay_ice only (plus gekkonet headers for
// the IceLink adapter type).

#include "netplay/pc_netplay_ice.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

namespace {

int sFailures = 0;

void check(bool ok, const char* what, int line)
{
	if (!ok) {
		++sFailures;
		std::printf("FAIL line %d: %s\n", line, what);
	}
}
#define CHECK(ok, what) check((ok), (what), __LINE__)

const char* kFakeSdp =
    "a=ice-ufrag:hostUfrag1234\r\n"
    "a=ice-pwd:hostPasswordPassword1234567890\r\n"
    "a=candidate:1 1 UDP 2113937151 192.168.2.61 50001 typ host\r\n"
    "a=candidate:2 1 UDP 16777215 74.125.250.129 19302 typ srflx\r\n"
    "a=end-of-candidates\r\n"
    "a=ice-options:ice2\r\n";

} // namespace

int main()
{
	using namespace pc_netplay_ice;

	// 1. Code round trip + strict rejection.
	{
		std::string offer, answer, err, sdp;
		bool isOffer = false;
		CHECK(ice_encode_code(true, kFakeSdp, &offer, &err), "encode offer");
		CHECK(ice_encode_code(false, kFakeSdp, &answer, &err), "encode answer");
		CHECK(offer.compare(0, 6, "NPIX1-") == 0 && offer.find('\n') == std::string::npos,
		      "offer is one line with version prefix");
		CHECK(ice_decode_code(offer, &isOffer, &sdp, &err) && isOffer && sdp == kFakeSdp,
		      "offer round trip");
		CHECK(ice_decode_code(answer, &isOffer, &sdp, &err) && !isOffer && sdp == kFakeSdp,
		      "answer round trip");
		// Garbage is rejected with a reason.
		const char* bad[] = {
			"",
			"not-a-code",
			"NPIX1-",
			"NPIX1-!!!not-base64!!!",
			"NPIX0-AAAA",
		};
		for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
			bool io = false;
			std::string s, e;
			CHECK(!ice_decode_code(bad[i], &io, &s, &e) && !e.empty(), "garbage rejected");
		}
		// Corrupted payload (flip a base64 char) fails the CRC/length check.
		{
			std::string corrupt = offer;
			CHECK(corrupt.size() > 12, "offer long enough to corrupt");
			corrupt[12] = (corrupt[12] == 'A' ? 'B' : 'A');
			bool io = false;
			std::string s, e;
			CHECK(!ice_decode_code(corrupt, &io, &s, &e), "corrupted code rejected");
		}
		// Truncated code is rejected.
		{
			bool io = false;
			std::string s, e;
			CHECK(!ice_decode_code(offer.substr(0, offer.size() / 2), &io, &s, &e),
			      "truncated code rejected");
		}
		// Role confusion is caught by the socket layer, but kinds decode.
		CHECK(offer != answer, "offer and answer differ");
	}

	// 2. Relay filter unit checks on synthetic SDP.
	{
		CHECK(ice_candidate_line_is_relay("a=candidate:3 1 UDP 41819903 10.0.0.5 50002 typ relay"),
		      "relay line detected");
		CHECK(!ice_candidate_line_is_relay(
		          "a=candidate:1 1 UDP 2113937151 192.168.2.61 50001 typ host"),
		      "host line is not relay");
		std::string filtered = ice_filter_relay_candidates(kFakeSdp);
		CHECK(filtered.find(" typ host") == std::string::npos, "host filtered out");
		CHECK(filtered.find(" typ srflx") == std::string::npos, "srflx filtered out");
		CHECK(filtered.find("a=ice-ufrag:") != std::string::npos, "ufrag kept");
		// m5: no extra blank line is emitted for a trailing newline.
		CHECK(filtered.empty() || filtered.compare(filtered.size() - 4, 4, "\r\n\r\n") != 0,
		      "no trailing blank line");
		{
			const std::string noCand = "a=ice-ufrag:x\r\na=ice-pwd:y\r\n";
			CHECK(ice_filter_relay_candidates(noCand) == noCand, "CRLF round trip");
		}
		CHECK(!ice_sdp_has_relay(kFakeSdp), "fake SDP has no relay");
		CHECK(ice_sdp_has_relay("a=candidate:3 1 UDP 1 10.0.0.5 9 typ relay\r\n"),
		      "relay SDP detected");
	}

	// 3. In-process loopback pair with host candidates only.
	IceNetConfig cfg; // no STUN, no TURN: host candidates over loopback
	IceSocket hostSock, joinSock;
	{
		std::string offer, answer, err;
		CHECK(hostSock.host_create_offer(cfg, &offer, &err), "host offer gathers");
		if (!offer.empty()) std::printf("ice_test: offer bytes=%llu\n", (unsigned long long)offer.size());
		bool isOffer = false;
		std::string sdp;
		CHECK(ice_decode_code(offer, &isOffer, &sdp, &err) && isOffer, "offer decodes");
		CHECK(joinSock.join_create_answer(cfg, offer, &answer, &err), "join answer gathers");
		if (!answer.empty())
			std::printf("ice_test: answer bytes=%llu\n", (unsigned long long)answer.size());
		CHECK(hostSock.host_apply_answer(answer, &err), "host applies answer");
		double hostMs = -1, joinMs = -1;
		CHECK(hostSock.wait_connected(30000, &hostMs, &err), "host completed");
		CHECK(joinSock.wait_connected(30000, &joinMs, &err), "join completed");
		std::printf("ice_test: completed host=%.0fms join=%.0fms\n", hostMs, joinMs);
		CHECK(hostMs >= 0 && joinMs >= 0, "completed times reported");
	}

	// 4. 64 KiB through the channel interface + handshake ping-pong.
	{
		// 64 KiB in 1024-byte payloads; first 4 bytes of each payload are
		// the big-endian chunk index so order can be verified.
		const size_t kChunks = 64, kPayload = 1024;
		std::vector<uint8_t> want(64 * 1024);
		for (size_t i = 0; i < want.size(); ++i) want[i] = (uint8_t)((i * 2654435761u) >> 8);
		for (size_t c = 0; c < kChunks; ++c) {
			uint8_t payload[kPayload];
			payload[0] = (uint8_t)((c >> 24) & 0xFF);
			payload[1] = (uint8_t)((c >> 16) & 0xFF);
			payload[2] = (uint8_t)((c >> 8) & 0xFF);
			payload[3] = (uint8_t)(c & 0xFF);
			for (size_t i = 4; i < kPayload; ++i)
				payload[i] = want[c * kPayload + i];
			CHECK(hostSock.send_payload(kChannelGekko, payload, kPayload), "gekko send");
		}
		std::vector<uint8_t> got(64 * 1024, 0);
		std::vector<char> seen(kChunks, 0);
		size_t gotChunks = 0;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
		while (gotChunks < kChunks && std::chrono::steady_clock::now() < deadline) {
			std::vector<IceSocket::Datagram> grams = joinSock.recv();
			if (grams.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
				continue;
			}
			for (size_t i = 0; i < grams.size(); ++i) {
				CHECK(grams[i].channel == kChannelGekko, "gekko channel intact");
				if (grams[i].payload.size() != kPayload) continue;
				size_t c = ((size_t)grams[i].payload[0] << 24)
				    | ((size_t)grams[i].payload[1] << 16)
				    | ((size_t)grams[i].payload[2] << 8) | grams[i].payload[3];
				if (c >= kChunks || seen[c]) continue;
				seen[c] = 1;
				++gotChunks;
				memcpy(got.data() + c * kPayload + 4, grams[i].payload.data() + 4,
				       kPayload - 4);
			}
		}
		CHECK(gotChunks == kChunks, "all 64 chunks arrived");
		bool intact = (gotChunks == kChunks);
		if (intact) {
			for (size_t c = 0; c < kChunks && intact; ++c) {
				for (size_t i = 4; i < kPayload; ++i) {
					if (got[c * kPayload + i] != want[c * kPayload + i]) {
						intact = false;
						break;
					}
				}
			}
		}
		CHECK(intact, "64 KiB intact");
		// Reverse direction over the handshake channel.
		const uint8_t ping[5] = { 'p', 'i', 'n', 'g', '!' };
		CHECK(joinSock.send_payload(kChannelHandshake, ping, sizeof(ping)), "hs send");
		bool pong = false;
		const auto d2 = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (!pong && std::chrono::steady_clock::now() < d2) {
			std::vector<IceSocket::Datagram> grams = hostSock.recv();
			if (grams.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
				continue;
			}
			for (size_t i = 0; i < grams.size(); ++i) {
				if (grams[i].channel == kChannelHandshake && grams[i].payload.size() == 5
				    && memcmp(grams[i].payload.data(), ping, 5) == 0)
					pong = true;
			}
		}
		CHECK(pong, "handshake ping-pong");
	}

	// 5. m9: IceLink (GekkoNet adapter) round trip over the same pair.
	{
		IceLink joinLink(&joinSock);
		const uint8_t hello[6] = { 'g', 'e', 'k', 'k', 'o', '!' };
		CHECK(hostSock.send_payload(kChannelGekko, hello, sizeof(hello)), "icelink send");
		bool gotIt = false;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (!gotIt && std::chrono::steady_clock::now() < deadline) {
			int n = 0;
			GekkoNetResult** res = joinLink.receive_inner(&n);
			for (int i = 0; i < n; ++i) {
				if (res[i] != nullptr && res[i]->data_len == sizeof(hello)
				    && res[i]->data != nullptr
				    && memcmp(res[i]->data, hello, sizeof(hello)) == 0
				    && res[i]->addr.size == kAddrBytes)
					gotIt = true;
				// receive_inner owns the results; release via the adapter.
				if (res[i] != nullptr) {
					free(res[i]->addr.data);
					free(res[i]->data);
					free(res[i]);
				}
			}
			if (!gotIt) std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		CHECK(gotIt, "icelink adapter round trip");
	}

	if (sFailures == 0) std::printf("pc_netplay_ice_test: PASS\n");
	else std::printf("pc_netplay_ice_test: %d FAILURES\n", sFailures);
	return sFailures == 0 ? 0 : 1;
}
