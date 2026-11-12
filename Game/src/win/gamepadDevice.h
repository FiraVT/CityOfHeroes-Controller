/*
 * Gamepad device access through SDL3.
 *
 * This module only reads the device. It knows nothing about key codes,
 * binds or game commands: input.c turns the state it reports into the
 * same INP_* key codes the keyboard uses, so pads are bound with /bind and
 * keybind profiles like any other key.
 *
 * SDL3 replaced the old DirectInput joystick reader because DirectInput
 * cannot report a modern pad properly: Xbox-style pads put the right stick on
 * axes the old reader never read and merge both triggers into one axis, and
 * DirectInput gives no standard button layout, rumble or hot-plug.
 */
#ifndef GAMEPAD_DEVICE_H
#define GAMEPAD_DEVICE_H

#include <utilitieslib/stdtypes.h>

/* Buttons in the standard gamepad layout, named for an Xbox pad. */
typedef enum PadButton {
	PAD_BUTTON_A,	     /* bottom face button */
	PAD_BUTTON_B,	     /* right face button */
	PAD_BUTTON_X,	     /* left face button */
	PAD_BUTTON_Y,	     /* top face button */
	PAD_BUTTON_LB,	     /* left bumper */
	PAD_BUTTON_RB,	     /* right bumper */
	PAD_BUTTON_BACK,     /* Back / View / Select */
	PAD_BUTTON_START,    /* Start / Menu / Options */
	PAD_BUTTON_LSTICK,   /* left stick click */
	PAD_BUTTON_RSTICK,   /* right stick click */
	PAD_BUTTON_GUIDE,    /* Xbox / PS / Home button */
	PAD_BUTTON_MISC,     /* Share / Capture / Mic button */
	PAD_BUTTON_PADDLE1,  /* upper right rear paddle */
	PAD_BUTTON_PADDLE2,  /* upper left rear paddle */
	PAD_BUTTON_PADDLE3,  /* lower right rear paddle */
	PAD_BUTTON_PADDLE4,  /* lower left rear paddle */
	PAD_BUTTON_TOUCHPAD, /* PlayStation touchpad click */
	PAD_BUTTON_DPAD_UP,
	PAD_BUTTON_DPAD_DOWN,
	PAD_BUTTON_DPAD_LEFT,
	PAD_BUTTON_DPAD_RIGHT,
	PAD_BUTTON_COUNT
} PadButton;

/* Analog axes in the standard gamepad layout. */
typedef enum PadAxis {
	PAD_AXIS_LEFTX,	   /* left stick, negative = left */
	PAD_AXIS_LEFTY,	   /* left stick, negative = up */
	PAD_AXIS_RIGHTX,   /* right stick, negative = left */
	PAD_AXIS_RIGHTY,   /* right stick, negative = up */
	PAD_AXIS_LTRIGGER, /* left trigger, 0 = released */
	PAD_AXIS_RTRIGGER, /* right trigger, 0 = released */
	PAD_AXIS_COUNT
} PadAxis;

/*
 * One reading of the open gamepad.
 *
 * Stick axes range from -32768 to 32767 and trigger axes from 0 to 32767,
 * as SDL3 reports them. When no pad is open, connected is 0 and every other
 * field is zero.
 */
typedef struct PadState {
	int connected;
	S16 axes[PAD_AXIS_COUNT];
	U8 buttons[PAD_BUTTON_COUNT]; /* 1 while held */
} PadState;

/*
 * Start SDL3's gamepad subsystem and open the first connected pad.
 *
 * Safe to call when already started. Pads plugged in later are opened by
 * gamepadDeviceRead().
 *
 * Return: 0 on success, nonzero if SDL3 could not start. A start without any
 * pad connected is still a success.
 */
int gamepadDeviceStartup(void);

/*
 * Close the open pad and shut SDL3 down. Safe to call when not started.
 */
void gamepadDeviceShutdown(void);

/*
 * Handle pad connect and disconnect events and read the open pad.
 *
 * @state: output, overwritten with the current reading. Must not be NULL.
 *
 * Does nothing but clear @state when gamepadDeviceStartup() has not succeeded.
 * Only the main thread may call this.
 */
void gamepadDeviceRead(PadState *state);

/*
 * Rumble the open pad. @low and @high are the strengths of the low and high
 * frequency motors, 0 to 65535, for @ms milliseconds. 0, 0, 0 stops it.
 * Does nothing without an open pad or a pad that cannot rumble.
 */
void gamepadDeviceRumble(U16 low, U16 high, U32 ms);

/*
 * Name of the open pad, such as "Xbox Series X Controller", or "" when no
 * pad is open. Valid until the pad is closed.
 */
const char *gamepadDeviceName(void);

#endif /* GAMEPAD_DEVICE_H */
