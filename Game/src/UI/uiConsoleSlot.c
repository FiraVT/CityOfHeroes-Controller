/*
 * The console Enhancements screen: slotting and unslotting enhancements.
 */
#include "UI/uiConsoleHudInternal.h"

static int slotFits(Entity *e, int row, int column);
static int unslotterCount(Entity *e);

int slotScreenUp(void)
{
	// A question from the game about a placement has to be answered first,
	// and it draws under this screen.
	return slotOpen && consoleHudActive() &&
	       window_getMode(WDW_DIALOG) != WINDOW_DISPLAYING;
}

static void gatherSlotScreen(Entity *e)
{
	int i, j;

	slotInvCount = 0;
	for (i = 0; i < e->pchar->iNumBoostSlots && i < CHAR_BOOST_MAX; i++) {
		if (e->pchar->aBoosts[i])
			slotInv[slotInvCount++] = i;
	}

	slotRowCount = 0;
	for (i = 0; i < eaSize(&e->pchar->ppPowerSets); i++) {
		PowerSet *set = e->pchar->ppPowerSets[i];

		if (!powerset_ShowInInventory(set))
			continue;

		for (j = 0;
		     j < eaSize(&set->ppPowers) && slotRowCount < SLOT_MAX_ROWS;
		     j++) {
			Power *power = set->ppPowers[j];

			if (power && power_ShowInInventory(e->pchar, power) &&
			    eaSize(&power->ppBoosts) > 0) {
				slotRows[slotRowCount].set = i;
				slotRows[slotRowCount].power = j;
				slotRows[slotRowCount].slots =
					eaSize(&power->ppBoosts);
				slotRowCount++;
			}
		}
	}

	if (slotInvSelection >= slotInvCount)
		slotInvSelection = slotInvCount > 0 ? slotInvCount - 1 : 0;
	if (slotInvCount == 0)
		slotChoosing = 0;
}

static int slotFilled(Entity *e, int row, int column)
{
	Power *power;

	if (row < 0 || row >= slotRowCount || column < 0 ||
	    column >= slotRows[row].slots)
		return 0;
	power = e->pchar->ppPowerSets[slotRows[row].set]
			->ppPowers[slotRows[row].power];
	return power && column < eaSize(&power->ppBoosts) &&
	       power->ppBoosts[column] != NULL;
}

// A slot the D-pad can land on right now: one that takes the chosen
// enhancement, or one to empty.
static int slotTarget(Entity *e, int row, int column)
{
	return slotUnslotting ? slotFilled(e, row, column)
			      : slotFits(e, row, column);
}

static int slotPicking(void)
{
	return slotChoosing || slotUnslotting;
}

static int slotFits(Entity *e, int row, int column)
{
	return slotInvCount > 0 && row >= 0 && row < slotRowCount &&
	       column >= 0 && column < slotRows[row].slots &&
	       spec_CanPlace(e->pchar, slotRows[row].set, slotRows[row].power,
			     column, slotInv[slotInvSelection]);
}

// The fitting slot in a row closest to a column, or -1 if nothing in the row
// takes this enhancement.
static int nearestFit(Entity *e, int row, int column)
{
	int distance, best = -1;

	for (distance = 0; distance < 6 && best < 0; distance++) {
		if (slotTarget(e, row, column - distance))
			best = column - distance;
		else if (slotTarget(e, row, column + distance))
			best = column + distance;
	}
	return best;
}

static void stepSlotRow(Entity *e, int direction)
{
	int row;

	for (row = slotRow + direction; row >= 0 && row < slotRowCount;
	     row += direction) {
		int column = nearestFit(e, row, slotColumn);

		if (column >= 0) {
			slotRow = row;
			slotColumn = column;
			return;
		}
	}
}

static void stepSlotColumn(Entity *e, int direction)
{
	int column;

	for (column = slotColumn + direction;
	     column >= 0 && column < slotRows[slotRow].slots;
	     column += direction) {
		if (slotTarget(e, slotRow, column)) {
			slotColumn = column;
			return;
		}
	}
}

static void startChoosingSlot(Entity *e)
{
	slotUnslotting = 0;
	slotRow = -1;
	slotColumn = 0;
	stepSlotRow(e, 1);
	if (slotRow >= 0) {
		slotChoosing = 1;
		sndPlay("Grab4", SOUND_GAME);
	} else {
		slotRow = 0;
		addSystemChatMsg(
			"None of your powers can take that enhancement.",
			INFO_USER_ERROR, 0);
	}
}

static void startUnslotting(Entity *e)
{
	slotChoosing = 0;
	slotUnslotting = 1;
	slotRow = -1;
	slotColumn = 0;
	stepSlotRow(e, 1);
	if (slotRow >= 0) {
		sndPlay("Grab4", SOUND_GAME);
	} else {
		slotUnslotting = 0;
		slotRow = 0;
		addSystemChatMsg(
			"None of your powers has an enhancement to take out.",
			INFO_USER_ERROR, 0);
	}
}

static int inventoryHasRoom(Entity *e)
{
	int i;

	for (i = 0; i < e->pchar->iNumBoostSlots && i < CHAR_BOOST_MAX; i++) {
		if (!e->pchar->aBoosts[i])
			return 1;
	}
	return 0;
}

static void unslotNow(Entity *e)
{
	slotAsking = SLOT_ASK_NONE;
	if (!slotFilled(e, slotRow, slotColumn))
		return;

	switch (combo_unslotToInventory(slotRows[slotRow].set,
					slotRows[slotRow].power, slotColumn,
					0)) {
	case UNSLOT_STARTED:
		slotUnslotting = 0;
		break;
	case UNSLOT_NO_UNSLOTTER:
		addSystemChatMsg("Taking an enhancement out uses an "
				 "Enhancement Unslotter, and you have none.",
				 INFO_USER_ERROR, 0);
		break;
	case UNSLOT_INVENTORY_FULL:
		addSystemChatMsg("Your enhancement inventory is full.",
				 INFO_USER_ERROR, 0);
		break;
	}
}

static void unslotChosen(Entity *e)
{
	if (!slotFilled(e, slotRow, slotColumn))
		return;

	if (unslotterCount(e) <= 0)
		addSystemChatMsg("Taking an enhancement out uses an "
				 "Enhancement Unslotter, and you have none.",
				 INFO_USER_ERROR, 0);
	else if (!inventoryHasRoom(e))
		addSystemChatMsg("Your enhancement inventory is full.",
				 INFO_USER_ERROR, 0);
	else if (optionGet(kUO_HideUnslotPrompt))
		unslotNow(e);
	else {
		slotAsking = SLOT_ASK_UNSLOT;
		sndPlay("select", SOUND_GAME);
	}
}

static int unslotterCount(Entity *e)
{
	const SalvageItem *item = salvage_GetItem("S_EnhancementUnslotter");

	return item ? character_SalvageCount(e->pchar, item) : 0;
}

static void placeNow(Entity *e)
{
	slotAsking = SLOT_ASK_NONE;
	if (!slotFits(e, slotRow, slotColumn))
		return;

	spec_PlaceFromInventoryNow(
		slotRows[slotRow].set, slotRows[slotRow].power, slotColumn,
		slotInv[slotInvSelection], 0, 0, BIND_Z + 20);
	slotChoosing = 0;
}

static void placeChosenSlot(Entity *e)
{
	int question;

	if (!slotFits(e, slotRow, slotColumn))
		return;

	question = spec_PlaceQuestion(slotRows[slotRow].set,
				      slotRows[slotRow].power, slotColumn,
				      slotInv[slotInvSelection]);
	if (question == PLACE_NO_QUESTION) {
		placeNow(e);
	} else if (question != PLACE_INVALID) {
		slotAsking = SLOT_ASK_PLACE;
		slotAskKind = question;
		sndPlay("select", SOUND_GAME);
	}
}

int consoleSlotIsOpen(void)
{
	return slotScreenUp();
}

void consoleSlotToggle(void)
{
	char standardManage[] = "manage";

	if (!consoleHudActive()) {
		cmdParse(standardManage);
		return;
	}

	hudEditDiscard();
	menuOpen = 0;
	bindOpen = 0;
	incOpen = 0;
	invOpen = 0;
	if (navOpen)
		navClose();
	slotOpen = !slotOpen;
	slotChoosing = 0;
	slotUnslotting = 0;
	slotAsking = SLOT_ASK_NONE;
}

void slotPress(int modifiers, int button)
{
	Entity *e = playerPtr();

	if (!e || !e->pchar || modifiers)
		return;

	gatherSlotScreen(e);

	if (slotAsking) {
		switch (button) {
		case GP_A:
			if (slotAsking == SLOT_ASK_PLACE)
				placeNow(e);
			else
				unslotNow(e);
			break;
		case GP_B:
			slotAsking = SLOT_ASK_NONE;
			break;
		case GP_START:
			slotAsking = SLOT_ASK_NONE;
			slotOpen = 0;
			break;
		}
		return;
	}

	if (slotPicking()) {
		switch (button) {
		case GP_DPAD_UP:
			stepSlotRow(e, -1);
			break;
		case GP_DPAD_DOWN:
			stepSlotRow(e, 1);
			break;
		case GP_DPAD_LEFT:
			stepSlotColumn(e, -1);
			break;
		case GP_DPAD_RIGHT:
			stepSlotColumn(e, 1);
			break;
		case GP_A:
			if (slotUnslotting)
				unslotChosen(e);
			else
				placeChosenSlot(e);
			break;
		case GP_B:
			slotChoosing = 0;
			slotUnslotting = 0;
			break;
		case GP_START:
			slotOpen = 0;
			break;
		}
		return;
	}

	switch (button) {
	case GP_DPAD_LEFT:
		if (slotInvSelection > 0)
			slotInvSelection--;
		break;
	case GP_DPAD_RIGHT:
		if (slotInvSelection + 1 < slotInvCount)
			slotInvSelection++;
		break;
	case GP_DPAD_UP:
		if (slotInvSelection >= SLOT_INV_COLUMNS)
			slotInvSelection -= SLOT_INV_COLUMNS;
		break;
	case GP_DPAD_DOWN:
		if (slotInvSelection + SLOT_INV_COLUMNS < slotInvCount)
			slotInvSelection += SLOT_INV_COLUMNS;
		else if (slotInvSelection / SLOT_INV_COLUMNS <
			 (slotInvCount - 1) / SLOT_INV_COLUMNS)
			slotInvSelection = slotInvCount - 1;
		break;
	case GP_A:
		if (slotInvCount > 0)
			startChoosingSlot(e);
		break;
	case GP_Y:
		startUnslotting(e);
		break;
	case GP_X: {
		// Combining stays on the full screen.
		char manage[] = "manage";

		slotOpen = 0;
		cmdParse(manage);
		break;
	}
	case GP_B:
	case GP_START:
		slotOpen = 0;
		break;
	}
}

static void drawSlotInventory(Entity *e, F32 left, F32 top, F32 hudScale,
			      int mouseMoved)
{
	F32 step = SLOT_POG_STEP * hudScale;
	int i;

	for (i = 0; i < slotInvCount; i++) {
		F32 x = left + (i % SLOT_INV_COLUMNS) * step + step / 2;
		F32 y = top + (i / SLOT_INV_COLUMNS) * step + step / 2;
		int selected = i == slotInvSelection;
		CBox box;

		drawMenuEnhancement(e->pchar->aBoosts[slotInv[i]], x, y,
				    BIND_Z + 4, 0.85f * hudScale,
				    slotPicking() && !(selected && slotChoosing)
					    ? kBoostState_Disabled
					    : kBoostState_Normal,
				    0, true, MENU_GAME, 0);

		if (selected && !slotUnslotting)
			drawFlatFrame(PIX2, 8, x - step / 2 + 3 * hudScale,
				      y - step / 2 + 3 * hudScale, BIND_Z + 3,
				      step - 6 * hudScale, step - 6 * hudScale,
				      hudScale, FRAME_COLOR_HELD, 0x405878f0);

		BuildCBox(&box, x - step / 2, y - step / 2, step, step);
		if (!slotPicking() && !slotAsking && mouseCollision(&box)) {
			if (mouseMoved)
				slotInvSelection = i;
			if (mouseClickHit(&box, MS_LEFT)) {
				slotInvSelection = i;
				startChoosingSlot(e);
			}
		}
	}
}

static void drawSlotPowers(Entity *e, F32 left, F32 top, F32 width,
			   F32 hudScale, int mouseMoved)
{
	F32 rowStep = SLOT_ROW_STEP * hudScale;
	F32 pogStep = SLOT_ROW_POG_STEP * hudScale;
	int row;

	if (slotPicking()) {
		if (slotRow < slotTopRow)
			slotTopRow = slotRow;
		else if (slotRow >= slotTopRow + SLOT_VISIBLE_ROWS)
			slotTopRow = slotRow - SLOT_VISIBLE_ROWS + 1;
	}
	if (slotTopRow > slotRowCount - SLOT_VISIBLE_ROWS)
		slotTopRow = MAX(0, slotRowCount - SLOT_VISIBLE_ROWS);

	for (row = slotTopRow;
	     row < slotRowCount && row < slotTopRow + SLOT_VISIBLE_ROWS;
	     row++) {
		Power *power = e->pchar->ppPowerSets[slotRows[row].set]
				       ->ppPowers[slotRows[row].power];
		AtlasTex *icon = basepower_GetIcon(power->ppowBase);
		int place = row - slotTopRow;
		F32 rowLeft =
			left + (place / SLOT_ROWS_PER_COLUMN) * (width / 2);
		F32 y = top + (place % SLOT_ROWS_PER_COLUMN) * rowStep;
		F32 iconSize = 40 * hudScale;
		int anyFit = 0;
		int column;

		display_sprite(icon, rowLeft, y + (rowStep - iconSize) / 2,
			       BIND_Z + 4, iconSize / icon->width,
			       iconSize / icon->width, CLR_WHITE);
		drawText(&game_12, rowLeft + iconSize + 12 * hudScale,
			 y + 14 * hudScale, BIND_Z + 4, hudScale, CLR_WHITE,
			 CENTER_Y, textStd(power->ppowBase->pchDisplayName));

		for (column = 0; column < slotRows[row].slots; column++) {
			F32 x = rowLeft + iconSize + 12 * hudScale +
				column * pogStep + pogStep / 2;
			F32 pogY = y + 40 * hudScale;
			int fits = slotPicking() && slotTarget(e, row, column);
			int selected = slotPicking() && row == slotRow &&
				       column == slotColumn;
			CBox box;

			anyFit |= fits;
			drawMenuEnhancement(power->ppBoosts[column], x, pogY,
					    BIND_Z + 4, 0.6f * hudScale,
					    slotPicking() && !fits
						    ? kBoostState_Disabled
						    : kBoostState_Normal,
					    0, false, MENU_GAME, 0);

			if (selected)
				drawFlatFrame(
					PIX2, 8, x - pogStep / 2 + 2 * hudScale,
					pogY - pogStep / 2 + 2 * hudScale,
					BIND_Z + 3, pogStep - 4 * hudScale,
					pogStep - 4 * hudScale, hudScale,
					FRAME_COLOR_HELD, 0x405878f0);

			BuildCBox(&box, x - pogStep / 2, pogY - pogStep / 2,
				  pogStep, pogStep);
			if (fits && !slotAsking && mouseCollision(&box)) {
				if (mouseMoved) {
					slotRow = row;
					slotColumn = column;
				}
				if (mouseClickHit(&box, MS_LEFT)) {
					slotRow = row;
					slotColumn = column;
					if (slotUnslotting)
						unslotChosen(e);
					else
						placeChosenSlot(e);
				}
			}
		}

		if (slotChoosing && !anyFit)
			drawText(&game_12, rowLeft + width / 2 - 130 * hudScale,
				 y + 14 * hudScale, BIND_Z + 4, hudScale,
				 0xffffff60, CENTER_Y, "Can't take it");
		else if (slotUnslotting && !anyFit)
			drawText(&game_12, rowLeft + width / 2 - 130 * hudScale,
				 y + 14 * hudScale, BIND_Z + 4, hudScale,
				 0xffffff60, CENTER_Y, "Nothing slotted");
	}

	if (slotRowCount > SLOT_VISIBLE_ROWS) {
		char more[48];

		snprintf(more, sizeof(more), "Powers %d to %d of %d",
			 slotTopRow + 1,
			 MIN(slotRowCount, slotTopRow + SLOT_VISIBLE_ROWS),
			 slotRowCount);
		drawText(&game_12, left,
			 top + SLOT_ROWS_PER_COLUMN * rowStep + 10 * hudScale,
			 BIND_Z + 4, hudScale, 0xffffffa0, CENTER_Y, more);
	}
}

// The question before placing or taking out, drawn over the middle of the
// screen with the screen still up.
static void drawSlotQuestion(Entity *e, int width, int height, F32 centerX,
			     F32 centerY, F32 hudScale)
{
	static const Prompt askPrompts[] = { { GP_A, "Yes" }, { GP_B, "No" } };
	Power *power = e->pchar->ppPowerSets[slotRows[slotRow].set]
			       ->ppPowers[slotRows[slotRow].power];
	const char *powerName = textStd(power->ppowBase->pchDisplayName);
	F32 boxWidth = 620 * hudScale;
	F32 boxHeight = 170 * hudScale;
	F32 left = centerX - boxWidth / 2;
	F32 top = centerY - boxHeight / 2;
	char title[160];
	char detail[160];
	const Boost *boost;

	if (slotAsking == SLOT_ASK_PLACE) {
		const Boost *there = power->ppBoosts[slotColumn];

		boost = e->pchar->aBoosts[slotInv[slotInvSelection]];
		snprintf(title, sizeof(title), "Slot %s into %s?",
			 textStd(boost->ppowBase->pchDisplayName), powerName);
		if (slotAskKind == PLACE_REPLACE_WORSE)
			snprintf(detail, sizeof(detail),
				 "The %s already there is as good or better, "
				 "and it will be destroyed.",
				 textStd(there->ppowBase->pchDisplayName));
		else if (slotAskKind == PLACE_REPLACE)
			snprintf(detail, sizeof(detail),
				 "It replaces the %s already there, which will "
				 "be destroyed.",
				 textStd(there->ppowBase->pchDisplayName));
		else
			snprintf(detail, sizeof(detail),
				 "Once placed, it is locked into the power.");
	} else {
		boost = power->ppBoosts[slotColumn];
		snprintf(title, sizeof(title), "Take %s out of %s?",
			 boost ? textStd(boost->ppowBase->pchDisplayName)
			       : "it",
			 powerName);
		snprintf(detail, sizeof(detail),
			 "This uses one Enhancement Unslotter. You have %d.",
			 unslotterCount(e));
	}

	display_sprite(white_tex_atlas, 0, 0, BIND_Z + 10,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000060);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z + 11, boxWidth, boxHeight,
		      hudScale, FRAME_COLOR_HELD, 0x101820f8);
	drawText(&game_14, centerX, top + 42 * hudScale, BIND_Z + 14, hudScale,
		 CLR_WHITE, CENTER_X | CENTER_Y, title);
	drawText(&game_12, centerX, top + 78 * hudScale, BIND_Z + 14, hudScale,
		 0xffffffc0, CENTER_X | CENTER_Y, detail);
	drawPrompts(askPrompts, ARRAY_SIZE(askPrompts), centerX,
		    top + boxHeight - 36 * hudScale, BIND_Z + 14, hudScale);
}

void drawSlotScreen(Entity *e, int width, int height, F32 hudScale)
{
	static const Prompt pickPrompts[] = { { GP_A, "Choose" },
					      { GP_Y, "Unslot" },
					      { GP_X,
						"Combine on the full screen" },
					      { GP_B, "Close" } };
	static const Prompt placePrompts[] = { { GP_A, "Slot it" },
					       { GP_B, "Back" } };
	static const Prompt unslotPrompts[] = { { GP_A, "Take it out" },
						{ GP_B, "Back" } };
	F32 panelWidth = SCREEN_PANEL_WIDTH * hudScale;
	F32 panelHeight = 760 * hudScale;
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 invLeft = left + 36 * hudScale;
	F32 invTop = top + 110 * hudScale;
	F32 invWidth = SLOT_POG_STEP * SLOT_INV_COLUMNS * hudScale;
	F32 powersLeft = invLeft + invWidth + 70 * hudScale;
	F32 powersWidth = left + panelWidth - 36 * hudScale - powersLeft;
	int mouseMoved =
		gMouseInpCur.x != slotMouseX || gMouseInpCur.y != slotMouseY;

	slotMouseX = gMouseInpCur.x;
	slotMouseY = gMouseInpCur.y;

	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	gatherSlotScreen(e);

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000090);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	drawText(&game_18, left + panelWidth / 2, top + 36 * hudScale,
		 BIND_Z + 4, hudScale, CLR_WHITE, CENTER_X | CENTER_Y,
		 "Enhancements");
	drawText(&game_14, invLeft, invTop - 26 * hudScale, BIND_Z + 4,
		 hudScale, CLR_WHITE, CENTER_Y, "Inventory");
	drawText(&game_14, powersLeft, invTop - 26 * hudScale, BIND_Z + 4,
		 hudScale, CLR_WHITE, CENTER_Y, "Powers");

	if (slotUnslotting) {
		Power *power = e->pchar->ppPowerSets[slotRows[slotRow].set]
				       ->ppPowers[slotRows[slotRow].power];
		const Boost *boost = slotFilled(e, slotRow, slotColumn)
					     ? power->ppBoosts[slotColumn]
					     : NULL;
		int invRows = (slotInvCount + SLOT_INV_COLUMNS - 1) /
			      SLOT_INV_COLUMNS;
		F32 nameY = invTop + invRows * SLOT_POG_STEP * hudScale +
			    30 * hudScale;
		char unslotters[48];

		if (slotInvCount > 0)
			drawSlotInventory(e, invLeft, invTop, hudScale,
					  mouseMoved);
		else
			nameY = invTop + 20 * hudScale;

		snprintf(unslotters, sizeof(unslotters), "Unslotters: %d",
			 unslotterCount(e));
		if (boost)
			drawText(&game_14, invLeft, nameY, BIND_Z + 4, hudScale,
				 FRAME_COLOR_HELD, CENTER_Y,
				 textStd(boost->ppowBase->pchDisplayName));
		drawText(&game_12, invLeft, nameY + 26 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_Y,
			 "Choose one to take out");
		drawText(&game_12, invLeft, nameY + 50 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_Y, unslotters);
	} else if (slotInvCount > 0) {
		const Boost *boost =
			e->pchar->aBoosts[slotInv[slotInvSelection]];
		int invRows = (slotInvCount + SLOT_INV_COLUMNS - 1) /
			      SLOT_INV_COLUMNS;
		F32 nameY = invTop + invRows * SLOT_POG_STEP * hudScale +
			    30 * hudScale;
		char level[32];

		drawSlotInventory(e, invLeft, invTop, hudScale, mouseMoved);

		boost = e->pchar->aBoosts[slotInv[slotInvSelection]];
		snprintf(level, sizeof(level),
			 boost->iNumCombines ? "Level %d  +%d" : "Level %d",
			 boost->iLevel + 1, boost->iNumCombines);
		drawText(&game_14, invLeft, nameY, BIND_Z + 4, hudScale,
			 slotChoosing ? FRAME_COLOR_HELD : CLR_WHITE, CENTER_Y,
			 textStd(boost->ppowBase->pchDisplayName));
		drawText(&game_12, invLeft, nameY + 26 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_Y,
			 slotChoosing ? "Choose a slot for it" : level);
	} else {
		drawText(&game_14, invLeft, invTop + 20 * hudScale, BIND_Z + 4,
			 hudScale, CLR_WHITE, CENTER_Y, "No enhancements yet");
		drawText(&game_12, invLeft, invTop + 46 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_Y,
			 "Defeated foes and stores have them.");
	}

	drawSlotPowers(e, powersLeft, invTop, powersWidth, hudScale,
		       mouseMoved);

	if (slotAsking && !(slotAsking == SLOT_ASK_PLACE
				    ? slotFits(e, slotRow, slotColumn)
				    : slotFilled(e, slotRow, slotColumn)))
		slotAsking = SLOT_ASK_NONE;
	if (slotAsking)
		drawSlotQuestion(e, width, height, left + panelWidth / 2,
				 top + panelHeight / 2, hudScale);

	if (slotChoosing)
		drawPrompts(placePrompts, ARRAY_SIZE(placePrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 30 * hudScale, BIND_Z + 4,
			    hudScale);
	else if (slotUnslotting)
		drawPrompts(unslotPrompts, ARRAY_SIZE(unslotPrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 30 * hudScale, BIND_Z + 4,
			    hudScale);
	else
		drawPrompts(pickPrompts, ARRAY_SIZE(pickPrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 30 * hudScale, BIND_Z + 4,
			    hudScale);

	// Nothing behind the screen hears this click, including the world.
	collisions_off_for_rest_of_frame = 1;
}

const char *const incTabNames[INC_TAB_COUNT] = { "Equip", "Create", "Convert" };
const char *const incRarityNames[INC_RARITY_ROWS] = { "Very Rare", "Rare",
						      "Uncommon", "Common" };
const int incSlotRows[INC_SLOT_ROWS][2] = {
	{ kIncarnateSlot_Omega, -1 },
	{ kIncarnateSlot_Mind, kIncarnateSlot_Vitae },
	{ kIncarnateSlot_Hybrid, kIncarnateSlot_Genesis },
	{ kIncarnateSlot_Lore, kIncarnateSlot_Destiny },
	{ kIncarnateSlot_Judgement, kIncarnateSlot_Interface },
	{ kIncarnateSlot_Alpha, -1 },
};

int incTab;
int incAsking;
const DetailRecipe *incAskRecipe;

int incSlotRow = INC_SLOT_ROWS - 1;
int incSlotColumn;
int incInList;
int incListSelection;
int incListTop;
const BasePower *incOwned[INC_MAX_OWNED];
int incOwnedCount;

IncEntry *incEntries;
int incEntryCount;
Entity *incEntriesFor;

// Create: column 0 is the slots, 1 the trees of a slot, 2 the abilities of a
// tree, 3 the recipes for one.
int incCreateColumn;
int incCreateSlot;
int incCreateTree;
int incNodeSelection;
int incRouteSelection;
const char *incCreateSlots[INC_MAX_NAMES];
int incCreateSlotCount;
const char *incCreateTrees[INC_MAX_NAMES];
int incCreateTreeCount;
IncNode incNodes[INC_MAX_NODES];
int incNodeCount;
int incRowCounts[INC_RARITY_ROWS];
// For each ability, the abilities in the tree that any of its recipes uses up,
// one bit each.
U32 incFeeds[INC_MAX_NODES];
const DetailRecipe *incRoutes[INC_MAX_RECIPES];
int incRouteCount;
int incReadyRoutes;

// Convert: column 0 is what is converted, 1 how, 2 the recipes.
int incConvertColumn;
int incConvertSource;
int incConvertKind;
int incConvertSelection;
const char *incConvertSources[INC_MAX_NAMES];
int incConvertSourceCount;
const char *incConvertKinds[INC_MAX_NAMES];
int incConvertKindCount;
const DetailRecipe *incConvertRecipes[INC_MAX_RECIPES];
int incConvertRecipeCount;
