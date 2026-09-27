#pragma once
// Netplay M3 lockstep input record (issue #880): the GekkoNet `input_size`.
//
// Fixed, explicitly serialised 16-byte struct, little-endian, never a raw
// struct memcpy so the wire format is stable across compilers:
//   offset  size  field
//   0       2     buttons u16 (PAD_BUTTON_* bits)
//   2       1     stickX s8
//   3       1     stickY s8
//   4       1     substickX s8
//   5       1     substickY s8
//   6       1     triggerL u8
//   7       1     triggerR u8
//   8       2     control yaw u16, M2c quantisation (1/65536 turns)
//   10      1     flags u8 (reserved, always 0; cf. pc_input_log v2 flags)
//   11      5     zero padding to 16 bytes
//
// Engine-free and host-testable: only <cstdint>/<cstddef>. The session layer
// fills it from the local physical pad 0 sample plus the local control yaw,
// and injects received records into sControllerPad[0] (host/P1) and [1]
// (joiner/P2) plus the M2c yaw slots.

#include <cstddef>
#include <cstdint>

namespace pc_netplay_gekko {
constexpr size_t kInputBytes = 16;
constexpr uint8_t kFlagsNone = 0;
} // namespace pc_netplay_gekko

struct PcNetplayInput {
	uint16_t buttons = 0;
	int8_t stickX = 0;
	int8_t stickY = 0;
	int8_t substickX = 0;
	int8_t substickY = 0;
	uint8_t triggerL = 0;
	uint8_t triggerR = 0;
	uint16_t controlYaw = 0;
	uint8_t flags = 0;
};

// Encodes exactly 16 bytes. Returns bytes written (16).
inline size_t pc_netplay_input_encode(const PcNetplayInput& in, uint8_t out[16])
{
	out[0]  = (uint8_t)(in.buttons & 0xFF);
	out[1]  = (uint8_t)((in.buttons >> 8) & 0xFF);
	out[2]  = (uint8_t)in.stickX;
	out[3]  = (uint8_t)in.stickY;
	out[4]  = (uint8_t)in.substickX;
	out[5]  = (uint8_t)in.substickY;
	out[6]  = in.triggerL;
	out[7]  = in.triggerR;
	out[8]  = (uint8_t)(in.controlYaw & 0xFF);
	out[9]  = (uint8_t)((in.controlYaw >> 8) & 0xFF);
	out[10] = in.flags;
	out[11] = 0;
	out[12] = 0;
	out[13] = 0;
	out[14] = 0;
	out[15] = 0;
	return pc_netplay_gekko::kInputBytes;
}

// Decodes from the first 16 bytes. Returns false when avail < 16.
inline bool pc_netplay_input_decode(const uint8_t* data, size_t avail, PcNetplayInput& out)
{
	if (data == nullptr || avail < pc_netplay_gekko::kInputBytes) return false;
	out.buttons   = (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
	out.stickX    = (int8_t)data[2];
	out.stickY    = (int8_t)data[3];
	out.substickX = (int8_t)data[4];
	out.substickY = (int8_t)data[5];
	out.triggerL  = data[6];
	out.triggerR  = data[7];
	out.controlYaw = (uint16_t)(data[8] | ((uint16_t)data[9] << 8));
	out.flags     = data[10];
	return true;
}

// Neutral (hands-off) input: no buttons, centred sticks, yaw 0.
inline PcNetplayInput pc_netplay_input_neutral()
{
	return PcNetplayInput();
}
