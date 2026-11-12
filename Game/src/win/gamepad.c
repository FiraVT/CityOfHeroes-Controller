#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <utilitieslib/stdtypes.h>
#include <utilitieslib/utils/wininclude.h>
#include <utilitieslib/utils/utils.h>
#include <utilitieslib/utils/sysutil.h>
#include <utilitieslib/utils/file.h>
#include "win/gamepad.h"
#include "win/gamepadDevice.h"
#include "win/gamepadNav.h"
#include "win/input.h"
#include "win/win_init.h"
#include "cmdparse/cmdgame.h"
#include "UI/uiChat.h"
#include "UI/uiConsole.h"
#include "UI/uiConsoleHud.h"
#include "UI/uiContactDialog.h"
#include "UI/uiContextMenu.h"
#include "UI/uiFocus.h"
#include "UI/uiGame.h"
#include "UI/uiWindows.h"
#include "gameComm/wdwbase.h"
#include "entity/entity.h"
#include "entity/character_base.h"
#include "entity/entPlayer.h"
#include "gameComm/trayCommon.h"
#include "player/player.h"
#include "UI/uiKeybind.h"
#include <utilitieslib/utils/textparser.h>
#include <utilitieslib/version/AppRegCache.h>

#define GAMEPAD_TRACE_NAME "gamepad_debug.log"
#define GAMEPAD_NO_BIND -1
#define GAMEPAD_BIND_MOUSE -2
#define GAMEPAD_BIND_PLACE -3
// The button went to the keybind system as a pad key.
#define GAMEPAD_BIND_KEY -4

#define TRIGGER_PRESS 0.45f
#define TRIGGER_RELEASE 0.30f
#define FLICK_PRESS 0.60f
#define FLICK_RELEASE 0.35f
#define TICKS_PER_SECOND 30.0f
#define DEG_TO_RAD 0.01745329252f
#define CURSOR_REFERENCE_HEIGHT 1080.0f
#define LOW_HEALTH_RATIO 0.25f
#define LOW_HEALTH_PULSE_MS 1400

// A mouse user can open a dialogue with a pad plugged in, and must not have the
// cursor taken away.
#define RECENT_PAD_USE_MS 10000
// A popup wants an answer, so it takes the pad for anyone who has used it
// lately, not just this moment.
#define POPUP_PAD_USE_MS 300000

// Windows that appear this soon after arriving on a map were restored, not
// opened.
#define MAP_SETTLE_MS 5000

// A modifier released this soon, with nothing pressed alongside it, was a tap
// of the button itself.
#define MODIFIER_TAP_MS 350

#define DPAD_REPEAT_DELAY_MS 350
#define DPAD_REPEAT_EVERY_MS 110
#define KEYBOARD_AFTER_CLICK_MS 800
#define KEYBOARD_STICK_REPEAT_MS 170

#define TRAYS_PER_SET 3
#define TRAY_SET_COUNT 3

typedef struct GamepadSettings {
	F32 enabled;
	F32 moveDeadzone;
	F32 runThreshold;
	F32 minSpeed;
	F32 turnRate;
	F32 lookDeadzone;
	F32 lookYawSpeed;
	F32 lookPitchSpeed;
	F32 lookCurve;
	F32 invertY;
	F32 cursorSpeed;
	F32 rumble;
	F32 consoleUi;
	F32 consoleCompact;
	F32 consoleTeam;
	F32 consoleLeagueTeam;
	F32 consoleTeamList;
	F32 swapTargetSides;
	F32 autoTarget;
	F32 buttonStyle;
} GamepadSettings;

typedef struct GamepadSetting {
	const char *name;
	F32 *value;
	const char *comment;
} GamepadSetting;

static const char *buttonNames[GP_BUTTON_COUNT] = {
	"A",  "B",  "X",     "Y",	"BACK",	     "START",	  "L3",
	"R3", "LB", "RB",    "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
	"LT", "RT", "RS_UP", "RS_DOWN", "RS_LEFT",   "RS_RIGHT",
};

// The default layout is the Controller keybind profile in
// menu/defaultkey/defaultkeybindings.kb, bound like any other keys.

static GamepadSettings settings;
// When Options last changed a setting, so the file is written once the player
// stops adjusting.
static U64 settingsChangedAt;

// Stored per computer under the game's registry key, like the screen
// settings: a controller belongs to the machine, not the character.
static ParseTable settingRegistryInfo[] = {
	{ "enabled", TOK_F32(GamepadSettings, enabled, 0) },
	{ "move_deadzone", TOK_F32(GamepadSettings, moveDeadzone, 0) },
	{ "run_threshold", TOK_F32(GamepadSettings, runThreshold, 0) },
	{ "min_speed", TOK_F32(GamepadSettings, minSpeed, 0) },
	{ "turn_rate", TOK_F32(GamepadSettings, turnRate, 0) },
	{ "look_deadzone", TOK_F32(GamepadSettings, lookDeadzone, 0) },
	{ "look_yaw_speed", TOK_F32(GamepadSettings, lookYawSpeed, 0) },
	{ "look_pitch_speed", TOK_F32(GamepadSettings, lookPitchSpeed, 0) },
	{ "look_curve", TOK_F32(GamepadSettings, lookCurve, 0) },
	{ "invert_y", TOK_F32(GamepadSettings, invertY, 0) },
	{ "cursor_speed", TOK_F32(GamepadSettings, cursorSpeed, 0) },
	{ "rumble", TOK_F32(GamepadSettings, rumble, 0) },
	{ "console_ui", TOK_F32(GamepadSettings, consoleUi, 0) },
	{ "console_compact", TOK_F32(GamepadSettings, consoleCompact, 0) },
	{ "console_team", TOK_F32(GamepadSettings, consoleTeam, 0) },
	{ "console_league_team",
	  TOK_F32(GamepadSettings, consoleLeagueTeam, 0) },
	{ "console_team_list", TOK_F32(GamepadSettings, consoleTeamList, 0) },
	{ "swap_target_sides", TOK_F32(GamepadSettings, swapTargetSides, 0) },
	{ "auto_target", TOK_F32(GamepadSettings, autoTarget, 0) },
	{ "button_style", TOK_F32(GamepadSettings, buttonStyle, 0) },
	{ "", 0, 0 }
};

static const GamepadSetting settingTable[] = {
	{ "enabled", &settings.enabled,
	  "1 turns the gamepad on. Off, it has no effect at all. Options, "
	  "Gamepad sets it" },
	{ "move_deadzone", &settings.moveDeadzone,
	  "Left stick travel ignored, 0 to 0.9" },
	{ "run_threshold", &settings.runThreshold,
	  "Stick travel past the dead zone that reaches full speed, 0.05 to "
	  "1" },
	{ "min_speed", &settings.minSpeed,
	  "Slowest walking speed, as a share of full speed" },
	{ "turn_rate", &settings.turnRate,
	  "Degrees per second the character turns toward the stick" },
	{ "look_deadzone", &settings.lookDeadzone,
	  "Right stick travel ignored, 0 to 0.9" },
	{ "look_yaw_speed", &settings.lookYawSpeed,
	  "Camera turn speed at full stick, degrees per second" },
	{ "look_pitch_speed", &settings.lookPitchSpeed,
	  "Camera pitch speed at full stick, degrees per second" },
	{ "look_curve", &settings.lookCurve,
	  "1 is linear; higher values give finer control near the center" },
	{ "invert_y", &settings.invertY,
	  "1 flips camera pitch on top of the mouse invert option" },
	{ "cursor_speed", &settings.cursorSpeed,
	  "Cursor speed at full stick, pixels per second at 1080p" },
	{ "rumble", &settings.rumble, "Rumble strength, 0 turns it off" },
	{ "console_compact", &settings.consoleCompact,
	  "1 shows one cluster that switches with the triggers. "
	  "/console_compact switches it" },
	{ "console_team", &settings.consoleTeam,
	  "1 shows the team frames down the left of the console HUD. "
	  "/console_team switches it" },
	{ "console_league_team", &settings.consoleLeagueTeam,
	  "1 keeps the team frames in a league, beside the league's team "
	  "lists. /console_league_team switches it" },
	{ "console_team_list", &settings.consoleTeamList,
	  "1 shows a team as a short list, as the league's teams are, instead "
	  "of the frames. /console_team_list switches it" },
	{ "console_ui", &settings.consoleUi,
	  "1 uses the console HUD instead of the standard tray. /console_ui "
	  "switches it" },
	{ "button_style", &settings.buttonStyle,
	  "Button names shown: 0 from the pad, 1 Xbox, 2 PlayStation, 3 "
	  "Nintendo" },
	{ "swap_target_sides", &settings.swapTargetSides,
	  "1 swaps the bumpers, so LB targets foes and RB targets friends" },
	{ "auto_target", &settings.autoTarget,
	  "1 targets the nearest foe when an attack is fired with nothing to "
	  "hit" },
};

static int padReady;
static int padConnected;
static int configLoaded;
static PadState padState;

static int buttonDown[GP_BUTTON_COUNT];
static int activeBind[GP_BUTTON_COUNT];
// The pad modifiers held when each button went down, for its release.
static int pressModifiers[GP_BUTTON_COUNT];
static U64 modifierTapStart[GP_BUTTON_COUNT];
// True while menus have the pad: opening one enters this, and B or closing them
// leaves it.
static int cursorModeToggled;
// A real pointer on the right stick, for things focus cannot do such as sliders
// and the map.
static int freePointer;
static int pointerHidden;
// The D-pad put the pointer where it is, so the thing under it gets the focus
// highlight.
static int padFocused;
// The right stick moved the pointer since focus was last placed, so the D-pad
// steps from there.
static int stickSteered;
static POINT lastFocusPosition;
// B was pressed on a full screen menu this frame, for its Back or Cancel arrow
// to take.
static int frontEndBack;
// Start was pressed on a full screen menu this frame, for a forward arrow that
// does not take Enter.
static int frontEndForward;
// Menu mode was entered for a right click menu or the game menu. Once that is
// gone, with no window or popup left, the pad goes back to playing instead of
// staying stuck switching windows that are not there.
static int menuModeForPopup;
static int popupSeen;
// A D-pad press scrolled a list. Where the rows were, so focus can land on the
// very next one.
#define SCROLL_FOLLOW_MAX 256
static NavRect scrollFocused;
static float scrollMoved;
static int scrollMovedSeen;
static int scrollDirection;
static int scrollFollowFrames;
// Y was pressed with the store open this frame: sell or buy the row under the
// highlight.
static int quickTrade;
// The chord that last fired a power, so pressing it again can put a location
// power down.
static int lastPowerButton = GAMEPAD_NO_BIND;
static int lastPowerModifiers;

static const int menuWindows[] = { WDW_POWERLIST,
				   WDW_GROUP,
				   WDW_MAP,
				   WDW_FRIENDS,
				   WDW_INSPIRATION,
				   WDW_SUPERGROUP,
				   WDW_EMAIL,
				   WDW_EMAIL_COMPOSE,
				   WDW_CONTACT,
				   WDW_MISSION,
				   WDW_CLUE,
				   WDW_TRADE,
				   WDW_QUIT,
				   WDW_INFO,
				   WDW_HELP,
				   WDW_MISSION_SUMMARY,
				   WDW_LFG,
				   WDW_STORE,
				   WDW_DIALOG,
				   WDW_TITLE_SELECT,
				   WDW_DEATH,
				   WDW_MAP_SELECT,
				   WDW_COSTUME_SELECT,
				   WDW_ENHANCEMENT,
				   WDW_BADGES,
				   WDW_REWARD_CHOICE,
				   WDW_INVENTORY,
				   WDW_SALVAGE,
				   WDW_CONCEPTINV,
				   WDW_RECIPEINV,
				   WDW_INVENT,
				   WDW_PET,
				   WDW_WORKSHOP,
				   WDW_OPTIONS,
				   WDW_PLAQUE,
				   WDW_RECIPEINVENTORY,
				   WDW_AUCTIONHOUSE,
				   WDW_STOREDSALVAGE,
				   WDW_AMOUNTSLIDER,
				   WDW_COLORPICKER,
				   WDW_RECENTTEAM,
				   WDW_INCARNATE,
				   WDW_AUCTION,
				   WDW_LEAGUE,
				   WDW_TURNSTILE,
				   WDW_CONTACT_FINDER };
static U8 menuWindowWasOpen[ARRAY_SIZE(menuWindows)];
// Windows about to be opened again by code, not by the player, which must not
// take the pad when they do.
static U8 menuWindowQuiet[ARRAY_SIZE(menuWindows)];
// A console screen has been open since the last frame, and a stick held as it
// opened has not been let go.
static int navStickWasOpen;
static int navStickWait;
// Where the next first focus may start, so it passes over a strip of sub-tabs.
// 0 for anywhere.
static float focusMinTop;

#define MAX_OPEN_MENUS 16
static int openMenus[MAX_OPEN_MENUS];
static int openMenuCount;
static int activeMenu;
static int focusPending;
static U64 scrollNextStep;
static int padHasFocus;

// A menu that took the left stick has just closed, and the stick has not been
// let go since.
static int stickHeldFromMenu;
static int stickMenuWasOpen;
static F32 moveDirectionX;
static F32 moveDirectionY;
static F32 moveSpeed;
static int moveActive;
static F32 lookYaw;
static F32 lookPitch;
static int lookActive;

static U64 lastUpdateTicks;
static F32 cursorRemainderX;
static F32 cursorRemainderY;

static U64 lastPadUse;
// How many times the D-pad has moved the focus, so a screen holding focus on
// something can let go.
static int navMoves;
static U32 lastDialogueOptions;
static int traceEnabled;
static int dialogueOption;
static U64 dpadRepeatAt;
static U64 lastPadClick;
static int editableFocused;
static U64 keyboardStickNextStep;

static Entity *rumbleEntity;
static F32 rumbleLastHitPoints;
static U64 rumbleNextPulse;

void inpGamepadMouseButton(int rightButton, int down);
void inpGamepadScroll(int down);
void inpGamepadKeyEdge(int key);

static U64 gamepadTicks(void);

static F32 clampFloat(F32 value, F32 low, F32 high)
{
	return value < low ? low : value > high ? high : value;
}

static void setDefaultSettings(void)
{
	// Off until the player turns it on in Options, so a pad left plugged in
	// changes nothing.
	settings.enabled = 0;
	settings.moveDeadzone = 0.18f;
	settings.runThreshold = 0.60f;
	settings.minSpeed = 0.25f;
	settings.turnRate = 600;
	settings.lookDeadzone = 0.15f;
	settings.lookYawSpeed = 200;
	settings.lookPitchSpeed = 120;
	settings.lookCurve = 2;
	settings.invertY = 0;
	settings.cursorSpeed = 1100;
	settings.rumble = 1;
	settings.consoleUi = 0;
	// One cluster that switches with the triggers, so the bar takes little
	// room until the player asks for the full layout.
	settings.consoleCompact = 1;
	settings.consoleTeam = 1;
	settings.consoleLeagueTeam = 0;
	settings.consoleTeamList = 0;
	settings.swapTargetSides = 0;
	settings.autoTarget = 1;
	settings.buttonStyle = GP_STYLE_AUTO;
}

static void clampSettings(void)
{
	settings.moveDeadzone = clampFloat(settings.moveDeadzone, 0, 0.9f);
	settings.runThreshold = clampFloat(settings.runThreshold, 0.05f, 1);
	settings.minSpeed = clampFloat(settings.minSpeed, 0.05f, 1);
	settings.turnRate = clampFloat(settings.turnRate, 30, 3600);
	settings.lookDeadzone = clampFloat(settings.lookDeadzone, 0, 0.9f);
	settings.lookYawSpeed = clampFloat(settings.lookYawSpeed, 1, 1440);
	settings.lookPitchSpeed = clampFloat(settings.lookPitchSpeed, 1, 1440);
	settings.lookCurve = clampFloat(settings.lookCurve, 1, 5);
	settings.cursorSpeed = clampFloat(settings.cursorSpeed, 50, 8000);
	settings.rumble = clampFloat(settings.rumble, 0, 1);
	settings.buttonStyle = clampFloat(floorf(settings.buttonStyle + 0.5f),
					  GP_STYLE_AUTO, GP_STYLE_NINTENDO);
}

static int modifierBit(int button)
{
	switch (button) {
	case GP_LT:
		return GP_MOD_LT;
	case GP_RT:
		return GP_MOD_RT;
	case GP_LB:
		return GP_MOD_LB;
	case GP_RB:
		return GP_MOD_RB;
	}
	return 0;
}

static char *siblingPath(const char *name, char *buffer, size_t bufferSize)
{
	char dir[MAX_PATH];

	snprintf_s(buffer, bufferSize, bufferSize - 1, "%s/%s",
		   getExecutableDir(dir), name);
	return buffer;
}

static void trace(const char *format, ...)
{
	char path[MAX_PATH];
	char text[512];
	va_list args;
	FILE *file;

	if (!traceEnabled)
		return;

	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	text[sizeof(text) - 1] = 0;

	file = fopen(siblingPath(GAMEPAD_TRACE_NAME, path, sizeof(path)), "at");
	if (file) {
		fprintf(file, "%10u %s\n", (unsigned int)gamepadTicks(), text);
		fclose(file);
	}
}

// Console text also appears in the chat window, so the player sees this with
// the console closed.
static void tellPlayer(char *text)
{
	conPrintf("%s", text);
}

static void chordToString(int modifiers, int button, char *out, size_t outSize)
{
	snprintf_s(out, outSize, outSize - 1, "%s%s%s%s%s",
		   modifiers & GP_MOD_LT ? "LT+" : "",
		   modifiers & GP_MOD_RT ? "RT+" : "",
		   modifiers & GP_MOD_LB ? "LB+" : "",
		   modifiers & GP_MOD_RB ? "RB+" : "", buttonNames[button]);
}

// The keybind key code of each pad button.
static const int buttonKeys[GP_BUTTON_COUNT] = {
	INP_JOY1,	    INP_JOY2,
	INP_JOY3,	    INP_JOY4, // A B X Y
	INP_JOY7,	    INP_JOY8,
	INP_JOY9,	    INP_JOY10, // Back Start L3 R3
	INP_JOY5,	    INP_JOY6,  // LB RB
	INP_JOYPAD_UP,	    INP_JOYPAD_DOWN,
	INP_JOYPAD_LEFT,    INP_JOYPAD_RIGHT,
	INP_LTRIGGER,	    INP_RTRIGGER,
	INP_JOYSTICK2_UP,   INP_JOYSTICK2_DOWN,
	INP_JOYSTICK2_LEFT, INP_JOYSTICK2_RIGHT,
};

// The keybind modifier bits of the pad's own modifier bits.
static int keybindModifiers(int modifiers)
{
	return (modifiers & GP_MOD_LT ? KEYBIND_PADMOD_LT : 0) |
	       (modifiers & GP_MOD_RT ? KEYBIND_PADMOD_RT : 0) |
	       (modifiers & GP_MOD_LB ? KEYBIND_PADMOD_LB : 0) |
	       (modifiers & GP_MOD_RB ? KEYBIND_PADMOD_RB : 0);
}

// Sends a pad button to the keybind system with the modifiers it was pressed
// with.
static void sendKey(int button, int modifiers, int down)
{
	keybind_setPadModifiers(keybindModifiers(modifiers));
	inpUpdateKey(buttonKeys[button], down, timeGetTime());
	keybind_setPadModifiers(0);
}

static U64 gamepadTicks(void)
{
	return GetTickCount64();
}

static void saveSettings(void)
{
	char key[512];

	snprintf(key, sizeof(key), "%s\\Gamepad", regGetAppKey());
	ParserWriteRegistry(key, settingRegistryInfo, &settings, 0, 0);
}

void gamepadSaveConfig(void)
{
	saveSettings();
}

void gamepadSaveSettingsQuietly(void)
{
	settingsChangedAt = 0;
	saveSettings();
}

// /gamepad: the same switch as in Options, for when the pad itself is off and
// cannot turn itself back on.
void gamepadToggleEnabled(void)
{
	char on[] = "Gamepad on. Options, Gamepad has its settings.";
	char off[] = "Gamepad off. A controller now does nothing.";

	gamepadSetSetting("enabled", settings.enabled ? 0.0f : 1.0f);
	gamepadSaveSettingsQuietly();
	tellPlayer(settings.enabled ? on : off);
}

static void releaseEverything(void);

void gamepadReloadConfig(void)
{
	char key[512];

	releaseEverything();
	setDefaultSettings();

	snprintf(key, sizeof(key), "%s\\Gamepad", regGetAppKey());
	ParserReadRegistry(key, settingRegistryInfo, &settings);

	clampSettings();
	configLoaded = 1;
}

// Reads the pad, and lets go of everything when it goes away.
static void readDevice(void)
{
	int wasConnected = padConnected;

	gamepadDeviceRead(&padState);
	padConnected = padState.connected;
	padReady = 1;

	if (wasConnected && !padConnected) {
		releaseEverything();
		rumbleEntity = NULL;
	}
}

static F32 axisValue(PadAxis axis)
{
	return padState.axes[axis] / 32767.0f;
}

// A radial dead zone with the remaining travel rescaled to 0..1, so diagonals
// are not clipped and the response starts from zero at the edge of the dead
// zone.
static F32 readStick(PadAxis axisX, PadAxis axisY, F32 deadzone, F32 *x, F32 *y)
{
	F32 rawX = axisValue(axisX);
	F32 rawY = axisValue(axisY);
	F32 length = sqrtf(rawX * rawX + rawY * rawY);
	F32 magnitude;

	*x = 0;
	*y = 0;

	if (length <= deadzone)
		return 0;

	magnitude = clampFloat((length - deadzone) / (1 - deadzone), 0, 1);
	*x = rawX / length;
	*y = rawY / length;
	return magnitude;
}

static int updateHysteresis(int wasDown, F32 value, F32 pressAt, F32 releaseAt)
{
	return wasDown ? value >= releaseAt : value >= pressAt;
}

static int inGame(void)
{
	return isMenu(MENU_GAME);
}

static int cursorModeActive(void)
{
	if (inpIsMouseLocked())
		return 0;
	return cursorModeToggled || !inGame() || consoleWorldTargetActive() ||
	       window_getMode(WDW_CONTACT_DIALOG) == WINDOW_DISPLAYING ||
	       consoleNavIsOpen();
}

// A popup is up and no context menu sits over it.
static int dialogHasPad(void)
{
	return !contextMenu_IsActive() &&
	       window_getMode(WDW_DIALOG) == WINDOW_DISPLAYING;
}

// Tells navigation where these windows sit, for buttons another window drew on
// their behalf.
static void setScopeRects(const int *windows, int count)
{
	NavRect rects[MAX_OPEN_MENUS + 1];
	int rectCount = 0;
	int i;

	for (i = 0; i < count; i++) {
		float x, y, z, wd, ht, scale;
		int color, backColor;

		if (windows[i] < 0 || windows[i] == WDW_TRAY ||
		    window_getMode(windows[i]) != WINDOW_DISPLAYING ||
		    !window_getDims(windows[i], &x, &y, &z, &wd, &ht, &scale,
				    &color, &backColor)) {
			continue;
		}

		rects[rectCount].left = x;
		rects[rectCount].top = y;
		rects[rectCount].right = x + wd;
		rects[rectCount].bottom = y + ht;
		rectCount++;
	}

	gamepadNavSetScopeRects(rects, rectCount);
}

// Which windows the D-pad may reach right now. Staying inside them is what
// keeps focus off whatever is behind and off the chat tabs and buff icons on
// the way across the screen.
static int focusScope(int *windows)
{
	int count = 0;
	int i;

	// A context menu or popup has the pad, but only if it is on screen.
	// Full screen menus do not draw a popup left over from the game, and it
	// must not trap the D-pad there.
	if (contextMenu_IsActive() && gamepadNavHasWindow(NAV_CONTEXT_MENU)) {
		windows[0] = NAV_CONTEXT_MENU;
		gamepadNavSetScopeRects(NULL, 0);
		return 1;
	}
	if (dialogHasPad() && gamepadNavHasWindow(WDW_DIALOG)) {
		windows[0] = WDW_DIALOG;
		gamepadNavSetScopeRects(NULL, 0);
		return 1;
	}

	// The Navigation screen keeps focus inside the tab it shows. Only boxes
	// the tab drew count, since the windows behind the screen still report
	// theirs.
	if (inGame() && consoleNavHostedWindow() >= 0) {
		windows[0] = NAV_HOSTED;
		gamepadNavSetScopeRects(NULL, 0);
		return 1;
	}

	if (!inGame() || !openMenuCount) {
		gamepadNavSetScopeRects(NULL, 0);
		return 0;
	}

	// Holding A after clicking is a drag, and it has to be able to reach
	// the tray and the other open windows. An A press that opened this
	// window from the menu is still down for a moment, and is not one.
	if (buttonDown[GP_A] && activeBind[GP_A] == GAMEPAD_BIND_MOUSE) {
		for (i = 0; i < openMenuCount; i++)
			windows[count++] = openMenus[i];
		windows[count++] = WDW_TRAY;
	} else {
		windows[count++] = openMenus[activeMenu];
	}

	setScopeRects(windows, count);
	return count;
}

static void moveFocusTo(const POINT *origin, const NavPoint *target)
{
	lastFocusPosition.x = origin->x + target->x;
	lastFocusPosition.y = origin->y + target->y;
	SetCursorPos(lastFocusPosition.x, lastFocusPosition.y);
	padFocused = !freePointer;
	stickSteered = 0;
	// Outside the game some screens show no highlight, so there the pointer
	// itself marks the focus.
	pointerHidden = padFocused && inGame();
}

static void addOpenMenu(int window)
{
	int i;

	for (i = 0; i < openMenuCount; i++) {
		if (openMenus[i] == window) {
			activeMenu = i;
			focusPending = 1;
			return;
		}
	}

	if (openMenuCount < MAX_OPEN_MENUS) {
		activeMenu = openMenuCount;
		openMenus[openMenuCount++] = window;
		focusPending = 1;
	}
}

static void leaveMenus(void)
{
	cursorModeToggled = 0;
	menuModeForPopup = 0;
	popupSeen = 0;
	scrollFollowFrames = 0;
	freePointer = 0;
	pointerHidden = 0;
	padFocused = 0;
	focusPending = 0;
	openMenuCount = 0;
	activeMenu = 0;
}

// Opening a menu gives it the pad, and closing the last one gives the pad back
// to playing. A window open since login, or opened with the mouse, does not
// take the pad away.
static void updateOpenMenus(U64 now)
{
	static int wasInGame;
	static U64 inGameSince;
	int i, j;

	if (inGame() && !wasInGame)
		inGameSince = now;
	wasInGame = inGame();

	for (i = 0; i < ARRAY_SIZE(menuWindows); i++) {
		int isOpen = inGame() && window_getMode(menuWindows[i]) ==
						 WINDOW_DISPLAYING;
		// Windows restored on arrival were not opened by the player,
		// even one hidden from view like the auction house with no
		// server. Taking the pad for them leaves the D-pad steering an
		// invisible menu. A popup still wants an answer, so it may take
		// the pad at any time.
		int settling = now - inGameSince < MAP_SETTLE_MS &&
			       menuWindows[i] != WDW_DIALOG;
		// The pet window opens by itself when a summon brings out pets,
		// right after the pad fired the power. Taking the pad for it
		// would leave the camera stuck until the pets are gone. A
		// window the game opens while a console screen is up, like the
		// League window on joining one, would pull focus out of the
		// screen.
		int opensItself = menuWindows[i] == WDW_PET ||
				  menuWindowQuiet[i] || consoleNavIsOpen() ||
				  consoleHudSkipsWindow(menuWindows[i]) ||
				  consoleHudHidesWindow(menuWindows[i]);

		if (isOpen && !menuWindowWasOpen[i])
			menuWindowQuiet[i] = 0;

		if (isOpen && !menuWindowWasOpen[i] && !settling &&
		    !opensItself &&
		    now - lastPadUse <= (menuWindows[i] == WDW_DIALOG
						 ? POPUP_PAD_USE_MS
						 : RECENT_PAD_USE_MS)) {
			trace("menu opened: window %d", menuWindows[i]);
			addOpenMenu(menuWindows[i]);
			cursorModeToggled = 1;
		} else if (!isOpen && menuWindowWasOpen[i]) {
			for (j = 0; j < openMenuCount; j++) {
				if (openMenus[j] == menuWindows[i]) {
					openMenus[j] =
						openMenus[--openMenuCount];
					activeMenu = 0;
					focusPending = openMenuCount > 0;
					if (!openMenuCount)
						leaveMenus();
					break;
				}
			}
		}

		menuWindowWasOpen[i] = (U8)isOpen;
	}

	// A menu that pops up after the pad was used, like the game menu from
	// the wheel, a drop down list or a right click, takes focus on its
	// first item. It can open far from the pointer, out of D-pad reach.
	{
		static int contextWasUp;
		static int returnAfterMenu;
		static POINT beforeMenu;
		int contextUp = contextMenu_IsActive() &&
				gamepadNavHasWindow(NAV_CONTEXT_MENU);

		if (contextUp && !contextWasUp &&
		    now - lastPadUse <= RECENT_PAD_USE_MS) {
			trace("menu popped up, focusing its first item");
			// Once it is answered or closed, focus goes back to the
			// row or button that opened it.
			beforeMenu = lastFocusPosition;
			returnAfterMenu = padFocused && inGame();
			focusPending = 1;
			focusMinTop = 0;
			padFocused = 0;
			freePointer = 0;
			stickSteered = 0;
			// A bind like "menu" opens it while playing. The D-pad
			// and B then belong to the menu until it closes.
			if (inGame() && !cursorModeToggled) {
				cursorModeToggled = 1;
				menuModeForPopup = 1;
				popupSeen = 0;
			}
		} else if (!contextMenu_IsActive() && contextWasUp &&
			   returnAfterMenu && cursorModeActive()) {
			trace("menu gone, focus back to %d,%d", beforeMenu.x,
			      beforeMenu.y);
			lastFocusPosition = beforeMenu;
			SetCursorPos(beforeMenu.x, beforeMenu.y);
			padFocused = 1;
			pointerHidden = 1;
			focusPending = 0;
			returnAfterMenu = 0;
		}
		if (!contextMenu_IsActive())
			contextWasUp = 0;
		else if (contextUp)
			contextWasUp = 1;
	}

	// The right click or game menu the pad opened is gone, and nothing else
	// is open: back to playing.
	if (menuModeForPopup && cursorModeToggled && inGame()) {
		if (contextMenu_IsActive() ||
		    window_getMode(WDW_DIALOG) == WINDOW_DISPLAYING) {
			popupSeen = 1;
		} else if (popupSeen && !openMenuCount) {
			trace("menu closed, back to playing");
			leaveMenus();
		}
	}
}

// A full screen menu, like levelling up or the front end, starts with focus on
// its first button, so the first D-pad press does not depend on where the
// pointer was left. Its buttons are reported a frame after it opens, so focus
// waits a couple of frames rather than landing on the last screen's.
static void updateScreenChange(U64 now)
{
	static int lastMenu = -1;
	static int framesUntilFocus;
	int menu = current_menu();

	static int dialogWasUp;
	static int returnAfterDialog;
	static POINT beforeDialog;
	int dialogUp =
		!inGame() && window_getMode(WDW_DIALOG) == WINDOW_DISPLAYING;

	if (menu != lastMenu) {
		lastMenu = menu;
		returnAfterDialog = 0;
		framesUntilFocus =
			!inGame() && now - lastPadUse <= RECENT_PAD_USE_MS ? 2
									   : 0;
	} else if (framesUntilFocus > 0 && --framesUntilFocus == 0) {
		trace("screen changed, focusing its first button");
		focusPending = 1;
	}

	// Outside the world a popup is not one of the game's windows, so it is
	// caught here. It takes focus as it appears, and once answered focus
	// goes back to where it was on the screen behind it.
	if (dialogUp && !dialogWasUp && now - lastPadUse <= POPUP_PAD_USE_MS) {
		trace("popup on a full screen menu, focusing it");
		beforeDialog = lastFocusPosition;
		returnAfterDialog = padFocused;
		focusPending = 1;
	} else if (!dialogUp && dialogWasUp && returnAfterDialog && !inGame()) {
		trace("popup answered, focus back to %d,%d", beforeDialog.x,
		      beforeDialog.y);
		lastFocusPosition = beforeDialog;
		SetCursorPos(beforeDialog.x, beforeDialog.y);
		padFocused = 1;
		focusPending = 0;
		returnAfterDialog = 0;
	}
	dialogWasUp = dialogUp;
}

static void cycleOpenMenu(int direction)
{
	if (openMenuCount < 2)
		return;

	activeMenu = (activeMenu + direction + openMenuCount) % openMenuCount;
	focusPending = 1;
}

#define DIALOG_SETTLE_MS 300
static U64 dialogSeenAt;

static void updateFocus(void)
{
	POINT cursor;
	POINT origin = { 0, 0 };
	int windows[MAX_OPEN_MENUS + 1];
	int count, width = 0, height = 0;
	NavPoint target;

	if (!GetCursorPos(&cursor))
		return;

	// The mouse moved, so whoever is holding it gets their pointer back.
	if ((pointerHidden || padFocused) &&
	    (cursor.x != lastFocusPosition.x ||
	     cursor.y != lastFocusPosition.y)) {
		pointerHidden = 0;
		padFocused = 0;
	}

	if (!focusPending)
		dialogSeenAt = 0;
	if (!focusPending || !cursorModeActive() ||
	    !ClientToScreen(hwnd, &origin))
		return;

	count = focusScope(windows);
	windowClientSize(&width, &height);

	// A popup grows open over a few frames, and its buttons are not there
	// on the first of them, so its text links would win. It gets a moment
	// to finish before focus is placed.
	if (count == 1 && windows[0] == WDW_DIALOG) {
		if (!dialogSeenAt)
			dialogSeenAt = gamepadTicks();
		if (gamepadTicks() - dialogSeenAt < DIALOG_SETTLE_MS)
			return;
	}
	dialogSeenAt = 0;

	// A popup's buttons sit under its text, and the text can hold links, so
	// a popup starts on its buttons.
	if (count == 1 && windows[0] == WDW_DIALOG
		    ? gamepadNavLastRow(windows, count, width, height, &target)
	    : focusMinTop > 0
		    ? gamepadNavFirstBelow(windows, count, focusMinTop, width,
					   height, &target)
		    : gamepadNavFirst(windows, count, width, height, &target)) {
		focusMinTop = 0;
		trace("focus on %d,%d", target.x, target.y);
		moveFocusTo(&origin, &target);
		focusPending = 0;
	}
}

// A location power the pad fired starts aiming at the target, or at the ground
// ahead, rather than wherever the pointer was left. The pointer shows, and the
// right stick moves it from there.
static void updateLocationAim(U64 now)
{
	static int wasAiming;
	int aiming = inGame() && consoleWorldTargetActive();
	POINT origin = { 0, 0 };
	int x, y;

	if (aiming && !wasAiming && now - lastPadUse < 1000 &&
	    ClientToScreen(hwnd, &origin)) {
		if (consoleWorldTargetAimPoint(&x, &y)) {
			lastFocusPosition.x = origin.x + x;
			lastFocusPosition.y = origin.y + y;
			SetCursorPos(lastFocusPosition.x, lastFocusPosition.y);
		} else {
			gamepadCenterPointer();
		}
		pointerHidden = 0;
		padFocused = 0;
		trace("aiming a location power from %d,%d",
		      lastFocusPosition.x - origin.x,
		      lastFocusPosition.y - origin.y);
	}
	wasAiming = aiming;
}

// Two frames after a D-pad press scrolled a list, the rows have been drawn
// where they now are.
static void updateScrollFollow(void)
{
	NavRect after[SCROLL_FOLLOW_MAX];
	int windows[MAX_OPEN_MENUS + 1];
	POINT origin = { 0, 0 };
	int count, afterCount, width = 0, height = 0;
	NavPoint target;

	if (!scrollFollowFrames || --scrollFollowFrames)
		return;
	if (!scrollMovedSeen || !cursorModeActive() ||
	    !ClientToScreen(hwnd, &origin))
		return;

	count = focusScope(windows);
	windowClientSize(&width, &height);
	afterCount = gamepadNavTargets(windows, count, width, height, after,
				       SCROLL_FOLLOW_MAX);

	if (gamepadNavFollowScroll(&scrollFocused, after, afterCount,
				   scrollDirection, scrollMoved, &target)) {
		trace("list scrolled, focus to the next row at %d,%d", target.x,
		      target.y);
		moveFocusTo(&origin, &target);
	}
}

static int currentModifiers(void)
{
	return (buttonDown[GP_LT] ? GP_MOD_LT : 0) |
	       (buttonDown[GP_RT] ? GP_MOD_RT : 0) |
	       (buttonDown[GP_LB] ? GP_MOD_LB : 0) |
	       (buttonDown[GP_RB] ? GP_MOD_RB : 0);
}

static void runCommand(const char *command)
{
	char buffer[256];

	// cmdParse writes into the string it is given when it splits $$ chains.
	strncpy(buffer, command, sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = 0;

	cmdSetTimeStamp(timeGetTime());
	cmdParse(buffer);
}

static int isDpadButton(int button)
{
	return button == GP_DPAD_UP || button == GP_DPAD_DOWN ||
	       button == GP_DPAD_LEFT || button == GP_DPAD_RIGHT;
}

// Jumps the cursor to the next thing in the D-pad's direction. Up or down with
// nothing left to reach scrolls instead, so long lists can be walked to their
// end.
static void navigate(int button)
{
	int directionX = button == GP_DPAD_RIGHT  ? 1
			 : button == GP_DPAD_LEFT ? -1
						  : 0;
	int directionY = button == GP_DPAD_DOWN ? 1
			 : button == GP_DPAD_UP ? -1
						: 0;
	POINT cursor;
	POINT origin = { 0, 0 };
	int windows[MAX_OPEN_MENUS + 1];
	int count;
	int width = 0, height = 0;
	NavPoint target;

	windowClientSize(&width, &height);
	if (!GetCursorPos(&cursor) || !ClientToScreen(hwnd, &origin))
		return;

	count = focusScope(windows);

	// Nothing has the pad's focus yet: the mouse moved last, or the screen
	// was opened some other way. The first press lands on the screen's
	// first button, not on whatever is next to a pointer left anywhere. A
	// pointer the player steered with the right stick is where they meant
	// to be, so that one is kept.
	if (!padFocused && !freePointer && !stickSteered &&
	    (count == 1 && windows[0] == WDW_DIALOG
		     ? gamepadNavLastRow(windows, count, width, height, &target)
		     : gamepadNavFirst(windows, count, width, height,
				       &target))) {
		trace("navigate: nothing focused, start at %d,%d", target.x,
		      target.y);
		moveFocusTo(&origin, &target);
		focusPending = 0;
		return;
	}

	if ((count == 1 && windows[0] == WDW_DIALOG
		     ? gamepadNavPickWide(windows, count, cursor.x - origin.x,
					  cursor.y - origin.y, directionX,
					  directionY, width, height, &target)
		     : gamepadNavPick(windows, count, cursor.x - origin.x,
				      cursor.y - origin.y, directionX,
				      directionY, width, height, &target))) {
		trace("navigate to %d,%d", target.x, target.y);
		navMoves++;
		moveFocusTo(&origin, &target);
	} else if (inGame() && consoleNavOnMap()) {
		// No marker that way on the map: move the map that way instead,
		// so more of it comes into reach.
		trace("navigate: nothing there, move the map");
		consoleNavMapNudge(directionX, directionY);
	} else if (directionY) {
		trace("navigate: nothing there, scroll");

		// Remember the rows, so once the list has moved focus goes to
		// the next one, not one a whole wheel notch further on.
		if (gamepadNavFocused(windows, count, cursor.x - origin.x,
				      cursor.y - origin.y, width, height,
				      &scrollFocused)) {
			scrollMoved = 0;
			scrollMovedSeen = 0;
			scrollDirection = directionY;
			scrollFollowFrames = 2;
		}
		inpGamepadScroll(directionY > 0);
	} else {
		trace("navigate: nothing there");
	}
}

static void moveCursorToDialogueOption(int index)
{
	int x, y;
	POINT point = { 0, 0 };

	if (!contactDialogOptionPos(index, &x, &y))
		return;

	trace("cursor moved to dialogue option %d at %d,%d", index + 1, x, y);

	if (ClientToScreen(hwnd, &point))
		SetCursorPos(point.x + x, point.y + y);
}

static void stepDialogueOption(int direction)
{
	int count = contactDialogOptionCount();

	if (count <= 0)
		return;

	dialogueOption = (dialogueOption + direction + count) % count;
	moveCursorToDialogueOption(dialogueOption);
}

static void pressButton(int button)
{
	int modifiers = currentModifiers() & ~modifierBit(button);
	char chord[64];
	const char *command;

	activeBind[button] = GAMEPAD_NO_BIND;
	pressModifiers[button] = modifiers;
	lastPadUse = gamepadTicks();

	chordToString(modifiers, button, chord, sizeof(chord));

	if (gamepadKeyboardIsOpen()) {
		trace("%s: keyboard", chord);
		gamepadKeyboardPress(button);
		dpadRepeatAt = lastPadUse + DPAD_REPEAT_DELAY_MS;
		return;
	}

	// A text field holding focus, like the chat line, blocks every game
	// button, so B lets go of it.
	if (!modifiers && button == GP_B && uiFocusIsEditable()) {
		trace("%s: leave the text field", chord);
		uiReturnAllFocus();
		return;
	}

	if (consoleMenuIsOpen()) {
		trace("%s: radial menu", chord);
		if (button == GP_A)
			consoleMenuConfirm();
		else if (button == GP_B || button == GP_START)
			consoleMenuClose();
		else if (!modifiers && (button == GP_LB || button == GP_RB))
			consoleMenuPage(button);
		return;
	}

	if (consoleBindIsOpen()) {
		trace("%s: binding screen", chord);
		consoleBindPress(modifiers, button);
		dpadRepeatAt = lastPadUse + DPAD_REPEAT_DELAY_MS;
		return;
	}

	// The Navigation screen takes the bumpers, B and Y, and passes the
	// D-pad, A and X to the tab inside it. Chords never reach the game
	// behind it.
	if (inGame() && consoleNavIsOpen() &&
	    (modifiers || consoleNavPress(button))) {
		trace("%s: navigation screen", chord);
		return;
	}

	if (!modifiers && inGame() &&
	    window_getMode(WDW_CONTACT_DIALOG) == WINDOW_DISPLAYING) {
		if (button == GP_DPAD_UP || button == GP_DPAD_DOWN) {
			stepDialogueOption(button == GP_DPAD_DOWN ? 1 : -1);
			return;
		}
		if (button == GP_B) {
			trace("%s: leave dialogue", chord);
			contactDialogClose();
			return;
		}
	}

	// Anything pressed with a modifier held makes this a chord, so no held
	// modifier is a tap any more.
	if (modifiers)
		memset(modifierTapStart, 0, sizeof(modifierTapStart));
	else if (modifierBit(button)) {
		modifierTapStart[button] = lastPadUse;
		return;
	}

	// A text field has focus, so the keyboard is the way to fill it,
	// whatever else is going on.
	if (!modifiers && button == GP_Y && uiFocusIsEditable()) {
		trace("%s: open keyboard", chord);
		gamepadKeyboardOpen();
		return;
	}

	// Aiming a location power, its own button again puts it down where the
	// pointer is.
	if (inGame() && consoleWorldTargetActive() &&
	    lastPowerButton == button && lastPowerModifiers == modifiers) {
		trace("%s: place the location power", chord);
		inpGamepadMouseButton(0, 1);
		activeBind[button] = GAMEPAD_BIND_PLACE;
		return;
	}

	if (cursorModeActive() && !modifiers) {
		// Outside the world, Start is Enter: log in, enter the game,
		// Next in creation and Finish when levelling. Those screens
		// take Enter for their forward button, which the D-pad cannot
		// reach.
		if (button == GP_START && !inGame()) {
			trace("%s: enter", chord);
			inpGamepadKeyEdge(INP_RETURN);
			frontEndForward = 1;
			return;
		}

		if (button == GP_A || button == GP_X) {
			trace("%s: %s click", chord,
			      button == GP_X ? "right" : "left");
			inpGamepadMouseButton(button == GP_X, 1);
			activeBind[button] = GAMEPAD_BIND_MOUSE;
			if (button == GP_A)
				lastPadClick = lastPadUse;
			return;
		}

		if (button == GP_B) {
			// Back: one press from anywhere returns to the default
			// state. A context menu closes alone, and the front end
			// has its own Back buttons.
			trace("%s: back", chord);
			if (consoleCloseContextMenu())
				return;
			if (consoleWorldTargetActive()) {
				char cancel[] = "powexec_abort";
				runCommand(cancel);
			} else if (inGame()) {
				char back[] = "windowcloseextra";
				int i;

				runCommand(back);

				// That leaves the main windows open, like Team,
				// and B is meant to put everything the pad
				// opened away. A popup still wants its answer.
				for (i = 0; i < openMenuCount; i++) {
					if (openMenus[i] != WDW_DIALOG &&
					    window_getMode(openMenus[i]) ==
						    WINDOW_DISPLAYING)
						window_setMode(
							openMenus[i],
							WINDOW_SHRINKING);
				}
			} else if (isMenu(MENU_COMBINE_SPEC)) {
				// The Manage screen is a full screen menu
				// rather than a window, and Escape leaves it.
				char leave[] = "gamereturn";
				runCommand(leave);
			} else if (!inGame()) {
				// Creation, levelling and the like have a Back
				// or Cancel arrow at the bottom left.
				frontEndBack = 1;
			}
			leaveMenus();
			return;
		}

		// At a store, Y sells or buys whatever row has the highlight,
		// so nothing needs the button below.
		if (button == GP_Y && inGame() &&
		    window_getMode(WDW_STORE) == WINDOW_DISPLAYING) {
			trace("%s: quick sell or buy", chord);
			quickTrade = 1;
			return;
		}

		if (button == GP_R3) {
			freePointer = !freePointer;
			padFocused = !freePointer;
			pointerHidden = padFocused && inGame();
			trace("%s: free pointer %s", chord,
			      freePointer ? "on" : "off");
			return;
		}

		if (isDpadButton(button)) {
			navigate(button);
			dpadRepeatAt = lastPadUse + DPAD_REPEAT_DELAY_MS;
			return;
		}
	}

	if (!inGame() || !okToInput(0)) {
		trace("%s: blocked, %s", chord,
		      inGame() ? "a text field has focus"
			       : "not in the game world");
		return;
	}

	command = gamepadBoundCommand(modifiers, button);
	if (!command) {
		trace("%s: no binding", chord);
		return;
	}

	trace("%s: bind \"%s\"", chord, command);

	if (strnicmp(command, "powexec_", 8) == 0) {
		lastPowerButton = button;
		lastPowerModifiers = modifiers;
		if (settings.autoTarget && consoleAttackNeedsTarget(command)) {
			char nearest[] = "target_enemy_near";

			trace("%s: nothing to hit, target the nearest foe",
			      chord);
			runCommand(nearest);
		}
	}

	// The keybind system runs the command, and its release for +hold
	// commands.
	activeBind[button] = GAMEPAD_BIND_KEY;
	sendKey(button, modifiers, 1);
}

static void runModifierTap(int button)
{
	const char *command = gamepadBoundCommand(0, button);

	// In the tailor the bumpers jump between the parts list on the left and
	// the colors on the right.
	if ((button == GP_LB || button == GP_RB) && isMenu(MENU_TAILOR) &&
	    cursorModeActive() && gamepadConsoleUiEnabled()) {
		POINT cursor;
		POINT origin = { 0, 0 };
		int windows[MAX_OPEN_MENUS + 1];
		int count, width = 0, height = 0;
		NavPoint target;

		trace("%s tap: tailor side", buttonNames[button]);
		if (GetCursorPos(&cursor) && ClientToScreen(hwnd, &origin)) {
			count = focusScope(windows);
			windowClientSize(&width, &height);
			if (gamepadNavPickSide(windows, count,
					       cursor.x - origin.x,
					       cursor.y - origin.y,
					       button == GP_RB ? 1 : -1, width,
					       height, &target))
				moveFocusTo(&origin, &target);
		}
		return;
	}

	// With menus open the bumpers change window instead of target.
	if ((button == GP_LB || button == GP_RB) && inGame() &&
	    cursorModeToggled) {
		trace("%s tap: change window", buttonNames[button]);
		cycleOpenMenu(button == GP_RB ? 1 : -1);
		return;
	}

	if (!command) {
		trace("%s tap: no binding", buttonNames[button]);
	} else if (!inGame() || !okToInput(0)) {
		trace("%s tap: blocked, %s", buttonNames[button],
		      inGame() ? "a text field has focus"
			       : "not in the game world");
	} else {
		trace("%s tap: bind \"%s\"", buttonNames[button], command);
		sendKey(button, 0, 1);
		sendKey(button, 0, 0);
	}
}

static void releaseButton(int button)
{
	int index = activeBind[button];
	U64 tapStart = modifierTapStart[button];

	activeBind[button] = GAMEPAD_NO_BIND;
	modifierTapStart[button] = 0;

	if (tapStart && gamepadTicks() - tapStart <= MODIFIER_TAP_MS) {
		runModifierTap(button);
		return;
	}

	if (index == GAMEPAD_BIND_MOUSE) {
		inpGamepadMouseButton(button == GP_X, 0);
	} else if (index == GAMEPAD_BIND_PLACE) {
		inpGamepadMouseButton(0, 0);
	} else if (index == GAMEPAD_BIND_KEY) {
		sendKey(button, pressModifiers[button], 0);
	}
}

// Holding a direction keeps moving, on menus and on the keyboard.
static void updateDpadRepeat(U64 now)
{
	int navigating = gamepadKeyboardIsOpen() || consoleBindIsOpen() ||
			 (cursorModeActive() && !currentModifiers());
	int i;

	if (!navigating || now < dpadRepeatAt)
		return;

	for (i = GP_DPAD_UP; i <= GP_DPAD_RIGHT; i++) {
		if (buttonDown[i]) {
			dpadRepeatAt = now + DPAD_REPEAT_EVERY_MS;
			lastPadUse = now;

			if (gamepadKeyboardIsOpen())
				gamepadKeyboardPress(i);
			else if (consoleBindIsOpen())
				consoleBindSteer(i);
			else if (window_getMode(WDW_CONTACT_DIALOG) !=
				 WINDOW_DISPLAYING)
				navigate(i);
			return;
		}
	}
}

static void setButton(int button, int down)
{
	if (down == buttonDown[button])
		return;

	// Modifier state for a press includes everything already held, so
	// update it first.
	buttonDown[button] = down;
	// Holds bound to a trigger or bumper mix last while the mix is held.
	keybind_setPadModifiersHeld(keybindModifiers(currentModifiers()));

	if (down)
		pressButton(button);
	else
		releaseButton(button);
}

static void releaseEverything(void)
{
	int i;

	// Losing focus while holding a modifier is not a tap.
	memset(modifierTapStart, 0, sizeof(modifierTapStart));

	for (i = 0; i < GP_BUTTON_COUNT; i++)
		setButton(i, 0);

	moveActive = 0;
	lookActive = 0;
	moveSpeed = 0;
	lookYaw = 0;
	lookPitch = 0;

	gamepadDeviceRumble(0, 0, 0);
}

static void moveCursor(F32 x, F32 y, F32 magnitude, F32 seconds)
{
	int width = 0, height = 0;
	POINT origin = { 0, 0 };
	POINT cursor;
	F32 pixels;

	if (magnitude <= 0 || !GetCursorPos(&cursor))
		return;

	windowClientSize(&width, &height);
	if (width <= 0 || height <= 0 || !ClientToScreen(hwnd, &origin))
		return;

	pixels = settings.cursorSpeed * (height / CURSOR_REFERENCE_HEIGHT) *
		 magnitude * magnitude * seconds;
	cursorRemainderX += x * pixels;
	cursorRemainderY += y * pixels;

	cursor.x += (LONG)cursorRemainderX;
	cursor.y += (LONG)cursorRemainderY;
	cursorRemainderX -= (LONG)cursorRemainderX;
	cursorRemainderY -= (LONG)cursorRemainderY;

	cursor.x = cursor.x < origin.x		  ? origin.x
		   : cursor.x >= origin.x + width ? origin.x + width - 1
						  : cursor.x;
	cursor.y = cursor.y < origin.y		   ? origin.y
		   : cursor.y >= origin.y + height ? origin.y + height - 1
						   : cursor.y;

	SetCursorPos(cursor.x, cursor.y);
}

// Lands on the first option when a contact dialogue opens or moves to its next
// page.
static void updateDialogueCursor(void)
{
	int x, y;
	U32 options;

	if (!contactDialogFirstOption(&x, &y, &options) ||
	    options == lastDialogueOptions)
		return;

	lastDialogueOptions = options;
	dialogueOption = 0;

	if (inpIsMouseLocked() ||
	    gamepadTicks() - lastPadUse > RECENT_PAD_USE_MS)
		return;

	moveCursorToDialogueOption(0);
}

static void updateRumble(void)
{
	Entity *player;
	U64 now;
	F32 hitPoints, maxHitPoints;

	if (settings.rumble <= 0 || !inGame())
		return;

	player = playerPtr();
	if (!player || !player->pchar) {
		rumbleEntity = NULL;
		return;
	}

	hitPoints = player->pchar->attrCur.fHitPoints;
	maxHitPoints = player->pchar->attrMax.fHitPoints;
	if (maxHitPoints <= 0)
		return;

	if (player != rumbleEntity) {
		rumbleEntity = player;
		rumbleLastHitPoints = hitPoints;
		return;
	}

	now = gamepadTicks();

	if (rumbleLastHitPoints - hitPoints > maxHitPoints * 0.01f) {
		F32 strength = clampFloat(0.25f + 1.5f *
							  (rumbleLastHitPoints -
							   hitPoints) /
							  maxHitPoints,
					  0, 1) *
			       settings.rumble;

		gamepadDeviceRumble((U16)(strength * 65535),
				    (U16)(strength * 40000), 180);
	} else if (hitPoints > 0 &&
		   hitPoints < maxHitPoints * LOW_HEALTH_RATIO &&
		   now >= rumbleNextPulse) {
		rumbleNextPulse = now + LOW_HEALTH_PULSE_MS;
		gamepadDeviceRumble((U16)(settings.rumble * 22000), 0, 120);
	}

	rumbleLastHitPoints = hitPoints;
}

void gamepadUpdate(void)
{
	static const struct {
		int button;
		PadButton padButton;
	} digitalButtons[] = {
		{ GP_LB, PAD_BUTTON_LB },
		{ GP_RB, PAD_BUTTON_RB },
		{ GP_A, PAD_BUTTON_A },
		{ GP_B, PAD_BUTTON_B },
		{ GP_X, PAD_BUTTON_X },
		{ GP_Y, PAD_BUTTON_Y },
		{ GP_BACK, PAD_BUTTON_BACK },
		{ GP_START, PAD_BUTTON_START },
		{ GP_L3, PAD_BUTTON_LSTICK },
		{ GP_R3, PAD_BUTTON_RSTICK },
		{ GP_DPAD_UP, PAD_BUTTON_DPAD_UP },
		{ GP_DPAD_DOWN, PAD_BUTTON_DPAD_DOWN },
		{ GP_DPAD_LEFT, PAD_BUTTON_DPAD_LEFT },
		{ GP_DPAD_RIGHT, PAD_BUTTON_DPAD_RIGHT },
	};
	F32 stickX, stickY, magnitude;
	F32 lookX, lookY, lookMagnitude;
	F32 seconds;
	U64 now;
	int holdingTargetMode;
	int cursorMode;
	int i;

	if (!configLoaded)
		gamepadReloadConfig();

	// B is Back for one frame only, so a screen without a Back button does
	// not keep it for the next one.
	frontEndBack = 0;
	frontEndForward = 0;
	quickTrade = 0;

	// Settings changed from Options are written out once the player has
	// stopped adjusting them.
	if (settingsChangedAt && gamepadTicks() - settingsChangedAt > 1000) {
		settingsChangedAt = 0;
		saveSettings();
	}

	if (!settings.enabled) {
		if (padHasFocus)
			releaseEverything();
		padHasFocus = 0;
		return;
	}

	readDevice();
	gamepadNavBeginFrame();

	now = gamepadTicks();
	seconds = clampFloat((now - lastUpdateTicks) / 1000.0f, 0, 0.1f);
	lastUpdateTicks = now;

	if (!padConnected) {
		padHasFocus = 0;
		return;
	}

	padHasFocus = 1;

	// Triggers and bumpers go first so a chord pressed on a single frame
	// still sees its modifiers.
	setButton(GP_LT, updateHysteresis(buttonDown[GP_LT],
					  axisValue(PAD_AXIS_LTRIGGER),
					  TRIGGER_PRESS, TRIGGER_RELEASE));
	setButton(GP_RT, updateHysteresis(buttonDown[GP_RT],
					  axisValue(PAD_AXIS_RTRIGGER),
					  TRIGGER_PRESS, TRIGGER_RELEASE));

	for (i = 0; i < ARRAY_SIZE(digitalButtons); i++) {
		PadButton padButton = digitalButtons[i].padButton;

		// Swap Target Bumpers in Options trades the two bumpers, foes
		// on the left and friends on the right.
		if (settings.swapTargetSides && padButton == PAD_BUTTON_LB)
			padButton = PAD_BUTTON_RB;
		else if (settings.swapTargetSides && padButton == PAD_BUTTON_RB)
			padButton = PAD_BUTTON_LB;

		setButton(digitalButtons[i].button,
			  padState.buttons[padButton]);
	}

	updateDpadRepeat(now);

	// A field that took focus from an A press wants typing, so bring the
	// keyboard up for it.
	{
		int editable = uiFocusIsEditable();

		if (editable && !editableFocused &&
		    now - lastPadClick <= KEYBOARD_AFTER_CLICK_MS &&
		    !gamepadKeyboardIsOpen()) {
			trace("text field focused, opening keyboard");
			gamepadKeyboardOpen();
		}

		editableFocused = editable;

		if (gamepadKeyboardIsOpen() && !editable)
			gamepadKeyboardClose();
	}

	holdingTargetMode = buttonDown[GP_LB] || buttonDown[GP_RB];

	// Right stick flicks act as buttons while a targeting mode is held.
	{
		F32 rawX = axisValue(PAD_AXIS_RIGHTX);
		F32 rawY = axisValue(PAD_AXIS_RIGHTY);

		setButton(GP_RS_RIGHT,
			  holdingTargetMode &&
				  updateHysteresis(buttonDown[GP_RS_RIGHT],
						   rawX, FLICK_PRESS,
						   FLICK_RELEASE));
		setButton(GP_RS_LEFT,
			  holdingTargetMode &&
				  updateHysteresis(buttonDown[GP_RS_LEFT],
						   -rawX, FLICK_PRESS,
						   FLICK_RELEASE));
		setButton(GP_RS_DOWN,
			  holdingTargetMode &&
				  updateHysteresis(buttonDown[GP_RS_DOWN], rawY,
						   FLICK_PRESS, FLICK_RELEASE));
		setButton(GP_RS_UP,
			  holdingTargetMode &&
				  updateHysteresis(buttonDown[GP_RS_UP], -rawY,
						   FLICK_PRESS, FLICK_RELEASE));
	}

	cursorMode = cursorModeActive();

	{
		int stickMenuOpen =
			consoleMenuIsOpen() || consoleBindIsOpen() ||
			gamepadKeyboardIsOpen() || consoleNavIsOpen();

		if (stickMenuWasOpen && !stickMenuOpen)
			stickHeldFromMenu = 1;
		stickMenuWasOpen = stickMenuOpen;
	}

	// Left stick: movement in game, a second cursor stick in the front end.
	magnitude = readStick(PAD_AXIS_LEFTX, PAD_AXIS_LEFTY,
			      settings.moveDeadzone, &stickX, &stickY);
	if (magnitude > 0)
		lastPadUse = now;

	if (!inGame()) {
		// The front end has no character to move, so this stick walks
		// focus like the D-pad.
		moveActive = 0;
		moveSpeed = 0;

		if (magnitude > 0.5f && now >= keyboardStickNextStep &&
		    !gamepadKeyboardIsOpen()) {
			keyboardStickNextStep = now + DPAD_REPEAT_DELAY_MS;
			navigate(fabsf(stickX) > fabsf(stickY)
					 ? (stickX > 0 ? GP_DPAD_RIGHT
						       : GP_DPAD_LEFT)
					 : (stickY > 0 ? GP_DPAD_DOWN
						       : GP_DPAD_UP));
		}
	} else if (stickHeldFromMenu) {
		// The thumb that steered a menu is often still pushed when the
		// menu closes. Walking off with it is a surprise, so the stick
		// waits until it is let go.
		moveActive = 0;
		moveSpeed = 0;
		if (magnitude <= 0)
			stickHeldFromMenu = 0;
	} else {
		moveActive = magnitude > 0;
		moveDirectionX = stickX;
		moveDirectionY = -stickY;
		moveSpeed =
			moveActive
				? clampFloat(magnitude / settings.runThreshold,
					     settings.minSpeed, 1)
				: 0;
	}

	// Right stick: cursor or camera, unless a targeting mode has claimed
	// it.
	lookMagnitude = readStick(PAD_AXIS_RIGHTX, PAD_AXIS_RIGHTY,
				  settings.lookDeadzone, &lookX, &lookY);
	if (lookMagnitude > 0)
		lastPadUse = now;

	updateDialogueCursor();
	updateOpenMenus(now);
	updateScreenChange(now);
	updateFocus();
	updateScrollFollow();
	updateLocationAim(now);

	lookActive = 0;

	// The keyboard takes the sticks too, with a stick push stepping like
	// the D-pad.
	if (gamepadKeyboardIsOpen()) {
		moveActive = 0;
		moveSpeed = 0;
		lookYaw = 0;
		lookPitch = 0;

		if ((magnitude > 0.5f || lookMagnitude > 0.5f) &&
		    now >= keyboardStickNextStep) {
			F32 x = magnitude > 0.5f ? stickX : lookX;
			F32 y = magnitude > 0.5f ? stickY : lookY;

			keyboardStickNextStep = now + KEYBOARD_STICK_REPEAT_MS;
			gamepadKeyboardPress(
				fabsf(x) > fabsf(y)
					? (x > 0 ? GP_DPAD_RIGHT : GP_DPAD_LEFT)
					: (y > 0 ? GP_DPAD_DOWN : GP_DPAD_UP));
		}

		updateRumble();
		return;
	}

	// The Navigation screen is a menu, so the character stands still. On
	// the map the left stick is the pointer, the right stick moves the map
	// and the triggers zoom. On the lists the left stick steps like the
	// D-pad and the right stick scrolls.
	if (inGame() && consoleNavIsOpen()) {
		moveActive = 0;
		moveSpeed = 0;
		lookYaw = 0;
		lookPitch = 0;

		// A stick still pushed from picking the screen on the wheel
		// waits to be let go, so it does not walk off through the new
		// screen.
		if (!navStickWasOpen &&
		    (magnitude > 0.3f || lookMagnitude > 0.3f))
			navStickWait = 1;
		navStickWasOpen = 1;
		if (navStickWait) {
			if (magnitude <= 0.3f && lookMagnitude <= 0.3f)
				navStickWait = 0;
			magnitude = 0;
			lookMagnitude = 0;
		}

		if (consoleNavOnMap() && consoleNavHostedWindow() >= 0) {
			if (magnitude > 0) {
				pointerHidden = 0;
				padFocused = 0;
				stickSteered = 1;
				moveCursor(stickX, stickY, magnitude, seconds);
			}
			consoleNavMapControl(lookX, lookY, lookMagnitude,
					     buttonDown[GP_LT],
					     buttonDown[GP_RT], seconds);
		} else {
			if (magnitude > 0.5f && now >= keyboardStickNextStep) {
				keyboardStickNextStep =
					now + KEYBOARD_STICK_REPEAT_MS;
				navigate(fabsf(stickX) > fabsf(stickY)
						 ? (stickX > 0 ? GP_DPAD_RIGHT
							       : GP_DPAD_LEFT)
						 : (stickY > 0 ? GP_DPAD_DOWN
							       : GP_DPAD_UP));
			}
			if (lookMagnitude > 0.5f &&
			    fabsf(lookY) > fabsf(lookX) &&
			    now >= scrollNextStep) {
				scrollNextStep = now + DPAD_REPEAT_EVERY_MS;
				inpGamepadScroll(lookY > 0);
			}
		}

		updateRumble();
		return;
	}

	navStickWasOpen = 0;

	// The binding screen is a menu too, so the character stands still and a
	// stick push steps the list.
	if (consoleBindIsOpen()) {
		moveActive = 0;
		moveSpeed = 0;
		lookYaw = 0;
		lookPitch = 0;

		// An open description takes the right stick for scrolling.
		if (lookMagnitude > 0 &&
		    consoleBindLook(lookY * lookMagnitude, seconds))
			lookMagnitude = 0;

		if ((magnitude > 0.5f || lookMagnitude > 0.5f) &&
		    now >= keyboardStickNextStep) {
			F32 x = magnitude > 0.5f ? stickX : lookX;
			F32 y = magnitude > 0.5f ? stickY : lookY;

			keyboardStickNextStep = now + KEYBOARD_STICK_REPEAT_MS;
			consoleBindSteer(
				fabsf(x) > fabsf(y)
					? (x > 0 ? GP_DPAD_RIGHT : GP_DPAD_LEFT)
					: (y > 0 ? GP_DPAD_DOWN : GP_DPAD_UP));
		}

		updateRumble();
		return;
	}

	// The radial menu has both sticks while it is open, and the character
	// stands still.
	if (consoleMenuIsOpen()) {
		moveActive = 0;
		moveSpeed = 0;
		lookYaw = 0;
		lookPitch = 0;

		if (magnitude > 0)
			consoleMenuSteer(stickX, stickY);
		if (lookMagnitude > 0)
			consoleMenuSteer(lookX, lookY);

		updateRumble();
		return;
	}

	lookYaw = 0;
	lookPitch = 0;

	if (!holdingTargetMode) {
		if (cursorMode) {
			// Lists scroll on this stick while focus is on them.
			// Placing a location power, the front end's sliders and
			// anything else focus cannot reach use it as a real
			// pointer.
			if (freePointer || consoleWorldTargetActive() ||
			    !inGame() || !openMenuCount) {
				if (lookMagnitude > 0) {
					pointerHidden = 0;
					padFocused = 0;
					stickSteered = 1;
				}
				moveCursor(lookX, lookY, lookMagnitude,
					   seconds);
			} else if (lookMagnitude > 0.5f &&
				   fabsf(lookY) > fabsf(lookX) &&
				   now >= scrollNextStep) {
				scrollNextStep = now + DPAD_REPEAT_EVERY_MS;
				inpGamepadScroll(lookY > 0);
			}
		} else if (lookMagnitude > 0 && inGame()) {
			F32 response = powf(lookMagnitude, settings.lookCurve);

			lookActive = 1;
			lookYaw = lookX * response * settings.lookYawSpeed *
				  DEG_TO_RAD / TICKS_PER_SECOND;
			lookPitch = lookY * response * settings.lookPitchSpeed *
				    DEG_TO_RAD / TICKS_PER_SECOND;

			if (settings.invertY)
				lookPitch = -lookPitch;
		}
	}

	updateRumble();
}

void gamepadFocusLost(void)
{
	if (!padReady)
		return;

	gamepadNavBeginFrame();
	readDevice();
	releaseEverything();
	lastUpdateTicks = gamepadTicks();
}

void gamepadShutdown(void)
{
	if (!padReady)
		return;

	releaseEverything();
	padReady = 0;
	padConnected = 0;
}

int gamepadIsConnected(void)
{
	return padConnected && settings.enabled;
}

int gamepadGetMove(F32 *directionX, F32 *directionY, F32 *speed)
{
	if (!moveActive)
		return 0;

	*directionX = moveDirectionX;
	*directionY = moveDirectionY;
	*speed = moveSpeed;
	return 1;
}

int gamepadGetLook(F32 *yaw, F32 *pitch)
{
	if (!lookActive)
		return 0;

	*yaw = lookYaw;
	*pitch = lookPitch;
	return 1;
}

int gamepadLookHeld(void)
{
	return lookActive;
}

F32 gamepadTurnRate(void)
{
	return settings.turnRate * DEG_TO_RAD / TICKS_PER_SECOND;
}

void gamepadToggleCursorMode(void)
{
	if (cursorModeToggled) {
		leaveMenus();
	} else {
		cursorModeToggled = 1;
		freePointer = 1;
	}
}

void gamepadPrintStatus(void)
{
	char text[256];

	if (!settings.enabled)
		snprintf(text, sizeof(text),
			 "Gamepad controls are off. Options, Gamepad turns "
			 "them on");
	else if (!padConnected)
		snprintf(text, sizeof(text), "No gamepad connected");
	else
		snprintf(text, sizeof(text), "Gamepad: %s, menus %s",
			 gamepadDeviceName(),
			 cursorModeToggled ? "open" : "closed");

	tellPlayer(text);
}

void gamepadSetCursorMode(int on)
{
	if (on) {
		cursorModeToggled = 1;
		focusPending = 1;
		menuModeForPopup = 1;
		popupSeen = 0;
	} else {
		leaveMenus();
	}
}

void gamepadDebugNote(const char *text)
{
	trace("%s", text);
}

void gamepadNoteListScroll(float pixels)
{
	if (!scrollFollowFrames)
		return;
	scrollMoved += pixels;
	scrollMovedSeen = 1;
}

int gamepadUsedWithin(int milliseconds)
{
	return padReady && lastPadUse &&
	       gamepadTicks() - lastPadUse <= (U64)milliseconds;
}

int gamepadTakeQuickTrade(void)
{
	int pressed = quickTrade;

	quickTrade = 0;
	return pressed;
}

int gamepadTakeForward(void)
{
	int pressed = frontEndForward;

	frontEndForward = 0;
	return pressed;
}

int gamepadTakeBack(void)
{
	int pressed = frontEndBack;

	frontEndBack = 0;
	return pressed;
}

void gamepadCenterPointer(void)
{
	POINT origin = { 0, 0 };
	int width = 0, height = 0;

	if (!ClientToScreen(hwnd, &origin))
		return;

	windowClientSize(&width, &height);
	lastFocusPosition.x = origin.x + width / 2;
	lastFocusPosition.y = origin.y + height / 2;
	SetCursorPos(lastFocusPosition.x, lastFocusPosition.y);
}

void gamepadQuietOpen(int window)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(menuWindows); i++) {
		if (menuWindows[i] == window)
			menuWindowQuiet[i] = 1;
	}
}

void gamepadRequestFocusBelow(float top)
{
	gamepadRequestFocus();
	focusMinTop = top;
}

void gamepadRequestFocus(void)
{
	focusMinTop = 0;
	focusPending = 1;
	padFocused = 0;
	freePointer = 0;
	stickSteered = 0;
}

void gamepadPointAt(int x, int y)
{
	POINT origin = { 0, 0 };

	if (!hwnd || !ClientToScreen(hwnd, &origin))
		return;

	lastFocusPosition.x = origin.x + x;
	lastFocusPosition.y = origin.y + y;
	SetCursorPos(lastFocusPosition.x, lastFocusPosition.y);
	focusPending = 0;
	pointerHidden = 0;
	padFocused = 0;
	stickSteered = 1;
}

void gamepadFocusOnWindow(int window)
{
	addOpenMenu(window);
	cursorModeToggled = 1;
}

int gamepadNavMoves(void)
{
	return navMoves;
}

// Puts the pad's focus on a point in the client area, for a screen that knows
// what should have it first. Only for a player on the pad who is in the menus;
// a mouse player keeps the pointer where it is.
int gamepadFocusPoint(int x, int y)
{
	POINT origin = { 0, 0 };
	NavPoint target;

	if (!padReady || !lastPadUse ||
	    gamepadTicks() - lastPadUse > RECENT_PAD_USE_MS ||
	    !cursorModeActive() || freePointer ||
	    !ClientToScreen(hwnd, &origin)) {
		return 0;
	}

	target.x = x;
	target.y = y;
	focusPending = 0;
	moveFocusTo(&origin, &target);
	return 1;
}

int gamepadPointerHidden(void)
{
	return pointerHidden && cursorModeActive();
}

int gamepadGetFocusBox(float *left, float *top, float *right, float *bottom)
{
	POINT cursor;
	POINT origin = { 0, 0 };
	int windows[MAX_OPEN_MENUS + 1];
	int count, width = 0, height = 0;
	NavRect box;

	if (!padFocused)
		return 0;
	if (!cursorModeActive())
		return 0;
	if (!GetCursorPos(&cursor) || !ClientToScreen(hwnd, &origin))
		return 0;

	count = focusScope(windows);
	windowClientSize(&width, &height);

	if (!gamepadNavFocused(windows, count, cursor.x - origin.x,
			       cursor.y - origin.y, width, height, &box))
		return 0;

	*left = box.left;
	*top = box.top;
	*right = box.right;
	*bottom = box.bottom;
	return 1;
}

int gamepadOpenWindowCount(void)
{
	return openMenuCount;
}

GamepadButtonStyle gamepadButtonStyle(void)
{
	if (settings.buttonStyle != GP_STYLE_AUTO)
		return (GamepadButtonStyle)settings.buttonStyle;

	switch (gamepadDeviceFamily()) {
	case PAD_FAMILY_PLAYSTATION:
		return GP_STYLE_PLAYSTATION;
	case PAD_FAMILY_NINTENDO:
		return GP_STYLE_NINTENDO;
	default:
		return GP_STYLE_XBOX;
	}
}

const char *gamepadButtonLabel(int button)
{
	// In GamepadButton order: A B X Y Back Start L3 R3 LB RB, the D-pad, LT
	// RT, then the right stick.
	static const char *labels[][GP_BUTTON_COUNT] = {
		[GP_STYLE_XBOX] = { "A",	  "B",		"X",
				    "Y",	  "Back",	"Start",
				    "L3",	  "R3",		"LB",
				    "RB",	  "Up",		"Down",
				    "Left",	  "Right",	"LT",
				    "RT",	  "Stick up",	"Stick down",
				    "Stick left", "Stick right" },
		[GP_STYLE_PLAYSTATION] = { "Cross",	 "Circle",
					   "Square",	 "Triangle",
					   "Share",	 "Options",
					   "L3",	 "R3",
					   "L1",	 "R1",
					   "Up",	 "Down",
					   "Left",	 "Right",
					   "L2",	 "R2",
					   "Stick up",	 "Stick down",
					   "Stick left", "Stick right" },
		// Nintendo prints A on the right and B at the bottom.
		[GP_STYLE_NINTENDO] = { "B",	      "A",
					"Y",	      "X",
					"Minus",      "Plus",
					"LS",	      "RS",
					"L",	      "R",
					"Up",	      "Down",
					"Left",	      "Right",
					"ZL",	      "ZR",
					"Stick up",   "Stick down",
					"Stick left", "Stick right" },
	};

	if (button < 0 || button >= GP_BUTTON_COUNT)
		return "";
	return labels[gamepadButtonStyle()][button];
}

const char *gamepadModifierLabel(int modifier)
{
	switch (modifier) {
	case GP_MOD_LT:
		return gamepadButtonLabel(GP_LT);
	case GP_MOD_RT:
		return gamepadButtonLabel(GP_RT);
	case GP_MOD_LB:
		return gamepadButtonLabel(GP_LB);
	case GP_MOD_RB:
		return gamepadButtonLabel(GP_RB);
	}
	return "";
}

const char *gamepadLabelText(const char *text)
{
	static const struct {
		const char *word;
		int button;
	} words[] = {
		{ "LB", GP_LB },     { "RB", GP_RB },	    { "LT", GP_LT },
		{ "RT", GP_RT },     { "L3", GP_L3 },	    { "R3", GP_R3 },
		{ "Back", GP_BACK }, { "Start", GP_START },
	};
	// Prompts are measured and then drawn, so a few results are kept.
	static char buffers[8][160];
	static int next;
	char *out;
	size_t used = 0;
	const char *p = text;

	if (!text || gamepadButtonStyle() == GP_STYLE_XBOX)
		return text;

	out = buffers[next];
	next = (next + 1) % ARRAY_SIZE(buffers);

	while (*p && used + 1 < sizeof(buffers[0])) {
		const char *replacement = NULL;
		size_t length = 0;
		int i;

		if (p == text || !isalnum((unsigned char)p[-1])) {
			for (i = 0; i < ARRAY_SIZE(words); i++) {
				size_t n = strlen(words[i].word);

				if (!strncmp(p, words[i].word, n) &&
				    !isalnum((unsigned char)p[n])) {
					replacement = gamepadButtonLabel(
						words[i].button);
					length = n;
					break;
				}
			}
		}

		if (replacement) {
			size_t n = strlen(replacement);

			if (used + n + 1 > sizeof(buffers[0]))
				break;
			memcpy(out + used, replacement, n);
			used += n;
			p += length;
		} else {
			out[used++] = *p++;
		}
	}
	out[used] = '\0';
	return out;
}

const char *gamepadKeyTextSuffix(void)
{
	switch (gamepadButtonStyle()) {
	case GP_STYLE_PLAYSTATION:
		return "_PS";
	case GP_STYLE_NINTENDO:
		return "_NS";
	default:
		return "";
	}
}

int gamepadConsoleCompact(void)
{
	return settings.consoleCompact != 0;
}

void gamepadToggleConsoleCompact(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	settings.consoleCompact = settings.consoleCompact ? 0.0f : 1.0f;
	saveSettings();
}

int gamepadConsoleTeam(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	return settings.consoleTeam != 0;
}

void gamepadToggleConsoleTeam(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	settings.consoleTeam = settings.consoleTeam ? 0.0f : 1.0f;
	saveSettings();
}

int gamepadConsoleLeagueTeam(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	return settings.consoleLeagueTeam != 0;
}

void gamepadToggleConsoleLeagueTeam(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	settings.consoleLeagueTeam = settings.consoleLeagueTeam ? 0.0f : 1.0f;
	saveSettings();
}

int gamepadConsoleTeamList(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	return settings.consoleTeamList != 0;
}

void gamepadToggleConsoleTeamList(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	settings.consoleTeamList = settings.consoleTeamList ? 0.0f : 1.0f;
	saveSettings();
}

int gamepadCursorModeToggled(void)
{
	return cursorModeToggled;
}

void gamepadStartChat(void)
{
	char start[] = "startchat";

	runCommand(start);
	gamepadKeyboardOpen();
}

int gamepadConsoleUiEnabled(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	// The gamepad switch in Options covers its interface too, so with it
	// off the game is untouched.
	return settings.enabled != 0 && settings.consoleUi != 0;
}

int gamepadEnabled(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	return settings.enabled != 0;
}

static const GamepadSetting *findSetting(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(settingTable); i++) {
		if (stricmp(settingTable[i].name, name) == 0)
			return &settingTable[i];
	}
	return NULL;
}

F32 gamepadGetSetting(const char *name)
{
	const GamepadSetting *setting;

	if (!configLoaded)
		gamepadReloadConfig();

	setting = findSetting(name);
	return setting ? *setting->value : 0;
}

void gamepadSetSetting(const char *name, F32 value)
{
	const GamepadSetting *setting;

	if (!configLoaded)
		gamepadReloadConfig();

	setting = findSetting(name);
	if (!setting || *setting->value == value)
		return;

	*setting->value = value;
	clampSettings();
	settingsChangedAt = gamepadTicks() ? gamepadTicks() : 1;

	// Turned off from Options: let go of anything held, so nothing stays
	// pressed or moving.
	if (!settings.enabled && padHasFocus) {
		releaseEverything();
		leaveMenus();
		padHasFocus = 0;
	}
}

void gamepadToggleConsoleUi(void)
{
	if (!configLoaded)
		gamepadReloadConfig();

	settings.consoleUi = settings.consoleUi ? 0.0f : 1.0f;
	saveSettings();
}

int gamepadHeldModifiers(void)
{
	return currentModifiers();
}

int gamepadCursorModeActive(void)
{
	return cursorModeActive();
}

const char *gamepadBoundCommand(int modifiers, int button)
{
	return keybind_commandFor(
		buttonKeys[button],
		keybind_padModifier(keybindModifiers(modifiers)));
}

void gamepadSeparateTrays(void)
{
	Entity *player = playerPtr();
	const int *trays;
	const int trayCount = TRAY_SET_COUNT * TRAYS_PER_SET;
	char command[96];

	if (!player || !player->pl || !player->pl->tray)
		return;

	trays = player->pl->tray->current_trays;

	if (trays[kCurTrayType_Alt] != trays[kCurTrayType_Primary] &&
	    trays[kCurTrayType_Alt2] != trays[kCurTrayType_Primary] &&
	    trays[kCurTrayType_Alt] != trays[kCurTrayType_Alt2]) {
		return;
	}

	// The main tray stays where the player left it. goto_tray counts
	// from 1.
	snprintf(command, sizeof(command),
		 "goto_tray_alt %d$$goto_tray_alt2 %d",
		 (trays[kCurTrayType_Primary] + 1) % trayCount + 1,
		 (trays[kCurTrayType_Primary] + 2) % trayCount + 1);
	trace("separating trays: %s", command);
	runCommand(command);
}

void gamepadChangeSet(int direction)
{
	Entity *player = playerPtr();
	int primary, next;
	char command[96];

	if (!player || !player->pl || !player->pl->tray)
		return;

	primary = player->pl->tray->current_trays[kCurTrayType_Primary];
	next = ((primary / TRAYS_PER_SET + direction) % TRAY_SET_COUNT +
		TRAY_SET_COUNT) %
	       TRAY_SET_COUNT;

	// goto_tray counts from 1.
	snprintf(command, sizeof(command),
		 "goto_tray %d$$goto_tray_alt %d$$goto_tray_alt2 %d",
		 next * TRAYS_PER_SET + 1, next * TRAYS_PER_SET + 2,
		 next * TRAYS_PER_SET + 3);
	runCommand(command);

	snprintf(command, sizeof(command), "Power set %d of %d", next + 1,
		 TRAY_SET_COUNT);
	tellPlayer(command);
}

void gamepadToggleTrace(void)
{
	char path[MAX_PATH];
	char text[MAX_PATH + 64];

	traceEnabled = !traceEnabled;
	siblingPath(GAMEPAD_TRACE_NAME, path, sizeof(path));

	if (traceEnabled)
		trace("--- trace started, pad: %s",
		      padConnected ? gamepadDeviceName() : "none");

	snprintf(text, sizeof(text),
		 traceEnabled ? "Gamepad trace on, writing to %s"
			      : "Gamepad trace off, saved in %s",
		 path);
	tellPlayer(text);
}
