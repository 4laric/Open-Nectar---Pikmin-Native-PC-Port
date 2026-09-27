// Host-run unit test for the PKNI v1 encode/decode API (issue #878).
// Round-trip, truncated file, bad magic. Engine-free: links only
// pc_input_log.cpp's pure functions (no PADStatus, no globals).

#include "netplay/pc_input_log.h"

#include "Dolphin/pad.h"
#include "netplay/pc_state_hash.h"

#include <cstdio>
#include <cstring>
#include <vector>

// Link stubs: this test exercises only the pure encode/decode API, but it
// links pc_input_log.cpp whole, so the engine hooks it calls need symbols.
// They are never invoked below.
static PADStatus sStubPads[4];
PADStatus* pc_netplay_pad_status(void) { return sStubPads; }
void pc_state_hash_before_first_tick(void) {}

namespace {
int sFailures = 0;

void check(bool cond, const char* what)
{
	if (!cond) {
		std::printf("FAIL: %s\n", what);
		++sFailures;
	}
}

void fillPads(PcInputPad pads[4])
{
	// Cover edges: full button word, stick extremes, trigger extremes,
	// both err signs.
	const uint16_t buttons[4] = { 0x0000, 0xFFFF, 0x0100 | 0x0040, 0x1000 };
	const int8_t sticks[4][4] = { { 0, 0, 0, 0 }, { 127, 127, -128, -128 }, { -1, 1, 100, -100 }, { 32, -33, 0, 7 } };
	for (int p = 0; p < 4; ++p) {
		pads[p].button       = buttons[p];
		pads[p].stickX       = sticks[p][0];
		pads[p].stickY       = sticks[p][1];
		pads[p].substickX    = sticks[p][2];
		pads[p].substickY    = sticks[p][3];
		pads[p].triggerLeft  = (uint8_t)(p * 85);
		pads[p].triggerRight = (uint8_t)(255 - p * 85);
		pads[p].analogA      = (uint8_t)(17 + p);
		pads[p].analogB      = (uint8_t)(200 - p);
		pads[p].err          = (p == 0) ? (int8_t)0 : (int8_t)-1;
	}
}

bool padsEqual(const PcInputPad a[4], const PcInputPad b[4])
{
	// Field-wise: PcInputPad may carry padding bytes that encode/decode
	// never touches.
	for (int p = 0; p < 4; ++p) {
		if (a[p].button != b[p].button || a[p].stickX != b[p].stickX || a[p].stickY != b[p].stickY
		    || a[p].substickX != b[p].substickX || a[p].substickY != b[p].substickY
		    || a[p].triggerLeft != b[p].triggerLeft || a[p].triggerRight != b[p].triggerRight
		    || a[p].analogA != b[p].analogA || a[p].analogB != b[p].analogB || a[p].err != b[p].err) {
			return false;
		}
	}
	return true;
}
} // namespace

int main()
{
	// Round-trip through encode/decode.
	PcInputPad pads[4];
	fillPads(pads);
	uint8_t rec[44];
	check(pc_input_log_encode_tick(pads, rec) == 44, "encode returns 44 bytes");
	PcInputPad back[4];
	std::memset(back, 0xAA, sizeof(back));
	check(pc_input_log_decode_tick(rec, sizeof(rec), back), "decode of full record succeeds");
	check(padsEqual(pads, back), "round-trip preserves every field");

	// Byte layout is explicit little-endian, not a struct dump.
	check(rec[0] == 0x00 && rec[1] == 0x00, "pad0 button LE");
	PcInputPad hi[4];
	std::memset(hi, 0, sizeof(hi));
	hi[1].button = 0x0100;
	uint8_t recHi[44];
	pc_input_log_encode_tick(hi, recHi);
	check(recHi[11] == 0x00 && recHi[12] == 0x01, "button serialised LE, not host order dependent");
	check(recHi[13] == 0 && recHi[22] == 0, "sticks at fixed offsets");

	// Truncated file: decode must fail for every short length.
	for (size_t len = 0; len < 44; ++len) {
		PcInputPad tmp[4];
		char what[64];
		std::snprintf(what, sizeof(what), "decode rejects %llu bytes", (unsigned long long)len);
		check(!pc_input_log_decode_tick(rec, len, tmp), what);
	}
	check(!pc_input_log_decode_tick(nullptr, 44, back), "decode rejects null data");
	check(!pc_input_log_decode_tick(rec, 44, nullptr), "decode rejects null pads");

	// Header round-trip.
	std::vector<uint8_t> header;
	pc_input_log_write_header(header);
	check(header.size() == 10, "header is 10 bytes");
	check(header[0] == 'P' && header[1] == 'K' && header[2] == 'N' && header[3] == 'I', "header magic");
	uint16_t version = 0, padCount = 0, recordSize = 0;
	check(pc_input_log_read_header(header.data(), header.size(), version, padCount, recordSize)
	              == PC_INPUT_HEADER_OK
	      && version == 1 && padCount == 4 && recordSize == 44, "header reads back v1/4/44");

	// Bad magic.
	std::vector<uint8_t> bad = header;
	bad[0]                 = 'X';
	check(pc_input_log_read_header(bad.data(), bad.size(), version, padCount, recordSize)
	          == PC_INPUT_HEADER_BAD_MAGIC,
	      "bad magic rejected");

	// Truncated header.
	for (size_t len = 0; len < 10; ++len) {
		check(pc_input_log_read_header(header.data(), len, version, padCount, recordSize)
		              == PC_INPUT_HEADER_TRUNCATED,
		      "short header rejected");
	}

	// Unsupported version / pad count / record size.
	std::vector<uint8_t> wrong = header;
	wrong[4]                   = 2; // version 2
	check(pc_input_log_read_header(wrong.data(), wrong.size(), version, padCount, recordSize)
	          == PC_INPUT_HEADER_UNSUPPORTED,
	      "future version rejected");
	wrong = header;
	wrong[6] = 2; // pad count 2
	check(pc_input_log_read_header(wrong.data(), wrong.size(), version, padCount, recordSize)
	          == PC_INPUT_HEADER_UNSUPPORTED,
	      "wrong pad count rejected");

	if (sFailures == 0) {
		std::printf("pc_input_log_test: all checks passed\n");
		return 0;
	}
	std::printf("pc_input_log_test: %d failure(s)\n", sFailures);
	return 1;
}
