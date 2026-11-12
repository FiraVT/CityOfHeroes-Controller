/*
 * The console HUD: radial menu, button prompts, power clusters, on-screen
 * keyboard and the frame that draws every console screen.
 */
#include "UI/uiConsoleHudInternal.h"

void inpGamepadEditKey(int key);
void inpGamepadTypeCharacter(char c);

#define HUD_REFERENCE_HEIGHT 1080.0f
#define MENU_Z 200000.0f

#define CELL_SIZE 42.0f
#define CELL_GAP 2.0f
#define CLUSTER_HALF_SPREAD 92.0f
#define HELD_LAYER_GROWTH 1.15f

#define COLOR_BUTTON_A 0x70e070ff
#define COLOR_BUTTON_B 0xf07070ff
#define COLOR_BUTTON_X 0x70a8ffff
#define COLOR_BUTTON_Y 0xf0d860ff
#define COLOR_CROSS 0x7fb3f5ff
#define COLOR_CIRCLE 0xf2707aff
#define COLOR_SQUARE 0xe591cfff
#define COLOR_TRIANGLE 0x3fd6a8ff
#define COLOR_NINTENDO 0xe0e0e0ff
#define COLOR_FRIEND 0x60e080ff

#define TRAYS_PER_SET 3
#define MENU_PAGE_COUNT 3
#define MENU_PAGE_MAX 8
#define MENU_RADIUS 230.0f
#define MENU_TILE_WIDTH 160.0f
#define MENU_TILE_HEIGHT 50.0f
#define MENU_STEER_MINIMUM 0.35f

#define TWO_PI_F 6.2831853f

// A window of -1 is the game's own menu, which is not in the window list.
// A window of -2 is drawn by this file, and has no focus to hand over.
typedef struct MenuEntry {
	const char *label;
	const char *command;
	int window;
} MenuEntry;

// LB and RB turn the page. The first page is what is wanted in play, the second
// is other people, the third the HUD itself.
static const MenuEntry menuPlay[] = {
	{ "Powers", "console_powers", -2 },
	{ "Enhancements", "console_enhancements", -2 },
	{ "Inspirations", "console_inspirations", -2 },
	{ "Navigation", "console_nav", -2 },
	{ "Invention", "console_invention", -2 },
	{ "Chat", "gamepad_chat", -2 },
	{ "Game Menu", "console_game_menu", -2 },
};

// Search, Team, Friends, League and Supergroup open as tabs of the Social
// screen.
static const MenuEntry menuSocial[] = {
	{ "Team", "console_nav_tab team", -2 },
	{ "Search", "console_nav_tab search", -2 },
	{ "Friends", "console_nav_tab friends", -2 },
	{ "League", "console_nav_tab league", -2 },
	{ "Supergroup", "console_nav_tab supergroup", -2 },
	{ "Email", "show email", WDW_EMAIL },
};

static const MenuEntry menuOptions[] = {
	{ "Edit HUD", "console_hud_edit", -2 },
	{ "Compact HUD", "console_compact", -2 },
	{ "Team Frames", "console_team", -2 },
	{ "Team in League", "console_league_team", -2 },
	{ "Team as List", "console_team_list", -2 },
};

typedef struct MenuPage {
	const char *name;
	const MenuEntry *entries;
	int count;
} MenuPage;

static const MenuPage menuPages[MENU_PAGE_COUNT] = {
	{ "Play", menuPlay, ARRAY_SIZE(menuPlay) },
	{ "Social", menuSocial, ARRAY_SIZE(menuSocial) },
	{ "Options", menuOptions, ARRAY_SIZE(menuOptions) },
};

// Incarnates joins the Play page, after Enhancements, once the character has
// unlocked a slot. Until then the page keeps its seven.
#define MENU_INCARNATES_AT 2
static const MenuEntry menuIncarnates = { "Incarnates", "console_incarnates",
					  -2 };

static int menuShowsIncarnates(int page)
{
	Entity *e = playerPtr();
	int slot;

	if (page != 0 || !e || !e->pchar)
		return 0;
	for (slot = 0; slot < kIncarnateSlot_Count; slot++) {
		if (IncarnateSlot_isActivated(e, slot))
			return 1;
	}
	return 0;
}

static int menuCount(int page)
{
	return menuPages[page].count + menuShowsIncarnates(page);
}

static const MenuEntry *menuEntry(int page, int index)
{
	if (menuShowsIncarnates(page)) {
		if (index == MENU_INCARNATES_AT)
			return &menuIncarnates;
		if (index > MENU_INCARNATES_AT)
			index--;
	}
	return &menuPages[page].entries[index];
}

static Entity *traysCheckedFor;
static Entity *sprintCheckedFor;
int menuOpen;
static int menuPage;
static int menuSelection = -1;

int navOpen;

int hudEditOpen;
int gameMenuOpen;
int inspOpen;

int cursorOverHud;
F32 clusterBaseZ = HUD_Z;
int bindScreenDrawing;

int consoleHudActive(void)
{
	return gamepadConsoleUiEnabled() && isMenu(MENU_GAME) && playerPtr() &&
	       !baseedit_Mode();
}

int consoleHudSkipsWindow(int window)
{
	// The team frames down the left take the Team window's place, and the
	// Social screen's Team and League tabs manage the team. Those tabs draw
	// the windows themselves.
	return consoleHudActive() &&
	       (window == WDW_GROUP || window == WDW_LEAGUE);
}

int consoleHudHidesWindow(int window)
{
	if (!consoleHudActive())
		return 0;

	return window == WDW_TRAY || window == WDW_STAT_BARS ||
	       window == WDW_TRADE;
}

int consoleOverlayActive(void)
{
	return contextMenu_IsActive() ||
	       window_getMode(WDW_DIALOG) == WINDOW_DISPLAYING;
}

void contextMenuForTarget(Entity *e);

// The menu a right click on the target gives, for inviting, trading, adding a
// friend and the rest.
void consoleTargetMenu(void)
{
	if (!current_target) {
		addSystemChatMsg(textStd("NoTargetString"), INFO_USER_ERROR, 0);
		return;
	}

	gamepadCenterPointer();
	contextMenuForTarget(current_target);
	gamepadSetCursorMode(1);
}

static int hasOtherTarget(void)
{
	return current_target && current_target != playerPtr();
}

// Y: with something targeted, its right click menu. With nothing targeted, the
// menu wheel.
void consoleTargetOrMenu(void)
{
	if (hasOtherTarget())
		consoleTargetMenu();
	else
		consoleMenuToggle();
}

int consoleCloseContextMenu(void)
{
	if (combobox_closeOpen())
		return 1;

	if (!contextMenu_IsActive())
		return 0;

	contextMenu_closeAll();
	return 1;
}

int consoleWorldTargetActive(void)
{
	return cursor.target_world != kWorldTarget_None;
}

// Where a location power should start aiming, in client pixels: the target's
// feet when there is a target on screen, otherwise the ground a short way ahead
// of the camera.
int consoleWorldTargetAimPoint(int *x, int *y)
{
	Entity *e = playerPtr();
	Vec3 ground, view, toPoint;
	Vec2 pixel;
	int w, h;

	if (!e)
		return 0;

	if (current_target && current_target != e) {
		copyVec3(ENTPOS(current_target), ground);
	} else {
		// The camera's third axis points back, so ahead is against it,
		// kept level with the ground.
		Vec3 ahead = { -cam_info.cammat[2][0], 0,
			       -cam_info.cammat[2][2] };

		if (lengthVec3(ahead) < 0.01f)
			return 0;
		normalVec3(ahead);
		copyVec3(ENTPOS(e), ground);
		scaleAddVec3(ahead, 20.f, ground, ground);
	}

	subVec3(ground, cam_info.cammat[3], toPoint);
	if (dotVec3(toPoint, cam_info.cammat[2]) > 0)
		return 0;

	mulVecMat4(ground, cam_info.viewmat, view);
	gfxWindowScreenPos(view, pixel);
	windowSize(&w, &h);
	*x = (int)pixel[0];
	*y = h - (int)pixel[1];
	return *x >= 0 && *x < w && *y >= 0 && *y < h;
}

int consoleMenuIsOpen(void)
{
	return menuOpen && consoleHudActive();
}

void consoleMenuClose(void)
{
	menuOpen = 0;
}

void consoleMenuToggle(void)
{
	char standardMenu[] = "menu";

	if (!consoleHudActive()) {
		cmdParse(standardMenu);
		return;
	}

	menuOpen = !menuOpen;
	menuPage = 0;
	menuSelection = -1;
	if (menuOpen && navOpen)
		navClose();
}

void consoleMenuSteer(F32 x, F32 y)
{
	int count = menuCount(menuPage);

	if (x * x + y * y < MENU_STEER_MINIMUM * MENU_STEER_MINIMUM)
		return;

	menuSelection =
		(int)floorf(atan2f(x, -y) / (TWO_PI_F / count) + 0.5f + count) %
		count;
}

// The bumpers as the player holds them: with Swap Target Bumpers on, the game's
// LB is the right one.
int bumperDirection(int button)
{
	int right = button == GP_RB;

	if (gamepadGetSetting("swap_target_sides") != 0)
		right = !right;
	return right ? 1 : -1;
}

void consoleMenuPage(int button)
{
	menuPage = (menuPage + bumperDirection(button) + MENU_PAGE_COUNT) %
		   MENU_PAGE_COUNT;
	menuSelection = -1;
}

void consoleMenuConfirm(void)
{
	char command[64];
	const MenuEntry *entry;

	if (menuSelection < 0 || menuSelection >= menuCount(menuPage))
		return;

	entry = menuEntry(menuPage, menuSelection);
	strncpy(command, entry->command, sizeof(command) - 1);
	command[sizeof(command) - 1] = 0;
	menuOpen = 0;
	cmdParse(command);

	// Whatever just opened has the D-pad, and B closes it and returns to
	// playing.
	if (entry->window >= 0)
		gamepadFocusOnWindow(entry->window);
	else if (entry->window == -1)
		gamepadSetCursorMode(1);
}

// Which tray slot a binding fires, so the HUD follows any layout in the
// keybinds.
int slotForCommand(const char *command, CurTrayType *tray, int *slot)
{
	if (!command)
		return 0;

	if (sscanf(command, "powexec_slot %d", slot) == 1)
		*tray = kCurTrayType_Primary;
	else if (sscanf(command, "powexec_altslot %d", slot) == 1)
		*tray = kCurTrayType_Alt;
	else if (sscanf(command, "powexec_alt2slot %d", slot) == 1)
		*tray = kCurTrayType_Alt2;
	else
		return 0;

	(*slot)--;
	return *slot >= 0 && *slot < TRAY_SLOTS;
}

// True when the binding fires an attack on a foe and nothing it can use is
// targeted. A friend targeted is left alone, because the game passes the attack
// on to that friend's target.
int consoleAttackNeedsTarget(const char *command)
{
	Entity *e = playerPtr();
	CurTrayType tray;
	TrayObj *object;
	PowerSet *set;
	int slot;

	if (!e || !e->pchar || !slotForCommand(command, &tray, &slot))
		return 0;

	object = trayslot_GetCurrent(e, tray, slot);
	if (!object || object->type != kTrayItemType_Power ||
	    object->iset < 0 || object->iset >= eaSize(&e->pchar->ppPowerSets))
		return 0;

	set = e->pchar->ppPowerSets[object->iset];
	if (!set || object->ipow < 0 ||
	    object->ipow >= eaSize(&set->ppPowers) ||
	    !set->ppPowers[object->ipow] ||
	    !set->ppPowers[object->ipow]->ppowBase ||
	    set->ppPowers[object->ipow]->ppowBase->eTargetType !=
		    kTargetType_Foe)
		return 0;

	if (current_target && current_target != e && current_target->pchar &&
	    (character_TargetMatchesType(e->pchar, current_target->pchar,
					 kTargetType_Foe, false) ||
	     character_TargetMatchesType(e->pchar, current_target->pchar,
					 kTargetType_Friend, false)))
		return 0;

	return 1;
}

// Sprint is wanted without holding a trigger, and a new character has it
// nowhere on the bare D-pad. Unless the player has put it on one of those four
// buttons already, it goes on D-pad Up when that is free.
static void bindSprintIfMissing(Entity *e)
{
	static const int dpad[] = { GP_DPAD_UP, GP_DPAD_DOWN, GP_DPAD_LEFT,
				    GP_DPAD_RIGHT };
	CurTrayType tray;
	TrayObj *current;
	int sprintSet = -1, sprintPower = -1;
	int slot, i, j;

	for (i = 0; i < eaSize(&e->pchar->ppPowerSets) && sprintSet < 0; i++) {
		PowerSet *set = e->pchar->ppPowerSets[i];

		for (j = 0; j < eaSize(&set->ppPowers); j++) {
			if (set->ppPowers[j] && set->ppPowers[j]->ppowBase &&
			    stricmp(set->ppPowers[j]->ppowBase->pchName,
				    "Sprint") == 0) {
				sprintSet = i;
				sprintPower = j;
				break;
			}
		}
	}

	if (sprintSet < 0) {
		gamepadDebugNote("sprint: the character has no Sprint");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(dpad); i++) {
		if (slotForCommand(gamepadBoundCommand(0, dpad[i]), &tray,
				   &slot)) {
			TrayObj *object = trayslot_GetCurrent(e, tray, slot);

			if (object && object->type == kTrayItemType_Power &&
			    object->iset == sprintSet &&
			    object->ipow == sprintPower) {
				gamepadDebugNote(
					"sprint: already on the D-pad");
				return;
			}
		}
	}

	if (!slotForCommand(gamepadBoundCommand(0, GP_DPAD_UP), &tray, &slot)) {
		gamepadDebugNote(
			"sprint: D-pad Up is not bound to a tray slot");
		return;
	}

	current = trayslot_GetCurrent(e, tray, slot);
	if (!current || current->type == kTrayItemType_None) {
		TrayObj object;

		buildPowerTrayObj(&object, sprintSet, sprintPower, 0);
		gamepadDebugNote(
			trayslot_Place(e, tray, slot, &object)
				? "sprint: placed on D-pad Up"
				: "sprint: the tray would not take it");
	} else {
		gamepadDebugNote("sprint: D-pad Up already holds something");
	}
}

static const char *labelForCommand(const char *command)
{
	static const struct {
		const char *start;
		const char *label;
	} labels[] = {
		{ "+up", "Jump" },
		{ "interact", "Use" },
		{ "powexec_unqueue", "Back" },
		{ "console_menu", "Menu" },
	};
	int i;

	if (!command)
		return NULL;

	if (!strncmp(command, "target_or_menu", 14))
		return hasOtherTarget() ? "Options" : "Menu";

	for (i = 0; i < ARRAY_SIZE(labels); i++) {
		if (!strncmp(command, labels[i].start, strlen(labels[i].start)))
			return labels[i].label;
	}
	return NULL;
}

static int isFaceButton(int button)
{
	return button == GP_A || button == GP_B || button == GP_X ||
	       button == GP_Y;
}

static const char *buttonLetter(int button)
{
	return isFaceButton(button) ? gamepadButtonLabel(button) : "";
}

static int buttonColor(int button)
{
	static const int colors[][4] = {
		[GP_STYLE_XBOX] = { COLOR_BUTTON_A, COLOR_BUTTON_B,
				    COLOR_BUTTON_X, COLOR_BUTTON_Y },
		[GP_STYLE_PLAYSTATION] = { COLOR_CROSS, COLOR_CIRCLE,
					   COLOR_SQUARE, COLOR_TRIANGLE },
		// A Switch pad's buttons are all one colour.
		[GP_STYLE_NINTENDO] = { COLOR_NINTENDO, COLOR_NINTENDO,
					COLOR_NINTENDO, COLOR_NINTENDO },
	};

	if (!isFaceButton(button))
		return CLR_WHITE;
	return colors[gamepadButtonStyle()][button - GP_A];
}

// A bar @length long and @thick wide centred on @x, @y, turned @angle radians.
static void drawSymbolBar(F32 x, F32 y, F32 z, F32 length, F32 thick, F32 angle,
			  int color)
{
	static AtlasTex *white;

	if (!white)
		white = atlasLoadTexture("white");
	display_rotated_sprite(white, x - length / 2, y - thick / 2, z,
			       length / white->width, thick / white->height,
			       color, angle, 0);
}

// The outline of a regular polygon of @sides around @x, @y, with its first
// corner @start radians round from the right.
static void drawOutline(F32 x, F32 y, F32 z, F32 radius, int sides, F32 start,
			F32 thick, int color)
{
	int i;

	for (i = 0; i < sides; i++) {
		F32 a = start + TWO_PI_F * i / sides;
		F32 b = start + TWO_PI_F * (i + 1) / sides;
		F32 ax = x + radius * cosf(a), ay = y + radius * sinf(a);
		F32 bx = x + radius * cosf(b), by = y + radius * sinf(b);
		F32 length =
			sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));

		drawSymbolBar((ax + bx) / 2, (ay + by) / 2, z, length + thick,
			      thick, atan2f(by - ay, bx - ax), color);
	}
}

// A PlayStation face button's shape, @size across, centred on @x, @y.
static void drawPlayStationSymbol(int button, F32 x, F32 y, F32 z, F32 size,
				  int color)
{
	F32 thick = size * 0.14f > 1.5f ? size * 0.14f : 1.5f;
	F32 radius = size / 2;

	switch (button) {
	case GP_A:
		drawSymbolBar(x, y, z, size * 1.1f, thick, TWO_PI_F / 8, color);
		drawSymbolBar(x, y, z, size * 1.1f, thick, -TWO_PI_F / 8,
			      color);
		break;
	case GP_B:
		drawOutline(x, y, z, radius, 16, 0, thick, color);
		break;
	case GP_X:
		drawOutline(x, y, z, radius * 1.1f, 4, TWO_PI_F / 8, thick,
			    color);
		break;
	case GP_Y:
		// Pointing up, with its middle a little low so it looks
		// centred.
		drawOutline(x, y + radius * 0.15f, z, radius * 1.15f, 3,
			    -TWO_PI_F / 4, thick, color);
		break;
	}
}

// A face button's mark: its letter, or its shape on a PlayStation pad.
static void drawFaceMark(TTDrawContext *font, int button, F32 x, F32 y, F32 z,
			 F32 textScale, F32 symbolSize, int color)
{
	if (gamepadButtonStyle() == GP_STYLE_PLAYSTATION)
		drawPlayStationSymbol(button, x, y, z, symbolSize, color);
	else
		drawText(font, x, y, z, textScale, color, CENTER_X | CENTER_Y,
			 buttonLetter(button));
}

void drawText(TTDrawContext *context, F32 x, F32 y, F32 z, F32 scale, int color,
	      int flags, const char *text)
{
	font(context);
	font_color(color, color);
	cprntEx(x, y, z, scale, scale, flags | NO_MSPRINT, "%s", text);
}

static void drawDpadArrow(int button, F32 x, F32 y, F32 z, F32 size, int color)
{
	const char *names[] = { "Jelly_arrow_up.tga", "Jelly_arrow_down.tga",
				"tray_arrow_L.tga", "tray_arrow_R.tga" };
	AtlasTex *arrow = atlasLoadTexture(names[button - GP_DPAD_UP]);
	F32 scale =
		size * 0.52f /
		(arrow->width > arrow->height ? arrow->width : arrow->height);

	display_sprite(arrow, x - arrow->width * scale / 2,
		       y - arrow->height * scale / 2, z, scale, scale, color);
}

// The front end and the full screen menus each leave sprite scaling in their
// own state, so pad overlays are drawn in window pixels with scaling off, and
// whatever was set is put back afterwards.
typedef struct PixelSpace {
	SpriteScalingMode mode;
	ScreenScaleOption option;
	int disabled;
} PixelSpace;

static void beginPixelSpace(PixelSpace *saved)
{
	saved->mode = getSpriteScaleMode();
	saved->option = getScreenScaleOption();
	saved->disabled = is_scrn_scaling_disabled();
	setScalingModeAndOption(SPRITE_XY_NO_SCALE, SSO_SCALE_NONE);
	set_scrn_scaling_disabled(1);
}

static void endPixelSpace(const PixelSpace *saved)
{
	set_scrn_scaling_disabled(saved->disabled);
	setScalingModeAndOption(saved->mode, saved->option);
}

// The size of what is being drawn on, in window pixels. The login screen draws
// in a 4:3 area in the middle of a wide window, and its pixels start at that
// area's corner, not the window's.
static void drawAreaSize(int *width, int *height)
{
	F32 xsc, ysc, xOffset, yOffset;

	windowClientSize(width, height);
	if (isLetterBoxMenu()) {
		calc_scrn_scaling_with_offset(&xsc, &ysc, &xOffset, &yOffset);
		*width = (int)(DEFAULT_SCRN_WD * xsc);
		*height = (int)(DEFAULT_SCRN_HT * ysc);
	}
}

void consoleFocusDraw(void)
{
	float left, top, right, bottom;
	PixelSpace saved;

	if (consoleBindIsOpen() ||
	    !gamepadGetFocusBox(&left, &top, &right, &bottom))
		return;

	// A tiny target, like a map marker, gets a box big enough to see,
	// centred on it.
	{
		float minimum = 30.0f;
		float middle;

		if (right - left < minimum) {
			middle = (left + right) / 2;
			left = middle - minimum / 2;
			right = middle + minimum / 2;
		}
		if (bottom - top < minimum) {
			middle = (top + bottom) / 2;
			top = middle - minimum / 2;
			bottom = middle + minimum / 2;
		}
	}

	beginPixelSpace(&saved);
	drawFlatFrame(PIX3, 6, left - 3, top - 3, MENU_Z - 4, right - left + 6,
		      bottom - top + 6, 1.0f, FRAME_COLOR_HELD, 0xffe08030);
	endPixelSpace(&saved);
}

static void drawButtonDisc(int button, const char *letter, F32 x, F32 y, F32 z,
			   F32 size, int color)
{
	static AtlasTex *ring;
	F32 scale;

	if (!ring)
		ring = atlasLoadTexture("tray_ring_power.tga");

	scale = size / ring->width;
	display_sprite(ring, x - size / 2, y - size / 2, z, scale, scale,
		       color);
	drawFaceMark(&game_12, button, x, y, z + 1, size / 26.0f, size * 0.42f,
		     color);
	(void)letter;
}

static void drawCell(Entity *e, int modifiers, int button, F32 x, F32 y, F32 z,
		     F32 size, F32 hudScale, int frameColor, int backColor)
{
	const char *command = gamepadBoundCommand(modifiers, button);
	const char *label = labelForCommand(command);
	CurTrayType tray = kCurTrayType_Primary;
	int slot = 0;
	int isSlot = slotForCommand(command, &tray, &slot);
	TrayObj *object = NULL;
	CBox box;

	drawFlatFrame(PIX2, 8, x - size / 2, y - size / 2, z, size, size,
		      hudScale, frameColor, backColor);

	BuildCBox(&box, x - size / 2, y - size / 2, size, size);
	if (mouseCollision(&box))
		cursorOverHud = 1;

	// The tray's own slot code, so this cell clicks, drags, takes drops and
	// opens a menu like a tray slot.
	if (isSlot) {
		trayslot_DisplayBare(e, tray, slot, x, y, z + 2, size,
				     CLR_WHITE, !bindScreenDrawing);
		object = trayslot_GetCurrent(e, tray, slot);
		if (object && object->type == kTrayItemType_None)
			object = NULL;
	} else if (command && command[0] != '+' && !bindScreenDrawing &&
		   mouseClickHit(&box, MS_LEFT)) {
		char buffer[128];

		strncpy(buffer, command, sizeof(buffer) - 1);
		buffer[sizeof(buffer) - 1] = 0;
		cmdParse(buffer);
	}

	if (object) {
		if (isFaceButton(button))
			drawFaceMark(
				&game_9, button, x + size / 2 - 5 * hudScale,
				y + size / 2 - 7 * hudScale, z + 8, hudScale,
				8 * hudScale, buttonColor(button));
	} else if (isFaceButton(button)) {
		drawButtonDisc(button, buttonLetter(button), x, y, z + 2,
			       size * 0.72f, buttonColor(button));
	} else {
		drawDpadArrow(button, x, y, z + 2, size,
			      command ? 0xffffffc0 : 0xffffff50);
	}

	if (!object && label)
		drawText(&game_9, x, y + size / 2 + 9 * hudScale, z + 8,
			 hudScale, CLR_WHITE, CENTER_X, label);
}

// One cluster: the D-pad on the left and the face buttons on the right, as they
// sit on the pad.
void drawCluster(Entity *e, int modifiers, const char *tag, F32 centerX,
		 F32 centerY, F32 hudScale, int heldLayer)
{
	static const struct {
		int button;
		int column;
		int row;
	} cells[] = {
        { GP_DPAD_UP, 0, -1 }, { GP_DPAD_DOWN, 0, 1 }, { GP_DPAD_LEFT, -1, 0 }, { GP_DPAD_RIGHT, 1, 0 },
    }, faces[] = {
        { GP_Y, 0, -1 }, { GP_A, 0, 1 }, { GP_X, -1, 0 }, { GP_B, 1, 0 },
    };
	int isHeld = heldLayer == modifiers;
	int isDim = heldLayer != 0 && !isHeld;
	F32 growth = isHeld && modifiers ? HELD_LAYER_GROWTH : 1.0f;
	F32 size = CELL_SIZE * hudScale * growth;
	F32 step = size + CELL_GAP * hudScale;
	F32 spread = CLUSTER_HALF_SPREAD * hudScale * growth;
	int frameColor = isHeld && modifiers ? FRAME_COLOR_HELD
			 : isDim	     ? FRAME_COLOR_DIM
					     : FRAME_COLOR;
	int backColor = isHeld && modifiers ? BACK_COLOR_HELD : BACK_COLOR;
	F32 z = clusterBaseZ + (isHeld ? 20.0f : 0.0f);
	int i;

	for (i = 0; i < ARRAY_SIZE(cells); i++) {
		drawCell(e, modifiers, cells[i].button,
			 centerX - spread + cells[i].column * step,
			 centerY + cells[i].row * step, z, size, hudScale,
			 frameColor, backColor);
		drawCell(e, modifiers, faces[i].button,
			 centerX + spread + faces[i].column * step,
			 centerY + faces[i].row * step, z, size, hudScale,
			 frameColor, backColor);
	}

	if (tag)
		drawText(&game_12, centerX, centerY, z + 8, hudScale,
			 isHeld	 ? FRAME_COLOR_HELD
			 : isDim ? 0xffffff60
				 : CLR_WHITE,
			 CENTER_X | CENTER_Y, gamepadLabelText(tag));
}

static void drawUnitFrame(Entity *e, F32 centerX, F32 y, F32 hudScale)
{
	F32 width = 300 * hudScale;
	F32 x = centerX - width / 2;
	char level[16];

	snprintf(level, sizeof(level), "%d",
		 character_CalcExperienceLevel(e->pchar) + 1);
	drawText(&game_14, x - 16 * hudScale, y + 21 * hudScale, HUD_Z + 8,
		 hudScale, CLR_WHITE, CENTER_X, level);

	status_drawConsoleFrame(x, y, HUD_Z, width, hudScale, FRAME_COLOR,
				BACK_COLOR);
}

static void drawTargetIndicator(const char *name, F32 x, F32 y, F32 hudScale,
				int color, int hasTarget)
{
	F32 size = 34 * hudScale;

	drawFlatFrame(PIX2, 8, x - size / 2, y - size / 2, HUD_Z, size, size,
		      hudScale, hasTarget ? color : (color & 0xffffff00) | 0x70,
		      BACK_COLOR);
	drawText(&game_12, x, y, HUD_Z + 4, hudScale,
		 hasTarget ? CLR_WHITE : color, CENTER_X | CENTER_Y, name);
}

// Returns the width drawn.
F32 drawPrompts(const Prompt *prompts, int count, F32 centerX, F32 y, F32 z,
		F32 hudScale)
{
	F32 discSize = 24 * hudScale;
	F32 itemWidths[8];
	const char *texts[8];
	F32 total = 0;
	F32 x;
	int i;

	// A hint without a button disc names its buttons in its text.
	for (i = 0; i < count && i < ARRAY_SIZE(texts); i++) {
		texts[i] = prompts[i].button >= 0
				   ? prompts[i].text
				   : gamepadLabelText(prompts[i].text);
		itemWidths[i] =
			(prompts[i].button >= 0 ? discSize + 4 * hudScale : 0) +
			str_wd_notranslate(&game_12, hudScale, hudScale,
					   texts[i]) +
			18 * hudScale;
		total += itemWidths[i];
	}

	x = centerX - total / 2;

	for (i = 0; i < count; i++) {
		F32 textX = x;

		if (prompts[i].button >= 0) {
			drawButtonDisc(prompts[i].button,
				       buttonLetter(prompts[i].button),
				       x + discSize / 2, y, z, discSize,
				       buttonColor(prompts[i].button));
			textX += discSize + 4 * hudScale;
		}

		drawText(&game_12, textX, y, z + 1, hudScale, CLR_WHITE,
			 CENTER_Y, texts[i]);
		x += itemWidths[i];
	}
	return total;
}

static int gatherPrompts(Prompt *prompts)
{
	int count = 0;
	const char *verb;

	if (gamepadKeyboardIsOpen())
		return 0;

	if (uiFocusIsEditable()) {
		prompts[0].button = GP_Y;
		prompts[0].text = "Keyboard";
		prompts[1].button = GP_B;
		prompts[1].text = "Close";
		return 2;
	}

	if (window_getMode(WDW_CONTACT_DIALOG) == WINDOW_DISPLAYING) {
		Prompt dialogue[] = { { GP_A, "Select" }, { GP_B, "Leave" } };
		memcpy(prompts, dialogue, sizeof(dialogue));
		return ARRAY_SIZE(dialogue);
	}

	if (gamepadCursorModeActive()) {
		Prompt cursorMode[] = { { GP_A, "Select" },
					{ GP_X, "Options" },
					{ GP_B, "Back" },
					{ -1, "LB / RB Windows" } };

		if (consoleWorldTargetActive()) {
			Prompt placing[] = { { GP_A, "Place" },
					     { GP_B, "Cancel" } };
			memcpy(prompts, placing, sizeof(placing));
			return ARRAY_SIZE(placing);
		}

		if (window_getMode(WDW_STORE) == WINDOW_DISPLAYING) {
			Prompt store[] = { { GP_A, "Select" },
					   { GP_Y, "Sell / Buy" },
					   { GP_X, "Options" },
					   { GP_B, "Back" } };
			memcpy(prompts, store, sizeof(store));
			return ARRAY_SIZE(store);
		}

		memcpy(prompts, cursorMode, sizeof(cursorMode));
		return gamepadOpenWindowCount() > 1
			       ? ARRAY_SIZE(cursorMode)
			       : ARRAY_SIZE(cursorMode) - 1;
	}

	// Holding both bumpers shows what they reach.
	if ((gamepadHeldModifiers() & (GP_MOD_LB | GP_MOD_RB)) ==
	    (GP_MOD_LB | GP_MOD_RB)) {
		Prompt shortcuts[] = {
			{ GP_X, "Powers" },	   { GP_Y, "Inspirations" },
			{ GP_B, "Inventory" },	   { GP_A, "Enhancements" },
			{ -1, "Up Missions" },	   { -1, "Down Chat" },
			{ -1, "Left / Right Set" }
		};
		memcpy(prompts, shortcuts, sizeof(shortcuts));
		return ARRAY_SIZE(shortcuts);
	}

	verb = interactPromptVerb();
	if (verb) {
		prompts[count].button = GP_X;
		prompts[count].text = verb;
		count++;
	}

	// Y names what it opens: the target's menu, or the menu wheel. Start is
	// always the wheel.
	{
		const char *y = gamepadBoundCommand(0, GP_Y);

		prompts[count].button = GP_Y;
		prompts[count].text = y && !strncmp(y, "target_or_menu", 14) &&
						      hasOtherTarget()
					      ? "Target Options"
					      : "Menu";
		count++;
	}

	prompts[count].button = -1;
	prompts[count].text = "LB + RB Shortcuts";
	count++;

	return count;
}

static void drawMenu(F32 centerX, F32 centerY, int screenWidth,
		     int screenHeight, F32 hudScale)
{
	static const Prompt menuPrompts[] = { { GP_A, "Open" },
					      { GP_B, "Close" } };
	F32 tileWidth = MENU_TILE_WIDTH * hudScale;
	F32 tileHeight = MENU_TILE_HEIGHT * hudScale;
	F32 radius = MENU_RADIUS * hudScale;
	int count = menuCount(menuPage);
	F32 pageY = centerY + radius + 62 * hudScale;
	F32 pageX = centerX - (MENU_PAGE_COUNT - 1) * 60 * hudScale;
	// A pointer left resting on a tile must not hold the choice against the
	// stick, so the mouse picks only once it moves.
	static int lastMouseX, lastMouseY;
	int mouseMoved =
		gMouseInpCur.x != lastMouseX || gMouseInpCur.y != lastMouseY;
	int i;

	lastMouseX = gMouseInpCur.x;
	lastMouseY = gMouseInpCur.y;

	display_sprite(white_tex_atlas, 0, 0, MENU_Z - 2,
		       (F32)screenWidth / white_tex_atlas->width,
		       (F32)screenHeight / white_tex_atlas->height, 0x00000090);

	for (i = 0; i < count; i++) {
		F32 angle = i * TWO_PI_F / count;
		F32 x = centerX + sinf(angle) * radius * 1.25f;
		F32 y = centerY - cosf(angle) * radius;
		CBox box;

		BuildCBox(&box, x - tileWidth / 2, y - tileHeight / 2,
			  tileWidth, tileHeight);

		if (mouseCollision(&box)) {
			if (mouseMoved)
				menuSelection = i;
			if (mouseClickHit(&box, MS_LEFT)) {
				menuSelection = i;
				consoleMenuConfirm();
			}
		}

		drawFlatFrame(PIX3, R10, x - tileWidth / 2, y - tileHeight / 2,
			      MENU_Z, tileWidth, tileHeight, hudScale,
			      i == menuSelection ? FRAME_COLOR_HELD
						 : FRAME_COLOR,
			      i == menuSelection ? 0x405878f0 : 0x101820e0);
		drawText(&game_14, x, y, MENU_Z + 4, hudScale,
			 i == menuSelection ? FRAME_COLOR_HELD : CLR_WHITE,
			 CENTER_X | CENTER_Y, menuEntry(menuPage, i)->label);
	}

	drawText(&game_18, centerX, centerY, MENU_Z + 4, hudScale, CLR_WHITE,
		 CENTER_X | CENTER_Y,
		 menuSelection >= 0 && menuSelection < count
			 ? menuEntry(menuPage, menuSelection)->label
			 : "Menu");

	// The pages, with the bumpers either side. A click on a page name turns
	// to it.
	drawText(&game_12, pageX - 80 * hudScale, pageY, MENU_Z + 4, hudScale,
		 0xffffffc0, CENTER_X | CENTER_Y, gamepadButtonLabel(GP_LB));
	for (i = 0; i < MENU_PAGE_COUNT; i++) {
		F32 x = pageX + i * 120 * hudScale;
		CBox box;

		BuildCBox(&box, x - 50 * hudScale, pageY - 15 * hudScale,
			  100 * hudScale, 30 * hudScale);
		if (mouseClickHit(&box, MS_LEFT)) {
			menuPage = i;
			menuSelection = -1;
		}

		drawFlatFrame(PIX2, 8, x - 50 * hudScale, pageY - 15 * hudScale,
			      MENU_Z, 100 * hudScale, 30 * hudScale, hudScale,
			      i == menuPage ? FRAME_COLOR_HELD
					    : FRAME_COLOR_DIM,
			      i == menuPage ? 0x405878f0 : 0x101820e0);
		drawText(&game_12, x, pageY, MENU_Z + 4, hudScale,
			 i == menuPage ? FRAME_COLOR_HELD : 0xffffffa0,
			 CENTER_X | CENTER_Y, menuPages[i].name);
	}
	drawText(&game_12,
		 pageX + (MENU_PAGE_COUNT - 1) * 120 * hudScale + 80 * hudScale,
		 pageY, MENU_Z + 4, hudScale, 0xffffffc0, CENTER_X | CENTER_Y,
		 gamepadButtonLabel(GP_RB));

	drawPrompts(menuPrompts, ARRAY_SIZE(menuPrompts), centerX,
		    pageY + 46 * hudScale, MENU_Z + 4, hudScale);
}

// Eleven character keys a row, then a column of action keys, so Enter is a key
// like any other.
#define KEYBOARD_CHARACTERS 11
#define KEYBOARD_COLUMNS (KEYBOARD_CHARACTERS + 1)
#define KEYBOARD_ROWS 4
#define KEYBOARD_Z 210000.0f
#define KEY_SIZE 46.0f
#define KEY_GAP 4.0f

static const char *const keyboardPages[3][KEYBOARD_ROWS] = {
	{ "1234567890-", "qwertyuiop'", "asdfghjkl;:", "zxcvbnm,.?/" },
	{ "1234567890_", "QWERTYUIOP\"", "ASDFGHJKL;:", "ZXCVBNM,.?/" },
	{ "!@#$%^&*()=", "+[]{}\\|<>~`", "$$-_'\";:,./", "?!&*()%#@+=" },
};

static const char *const keyboardActions[KEYBOARD_ROWS] = { "Del", "Space",
							    "Enter", "Close" };

static int keyboardOpen;
static int keyboardPage;
static int keyboardRow = 1;
static int keyboardColumn;

int gamepadKeyboardIsOpen(void)
{
	return keyboardOpen;
}

void gamepadKeyboardOpen(void)
{
	keyboardOpen = 1;
}

void gamepadKeyboardClose(void)
{
	keyboardOpen = 0;
}

void gamepadKeyboardPress(int button)
{
	switch (button) {
	case GP_DPAD_UP:
		keyboardRow = (keyboardRow + KEYBOARD_ROWS - 1) % KEYBOARD_ROWS;
		break;
	case GP_DPAD_DOWN:
		keyboardRow = (keyboardRow + 1) % KEYBOARD_ROWS;
		break;
	case GP_DPAD_LEFT:
		keyboardColumn = (keyboardColumn + KEYBOARD_COLUMNS - 1) %
				 KEYBOARD_COLUMNS;
		break;
	case GP_DPAD_RIGHT:
		keyboardColumn = (keyboardColumn + 1) % KEYBOARD_COLUMNS;
		break;
	case GP_A:
		if (keyboardColumn < KEYBOARD_CHARACTERS)
			inpGamepadTypeCharacter(
				keyboardPages[keyboardPage][keyboardRow]
					     [keyboardColumn]);
		else if (keyboardRow == 0)
			inpGamepadEditKey(INP_BACK);
		else if (keyboardRow == 1)
			inpGamepadTypeCharacter(' ');
		else if (keyboardRow == 2) {
			inpGamepadEditKey(INP_RETURN);
			keyboardOpen = 0;
		} else
			keyboardOpen = 0;
		break;
	case GP_X:
		inpGamepadEditKey(INP_BACK);
		break;
	case GP_Y:
		inpGamepadTypeCharacter(' ');
		break;
	case GP_LT:
		keyboardPage = keyboardPage == 1 ? 0 : 1;
		break;
	case GP_RT:
		keyboardPage = keyboardPage == 2 ? 0 : 2;
		break;
	case GP_START:
		inpGamepadEditKey(INP_RETURN);
		keyboardOpen = 0;
		break;
	case GP_B:
		keyboardOpen = 0;
		break;
	}
}

void gamepadKeyboardDraw(void)
{
	static const Prompt keyboardPrompts[] = { { GP_A, "Type" },
						  { GP_X, "Delete" },
						  { GP_Y, "Space" },
						  { GP_B, "Close" } };
	int width = 0, height = 0;
	F32 hudScale, key, step, left, top;
	int row, column;
	PixelSpace saved;

	if (!keyboardOpen)
		return;

	drawAreaSize(&width, &height);
	if (width <= 0 || height <= 0)
		return;

	// Placed by window pixels, so it lands in the same spot on the front
	// end and the full screen menus.
	beginPixelSpace(&saved);

	hudScale = height / HUD_REFERENCE_HEIGHT;
	key = KEY_SIZE * hudScale;
	step = (KEY_SIZE + KEY_GAP) * hudScale;
	left = (width - step * KEYBOARD_COLUMNS) / 2;
	top = height * 0.5f;

	// The login boxes are in the middle of the screen, so there it sits at
	// the bottom, under them.
	if (isLetterBoxMenu())
		top = height - step * KEYBOARD_ROWS - 82 * hudScale;

	drawFlatFrame(PIX3, R10, left - 14 * hudScale, top - 14 * hudScale,
		      KEYBOARD_Z - 2, step * KEYBOARD_COLUMNS + 28 * hudScale,
		      step * KEYBOARD_ROWS + 76 * hudScale, hudScale,
		      FRAME_COLOR, 0x101820f0);

	for (row = 0; row < KEYBOARD_ROWS; row++) {
		for (column = 0; column < KEYBOARD_COLUMNS; column++) {
			int selected =
				row == keyboardRow && column == keyboardColumn;
			F32 x = left + column * step;
			F32 y = top + row * step;
			int action = column >= KEYBOARD_CHARACTERS;
			char label[2];

			label[0] = action ? 0
					  : keyboardPages[keyboardPage][row]
							 [column];
			label[1] = 0;

			drawFlatFrame(
				PIX2, 8, x, y, KEYBOARD_Z, key, key, hudScale,
				selected ? FRAME_COLOR_HELD : FRAME_COLOR,
				selected ? 0x405878f0
					 : (action ? 0x2a3a50f0 : 0x202c38f0));
			drawText(action ? &game_9 : &game_14, x + key / 2,
				 y + key / 2, KEYBOARD_Z + 2, hudScale,
				 selected ? FRAME_COLOR_HELD : CLR_WHITE,
				 CENTER_X | CENTER_Y,
				 action ? keyboardActions[row] : label);
		}
	}

	drawPrompts(keyboardPrompts, ARRAY_SIZE(keyboardPrompts),
		    width * 0.5f - 110 * hudScale,
		    top + step * KEYBOARD_ROWS + 14 * hudScale, KEYBOARD_Z + 2,
		    hudScale);
	drawTextRight(&game_12,
		      left + step * KEYBOARD_COLUMNS - KEY_GAP * hudScale,
		      top + step * KEYBOARD_ROWS + 14 * hudScale,
		      KEYBOARD_Z + 2, hudScale, CLR_WHITE,
		      gamepadLabelText(
			      keyboardPage == 2
				      ? "LT Letters  RT Letters  Start Enter"
			      : keyboardPage == 1
				      ? "LT Lower  RT Symbols  Start Enter"
				      : "LT Upper  RT Symbols  Start Enter"));

	endPixelSpace(&saved);
}

BindPower bindPowers[BIND_MAX_POWERS];
int bindPowerCount;
int bindOpen;
int bindPlacing;
int bindSelection;
int bindTopRow;
int bindCollisionsBefore;
int bindMouseX, bindMouseY;
// Y on a power shows what it does beside the list. The right stick scrolls a
// long description.
int bindInfoOpen;
F32 bindInfoScroll;
int bindInfoLines;
int bindInfoVisibleLines;

// What the powers window offers that can go on a button. Auto powers are always
// on and have no button.
void gatherBindPowers(Entity *e)
{
	int i, j;

	bindPowerCount = 0;

	for (i = 0; i < eaSize(&e->pchar->ppPowerSets); i++) {
		PowerSet *set = e->pchar->ppPowerSets[i];

		if (!powerset_ShowInInventory(set))
			continue;

		for (j = 0; j < eaSize(&set->ppPowers) &&
			    bindPowerCount < BIND_MAX_POWERS;
		     j++) {
			Power *power = set->ppPowers[j];

			if (power && power_ShowInInventory(e->pchar, power) &&
			    power->ppowBase->eType != kPowerType_Auto) {
				bindPowers[bindPowerCount].set = i;
				bindPowers[bindPowerCount].power = j;
				bindPowerCount++;
			}
		}
	}

	if (bindSelection >= bindPowerCount)
		bindSelection = bindPowerCount > 0 ? bindPowerCount - 1 : 0;
}
void consoleHudDraw(void)
{
	Entity *e;
	int width = 0, height = 0;
	F32 hudScale, screenScale, centerX, topRow, sideRow, bottomRow;
	int held;
	Prompt prompts[8];
	int promptCount;
	char setLabel[16];
	HudFrame place;
	int editing;
	// A power being dragged needs every layer to land on.
	int compact = gamepadConsoleCompact() && !cursor.dragging;
	int collisionsBefore = collisions_off_for_rest_of_frame;

	invTick();

	if (!consoleHudActive()) {
		menuOpen = 0;
		bindOpen = 0;
		slotOpen = 0;
		incOpen = 0;
		invOpen = 0;
		hudEditDiscard();
		if (navOpen)
			navClose();
		return;
	}

	e = playerPtr();
	if (!e->pchar || !e->pl)
		return;

	// Each layer needs its own tray, and a new character has all three on
	// the same one.
	if (e != traysCheckedFor) {
		traysCheckedFor = e;
		gamepadSeparateTrays();
	}
	// Once the trays have been pulled apart, so Sprint lands on the tray
	// its button fires.
	else if (e != sprintCheckedFor &&
		 e->pl->tray->current_trays[kCurTrayType_Alt] !=
			 e->pl->tray->current_trays[kCurTrayType_Primary]) {
		sprintCheckedFor = e;
		bindSprintIfMissing(e);
	}

	windowClientSize(&width, &height);
	if (width <= 0 || height <= 0)
		return;

	hudScale = height / HUD_REFERENCE_HEIGHT;

	// A trade opens when the other player says yes, over whatever was up.
	if (tradeScreenShown()) {
		F32 tradeScale = (width - 32.0f) / SCREEN_PANEL_WIDTH < hudScale
					 ? (width - 32.0f) / SCREEN_PANEL_WIDTH
					 : hudScale;

		if (!tradeWasUp) {
			tradeWasUp = 1;
			tradeEditing = 0;
			tradeConfirming = 0;
			tradeTab = TRADE_TAB_INSP;
			tradeSelection = 0;
			tradeTopRow = 0;
			menuOpen = 0;
			gameMenuOpen = 0;
			inspOpen = 0;
			if (navOpen)
				navClose();
		}
		drawTradeScreen(e, width, height, tradeScale);
		return;
	}
	tradeWasUp = 0;

	if (navOpen) {
		// A window a tab opened takes the pad, and once it closes the
		// pad goes back into the tab.
		static int sideWindowWasUp;
		int sideWindowUp = navSideWindowUp();

		if (sideWindowUp && !sideWindowWasUp)
			gamepadFocusOnWindow(WDW_CONTACT_FINDER);
		else if (!sideWindowUp && sideWindowWasUp)
			navFocusFrames = 2;
		sideWindowWasUp = sideWindowUp;

		if (navCovers())
			drawNavScreen(width, height, hudScale);
		return;
	}

	// The Powers, Enhancements, Incarnates and Invention screens are 1420
	// wide at 1080 high. A window narrower than that for its height, such
	// as 5:4, draws them smaller so they stay on screen.
	screenScale = (width - 32.0f) / SCREEN_PANEL_WIDTH < hudScale
			      ? (width - 32.0f) / SCREEN_PANEL_WIDTH
			      : hudScale;

	if (bindOpen) {
		drawBindScreen(e, width, height, screenScale);
		return;
	}

	if (slotScreenUp()) {
		drawSlotScreen(e, width, height, screenScale);
		return;
	}

	if (incScreenUp()) {
		drawIncScreen(e, width, height, screenScale);
		return;
	}

	if (invScreenUp()) {
		drawInvScreen(e, width, height, screenScale);
		return;
	}

	if (gameMenuUp()) {
		drawGameMenu(width, height, hudScale);
		return;
	}

	if (inspScreenUp()) {
		drawInspScreen(e, width, height, hudScale);
		return;
	}

	centerX = width * 0.5f;
	topRow = height - 240 * hudScale;
	sideRow = height - 160 * hudScale;
	bottomRow = height - 86 * hudScale;
	held = gamepadHeldModifiers() & (GP_MOD_LT | GP_MOD_RT);
	editing = hudEditUp();

	if (compact)
		topRow = bottomRow;

	hudLoadLayout();
	memset(hudShown, 0, sizeof(hudShown));

	// While the layout is edited the pieces are only pictures: the edit
	// mode has the mouse, and the windows behind were shut off from it as
	// for any console screen.
	if (editing) {
		collisionsBefore = bindCollisionsBefore;
		collisions_off_for_rest_of_frame = 1;
	}

	place = hudPlace(HUD_PIECE_CHARACTER, centerX, topRow - 112 * hudScale,
			 hudScale, width, height);
	if (!hudHidden(HUD_PIECE_CHARACTER))
		drawUnitFrame(e, place.x, place.y, place.scale);

	// The radial menu is on top of these, and gets the mouse first. The
	// D-pad treats them as the tray, so a power being dragged with held A
	// can reach them.
	cursorOverHud = 0;
	if (menuOpen)
		collisions_off_for_rest_of_frame = 1;
	gamepadNavSetWindow(WDW_TRAY);

	if (compact) {
		// One cluster, showing only the layer in the player's hands.
		place = hudPlace(HUD_PIECE_POWERS, centerX, bottomRow, hudScale,
				 width, height);
		if (!hudHidden(HUD_PIECE_POWERS))
			drawCluster(
				e, held,
				held == GP_MOD_LT   ? gamepadButtonLabel(GP_LT)
				: held == GP_MOD_RT ? gamepadButtonLabel(GP_RT)
				: held		    ? "LT + RT"
						    : NULL,
				place.x, place.y, place.scale, held);
	} else {
		place = hudPlace(HUD_PIECE_POWERS, centerX, topRow, hudScale,
				 width, height);
		if (!hudHidden(HUD_PIECE_POWERS))
			drawCluster(e, 0, NULL, place.x, place.y, place.scale,
				    held);
		place = hudPlace(HUD_PIECE_LT, centerX - 345 * hudScale,
				 sideRow, hudScale, width, height);
		if (!hudHidden(HUD_PIECE_LT))
			drawCluster(e, GP_MOD_LT, gamepadButtonLabel(GP_LT),
				    place.x, place.y, place.scale, held);
		place = hudPlace(HUD_PIECE_RT, centerX + 345 * hudScale,
				 sideRow, hudScale, width, height);
		if (!hudHidden(HUD_PIECE_RT))
			drawCluster(e, GP_MOD_RT, gamepadButtonLabel(GP_RT),
				    place.x, place.y, place.scale, held);
		place = hudPlace(HUD_PIECE_BOTH, centerX, bottomRow, hudScale,
				 width, height);
		if (!hudHidden(HUD_PIECE_BOTH)) {
			drawCluster(e, GP_MOD_LT | GP_MOD_RT, "LT + RT",
				    place.x, place.y, place.scale, held);

			// The tray set goes with the LT + RT cluster, beside
			// it.
			snprintf(
				setLabel, sizeof(setLabel), "Set %d",
				e->pl->tray->current_trays
							[kCurTrayType_Primary] /
						TRAYS_PER_SET +
					1);
			drawText(&game_12, place.x + 205 * place.scale,
				 place.y + 30 * place.scale, HUD_Z + 4,
				 place.scale, 0xffffffc0, CENTER_X | CENTER_Y,
				 setLabel);
		}
	}

	gamepadNavSetWindow(NAV_OVERLAY);
	hudMemberCount = 0;
	place = hudPlace(HUD_PIECE_TEAM, 20 * hudScale,
			 TEAM_FRAME_TOP * hudScale, hudScale, width, height);
	// The team frames can be switched off from the menu's Options page;
	// then the pets go where they like.
	if (gamepadConsoleTeam()) {
		// In a league the league lists hold every team, yours too, so
		// the frames step aside unless kept.
		int inLeague = e->league && e->league->members.count > 1;

		int asList = !inLeague && gamepadConsoleTeamList() &&
			     e->teamup && e->teamup->members.count > 1;

		// Off a league the team can be the same short list the league
		// uses, in the team piece's place.
		if (asList && !hudHidden(HUD_PIECE_TEAM)) {
			int roster[MAX_TEAM_MEMBERS];
			int k, n = 0;
			F32 listHeight;

			for (k = 0; k < e->teamup->members.count &&
				    k < MAX_TEAM_MEMBERS;
			     k++)
				roster[n++] = k;
			listHeight = drawMemberBlock(e, &e->teamup->members,
						     roster, n, 0, place.x,
						     place.y, place.scale);
			hudBoxes[HUD_PIECE_TEAM].height =
				listHeight / place.scale;
			keepPetsOffTeam(place, listHeight, width, hudScale);
		} else if (!hudHidden(HUD_PIECE_TEAM) &&
			   (!inLeague || gamepadConsoleLeagueTeam())) {
			F32 teamHeight = drawTeamFrames(e, place.x, place.y,
							place.scale);

			// Off a team the edit mode still shows where the frames
			// will go.
			hudBoxes[HUD_PIECE_TEAM].height =
				teamHeight > 0
					? teamHeight / place.scale
					: hudPieces[HUD_PIECE_TEAM].box.height;
			keepPetsOffTeam(place, teamHeight, width, hudScale);
		}
		drawLeagueTeams(e, width, height, hudScale);
	} else {
		hudShown[HUD_PIECE_TEAM] = 0;
	}
	memcpy(hudMembersShown, hudMembers, sizeof(HudMember) * hudMemberCount);
	hudMembersShownCount = hudMemberCount;
	collisions_off_for_rest_of_frame = editing ? 1 : collisionsBefore;

	{
		// Swap Target Bumpers in Options puts foes on the left and
		// friends on the right.
		int swapped = gamepadGetSetting("swap_target_sides") != 0;

		place = hudPlace(HUD_PIECE_TARGETS, centerX,
				 topRow - 72 * hudScale, hudScale, width,
				 height);
		if (!hudHidden(HUD_PIECE_TARGETS))
			drawTargetIndicator(
				swapped ? gamepadButtonLabel(GP_RB)
					: gamepadButtonLabel(GP_LB),
				place.x + (swapped ? 150 : -150) * place.scale,
				place.y, place.scale, COLOR_FRIEND,
				current_target && current_target->pchar &&
					!character_TargetIsFoe(
						e->pchar,
						current_target->pchar));
		if (!hudHidden(HUD_PIECE_TARGETS))
			drawTargetIndicator(
				swapped ? gamepadButtonLabel(GP_LB)
					: gamepadButtonLabel(GP_RB),
				place.x + (swapped ? -150 : 150) * place.scale,
				place.y, place.scale, COLOR_ENEMY,
				current_target && current_target->pchar &&
					character_TargetIsFoe(
						e->pchar,
						current_target->pchar));
	}

	// While editing, the hints are the edit mode's own, so the pad's
	// buttons for it are on screen.
	if (editing) {
		promptCount = hudEditPromptCount;
		memcpy(prompts, hudEditPrompts,
		       hudEditPromptCount * sizeof(hudEditPrompts[0]));
	} else {
		promptCount = gatherPrompts(prompts);
	}
	place = hudPlace(HUD_PIECE_HINTS, centerX, height - 18 * hudScale,
			 hudScale, width, height);
	if (promptCount && !hudHidden(HUD_PIECE_HINTS)) {
		F32 hintsWidth = drawPrompts(prompts, promptCount, place.x,
					     place.y, HUD_Z + 40, place.scale);

		hudBoxes[HUD_PIECE_HINTS].width = hintsWidth / place.scale + 20;
		hudBoxes[HUD_PIECE_HINTS].left =
			-hudBoxes[HUD_PIECE_HINTS].width / 2;
	}

	if (menuOpen)
		drawMenu(centerX, height * 0.42f, width, height, hudScale);

	if (editing) {
		collisions_off_for_rest_of_frame = collisionsBefore;
		drawHudEdit(width, hudScale);
		// Nothing behind the edit mode takes the mouse.
		collisions_off_for_rest_of_frame = 1;
		return;
	}

	// A click on the HUD is not a click on the world behind it.
	if (cursorOverHud)
		collisions_off_for_rest_of_frame = 1;
}
