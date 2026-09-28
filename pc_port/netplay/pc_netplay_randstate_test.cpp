// Netplay M4 lane A randstate unit test (issue #885, fix round 1).
//
// Covers the 64-byte v2 PcRandState encoder/decoder (explicit LE layout,
// CRC, version/reserved rejection), payload_equal, the fragment sequence
// byte, and the Reassembler (in-order completion at frame F arming F+1,
// duplicate/stale/incomplete no-ops, fragment-0 generation-boundary reset
// for a mid-transfer publish). Engine-free: links pc_netplay_randstate.cpp
// only (no game, no SDL, no sockets).

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
	memset(st.checks, 0, sizeof(st.checks));
	st.checks[0] = 0x05; // slots 0 and 2
	st.checks[57 / 8] |= (uint8_t)(1u << (57 % 8));   // high slot, schema-5 max
	st.checks[118 / 8] |= (uint8_t)(1u << (118 % 8)); // schema-8+ range
	st.checks[150 / 8] |= (uint8_t)(1u << (150 % 8)); // growth headroom
	for (int i = 0; i < 12; ++i) st.stats[i] = (uint8_t)(i % 3);
	for (int i = 0; i < 9; ++i) st.benefits[i] = (uint8_t)(i + 1);
	st.gen = gen;
	return st;
}

bool has_slot(const pc_randstate::PcRandState& st, unsigned slot)
{
	return (st.checks[slot / 8] & (uint8_t)(1u << (slot % 8))) != 0;
}

void feed_wire(pc_randstate::Reassembler& r, const uint8_t wire[64], uint32_t frame)
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
	// 1. Encode layout: 64 bytes, explicit LE fields.
	{
		PcRandState st = sample_state(0x01020304u);
		uint8_t wire[kStateBytes];
		CHECK(encode(st, wire) == kStateBytes, "encode 64 bytes");
		CHECK(kStateBytes == 64 && kFragCount == 16, "v2 sizes");
		CHECK(wire[0] == 2, "ver at 0");
		CHECK(wire[1] == 1 && wire[2] == 7 && wire[3] == 0xA5 && wire[4] == 3
		          && wire[5] == 1,
		      "ready/repairs/unlocks/flarlic/emperor");
		CHECK(wire[6] == 0x02 && wire[7] == 0x01, "deathLinks LE");
		CHECK(wire[8] == 0x05, "checks byte 0");
		CHECK(wire[56] == 0x04 && wire[57] == 0x03 && wire[58] == 0x02
		          && wire[59] == 0x01,
		      "gen LE at 56");
		PcRandState out;
		CHECK(decode(wire, sizeof(wire), out), "decode ok");
		CHECK(out.ready == 1 && out.repairs == 7 && out.unlocks == 0xA5
		          && out.flarlic == 3 && out.emperor == 1 && out.deathLinks == 258
		          && out.gen == 0x01020304u,
		      "decoded fields");
		CHECK(has_slot(out, 0) && has_slot(out, 2) && has_slot(out, 57)
		          && has_slot(out, 118) && has_slot(out, 150),
		      "decoded high check slots");
		CHECK(!has_slot(out, 1) && !has_slot(out, 58) && !has_slot(out, 191),
		      "unset slots stay zero");
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
	// 2. Decode rejects garbage (bitset itself is verbatim: no hi-bits ban).
	{
		PcRandState st = sample_state(9);
		uint8_t wire[kStateBytes];
		encode(st, wire);
		PcRandState out;
		CHECK(!decode(wire, kStateBytes - 1, out), "short buffer rejected");
		uint8_t bad[kStateBytes];
		memcpy(bad, wire, sizeof(bad));
		bad[0] = 1; // v1 version
		CHECK(!decode(bad, sizeof(bad), out), "old version rejected");
		memcpy(bad, wire, sizeof(bad));
		bad[53] = 1;
		CHECK(!decode(bad, sizeof(bad), out), "nonzero reserved rejected");
		memcpy(bad, wire, sizeof(bad));
		bad[40] ^= 0xFF; // corrupt a stats byte: CRC must fail
		CHECK(!decode(bad, sizeof(bad), out), "corrupt payload rejected");
		memcpy(bad, wire, sizeof(bad));
		bad[63] ^= 0x01; // corrupt the CRC itself
		CHECK(!decode(bad, sizeof(bad), out), "corrupt crc rejected");
		// All-ones bitset still decodes (catalog validation is apply-side).
		memcpy(bad, wire, sizeof(bad));
		for (size_t i = 0; i < kCheckBytes; ++i) bad[8 + i] = 0xFF;
		{
			uint32_t crc = crc32(bad, kPayloadBytes);
			bad[60] = (uint8_t)(crc & 0xFF);
			bad[61] = (uint8_t)((crc >> 8) & 0xFF);
			bad[62] = (uint8_t)((crc >> 16) & 0xFF);
			bad[63] = (uint8_t)((crc >> 24) & 0xFF);
		}
		CHECK(decode(bad, sizeof(bad), out), "full bitset decodes");
		CHECK(has_slot(out, 191), "top slot survives");
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
		b = sample_state(4);
		b.checks[118 / 8] ^= (uint8_t)(1u << (118 % 8));
		CHECK(!payload_equal(a, b), "different high check bit");
	}
	// 4. Fragment sequence byte: stream 0 in the high nibble, idx low.
	{
		for (uint8_t i = 0; i < kFragCount; ++i) {
			const uint8_t s = frag_seq_make(i);
			CHECK(frag_seq_stream(s) == kStreamId && frag_seq_index(s) == i, "seq split");
		}
		CHECK(frag_seq_index(0xFF) == 0x0F, "nibble split");
	}
	// 5. Reassembler: 16 in-order fragments complete at frame F, arming F+1.
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
		CHECK(!r.has_pending(), "15 of 16 fragments: incomplete, no pending");
		// Wrong stream id and HAS_CHUNK=0 are ignored.
		for (int i = 0; i < 4; ++i) payload[i] = wire[60 + i];
		r.feed(true, 0x10, payload, true, 50); // stream 1, idx 0
		CHECK(!r.has_pending(), "foreign stream ignored");
		r.feed(false, frag_seq_make(15), payload, true, 50); // no chunk
		CHECK(!r.has_pending(), "chunkless input ignored");
		// A new generation's fragment 0 mid-transfer is a boundary reset.
		for (int i = 0; i < 4; ++i) payload[i] = wire[i];
		r.feed(true, frag_seq_make(0), payload, false, 50);
		// The reset dropped the partial transfer; nothing is pending.
		CHECK(!r.has_pending(), "frag 0 reset drops the partial");
		// Re-feed the whole generation after the reset: completes normally.
		feed_wire(r, wire, 51);
		CHECK(r.has_pending() && r.pending_gen() == 2 && r.pending_frame() == 52,
		      "retransmit after reset completes");
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
	// 8. M1: a publish mid-transfer no longer mixes generations. Feed 5
	// frags of gen 1, then all 16 of gen 2 (fragment 0 opens the new
	// generation and resets the partial). Gen 2 must still apply, with gen
	// 1's prefix discarded identically on both peers.
	{
		Reassembler r;
		PcRandState a = sample_state(1), b = sample_state(2);
		b.repairs = 9;
		uint8_t wa[kStateBytes], wb[kStateBytes];
		encode(a, wa);
		encode(b, wb);
		for (uint8_t idx = 0; idx < 5; ++idx) {
			uint8_t p[4];
			for (int i = 0; i < 4; ++i) p[i] = wa[idx * 4 + i];
			r.feed(true, frag_seq_make(idx), p, false, 500);
		}
		CHECK(!r.has_pending(), "interrupted gen 1 does not complete");
		feed_wire(r, wb, 500);
		CHECK(r.has_pending() && r.pending_gen() == 2, "gen 2 applies after interruption");
		PcRandState got;
		CHECK(r.take_pending(got) && got.gen == 2 && got.repairs == 9, "gen 2 content");
		CHECK(payload_equal(got, b), "gen 2 payload intact");
	}
	// 9. Input codec carries the fragment bytes; idle inputs stay zero-pad.
	{
		PcNetplayInput in;
		in.buttons = 0x1234;
		in.flags = pc_netplay_gekko::kFlagsRandChunk | pc_netplay_gekko::kFlagsRandLast;
		in.fragSeq = frag_seq_make(15);
		in.fragData[0] = 0xDE;
		in.fragData[1] = 0xAD;
		in.fragData[2] = 0xBE;
		in.fragData[3] = 0xEF;
		uint8_t wire[16];
		CHECK(pc_netplay_input_encode(in, wire) == 16, "input encode 16");
		CHECK(wire[10] == 0x03 && wire[11] == 0x0F && wire[12] == 0xDE
		          && wire[15] == 0xEF,
		      "flags/seq/payload on the wire");
		PcNetplayInput out;
		CHECK(pc_netplay_input_decode(wire, 16, out), "input decode");
		CHECK(out.buttons == 0x1234 && out.flags == 0x03 && out.fragSeq == 0x0F
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
