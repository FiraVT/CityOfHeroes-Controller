/*
 * Console inspirations: the game's own tray with the D-pad on it, and a list
 * beside it for combining three of the same inspiration.
 */
#include "UI/uiConsoleHudInternal.h"
#include "UI/uiInspiration.h"

//------------------------------------------------------------------------------------------------------
// Inspirations
//------------------------------------------------------------------------------------------------------

// Inspirations open the game's own tray, which the D-pad moves through and A
// uses. Y on one held three times opens a short list beside the tray of what
// the three can become: another inspiration of the same size, as the tray's
// right click menu offers.
#define INSP_CHOICE_HEIGHT 34.0f

static int inspColumn;
static int inspRow;
static int inspChoice;
static int inspMouseX, inspMouseY;

int inspScreenUp(void)
{
	return inspOpen && consoleHudActive() &&
	       window_getMode(WDW_INSPIRATION) == WINDOW_DISPLAYING &&
	       window_getMode(WDW_DIALOG) != WINDOW_DISPLAYING;
}

static const BasePower *inspAt(Entity *e, int column, int row)
{
	if (!e || !e->pchar || column < 0 || row < 0 ||
	    column >= e->pchar->iNumInspirationCols ||
	    row >= e->pchar->iNumInspirationRows)
		return NULL;
	return e->pchar->aInspirations[column][row];
}

static int inspCount(Entity *e, const BasePower *insp)
{
	int count = 0;
	int i, j;

	for (i = 0; i < e->pchar->iNumInspirationCols; i++)
		for (j = 0; j < e->pchar->iNumInspirationRows; j++)
			if (insp && e->pchar->aInspirations[i][j] == insp)
				count++;
	return count;
}

// The inspirations of the same size, for combining. NULL for any that cannot be
// combined.
static const char **inspSizeList(const BasePower *insp, const char **size)
{
	static const char *sizes[] = { "Small", "Medium", "Large" };
	const char **lists[3];
	int i, j;

	lists[0] = g_smallInspirations;
	lists[1] = g_mediumInspirations;
	lists[2] = g_largeInspirations;
	if (!insp || !insp->pchName)
		return NULL;
	for (i = 0; i < 3; i++) {
		for (j = 0; lists[i][j]; j++) {
			if (stricmp(lists[i][j], insp->pchName) == 0) {
				*size = sizes[i];
				return lists[i];
			}
		}
	}
	return NULL;
}

static int inspCanCombine(Entity *e, const BasePower *insp)
{
	const char *size;

	return insp && inspSizeList(insp, &size) && inspCount(e, insp) >= 3;
}

void consoleInspirationsToggle(void)
{
	char standardWindow[] = "window_toggle insp";
	char showWindow[] = "show insp";

	if (!consoleHudActive()) {
		cmdParse(standardWindow);
		return;
	}

	hudEditDiscard();
	gameMenuOpen = 0;
	menuOpen = 0;
	bindOpen = 0;
	slotOpen = 0;
	incOpen = 0;
	inspOpen = 0;
	if (invOpen)
		invClose();
	if (navOpen)
		navClose();
	cmdParse(showWindow);
	gamepadFocusOnWindow(WDW_INSPIRATION);
	inspirationFocusFirstSlot();
}

// The highlighted inspiration in the game's tray, when three of it can be
// combined.
static const BasePower *inspHoveredCombinable(Entity *e, int *column,
					      int *row)
{
	const BasePower *insp;

	if (!consoleHudActive() || !e || !e->pchar ||
	    !inspirationHoveredSlot(column, row))
		return NULL;
	insp = inspAt(e, *column, *row);
	return inspCanCombine(e, insp) ? insp : NULL;
}

int consoleInspirationsCanCombine(void)
{
	int column, row;

	return inspHoveredCombinable(playerPtr(), &column, &row) != NULL;
}

int consoleInspirationsCombine(void)
{
	Entity *e = playerPtr();
	const BasePower *insp;
	const char *size;
	const char **list;
	int column, row;

	if (!consoleHudActive() || !e || !e->pchar ||
	    !inspirationHoveredSlot(&column, &row))
		return 0;

	insp = inspHoveredCombinable(e, &column, &row);
	if (!insp) {
		addSystemChatMsg(inspAt(e, column, row)
					 ? "Three of the same inspiration can "
					   "be combined into another."
					 : "That slot is empty.",
				 INFO_USER_ERROR, 0);
		return 1;
	}

	list = inspSizeList(insp, &size);
	inspColumn = column;
	inspRow = row;
	inspChoice = stricmp(list[0], insp->pchName) == 0 ? 1 : 0;
	inspMouseX = gMouseInpCur.x;
	inspMouseY = gMouseInpCur.y;
	inspOpen = 1;
	return 1;
}

static void inspCombine(Entity *e)
{
	const BasePower *insp = inspAt(e, inspColumn, inspRow);
	const char *size;
	const char **list = inspSizeList(insp, &size);
	char command[160];

	inspOpen = 0;
	if (!list || !inspCanCombine(e, insp) ||
	    stricmp(list[inspChoice], insp->pchName) == 0)
		return;
	snprintf(command, sizeof(command), "mergeInsp %s %s", insp->pchName,
		 list[inspChoice]);
	command[sizeof(command) - 1] = 0;
	cmdParse(command);
}

// Up and down through the choices, past the one being given up.
static void inspStepChoice(Entity *e, int direction)
{
	const BasePower *insp = inspAt(e, inspColumn, inspRow);
	const char *size;
	const char **list = inspSizeList(insp, &size);
	int count = 0;
	int next;

	if (!list)
		return;
	while (list[count])
		count++;
	next = inspChoice + direction;
	if (next >= 0 && next < count &&
	    stricmp(list[next], insp->pchName) == 0)
		next += direction;
	if (next >= 0 && next < count)
		inspChoice = next;
}

void inspPress(int modifiers, int button)
{
	Entity *e = playerPtr();

	(void)modifiers;
	if (!e || !e->pchar) {
		inspOpen = 0;
		return;
	}

	switch (button) {
	case GP_DPAD_UP:
		inspStepChoice(e, -1);
		break;
	case GP_DPAD_DOWN:
		inspStepChoice(e, 1);
		break;
	case GP_A:
		inspCombine(e);
		break;
	case GP_B:
	case GP_Y:
		inspOpen = 0;
		break;
	}
}

// The list sits beside the game's tray, on whichever side has room, level with
// its bottom.
void drawInspCombine(Entity *e, int width, int height, F32 hudScale)
{
	static const Prompt prompts[] = { { GP_A, "Combine" },
					  { GP_B, "Back" } };
	const BasePower *chosen = inspAt(e, inspColumn, inspRow);
	const char *size;
	const char **list = inspSizeList(chosen, &size);
	F32 pad = 14 * hudScale;
	F32 titleHeight = 40 * hudScale;
	F32 promptHeight = 48 * hudScale;
	F32 rowHeight = INSP_CHOICE_HEIGHT * hudScale;
	F32 listWidth = 300 * hudScale;
	F32 panelWidth = pad + listWidth + pad;
	F32 panelHeight, left, top, y;
	float wx, wy, wz, wwd, wht, wscale;
	int color, backColor;
	int mouseMoved =
		gMouseInpCur.x != inspMouseX || gMouseInpCur.y != inspMouseY;
	int count = 0;
	char header[160];
	int i;

	if (!list || !inspCanCombine(e, chosen) ||
	    !window_getDims(WDW_INSPIRATION, &wx, &wy, &wz, &wwd, &wht,
			    &wscale, &color, &backColor)) {
		inspOpen = 0;
		return;
	}
	inspMouseX = gMouseInpCur.x;
	inspMouseY = gMouseInpCur.y;

	while (list[count])
		count++;
	panelHeight = titleHeight + count * rowHeight + pad + promptHeight;
	left = wx - panelWidth - 8 * hudScale;
	if (left < 8 * hudScale)
		left = wx + wwd + 8 * hudScale;
	top = wy + wht - panelHeight;
	if (top < 8 * hudScale)
		top = 8 * hudScale;
	if (top + panelHeight > height - 8 * hudScale)
		top = height - 8 * hudScale - panelHeight;

	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820e0);
	snprintf(header, sizeof(header), "Combine three %s into:",
		 textStd(chosen->pchDisplayName));
	header[sizeof(header) - 1] = 0;
	drawText(&game_12, left + pad, top + titleHeight / 2, BIND_Z + 4,
		 hudScale, CLR_WHITE, CENTER_Y, header);

	y = top + titleHeight;
	for (i = 0; list[i]; i++) {
		const BasePower *option = basePower_inspirationGetByName(
			(char *)size, (char *)list[i]);
		int same = stricmp(list[i], chosen->pchName) == 0;
		int selected = i == inspChoice;
		F32 x = left + pad;
		CBox box;

		if (!option)
			continue;

		BuildCBox(&box, x, y, listWidth, rowHeight - 4 * hudScale);
		if (!same && mouseCollision(&box)) {
			if (mouseMoved)
				inspChoice = i;
			if (mouseClickHit(&box, MS_LEFT)) {
				inspChoice = i;
				inspCombine(e);
				return;
			}
		}

		drawFlatFrame(PIX2, 8, x, y, BIND_Z + 2, listWidth,
			      rowHeight - 4 * hudScale, hudScale,
			      selected ? FRAME_COLOR_HELD : FRAME_COLOR_DIM,
			      selected ? 0x405878f0 : 0x101820c0);
		if (option->pchIconName) {
			AtlasTex *icon = atlasLoadTexture(option->pchIconName);
			F32 iconScale = (rowHeight - 12 * hudScale) / icon->width;

			display_sprite(icon, x + 4 * hudScale, y + 4 * hudScale,
				       BIND_Z + 4, iconScale, iconScale,
				       same ? 0xffffff50 : CLR_WHITE);
		}
		drawText(&game_12, x + rowHeight + 4 * hudScale,
			 y + (rowHeight - 4 * hudScale) / 2, BIND_Z + 4,
			 hudScale,
			 same	    ? 0xffffff60
			 : selected ? FRAME_COLOR_HELD
				    : CLR_WHITE,
			 CENTER_Y, textStd(option->pchDisplayName));
		y += rowHeight;
	}

	drawPrompts(prompts, ARRAY_SIZE(prompts), left + panelWidth / 2,
		    top + panelHeight - promptHeight / 2, BIND_Z + 6, hudScale);
}
