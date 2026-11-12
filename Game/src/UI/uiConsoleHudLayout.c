/*
 * Where each piece of the console HUD sits, and the screen that moves and sizes
 * them.
 */
#include "UI/uiConsoleHudInternal.h"

// ------------------------------------------------------------------ HUD layout
// The pieces of the play HUD can be moved and resized in an edit mode, opened
// from the menu wheel's Options page or with /console_hud_edit. Each piece
// keeps how far it has moved from where it starts, in pixels of a 1080 high
// screen, and its size, so a layout carries over to other resolutions. The
// layout is kept in console_hud.txt beside the game. None of this touches the
// game's own HUD, without the console HUD.

#define HUD_LAYOUT_NAME "console_hud.txt"
#define HUD_SIZE_MIN 0.5f
#define HUD_SIZE_MAX 2.0f
#define HUD_EDIT_STEP 10.0f
#define HUD_EDIT_SIZE_STEP 0.05f


const HudPiece hudPieces[HUD_PIECE_COUNT] = {
	{ "character", "Character", { -175, -60, 360, 110 } },
	{ "powers", "Powers", { -162, -68, 324, 153 } },
	{ "powers_lt", "LT powers", { -162, -68, 324, 153 } },
	{ "powers_rt", "RT powers", { -162, -68, 324, 153 } },
	{ "powers_lt_rt", "LT + RT powers", { -162, -68, 400, 153 } },
	{ "targets", "Target markers", { -172, -20, 344, 40 } },
	{ "team", "Team", { 0, 0, 250, 190 } },
	{ "league_1", "League team 1", { 0, 0, 170, 204 } },
	{ "league_2", "League team 2", { 0, 0, 170, 204 } },
	{ "league_3", "League team 3", { 0, 0, 170, 204 } },
	{ "league_4", "League team 4", { 0, 0, 170, 204 } },
	{ "league_5", "League team 5", { 0, 0, 170, 204 } },
	{ "league_6", "League team 6", { 0, 0, 170, 204 } },
	{ "hints", "Button hints", { -250, -16, 500, 32 } },
};

HudPlace hudLayout[HUD_PIECE_COUNT];
static HudPlace hudLayoutBefore[HUD_PIECE_COUNT];
static int hudLayoutLoaded;
// The team and the hints change size with what they show, so each has the box
// it last drew.
HudBox hudBoxes[HUD_PIECE_COUNT];
// What was drawn this frame, for the edit mode to outline and grab.
HudRect hudDrawn[HUD_PIECE_COUNT];
F32 hudAnchorX[HUD_PIECE_COUNT];
int hudShown[HUD_PIECE_COUNT];

int hudEditPiece;
int hudDragPiece = -1;
int hudDragSizing;
int hudDragX, hudDragY;
HudPlace hudDragFrom;
F32 hudDragReach;

const Prompt hudEditPrompts[] = {
	{ -1, "LB / RB Piece" }, { -1, "D-pad Move" }, { GP_Y, "Larger" },
	{ GP_X, "Smaller" },	 { -1, "L3 Hide" },    { -1, "Back Reset" },
	{ GP_A, "Save" },	 { GP_B, "Cancel" },
};
const int hudEditPromptCount = ARRAY_SIZE(hudEditPrompts);

F32 hudClampSize(F32 size)
{
	return size < HUD_SIZE_MIN   ? HUD_SIZE_MIN
	       : size > HUD_SIZE_MAX ? HUD_SIZE_MAX
				     : size;
}
void hudResetPiece(HudPlace *place)
{
	place->x = 0;
	place->y = 0;
	place->size =
		place == &hudLayout[HUD_PIECE_TEAM] ||
				(place >= &hudLayout[HUD_PIECE_LEAGUE_1] &&
				 place <= &hudLayout[HUD_PIECE_LEAGUE_6])
			? HUD_TEAM_DEFAULT_SIZE
			: 1;
	place->hidden = 0;
}

// Hidden pieces are not drawn, except in the edit mode, where they can be shown
// again.
int hudHidden(int piece)
{
	return hudLayout[piece].hidden && !hudEditUp();
}

static char *hudLayoutPath(char *buffer, size_t bufferSize)
{
	char dir[512];

	snprintf_s(buffer, bufferSize, bufferSize - 1, "%s/%s",
		   getExecutableDir(dir), HUD_LAYOUT_NAME);
	return buffer;
}

void hudLoadLayout(void)
{
	char path[512];
	char line[256];
	FILE *file;
	int i;

	if (hudLayoutLoaded)
		return;
	hudLayoutLoaded = 1;

	for (i = 0; i < HUD_PIECE_COUNT; i++) {
		hudResetPiece(&hudLayout[i]);
		hudBoxes[i] = hudPieces[i].box;
	}

	file = fopen(hudLayoutPath(path, sizeof(path)), "rt");
	if (!file)
		return;

	while (fgets(line, sizeof(line), file)) {
		char key[32];
		F32 x, y, size;
		int hidden = 0;

		if (line[0] == '#' || sscanf(line, "%31s %f %f %f %d", key, &x,
					     &y, &size, &hidden) < 4)
			continue;

		for (i = 0; i < HUD_PIECE_COUNT; i++) {
			if (stricmp(key, hudPieces[i].key))
				continue;
			hudLayout[i].x = x;
			hudLayout[i].y = y;
			hudLayout[i].size = hudClampSize(size);
			hudLayout[i].hidden = hidden != 0;
		}
	}
	fclose(file);
}

static void hudSaveLayout(void)
{
	char path[512];
	FILE *file = fopen(hudLayoutPath(path, sizeof(path)), "wt");
	int i;

	if (!file) {
		addSystemChatMsg("The HUD layout could not be saved.",
				 INFO_USER_ERROR, 0);
		return;
	}

	fprintf(file, "# The console HUD's layout, from its edit mode "
		      "(/console_hud_edit).\n");
	fprintf(file, "# Each line is a piece, how far it has moved across and "
		      "down in pixels of a 1080 high screen,\n");
	fprintf(file, "# its size, from 0.5 to 2, and 1 if it is hidden.\n");
	for (i = 0; i < HUD_PIECE_COUNT; i++)
		fprintf(file, "%s %.1f %.1f %.2f %d\n", hudPieces[i].key,
			hudLayout[i].x, hudLayout[i].y, hudLayout[i].size,
			hudLayout[i].hidden);
	fclose(file);
}

// Where a piece goes this frame, from where it starts. Nothing is moved or
// grown off the screen.
HudFrame hudPlace(int piece, F32 x, F32 y, F32 hudScale, int screenWidth,
		  int screenHeight)
{
	HudPlace *place = &hudLayout[piece];
	const HudBox *box = &hudBoxes[piece];
	HudFrame frame;
	F32 left, top, width, height;
	F32 shiftX = 0, shiftY = 0;

	frame.scale = hudScale * place->size;
	frame.x = x + place->x * hudScale;
	frame.y = y + place->y * hudScale;

	left = frame.x + box->left * frame.scale;
	top = frame.y + box->top * frame.scale;
	width = box->width * frame.scale;
	height = box->height * frame.scale;

	if (left + width > screenWidth)
		shiftX = screenWidth - (left + width);
	if (left + shiftX < 0)
		shiftX = -left;
	if (top + height > screenHeight)
		shiftY = screenHeight - (top + height);
	if (top + shiftY < 0)
		shiftY = -top;

	if (shiftX != 0 || shiftY != 0) {
		frame.x += shiftX;
		frame.y += shiftY;
		left += shiftX;
		top += shiftY;
		// The piece being edited stops at the edge rather than going on
		// out of reach. The rest only show where they fit, and keep the
		// layout they were given.
		if (hudEditOpen && piece == hudEditPiece) {
			place->x += shiftX / hudScale;
			place->y += shiftY / hudScale;
		}
	}

	hudDrawn[piece].x = left;
	hudDrawn[piece].y = top;
	hudDrawn[piece].width = width;
	hudDrawn[piece].height = height;
	hudAnchorX[piece] = frame.x;
	hudShown[piece] = 1;
	return frame;
}

int hudEditUp(void)
{
	return hudEditOpen && consoleHudActive() &&
	       window_getMode(WDW_DIALOG) != WINDOW_DISPLAYING;
}

int consoleHudEditIsOpen(void)
{
	return hudEditOpen && consoleHudActive();
}

// Save keeps the layout and writes it out. Anything else puts back the layout
// the edit began with.
void hudEditClose(int keep)
{
	if (!hudEditOpen)
		return;

	if (keep)
		hudSaveLayout();
	else
		memcpy(hudLayout, hudLayoutBefore, sizeof(hudLayout));
	hudEditOpen = 0;
	hudDragPiece = -1;
}

void hudEditDiscard(void)
{
	hudEditClose(0);
}

void consoleHudEditToggle(void)
{
	if (!consoleHudActive()) {
		addSystemChatMsg(
			"The HUD layout is edited with the console HUD on.",
			INFO_USER_ERROR, 0);
		return;
	}

	if (hudEditOpen) {
		hudEditClose(1);
		return;
	}

	gameMenuOpen = 0;
	inspOpen = 0;
	menuOpen = 0;
	bindOpen = 0;
	slotOpen = 0;
	incOpen = 0;
	if (invOpen)
		invClose();
	if (navOpen)
		navClose();

	hudLoadLayout();
	memcpy(hudLayoutBefore, hudLayout, sizeof(hudLayout));
	hudEditOpen = 1;
	hudEditPiece = HUD_PIECE_CHARACTER;
	hudDragPiece = -1;
}

static void hudEditStepPiece(int direction)
{
	int i;

	for (i = 0; i < HUD_PIECE_COUNT; i++) {
		hudEditPiece = (hudEditPiece + direction + HUD_PIECE_COUNT) %
			       HUD_PIECE_COUNT;
		if (hudShown[hudEditPiece])
			return;
	}
}

void hudEditPress(int modifiers, int button)
{
	HudPlace *place = &hudLayout[hudEditPiece];

	(void)modifiers;
	switch (button) {
	case GP_LB:
		hudEditStepPiece(-1);
		break;
	case GP_RB:
		hudEditStepPiece(1);
		break;
	case GP_DPAD_UP:
		place->y -= HUD_EDIT_STEP;
		break;
	case GP_DPAD_DOWN:
		place->y += HUD_EDIT_STEP;
		break;
	case GP_DPAD_LEFT:
		place->x -= HUD_EDIT_STEP;
		break;
	case GP_DPAD_RIGHT:
		place->x += HUD_EDIT_STEP;
		break;
	case GP_Y:
		place->size = hudClampSize(place->size + HUD_EDIT_SIZE_STEP);
		break;
	case GP_X:
		place->size = hudClampSize(place->size - HUD_EDIT_SIZE_STEP);
		break;
	case GP_BACK:
		hudResetPiece(place);
		break;
	case GP_L3:
		place->hidden = !place->hidden;
		break;
	case GP_A:
	case GP_START:
		hudEditClose(1);
		break;
	case GP_B:
		hudEditClose(0);
		break;
	}
}

// The right stick sizes the chosen piece: up to grow it, down to shrink it.
int hudEditLook(F32 y, F32 seconds)
{
	HudPlace *place = &hudLayout[hudEditPiece];

	place->size = hudClampSize(place->size - y * seconds * 0.8f);
	return 1;
}

int drawHudEditButton(const char *label, F32 x, F32 y, F32 width, F32 height,
		      F32 z, F32 hudScale)
{
	CBox box;
	int hovered;

	BuildCBox(&box, x, y, width, height);
	hovered = mouseCollision(&box);
	drawFlatFrame(PIX2, 8, x, y, z, width, height, hudScale,
		      hovered ? FRAME_COLOR_HELD : FRAME_COLOR,
		      hovered ? 0x405878f0 : 0x101820f0);
	drawText(&game_12, x + width / 2, y + height / 2, z + 2, hudScale,
		 hovered ? FRAME_COLOR_HELD : CLR_WHITE, CENTER_X | CENTER_Y,
		 label);
	return mouseClickHit(&box, MS_LEFT);
}
