/*
 * The console Inspirations screen: using and combining inspirations.
 */
#include "UI/uiConsoleHudInternal.h"

//------------------------------------------------------------------------------------------------------
// Inspirations
//------------------------------------------------------------------------------------------------------

// The inspiration tray, big, with what each one does beside it. Three of the
// same can be combined into one other inspiration of the same size, as the
// tray's right click menu does.
#define INSP_ICON_SIZE 88.0f
#define INSP_ICON_STEP 104.0f
#define INSP_CHOICE_HEIGHT 46.0f

static int inspColumn;
static int inspRow;
static int inspCombining;
static int inspChoice;
static int inspMouseX, inspMouseY;

int inspScreenUp(void)
{
	return inspOpen && consoleHudActive() &&
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

	if (!consoleHudActive()) {
		cmdParse(standardWindow);
		return;
	}

	if (inspOpen) {
		inspOpen = 0;
		return;
	}

	hudEditDiscard();
	gameMenuOpen = 0;
	menuOpen = 0;
	bindOpen = 0;
	slotOpen = 0;
	incOpen = 0;
	if (invOpen)
		invClose();
	if (navOpen)
		navClose();
	inspOpen = 1;
	inspColumn = 0;
	inspRow = 0;
	inspCombining = 0;
	inspMouseX = gMouseInpCur.x;
	inspMouseY = gMouseInpCur.y;
}

static void inspStartCombining(Entity *e)
{
	const BasePower *insp = inspAt(e, inspColumn, inspRow);
	const char *size;
	const char **list = inspSizeList(insp, &size);

	if (!list || !inspCanCombine(e, insp))
		return;
	inspCombining = 1;
	inspChoice = stricmp(list[0], insp->pchName) == 0 ? 1 : 0;
}

static void inspCombine(Entity *e)
{
	const BasePower *insp = inspAt(e, inspColumn, inspRow);
	const char *size;
	const char **list = inspSizeList(insp, &size);
	char command[160];

	inspCombining = 0;
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
	if (!e || !e->pchar)
		return;

	if (inspCombining) {
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
			inspCombining = 0;
			break;
		}
		return;
	}

	// Row 0 is the bottom row, as in the tray, so up is the next row.
	switch (button) {
	case GP_DPAD_LEFT:
		if (inspColumn > 0)
			inspColumn--;
		break;
	case GP_DPAD_RIGHT:
		if (inspColumn < e->pchar->iNumInspirationCols - 1)
			inspColumn++;
		break;
	case GP_DPAD_UP:
		if (inspRow < e->pchar->iNumInspirationRows - 1)
			inspRow++;
		break;
	case GP_DPAD_DOWN:
		if (inspRow > 0)
			inspRow--;
		break;
	case GP_A:
		if (inspAt(e, inspColumn, inspRow))
			inspiration_activateSlot(inspColumn, inspRow);
		break;
	case GP_Y:
		inspStartCombining(e);
		break;
	case GP_B:
	case GP_START:
		inspOpen = 0;
		break;
	}
}

void drawInspScreen(Entity *e, int width, int height, F32 hudScale)
{
	static const Prompt usePrompts[] = { { GP_A, "Use" },
					     { GP_Y, "Combine 3" },
					     { GP_B, "Close" } };
	static const Prompt plainPrompts[] = { { GP_A, "Use" },
					       { GP_B, "Close" } };
	static const Prompt emptyPrompts[] = { { GP_B, "Close" } };
	static const Prompt combinePrompts[] = { { GP_A, "Combine" },
						 { GP_B, "Back" } };
	int columns = e->pchar->iNumInspirationCols;
	int rows = e->pchar->iNumInspirationRows;
	F32 iconSize = INSP_ICON_SIZE * hudScale;
	F32 step = INSP_ICON_STEP * hudScale;
	F32 panelWidth = 1100 * hudScale;
	F32 panelHeight = 640 * hudScale;
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 gridLeft = left + 48 * hudScale;
	F32 gridTop = top + 110 * hudScale;
	F32 sideLeft = gridLeft + columns * step + 40 * hudScale;
	F32 sideWidth = left + panelWidth - 48 * hudScale - sideLeft;
	int mouseMoved =
		gMouseInpCur.x != inspMouseX || gMouseInpCur.y != inspMouseY;
	const BasePower *chosen;
	const Prompt *prompts;
	int promptCount;
	int i, j;

	inspMouseX = gMouseInpCur.x;
	inspMouseY = gMouseInpCur.y;
	if (inspColumn >= columns)
		inspColumn = columns - 1;
	if (inspRow >= rows)
		inspRow = rows - 1;

	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000090);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	drawText(&game_18, left + panelWidth / 2, top + 50 * hudScale,
		 BIND_Z + 4, hudScale * 1.3f, CLR_WHITE, CENTER_X | CENTER_Y,
		 "Inspirations");

	for (i = 0; i < columns; i++) {
		for (j = 0; j < rows; j++) {
			const BasePower *insp = e->pchar->aInspirations[i][j];
			F32 x = gridLeft + i * step;
			F32 y = gridTop + (rows - 1 - j) * step;
			int selected = i == inspColumn && j == inspRow;
			CBox box;

			BuildCBox(&box, x, y, iconSize, iconSize);
			if (!inspCombining && mouseCollision(&box)) {
				if (mouseMoved) {
					inspColumn = i;
					inspRow = j;
				}
				if (mouseClickHit(&box, MS_LEFT) && insp) {
					inspColumn = i;
					inspRow = j;
					inspiration_activateSlot(i, j);
				} else if (mouseClickHit(&box, MS_RIGHT) &&
					   insp) {
					inspColumn = i;
					inspRow = j;
					inspStartCombining(e);
				}
			}

			drawFlatFrame(
				PIX2, 8, x - 4 * hudScale, y - 4 * hudScale,
				BIND_Z + 2, iconSize + 8 * hudScale,
				iconSize + 8 * hudScale, hudScale,
				selected ? FRAME_COLOR_HELD : FRAME_COLOR_DIM,
				selected ? 0x405878f0 : 0x101820e0);
			if (insp && insp->pchIconName) {
				AtlasTex *icon =
					atlasLoadTexture(insp->pchIconName);
				F32 iconScale = iconSize / icon->width;

				display_sprite(icon, x, y, BIND_Z + 4,
					       iconScale, iconScale,
					       inspCombining && !selected
						       ? 0xffffff60
						       : CLR_WHITE);
			}
		}
	}
	drawText(&game_12, gridLeft + columns * step / 2 - 8 * hudScale,
		 gridTop + rows * step + 10 * hudScale, BIND_Z + 4, hudScale,
		 0xffffffa0, CENTER_X | CENTER_Y,
		 "The bottom row is the tray's first row");

	chosen = inspAt(e, inspColumn, inspRow);
	if (inspCombining && chosen) {
		const char *size;
		const char **list = inspSizeList(chosen, &size);
		char header[160];
		F32 y = gridTop;

		snprintf(header, sizeof(header), "Combine three %s into:",
			 textStd(chosen->pchDisplayName));
		header[sizeof(header) - 1] = 0;
		drawText(&game_18, sideLeft, y + 14 * hudScale, BIND_Z + 4,
			 hudScale, CLR_WHITE, CENTER_Y, header);
		y += 44 * hudScale;

		for (i = 0; list && list[i]; i++) {
			const BasePower *option =
				basePower_inspirationGetByName((char *)size,
							       (char *)list[i]);
			int same = stricmp(list[i], chosen->pchName) == 0;
			int selected = i == inspChoice;
			F32 rowHeight = INSP_CHOICE_HEIGHT * hudScale;
			CBox box;

			if (!option)
				continue;

			BuildCBox(&box, sideLeft, y, sideWidth,
				  rowHeight - 4 * hudScale);
			if (!same && mouseCollision(&box)) {
				if (mouseMoved)
					inspChoice = i;
				if (mouseClickHit(&box, MS_LEFT)) {
					inspChoice = i;
					inspCombine(e);
					return;
				}
			}

			drawFlatFrame(
				PIX2, 8, sideLeft, y, BIND_Z + 2, sideWidth,
				rowHeight - 4 * hudScale, hudScale,
				selected ? FRAME_COLOR_HELD : FRAME_COLOR_DIM,
				selected ? 0x405878f0 : 0x101820e0);
			if (option->pchIconName) {
				AtlasTex *icon =
					atlasLoadTexture(option->pchIconName);
				F32 iconScale = (rowHeight - 12 * hudScale) /
						icon->width;

				display_sprite(icon, sideLeft + 6 * hudScale,
					       y + 4 * hudScale, BIND_Z + 4,
					       iconScale, iconScale,
					       same ? 0xffffff50 : CLR_WHITE);
			}
			drawText(&game_14, sideLeft + rowHeight + 6 * hudScale,
				 y + (rowHeight - 4 * hudScale) / 2, BIND_Z + 4,
				 hudScale,
				 same	    ? 0xffffff60
				 : selected ? FRAME_COLOR_HELD
					    : CLR_WHITE,
				 CENTER_Y, textStd(option->pchDisplayName));
			y += rowHeight;
		}
		prompts = combinePrompts;
		promptCount = ARRAY_SIZE(combinePrompts);
	} else if (chosen) {
		const char *help =
			chosen->pchDisplayHelp && chosen->pchDisplayHelp[0]
				? chosen->pchDisplayHelp
				: chosen->pchDisplayShortHelp;
		int count = inspCount(e, chosen);
		int canCombine = inspCanCombine(e, chosen);
		char line[160];
		F32 y = gridTop + 14 * hudScale;

		inspCombining = 0;
		drawText(&game_18, sideLeft, y, BIND_Z + 4, hudScale, CLR_WHITE,
			 CENTER_Y, textStd(chosen->pchDisplayName));
		y += 40 * hudScale;
		if (help)
			y += drawWrapped(&game_12, textStd(help), sideLeft, y,
					 BIND_Z + 4, sideWidth, 22 * hudScale,
					 hudScale, CLR_WHITE, 0, 8) *
			     22 * hudScale;
		y += 20 * hudScale;
		snprintf(line, sizeof(line),
			 count == 1 ? "You have 1 of these."
				    : "You have %d of these.",
			 count);
		line[sizeof(line) - 1] = 0;
		drawText(&game_12, sideLeft, y, BIND_Z + 4, hudScale,
			 0xffffffc0, CENTER_Y, line);
		y += 26 * hudScale;
		drawText(&game_12, sideLeft, y, BIND_Z + 4, hudScale,
			 canCombine ? 0x80ff80ff : 0xffffffa0, CENTER_Y,
			 canCombine ? "Three of them can be combined into "
				      "another of the same size."
				    : "Combining takes three of the same.");
		prompts = canCombine ? usePrompts : plainPrompts;
		promptCount = canCombine ? ARRAY_SIZE(usePrompts)
					 : ARRAY_SIZE(plainPrompts);
	} else {
		inspCombining = 0;
		drawText(&game_14, sideLeft, gridTop + 14 * hudScale,
			 BIND_Z + 4, hudScale, 0xffffffa0, CENTER_Y,
			 "An empty slot.");
		prompts = emptyPrompts;
		promptCount = ARRAY_SIZE(emptyPrompts);
	}

	drawPrompts(prompts, promptCount, left + panelWidth / 2,
		    top + panelHeight - 36 * hudScale, BIND_Z + 6, hudScale);
}
