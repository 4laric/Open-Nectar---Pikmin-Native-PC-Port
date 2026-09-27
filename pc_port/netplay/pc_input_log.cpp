// Input record/replay for the netplay determinism harness (issue #878).
// See pc_input_log.h for the file format.

#include "netplay/pc_input_log.h"

#include "netplay/pc_netplay_pad.h"
#include "netplay/pc_state_hash.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

size_t pc_input_log_encode_tick(const PcInputPad pads[4], uint8_t out[44])
{
	size_t at = 0;
	for (int p = 0; p < 4; ++p) {
		out[at++] = (uint8_t)(pads[p].button & 0xFF);
		out[at++] = (uint8_t)((pads[p].button >> 8) & 0xFF);
		out[at++] = (uint8_t)pads[p].stickX;
		out[at++] = (uint8_t)pads[p].stickY;
		out[at++] = (uint8_t)pads[p].substickX;
		out[at++] = (uint8_t)pads[p].substickY;
		out[at++] = pads[p].triggerLeft;
		out[at++] = pads[p].triggerRight;
		out[at++] = pads[p].analogA;
		out[at++] = pads[p].analogB;
		out[at++] = (uint8_t)pads[p].err;
	}
	return at;
}

bool pc_input_log_decode_tick(const uint8_t* data, size_t avail, PcInputPad pads[4])
{
	if (data == nullptr || pads == nullptr) return false;
	if (avail < pc_input_log::kRecordBytes) return false;
	size_t at = 0;
	for (int p = 0; p < 4; ++p) {
		pads[p].button       = (uint16_t)(data[at] | ((uint16_t)data[at + 1] << 8));
		pads[p].stickX       = (int8_t)data[at + 2];
		pads[p].stickY       = (int8_t)data[at + 3];
		pads[p].substickX    = (int8_t)data[at + 4];
		pads[p].substickY    = (int8_t)data[at + 5];
		pads[p].triggerLeft  = data[at + 6];
		pads[p].triggerRight = data[at + 7];
		pads[p].analogA      = data[at + 8];
		pads[p].analogB      = data[at + 9];
		pads[p].err          = (int8_t)data[at + 10];
		at += pc_input_log::kPadBytes;
	}
	return true;
}

void pc_input_log_write_header(std::vector<uint8_t>& out)
{
	out.push_back(pc_input_log::kMagic[0]);
	out.push_back(pc_input_log::kMagic[1]);
	out.push_back(pc_input_log::kMagic[2]);
	out.push_back(pc_input_log::kMagic[3]);
	out.push_back((uint8_t)(pc_input_log::kVersion & 0xFF));
	out.push_back((uint8_t)((pc_input_log::kVersion >> 8) & 0xFF));
	out.push_back((uint8_t)(pc_input_log::kPadCount & 0xFF));
	out.push_back((uint8_t)((pc_input_log::kPadCount >> 8) & 0xFF));
	const uint16_t rec = (uint16_t)pc_input_log::kRecordBytes;
	out.push_back((uint8_t)(rec & 0xFF));
	out.push_back((uint8_t)((rec >> 8) & 0xFF));
}

PcInputHeaderResult pc_input_log_read_header(const uint8_t* data, size_t len, uint16_t& version,
                                             uint16_t& padCount, uint16_t& recordSize)
{
	if (data == nullptr || len < pc_input_log::kHeaderBytes) return PC_INPUT_HEADER_TRUNCATED;
	if (data[0] != pc_input_log::kMagic[0] || data[1] != pc_input_log::kMagic[1]
	    || data[2] != pc_input_log::kMagic[2] || data[3] != pc_input_log::kMagic[3]) {
		return PC_INPUT_HEADER_BAD_MAGIC;
	}
	version    = (uint16_t)(data[4] | ((uint16_t)data[5] << 8));
	padCount   = (uint16_t)(data[6] | ((uint16_t)data[7] << 8));
	recordSize = (uint16_t)(data[8] | ((uint16_t)data[9] << 8));
	if (version != pc_input_log::kVersion || padCount != pc_input_log::kPadCount
	    || recordSize != pc_input_log::kRecordBytes) {
		return PC_INPUT_HEADER_UNSUPPORTED;
	}
	return PC_INPUT_HEADER_OK;
}

namespace {
// Runtime state. All inert until the first tick resolves the switches.
bool sArgvSeen       = false;
const char* sArgvRecord = nullptr;
const char* sArgvReplay = nullptr;
bool sInitialised    = false;
bool sRecordActive   = false;
bool sReplayActive   = false;
FILE* sRecordFile    = nullptr;
std::vector<uint8_t> sReplayBytes;
size_t sReplayTicks  = 0;
uint64_t sTickIndex  = 0;

const char* argvValue(int argc, char** argv, const char* flag)
{
	for (int i = 1; i + 1 < argc; ++i) {
		if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
	}
	return nullptr;
}
} // namespace

void pc_input_log_notify_argv(int argc, char** argv)
{
	sArgvSeen = true;
	if (argv == nullptr) return;
	sArgvRecord = argvValue(argc, argv, "--input-record");
	sArgvReplay = argvValue(argc, argv, "--input-replay");
}

void pc_input_log_flush(void)
{
	if (sRecordFile != nullptr) std::fflush(sRecordFile);
}

void pc_input_log_tick(void)
{
	if (!sInitialised) {
		sInitialised = true;
		const char* recordPath = sArgvRecord;
		const char* replayPath = sArgvReplay;
		if (recordPath == nullptr) recordPath = std::getenv("PIKMIN_INPUT_RECORD");
		if (replayPath == nullptr) replayPath = std::getenv("PIKMIN_INPUT_REPLAY");
		if (recordPath != nullptr && *recordPath != '\0') {
			sRecordFile = std::fopen(recordPath, "wb");
			if (sRecordFile != nullptr) {
				std::vector<uint8_t> header;
				pc_input_log_write_header(header);
				std::fwrite(header.data(), 1, header.size(), sRecordFile);
				sRecordActive = true;
				std::printf("[netplay] input record: %s\n", recordPath);
				std::fflush(stdout);
			} else {
				std::printf("[netplay] input record: cannot open %s\n", recordPath);
				std::fflush(stdout);
			}
		}
		if (replayPath != nullptr && *replayPath != '\0') {
			FILE* in = std::fopen(replayPath, "rb");
			if (in != nullptr) {
				std::fseek(in, 0, SEEK_END);
				long size = std::ftell(in);
				std::fseek(in, 0, SEEK_SET);
				if (size >= (long)pc_input_log::kHeaderBytes) {
					sReplayBytes.resize((size_t)size);
					size_t got = std::fread(sReplayBytes.data(), 1, (size_t)size, in);
					sReplayBytes.resize(got);
					uint16_t version = 0, pads = 0, rec = 0;
					if (pc_input_log_read_header(sReplayBytes.data(), sReplayBytes.size(), version, pads,
					                             rec)
					    == PC_INPUT_HEADER_OK) {
						size_t body = sReplayBytes.size() - pc_input_log::kHeaderBytes;
						sReplayTicks = body / pc_input_log::kRecordBytes;
						sReplayActive = true;
						std::printf("[netplay] input replay: %s (%llu ticks)\n", replayPath,
						            (unsigned long long)sReplayTicks);
					} else {
						std::printf("[netplay] input replay: bad header in %s\n", replayPath);
						sReplayBytes.clear();
					}
				} else {
					std::printf("[netplay] input replay: truncated file %s\n", replayPath);
				}
				std::fclose(in);
				std::fflush(stdout);
			} else {
				std::printf("[netplay] input replay: cannot open %s\n", replayPath);
				std::fflush(stdout);
			}
		}
		// Menu-dwell simulation for acceptance C. Runs before the first
		// tick's simulation, and only when the switch is set.
		pc_state_hash_before_first_tick();
		if (!sRecordActive && !sReplayActive) return;
	}

	if (!sRecordActive && !sReplayActive) return;

	PADStatus* pads = pc_netplay_pad_status();

	if (sReplayActive) {
		if (sTickIndex < sReplayTicks) {
			PcInputPad decoded[4];
			const uint8_t* rec = sReplayBytes.data() + pc_input_log::kHeaderBytes
			                     + sTickIndex * pc_input_log::kRecordBytes;
			if (pc_input_log_decode_tick(rec, pc_input_log::kRecordBytes, decoded)) {
				for (int p = 0; p < 4; ++p) {
					pads[p].button      = decoded[p].button;
					pads[p].stickX      = decoded[p].stickX;
					pads[p].stickY      = decoded[p].stickY;
					pads[p].substickX   = decoded[p].substickX;
					pads[p].substickY   = decoded[p].substickY;
					pads[p].triggerLeft = decoded[p].triggerLeft;
					pads[p].triggerRight = decoded[p].triggerRight;
					pads[p].analogA     = decoded[p].analogA;
					pads[p].analogB     = decoded[p].analogB;
					pads[p].err         = decoded[p].err;
				}
			}
		} else {
			// Past the end of the file: neutral pads. Pad 0 stays
			// connected; the other channels report no controller.
			for (int p = 0; p < 4; ++p) {
				pads[p].button       = 0;
				pads[p].stickX       = 0;
				pads[p].stickY       = 0;
				pads[p].substickX    = 0;
				pads[p].substickY    = 0;
				pads[p].triggerLeft  = 0;
				pads[p].triggerRight = 0;
				pads[p].analogA      = 0;
				pads[p].analogB      = 0;
				pads[p].err          = (p == 0) ? pc_input_log::kErrConnected
				                                : pc_input_log::kErrNoController;
			}
		}
	}

	if (sRecordActive) {
		PcInputPad cur[4];
		for (int p = 0; p < 4; ++p) {
			cur[p].button       = pads[p].button;
			cur[p].stickX       = pads[p].stickX;
			cur[p].stickY       = pads[p].stickY;
			cur[p].substickX    = pads[p].substickX;
			cur[p].substickY    = pads[p].substickY;
			cur[p].triggerLeft  = pads[p].triggerLeft;
			cur[p].triggerRight = pads[p].triggerRight;
			cur[p].analogA      = pads[p].analogA;
			cur[p].analogB      = pads[p].analogB;
			cur[p].err          = pads[p].err;
		}
		uint8_t out[44];
		pc_input_log_encode_tick(cur, out);
		std::fwrite(out, 1, sizeof(out), sRecordFile);
	}

	++sTickIndex;
}
