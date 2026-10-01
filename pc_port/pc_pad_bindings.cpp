/**
 * @file pc_pad_bindings.cpp
 * @brief Gamepad binding routing and its settings lines. See the header.
 *
 * Its own translation unit, with no SDL calls, so a test can link it without
 * the window, renderer and audio layer.
 */

#include "pc_pad_bindings.h"

#include <cstdlib>
#include <ostream>

// Default gamepad bindings (SDL_GameControllerButton). Declared in pc_window.h.
const int kDefaultGamepadBindings[PC_KEY_ACT_COUNT] = {
    /* PC_KEY_ACT_A       */ SDL_CONTROLLER_BUTTON_A,
    /* PC_KEY_ACT_B       */ SDL_CONTROLLER_BUTTON_B,
    /* PC_KEY_ACT_X       */ SDL_CONTROLLER_BUTTON_X,
    /* PC_KEY_ACT_Y       */ SDL_CONTROLLER_BUTTON_Y,
    /* PC_KEY_ACT_Z       */ SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    /* PC_KEY_ACT_START   */ SDL_CONTROLLER_BUTTON_START,
    /* PC_KEY_ACT_L       */ SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    /* PC_KEY_ACT_R       */ -1, // Right analog trigger supplies R by default
    /* PC_KEY_ACT_DPAD_UP    */ SDL_CONTROLLER_BUTTON_DPAD_UP,
    /* PC_KEY_ACT_DPAD_DOWN  */ SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    /* PC_KEY_ACT_DPAD_LEFT  */ SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    /* PC_KEY_ACT_DPAD_RIGHT */ SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    /* PC_KEY_ACT_STICK_UP   */ -1, // Analog stick, no default button
    /* PC_KEY_ACT_STICK_DOWN */ -1,
    /* PC_KEY_ACT_STICK_LEFT */ -1,
    /* PC_KEY_ACT_STICK_RIGHT*/ -1,
    /* PC_KEY_ACT_CSTICK_UP   */ -1, // C-stick, no default button
    /* PC_KEY_ACT_CSTICK_DOWN */ -1,
    /* PC_KEY_ACT_CSTICK_LEFT */ -1,
    /* PC_KEY_ACT_CSTICK_RIGHT*/ -1,
    /* PC_KEY_ACT_SWARM       */ -1, // Optional; D-pad Down is taken by the pad's own D-pad
    /* PC_KEY_ACT_LOCKON      */ SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    /* PC_KEY_ACT_FIRSTPERSON */ SDL_CONTROLLER_BUTTON_LEFTSTICK,
    /* PC_KEY_ACT_GYRO_RECENTER */ -1, // sin botón libre por defecto; se asigna en Controls
};

bool pc_pad_raw_bind_held(const PcPadRaw& raw, int bind)
{
	return pc_pad_bind_held(
	    bind, [&raw](int b) { return raw.button[b]; }, [&raw](int a) { return raw.axis[a]; });
}

namespace {

/// The trigger axis a binding names, or -1 if it is not a trigger binding.
int triggerAxisOf(int stored)
{
	if (stored < PC_GP_AXIS_BIND)
		return -1;
	const int axis = (stored - PC_GP_AXIS_BIND) / 2;
	return (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) ? axis : -1;
}

} // namespace

void pc_pad_route_build(const int* stored, PcPadRoute* route)
{
	for (int a = 0; a < PC_KEY_ACT_COUNT; a++) {
		const int s = stored[a];
		route->bind[a] = s == PC_GP_UNBOUND ? -1 : (s >= 0 ? s : kDefaultGamepadBindings[a]);
	}

	// L and R. The stock mapping also feeds each from its own analog trigger;
	// binding the action to anything else (or clearing it) drops that, and an
	// action bound to a trigger takes that trigger's analog value instead of
	// the on/off press it would otherwise become.
	static const int kStockAxis[2] = { SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT };
	static const int kAction[2]    = { PC_KEY_ACT_L, PC_KEY_ACT_R };
	for (int side = 0; side < 2; side++) {
		const int s = stored[kAction[side]];
		route->triggerAxis[side] = -1;
		if (s == PC_GP_DEFAULT) {
			route->triggerAxis[side] = kStockAxis[side];
		} else if (triggerAxisOf(s) >= 0) {
			route->triggerAxis[side]   = triggerAxisOf(s);
			route->bind[kAction[side]] = -1;
		}
	}

	static const int kStick[PC_PAD_DIR_COUNT]  = { PC_KEY_ACT_STICK_LEFT, PC_KEY_ACT_STICK_RIGHT,
		                                           PC_KEY_ACT_STICK_UP, PC_KEY_ACT_STICK_DOWN };
	static const int kCStick[PC_PAD_DIR_COUNT] = { PC_KEY_ACT_CSTICK_LEFT, PC_KEY_ACT_CSTICK_RIGHT,
		                                           PC_KEY_ACT_CSTICK_UP, PC_KEY_ACT_CSTICK_DOWN };
	for (int d = 0; d < PC_PAD_DIR_COUNT; d++) {
		route->stick[d]  = stored[kStick[d]] != PC_GP_UNBOUND;
		route->cstick[d] = stored[kCStick[d]] != PC_GP_UNBOUND;
	}
	route->freeCamSwarmDpad = stored[PC_KEY_ACT_SWARM] == PC_GP_DEFAULT;
}

void pc_pad_route_triggers(const PcPadRoute& route, const PcPadRaw& raw, int deadZoneRaw,
                           u16* button, u8* triggerL, u8* triggerR)
{
	if (pc_pad_raw_bind_held(raw, route.bind[PC_KEY_ACT_L])) {
		*button |= PAD_TRIGGER_L;
		*triggerL = 255;
	}
	if (pc_pad_raw_bind_held(raw, route.bind[PC_KEY_ACT_R])) {
		*button |= PAD_TRIGGER_R;
		*triggerR = 255;
	}
	if (route.triggerAxis[0] >= 0) {
		const int v = raw.axis[route.triggerAxis[0]];
		if (v > deadZoneRaw) {
			*triggerL = (u8)(v / 128);
			if (v > 30000) *button |= PAD_TRIGGER_L;
		}
	}
	if (route.triggerAxis[1] >= 0) {
		const int v = raw.axis[route.triggerAxis[1]];
		if (v > deadZoneRaw) {
			*triggerR = (u8)(v / 128);
			if (v > 30000) *button |= PAD_TRIGGER_R;
		}
	}
}

bool pc_pad_route_stick_live(const bool dir[PC_PAD_DIR_COUNT], int value, bool vertical)
{
	if (value == 0)
		return false;
	// Horizontal: positive is right. Vertical is passed already flipped to the
	// pad's Y-up convention, so positive is up.
	if (vertical)
		return value > 0 ? dir[PC_PAD_DIR_UP] : dir[PC_PAD_DIR_DOWN];
	return value > 0 ? dir[PC_PAD_DIR_RIGHT] : dir[PC_PAD_DIR_LEFT];
}

bool pc_pad_bind_value_valid(int value)
{
	if (value == PC_GP_UNBOUND || value == PC_GP_DEFAULT)
		return true;
	if (value >= 0 && value < SDL_CONTROLLER_BUTTON_MAX)
		return true;
	const int axis = (value - PC_GP_AXIS_BIND) / 2;
	return value >= PC_GP_AXIS_BIND && axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX;
}

void pc_pad_bindings_write(std::ostream& out, const int* keyboard, const int* pad, const int* padP2)
{
	for (int i = 0; i < PC_KEY_ACT_COUNT; i++)
		out << "key_" << i << " = " << keyboard[i] << "\n";
	for (int i = 0; i < PC_KEY_ACT_COUNT; i++)
		out << "gp_" << i << " = " << pad[i] << "\n";
	for (int i = 0; i < PC_KEY_ACT_COUNT; i++)
		out << "gp2_" << i << " = " << padP2[i] << "\n";
}

bool pc_pad_bindings_parse(const std::string& key, const std::string& val, int* keyboard, int* pad, int* padP2)
{
	if (key.rfind("key_", 0) == 0) {
		const int idx = atoi(key.substr(4).c_str());
		if (idx >= 0 && idx < PC_KEY_ACT_COUNT) {
			const int scancode = atoi(val.c_str());
			if (pc_bind_is_valid(scancode))
				keyboard[idx] = scancode;
		}
		return true;
	}
	if (key.rfind("gp_", 0) == 0 || key.rfind("gp2_", 0) == 0) {
		const bool p2 = key[2] == '2';
		const int idx = atoi(key.substr(p2 ? 4 : 3).c_str());
		if (idx >= 0 && idx < PC_KEY_ACT_COUNT) {
			const int button = atoi(val.c_str());
			if (pc_pad_bind_value_valid(button))
				(p2 ? padP2 : pad)[idx] = button;
		}
		return true;
	}
	return false;
}
