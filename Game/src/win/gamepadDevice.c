#include "win/gamepadDevice.h"

#include <string.h>
#include <SDL3/SDL.h>

#include "UI/uiConsole.h"

/* SDL3 button for each PadButton, in the same order. */
static const SDL_GamepadButton sdl_buttons[PAD_BUTTON_COUNT] = {
	SDL_GAMEPAD_BUTTON_SOUTH,	  SDL_GAMEPAD_BUTTON_EAST,
	SDL_GAMEPAD_BUTTON_WEST,	  SDL_GAMEPAD_BUTTON_NORTH,
	SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
	SDL_GAMEPAD_BUTTON_BACK,	  SDL_GAMEPAD_BUTTON_START,
	SDL_GAMEPAD_BUTTON_LEFT_STICK,	  SDL_GAMEPAD_BUTTON_RIGHT_STICK,
	SDL_GAMEPAD_BUTTON_GUIDE,	  SDL_GAMEPAD_BUTTON_MISC1,
	SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
	SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
	SDL_GAMEPAD_BUTTON_TOUCHPAD,	  SDL_GAMEPAD_BUTTON_DPAD_UP,
	SDL_GAMEPAD_BUTTON_DPAD_DOWN,	  SDL_GAMEPAD_BUTTON_DPAD_LEFT,
	SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
};

/* SDL3 axis for each PadAxis, in the same order. */
static const SDL_GamepadAxis sdl_axes[PAD_AXIS_COUNT] = {
	SDL_GAMEPAD_AXIS_LEFTX,	       SDL_GAMEPAD_AXIS_LEFTY,
	SDL_GAMEPAD_AXIS_RIGHTX,       SDL_GAMEPAD_AXIS_RIGHTY,
	SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
};

static int sdl_ready;
static SDL_Gamepad *pad;

static void open_first_pad(void)
{
	SDL_JoystickID *ids;
	int count = 0;
	int i;

	if (pad)
		return;

	ids = SDL_GetGamepads(&count);
	if (!ids)
		return;
	for (i = 0; i < count && !pad; i++)
		pad = SDL_OpenGamepad(ids[i]);
	SDL_free(ids);

	if (pad)
		conPrintf("Gamepad connected: %s", SDL_GetGamepadName(pad));
}

static void close_pad(void)
{
	if (!pad)
		return;
	SDL_CloseGamepad(pad);
	pad = NULL;
}

int gamepadDeviceStartup(void)
{
	if (sdl_ready)
		return 0;

	/*
	 * The game reads input only while it has focus (okToInput), so SDL
	 * may keep the device open in the background. Reading on SDL's own
	 * thread keeps XInput and raw input working without an SDL window.
	 */
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");

	if (!SDL_Init(SDL_INIT_GAMEPAD)) {
		conPrintf("Gamepad support unavailable: %s", SDL_GetError());
		return 1;
	}
	sdl_ready = 1;
	open_first_pad();
	return 0;
}

void gamepadDeviceShutdown(void)
{
	if (!sdl_ready)
		return;
	close_pad();
	SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	SDL_Quit();
	sdl_ready = 0;
}

void gamepadDeviceRead(PadState *state)
{
	SDL_Event event;
	int i;

	memset(state, 0, sizeof(*state));
	if (!sdl_ready)
		return;

	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
			open_first_pad();
		} else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && pad &&
			   event.gdevice.which == SDL_GetGamepadID(pad)) {
			conPrintf("Gamepad disconnected");
			close_pad();
			/* Fall back to any other pad still connected. */
			open_first_pad();
		}
	}
	if (!pad)
		return;

	state->connected = 1;
	for (i = 0; i < PAD_BUTTON_COUNT; i++)
		state->buttons[i] = SDL_GetGamepadButton(pad, sdl_buttons[i]);
	for (i = 0; i < PAD_AXIS_COUNT; i++)
		state->axes[i] = SDL_GetGamepadAxis(pad, sdl_axes[i]);
}

void gamepadDeviceRumble(U16 low, U16 high, U32 ms)
{
	if (pad)
		SDL_RumbleGamepad(pad, low, high, ms);
}

const char *gamepadDeviceName(void)
{
	const char *name = pad ? SDL_GetGamepadName(pad) : NULL;

	return name ? name : "";
}

PadFamily gamepadDeviceFamily(void)
{
	if (!pad)
		return PAD_FAMILY_XBOX;

	switch (SDL_GetGamepadType(pad)) {
	case SDL_GAMEPAD_TYPE_PS3:
	case SDL_GAMEPAD_TYPE_PS4:
	case SDL_GAMEPAD_TYPE_PS5:
		return PAD_FAMILY_PLAYSTATION;
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
		return PAD_FAMILY_NINTENDO;
	default:
		return PAD_FAMILY_XBOX;
	}
}
