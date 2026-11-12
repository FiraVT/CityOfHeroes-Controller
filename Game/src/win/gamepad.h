/* File gamepad.h
 *    Gamepad play and menu control: analog movement and camera, held
 *    trigger and bumper modifiers, targeting flicks, menu focus driven from
 *    the D-pad, a stick-driven pointer, rumble and the console HUD's state.
 *
 *    The pad itself is read by win/gamepadDevice.c. In play, buttons go to
 *    the keybind system as pad keys (PAD_A, DPAD_UP, LTRIGGER and so on)
 *    with the held modifiers (LT, RT, LB, RB), so the layout is a keybind
 *    profile and /bind changes it. Only menus, the on-screen keyboard and
 *    the console screens take buttons before the keybinds do.
 *
 *    Tuning is kept in the registry under the game's key, in "Gamepad",
 *    and changed from Options, Gamepad.
 */
#ifndef _GAMEPAD_H
#define _GAMEPAD_H

#include <utilitieslib/stdtypes.h>

C_DECLARATIONS_BEGIN

typedef enum GamepadButton {
	GP_A,
	GP_B,
	GP_X,
	GP_Y,
	GP_BACK,
	GP_START,
	GP_L3,
	GP_R3,
	GP_LB,
	GP_RB,
	GP_DPAD_UP,
	GP_DPAD_DOWN,
	GP_DPAD_LEFT,
	GP_DPAD_RIGHT,
	GP_LT,
	GP_RT,
	GP_RS_UP,
	GP_RS_DOWN,
	GP_RS_LEFT,
	GP_RS_RIGHT,
	GP_BUTTON_COUNT,
} GamepadButton;

typedef enum GamepadModifier {
	GP_MOD_LT = 1,
	GP_MOD_RT = 2,
	GP_MOD_LB = 4,
	GP_MOD_RB = 8,
} GamepadModifier;

// Called from inpUpdate() once per frame while the window has focus and gamepad
// controls are on. Reads the pad through win/gamepadDevice.c.
void gamepadUpdate(void);
// Called from inpUpdate() while the window has no focus. Releases anything
// being held.
void gamepadFocusLost(void);
void gamepadShutdown(void);

int gamepadIsConnected(void);

// Left stick. Returns 1 when the stick is past its dead zone. directionX is
// positive to the right, directionY is positive forward, and the two form a
// unit vector. speed is the 0..1 velocity scale.
int gamepadGetMove(F32 *directionX, F32 *directionY, F32 *speed);

// Right stick, as radians per 1/30 second tick. Signs match mouse_dx and
// mouse_dy. Returns 1 when the stick is steering the camera.
int gamepadGetLook(F32 *yaw, F32 *pitch);
int gamepadLookHeld(void);

// Radians per 1/30 second tick that a character may turn while following the
// stick.
F32 gamepadTurnRate(void);

void gamepadToggleCursorMode(void);
// Reads the settings from the registry again; saves them.
void gamepadReloadConfig(void);
void gamepadSaveConfig(void);
void gamepadPrintStatus(void);
void gamepadToggleTrace(void);
void gamepadSetCursorMode(int on);
int gamepadCursorModeToggled(void);

// Makes a window that was opened on purpose the focus of the D-pad, even if it
// was already open.
void gamepadFocusOnWindow(int window);
// Moves the pad's focus to a point in the client area, when the pad is in use
// in a menu. Returns 1 if it did.
int gamepadFocusPoint(int x, int y);
// Counts each D-pad move of the focus.
int gamepadNavMoves(void);
// Puts the pointer in the middle of the game window, where a menu opened from
// the pad will appear.
void gamepadCenterPointer(void);
// A window that code is about to open again, not the player, so it does not
// take the pad.
void gamepadQuietOpen(int window);
// Puts focus on the first thing in reach once it has been drawn, as when a
// window opens.
void gamepadRequestFocus(void);
// The same, but the first thing whose top is at or below this height in client
// pixels.
void gamepadRequestFocusBelow(float top);
// Moves the pointer to this point in client pixels and leaves it showing, for a
// screen that starts there.
void gamepadPointAt(int x, int y);
// True once if B was pressed on a full screen menu this frame, for its Back or
// Cancel arrow.
int gamepadTakeBack(void);
// True once if Start was pressed on a full screen menu this frame, for a
// forward arrow Enter does not reach.
int gamepadTakeForward(void);
// True once if Y was pressed at a store this frame, to sell or buy the row
// under the highlight.
int gamepadTakeQuickTrade(void);
// True when the controller was pressed or moved within this many milliseconds.
int gamepadUsedWithin(int milliseconds);
// A list scroll bar moved its rows this many pixels with the wheel, positive
// when they moved up.
void gamepadNoteListScroll(float pixels);

// The Options switch for all gamepad support. Off, the pad and its interface
// have no effect.
int gamepadEnabled(void);
// Read and change a gamepad setting by its registry name, as the Options tab
// does. A change applies at once; gamepadSaveConfig writes it out.
F32 gamepadGetSetting(const char *name);
void gamepadSetSetting(const char *name, F32 value);
// Saves the settings to the registry.
void gamepadSaveSettingsQuietly(void);
// /gamepad turns all gamepad support on or off, like the switch in Options.
void gamepadToggleEnabled(void);
// A line in gamepad_debug.log while /gamepad_debug is on, for code outside this
// file.
void gamepadDebugNote(const char *text);
// True while the D-pad is driving focus, so the pointer is hidden and a
// highlight is shown instead.
int gamepadPointerHidden(void);
// The focused box in client pixels, to be drawn with screen scaling off.
int gamepadGetFocusBox(float *left, float *top, float *right, float *bottom);
int gamepadOpenWindowCount(void);

int gamepadConsoleCompact(void);
void gamepadToggleConsoleCompact(void);
// Whether the console HUD shows the team frames, and switching them on or off.
int gamepadConsoleTeam(void);
void gamepadToggleConsoleTeam(void);
// Whether the team frames stay up in a league, where the league lists show
// every team.
int gamepadConsoleLeagueTeam(void);
void gamepadToggleConsoleLeagueTeam(void);
// Whether a team, off a league, is shown as a short list instead of the frames.
int gamepadConsoleTeamList(void);
void gamepadToggleConsoleTeamList(void);
void gamepadStartChat(void);

int gamepadConsoleUiEnabled(void);
void gamepadToggleConsoleUi(void);
int gamepadHeldModifiers(void);
int gamepadCursorModeActive(void);
const char *gamepadBoundCommand(int modifiers, int button);

// Moves all three trays together between the three sets of three.
void gamepadChangeSet(int direction);
void gamepadSeparateTrays(void);

C_DECLARATIONS_END

#endif
