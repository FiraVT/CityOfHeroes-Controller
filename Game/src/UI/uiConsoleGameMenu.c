/*
 * The console game menu.
 */
#include "UI/uiConsoleHudInternal.h"

//------------------------------------------------------------------------------------------------------
// Game menu
//------------------------------------------------------------------------------------------------------

// What the menu ring does not already open: the game's own windows for
// settings, costumes, rewards and leaving, as big tiles in the middle of the
// screen. The status bar's small menu stays as it was for play without the
// console HUD.
#define GAME_MENU_COLUMNS 3
#define GAME_MENU_TILE_WIDTH 300.0f
#define GAME_MENU_TILE_HEIGHT 76.0f
#define GAME_MENU_GAP 18.0f
#define GAME_MENU_MARGIN 48.0f

typedef enum GameMenuAction {
	GAME_MENU_WINDOW,
	GAME_MENU_TOGGLE,
	GAME_MENU_ARENA,
	GAME_MENU_ID_CARD,
	GAME_MENU_INFO,
	GAME_MENU_CHAT_HANDLE,
} GameMenuAction;

typedef struct GameMenuEntry {
	const char *label;
	GameMenuAction action;
	int window;
} GameMenuEntry;

static const GameMenuEntry gameMenuEntries[] = {
	{ "Options", GAME_MENU_WINDOW, WDW_OPTIONS },
	{ "Costumes", GAME_MENU_WINDOW, WDW_COSTUME_SELECT },
	{ "Paragon Rewards", GAME_MENU_WINDOW, WDW_LOYALTY_TREE },
	{ "Looking for Group", GAME_MENU_WINDOW, WDW_TURNSTILE },
	{ "Arena", GAME_MENU_ARENA, WDW_ARENA_LIST },
	{ "I.D. Card", GAME_MENU_ID_CARD, 0 },
	{ "Personal Info", GAME_MENU_INFO, WDW_INFO },
	{ "Chat Handle", GAME_MENU_CHAT_HANDLE, 0 },
	{ "Compass", GAME_MENU_TOGGLE, WDW_COMPASS },
	{ "Target Window", GAME_MENU_TOGGLE, WDW_TARGET },
	{ "Help", GAME_MENU_WINDOW, WDW_HELP },
	{ "Support", GAME_MENU_WINDOW, WDW_PETITION },
	{ "Quit", GAME_MENU_WINDOW, WDW_QUIT },
};

static int gameMenuSelection;
static int gameMenuMouseX, gameMenuMouseY;

int gameMenuUp(void)
{
	return gameMenuOpen && consoleHudActive() &&
	       window_getMode(WDW_DIALOG) != WINDOW_DISPLAYING;
}

int consoleGameMenuIsOpen(void)
{
	return gameMenuOpen && consoleHudActive();
}

void consoleGameMenuToggle(void)
{
	if (!consoleHudActive())
		return;

	if (gameMenuOpen) {
		gameMenuOpen = 0;
		return;
	}

	hudEditDiscard();
	inspOpen = 0;
	menuOpen = 0;
	bindOpen = 0;
	slotOpen = 0;
	incOpen = 0;
	if (invOpen)
		invClose();
	if (navOpen)
		navClose();
	gameMenuOpen = 1;
	gameMenuSelection = 0;
	gameMenuMouseX = gMouseInpCur.x;
	gameMenuMouseY = gMouseInpCur.y;
}

// The tiles this player has. Chat Handle needs the chat server.
static int gameMenuShown(int *shown)
{
	int count = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(gameMenuEntries); i++) {
		if (gameMenuEntries[i].action == GAME_MENU_CHAT_HANDLE &&
		    !UsingChatServer())
			continue;
		shown[count++] = i;
	}
	return count;
}

static void gameMenuActivate(const GameMenuEntry *entry)
{
	char arena[] = "arena_list";
	Entity *e = playerPtr();

	switch (entry->action) {
	case GAME_MENU_TOGGLE:
		// The compass and target windows go on and off without leaving
		// the menu.
		window_openClose(entry->window);
		return;
	case GAME_MENU_WINDOW:
		gameMenuOpen = 0;
		if (!windowUp(entry->window))
			window_openClose(entry->window);
		gamepadFocusOnWindow(entry->window);
		return;
	case GAME_MENU_ARENA:
		gameMenuOpen = 0;
		cmdParse(arena);
		gamepadFocusOnWindow(entry->window);
		return;
	case GAME_MENU_ID_CARD:
		gameMenuOpen = 0;
		start_menu(MENU_REGISTER);
		return;
	case GAME_MENU_INFO:
		gameMenuOpen = 0;
		if (e)
			info_Entity(e, 0);
		gamepadFocusOnWindow(entry->window);
		return;
	case GAME_MENU_CHAT_HANDLE:
		gameMenuOpen = 0;
		displayChatHandleDialog(NULL);
		return;
	}
}

void gameMenuPress(int button)
{
	int shown[ARRAY_SIZE(gameMenuEntries)];
	int count = gameMenuShown(shown);
	int column;

	if (count <= 0)
		return;
	if (gameMenuSelection >= count)
		gameMenuSelection = count - 1;
	column = gameMenuSelection % GAME_MENU_COLUMNS;

	switch (button) {
	case GP_DPAD_LEFT:
		if (column > 0)
			gameMenuSelection--;
		break;
	case GP_DPAD_RIGHT:
		if (column < GAME_MENU_COLUMNS - 1 &&
		    gameMenuSelection + 1 < count)
			gameMenuSelection++;
		break;
	case GP_DPAD_UP:
		if (gameMenuSelection >= GAME_MENU_COLUMNS)
			gameMenuSelection -= GAME_MENU_COLUMNS;
		break;
	case GP_DPAD_DOWN:
		// Down from above a short last row lands on its last tile.
		if (gameMenuSelection + GAME_MENU_COLUMNS < count)
			gameMenuSelection += GAME_MENU_COLUMNS;
		else if (gameMenuSelection / GAME_MENU_COLUMNS <
			 (count - 1) / GAME_MENU_COLUMNS)
			gameMenuSelection = count - 1;
		break;
	case GP_A:
		gameMenuActivate(&gameMenuEntries[shown[gameMenuSelection]]);
		break;
	case GP_B:
	case GP_START:
		gameMenuOpen = 0;
		break;
	}
}

void drawGameMenu(int width, int height, F32 hudScale)
{
	static const Prompt prompts[] = { { GP_A, "Open" }, { GP_B, "Close" } };
	int shown[ARRAY_SIZE(gameMenuEntries)];
	int count = gameMenuShown(shown);
	int rows = (count + GAME_MENU_COLUMNS - 1) / GAME_MENU_COLUMNS;
	F32 tileWidth = GAME_MENU_TILE_WIDTH * hudScale;
	F32 tileHeight = GAME_MENU_TILE_HEIGHT * hudScale;
	F32 gap = GAME_MENU_GAP * hudScale;
	F32 margin = GAME_MENU_MARGIN * hudScale;
	F32 panelWidth = GAME_MENU_COLUMNS * tileWidth +
			 (GAME_MENU_COLUMNS - 1) * gap + 2 * margin;
	F32 panelHeight = 100 * hudScale + rows * tileHeight +
			  (rows - 1) * gap + 80 * hudScale;
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 tilesTop = top + 100 * hudScale;
	// A pointer left resting on a tile must not hold the choice against the
	// D-pad, so the mouse picks only once it moves.
	int mouseMoved = gMouseInpCur.x != gameMenuMouseX ||
			 gMouseInpCur.y != gameMenuMouseY;
	int i;

	gameMenuMouseX = gMouseInpCur.x;
	gameMenuMouseY = gMouseInpCur.y;
	if (gameMenuSelection >= count)
		gameMenuSelection = count - 1;

	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x000000b0);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	drawText(&game_18, left + panelWidth / 2, top + 50 * hudScale,
		 BIND_Z + 4, hudScale * 1.3f, CLR_WHITE, CENTER_X | CENTER_Y,
		 "Game Menu");

	for (i = 0; i < count; i++) {
		const GameMenuEntry *entry = &gameMenuEntries[shown[i]];
		int row = i / GAME_MENU_COLUMNS;
		int column = i % GAME_MENU_COLUMNS;
		int inRow = row == rows - 1 ? count - row * GAME_MENU_COLUMNS
					    : GAME_MENU_COLUMNS;
		// A short last row sits in the middle.
		F32 x = left + margin +
			(GAME_MENU_COLUMNS - inRow) * (tileWidth + gap) / 2 +
			column * (tileWidth + gap);
		F32 y = tilesTop + row * (tileHeight + gap);
		int selected = i == gameMenuSelection;
		int toggle = entry->action == GAME_MENU_TOGGLE;
		CBox box;

		BuildCBox(&box, x, y, tileWidth, tileHeight);
		if (mouseCollision(&box)) {
			if (mouseMoved)
				gameMenuSelection = i;
			if (mouseClickHit(&box, MS_LEFT)) {
				gameMenuSelection = i;
				gameMenuActivate(entry);
				if (!gameMenuOpen)
					return;
			}
		}

		drawFlatFrame(PIX3, R10, x, y, BIND_Z + 2, tileWidth,
			      tileHeight, hudScale,
			      selected ? FRAME_COLOR_HELD : FRAME_COLOR,
			      selected ? 0x405878f0 : 0x101820e0);
		drawText(&game_18, x + tileWidth / 2,
			 y + tileHeight / 2 - (toggle ? 10 : 0) * hudScale,
			 BIND_Z + 6, hudScale,
			 selected ? FRAME_COLOR_HELD : CLR_WHITE,
			 CENTER_X | CENTER_Y, entry->label);
		if (toggle) {
			int up = windowUp(entry->window);

			drawText(&game_12, x + tileWidth / 2,
				 y + tileHeight / 2 + 16 * hudScale, BIND_Z + 6,
				 hudScale, up ? 0x80ff80ff : 0xffffffa0,
				 CENTER_X | CENTER_Y, up ? "Shown" : "Hidden");
		}
	}

	drawPrompts(prompts, ARRAY_SIZE(prompts), left + panelWidth / 2,
		    top + panelHeight - 40 * hudScale, BIND_Z + 6, hudScale);
}
