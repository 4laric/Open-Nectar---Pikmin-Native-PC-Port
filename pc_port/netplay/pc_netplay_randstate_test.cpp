// Netplay M4 lane A randstate unit test (issue #885).
//
// Covers the 44-byte PcRandState encoder/decoder (explicit LE layout, CRC,
// version/reserved/checks-hi rejection), payload_equal, the fragment
// sequence byte, and the Reassembler (in-order completion at frame F arming
// F+1, duplicate/stale/incomplete no-ops). Engine-free: links
// pc_netplay_randstate.cpp only (no game, no SDL, no sockets).

#include "netplay/pc_netplay_gekko_input.h"
#include "netplay/pc_netplay_randstate.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

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

pc_randstate::PcRandState sample_state(uint32_t gen)
{
	pc_randstate::PcRandState st;
	st.ver = pc_randstate::kVersion;
	st.ready = 1;
	st.repairs = 7;
	st.unlocks = 0xA5;
	st.flarlic = 3;
	st.emperor = 1;
	st.deathLinks = 258;
	st.checksLo = 0x00000005u;
	for (int i = 0; i < 12; ++i) st.stats[i] = (uint8_t)(i % 3);
	for (int i = 0; i < 9; ++i) st.benefits[i] = (uint8_t)(i + 1);
	st.gen = gen;
	return st;
}

void feed_wire(pc_randstate::Reassembler& r, const uint8_t wire[44], uint32_t frame)
{
	for (uint8_t idx = 0; idx < pc_randstate::kFragCount; ++idx) {
		uint8_t payload[4];
		for (int i = 0; i < 4; ++i) payload[i] = wire[idx * 4 + i];
		r.feed(true, pc_randstate::frag_seq_make(idx), payload,
		       idx + 1 == pc_randstate::kFragCount, frame);
	}
}

} // namespace

int main()
{
	using namespace pc_randstate;
	// 1. Encode layout: 44 bytes, explicit LE fields.
	{
		PcRandState st = sample_state(0x01020304u);
		uint8_t wire[kStateBytes];
		CHECK(encode(st, wire) == kStateBytes, "encode 44 bytes");
		CHECK(wire[0] == 1, "ver at 0");
		CHECK(wire[1] == 1 && wire[2] == 7 && wire[3] == 0xA5 && wire[4] == 3
		          && wire[5] == 1,
		      "ready/repairs/unlocks/flarlic/emperor");
		CHECK(wire[6] == 0x02 && wire[7] == 0x01, "deathLinks LE");
		CHECK(wire[8] == 0x05 && wire[9] == 0 && wire[10] == 0 && wire[11] == 0,
		      "checksLo LE");
		CHECK(wire[36] == 0x04 && wire[37] == 0x03 && wire[38] == 0x02
		          && wire[39] == 0x01,
		      "gen LE at 36");
		PcRandState out;
		CHECK(decode(wire, sizeof(wire), out), "decode ok");
		CHECK(out.ready == 1 && out.repairs == 7 && out.unlocks == 0xA5
		          && out.flarlic == 3 && out.emperor == 1 && out.deathLinks == 258
		          && out.checksLo == 5u && out.gen == 0x01020304u,
		      "decoded fields");
		for (int i = 0; i < 12; ++i) {
			if (out.stats[i] != (uint8_t)(i % 3)) {
				CHECK(false, "decoded stats");
				break;
			}
		}
		for (int i = 0; i < 9; ++i) {
			if (out.benefits[i] != (uint8_t)(i + 1)) {
				CHECK(false, "decoded benefits");
				break;
			}
		}
	}
	// 2. Decode rejects garbage.
	{
		PcRandState st = sample_state(9);
		uint8_t wire[kStateBytes];
		encode(st, wire);
		PcRandState out;
		CHECK(!decode(wire, kStateBytes - 1, out), "short buffer rejected");
		uint8_t bad[kStateBytes];
		memcpy(bad, wire, sizeof(bad));
		bad[0] = 2;
		CHECK(!decode(bad, sizeof(bad), out), "bad version rejected");
		memcpy(bad, wire, sizeof(bad));
		bad[33] = 1;
		CHECK(!decode(bad, sizeof(bad), out), "nonzero reserved rejected");
		memcpy(bad, wire, sizeof(bad));
		bad[11] = 0xC0; // checksLo bit 30/31
		CHECK(!decode(bad, sizeof(bad), out), "checks hi bits rejected");
		memcpy(bad, wire, sizeof(bad));
		bad[20] ^= 0xFF; // corrupt a stats byte: CRC must fail
		CHECK(!decode(bad, sizeof(bad), out), "corrupt payload rejected");
		memcpy(bad, wire, sizeof(bad));
		bad[43] ^= 0x01; // corrupt the CRC itself
		CHECK(!decode(bad, sizeof(bad), out), "corrupt crc rejected");
	}
	// 3. payload_equal ignores gen/crc.
	{
		PcRandState a = sample_state(1), b = sample_state(2);
		CHECK(payload_equal(a, b), "equal payloads, different gens");
		b.repairs = 8;
		CHECK(!payload_equal(a, b), "different repairs");
		b = sample_state(3);
		b.benefits[8] = 0;
		CHECK(!payload_equal(a, b), "different benefits");
	}
	// 4. Fragment sequence byte: stream 0 in the high nibble, idx low.
	{
		for (uint8_t i = 0; i < kFragCount; ++i) {
			const uint8_t s = frag_seq_make(i);
			CHECK(frag_seq_stream(s) == kStreamId && frag_seq_index(s) == i, "seq split");
		}
		CHECK(frag_seq_index(0xFF) == 0x0F, "nibble split");
	}
	// 5. Reassembler: 11 in-order fragments complete at frame F, arming F+1.
	{
		PcRandState st = sample_state(1);
		uint8_t wire[kStateBytes];
		encode(st, wire);
		Reassembler r;
		CHECK(!r.has_pending(), "nothing pending initially");
		feed_wire(r, wire, 100);
		CHECK(r.has_pending(), "complete transfer pends");
		CHECK(r.pending_gen() == 1 && r.pending_frame() == 101, "gen 1 arms frame 101");
		PcRandState got;
		CHECK(r.take_pending(got), "take pending");
		CHECK(got.gen == 1 && payload_equal(got, st), "pending content");
		CHECK(!r.has_pending(), "consumed");
		r.mark_applied(1);
		CHECK(r.applied_gen() == 1, "applied gen recorded");
		// Replaying the same generation is a stale no-op.
		feed_wire(r, wire, 200);
		CHECK(!r.has_pending(), "stale gen replay is a no-op");
	}
	// 6. Reassembler: duplicate / incomplete / uninterested fragments.
	{
		PcRandState st = sample_state(2);
		uint8_t wire[kStateBytes];
		encode(st, wire);
		Reassembler r;
		uint8_t payload[4] = { wire[0], wire[1], wire[2], wire[3] };
		r.feed(true, frag_seq_make(0), payload, false, 50);
		r.feed(true, frag_seq_make(0), payload, false, 50); // duplicate
		for (uint8_t idx = 1; idx < kFragCount - 1; ++idx) {
			for (int i = 0; i < 4; ++i) payload[i] = wire[idx * 4 + i];
			r.feed(true, frag_seq_make(idx), payload, false, 50);
		}
		CHECK(!r.has_pending(), "10 of 11 fragments: incomplete, no pending");
		// Wrong stream id and HAS_CHUNK=0 are ignored.
		for (int i = 0; i < 4; ++i) payload[i] = wire[40 + i];
		r.feed(true, 0x10, payload, true, 50); // stream 1, idx 0
		CHECK(!r.has_pending(), "foreign stream ignored");
		r.feed(false, frag_seq_make(10), payload, true, 50); // no chunk
		CHECK(!r.has_pending(), "chunkless input ignored");
		r.feed(true, 0x0B, payload, true, 50); // idx 11 out of range
		CHECK(!r.has_pending(), "out-of-range idx ignored");
		// The real last fragment completes the transfer.
		r.feed(true, frag_seq_make(10), payload, true, 50);
		CHECK(r.has_pending() && r.pending_gen() == 2 && r.pending_frame() == 51,
		      "last fragment completes");
	}
	// 7. Newer generation after an applied one completes normally.
	{
		Reassembler r;
		PcRandState a = sample_state(5), b = sample_state(6);
		uint8_t wa[kStateBytes], wb[kStateBytes];
		encode(a, wa);
		encode(b, wb);
		feed_wire(r, wa, 300);
		PcRandState got;
		CHECK(r.take_pending(got) && got.gen == 5, "gen 5 taken");
		r.mark_applied(5);
		feed_wire(r, wb, 400);
		CHECK(r.has_pending() && r.pending_gen() == 6 && r.pending_frame() == 401,
		      "gen 6 completes after gen 5 applied");
	}
	// 8. Input codec carries the fragment bytes; idle inputs stay zero-pad.
	{
		PcNetplayInput in;
		in.buttons = 0x1234;
		in.flags = pc_netplay_gekko::kFlagsRandChunk | pc_netplay_gekko::kFlagsRandLast;
		in.fragSeq = frag_seq_make(10);
		in.fragData[0] = 0xDE;
		in.fragData[1] = 0xAD;
		in.fragData[2] = 0xBE;
		in.fragData[3] = 0xEF;
		uint8_t wire[16];
		CHECK(pc_netplay_input_encode(in, wire) == 16, "input encode 16");
		CHECK(wire[10] == 0x03 && wire[11] == 0x0A && wire[12] == 0xDE
		          && wire[15] == 0xEF,
		      "flags/seq/payload on the wire");
		PcNetplayInput out;
		CHECK(pc_netplay_input_decode(wire, 16, out), "input decode");
		CHECK(out.buttons == 0x1234 && out.flags == 0x03 && out.fragSeq == 0x0A
		          && out.fragData[0] == 0xDE && out.fragData[3] == 0xEF,
		      "input frag round trip");
		PcNetplayInput idle;
		uint8_t wi[16];
		pc_netplay_input_encode(idle, wi);
		bool padZero = true;
		for (int i = 10; i < 16; ++i) padZero = padZero && wi[i] == 0;
		CHECK(padZero, "idle input wire unchanged (flags+pad zero)");
	}

	if (sFailures == 0) std::printf("pc_netplay_randstate_test: PASS\n");
	else std::printf("pc_netplay_randstate_test: %d FAILURES\n", sFailures);
	return sFailures == 0 ? 0 : 1;
}
