/*
 * The console Enhancements screen: slotting and unslotting enhancements.
 */
#include "UI/uiConsoleHudInternal.h"
#include "UI/uiPowerInfo.h"

static int slotFits(Entity *e, int row, int column);
static int unslotterCount(Entity *e);

// Looking over the powers with no enhancement in hand, to read what each one
// does now.
static int slotViewing;
// How far a power's numbers are scrolled by the right stick, and how far they
// can go this frame; 0 when they fit.
static F32 slotNumbersScroll;
static F32 slotNumbersMost;
// The first inventory row on screen.
static int slotInvFirstRow;

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
// enhancement, one to empty, or any slot while looking the powers over.
static int slotTarget(Entity *e, int row, int column)
{
	if (slotViewing)
		return row >= 0 && row < slotRowCount && column >= 0 &&
		       column < slotRows[row].slots;
	return slotUnslotting ? slotFilled(e, row, column)
			      : slotFits(e, row, column);
}

static int slotPicking(void)
{
	return slotChoosing || slotUnslotting || slotViewing;
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

// Past the end of a power's slots, left and right cross to the power level with
// it in the other column, or the nearest one there that can take the
// enhancement.
static void crossSlotColumn(Entity *e, int direction)
{
	int place = slotRow - slotTopRow;
	int first, last, target, distance;

	if (place < 0 || (direction > 0) == (place >= SLOT_ROWS_PER_COLUMN))
		return;

	first = slotTopRow + (direction > 0 ? SLOT_ROWS_PER_COLUMN : 0);
	last = MIN(first + SLOT_ROWS_PER_COLUMN - 1, slotRowCount - 1);
	target = MIN(slotRow + direction * SLOT_ROWS_PER_COLUMN, last);

	for (distance = 0; first <= last && distance < SLOT_ROWS_PER_COLUMN;
	     distance++) {
		int tries[2];
		int i;

		tries[0] = target - distance;
		tries[1] = target + distance;
		for (i = 0; i < 2; i++) {
			int row = tries[i];
			int column;

			if (row < first || row > last)
				continue;
			column = nearestFit(e, row,
					    direction > 0 ? 0
							  : slotRows[row].slots - 1);
			if (column >= 0) {
				slotRow = row;
				slotColumn = column;
				return;
			}
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
	crossSlotColumn(e, direction);
}

static void startChoosingSlot(Entity *e)
{
	slotUnslotting = 0;
	slotViewing = 0;
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

// Over to the powers with nothing in hand, starting at the top of the ones on
// screen.
static void startViewing(void)
{
	if (slotRowCount <= 0)
		return;
	slotChoosing = 0;
	slotUnslotting = 0;
	slotViewing = 1;
	slotRow = MIN(slotTopRow, slotRowCount - 1);
	slotColumn = 0;
	sndPlay("select", SOUND_GAME);
}

static void startUnslotting(Entity *e)
{
	slotChoosing = 0;
	slotViewing = 0;
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
	slotViewing = 0;
	slotAsking = SLOT_ASK_NONE;
}

int slotLook(F32 y, F32 seconds)
{
	if (slotNumbersMost <= 0)
		return 0;
	slotNumbersScroll += y * seconds * 600.0f;
	if (slotNumbersScroll < 0)
		slotNumbersScroll = 0;
	if (slotNumbersScroll > slotNumbersMost)
		slotNumbersScroll = slotNumbersMost;
	return 1;
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
			// Off the left edge of the powers, back to the
			// inventory.
			if (slotViewing && slotColumn == 0 &&
			    slotRow - slotTopRow < SLOT_ROWS_PER_COLUMN)
				slotViewing = 0;
			else
				stepSlotColumn(e, -1);
			break;
		case GP_DPAD_RIGHT:
			stepSlotColumn(e, 1);
			break;
		case GP_A:
			if (slotViewing)
				break;
			if (slotUnslotting)
				unslotChosen(e);
			else
				placeChosenSlot(e);
			break;
		case GP_Y:
			if (slotViewing)
				unslotChosen(e);
			break;
		case GP_LB:
		case GP_B:
			slotChoosing = 0;
			slotUnslotting = 0;
			slotViewing = 0;
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
		// Past the inventory's right edge, over to the powers.
		if (slotInvCount == 0 ||
		    slotInvSelection % SLOT_INV_COLUMNS ==
			    SLOT_INV_COLUMNS - 1 ||
		    slotInvSelection + 1 >= slotInvCount)
			startViewing();
		else
			slotInvSelection++;
		break;
	case GP_RB:
		startViewing();
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

// Rows the inventory takes on screen: all of them, up to SLOT_INV_ROWS.
static int slotInvShownRows(void)
{
	int rows = (slotInvCount + SLOT_INV_COLUMNS - 1) / SLOT_INV_COLUMNS;

	return MIN(rows, SLOT_INV_ROWS);
}

static void drawSlotInventory(Entity *e, F32 left, F32 top, F32 hudScale,
			      int mouseMoved)
{
	F32 step = SLOT_POG_STEP * hudScale;
	int rows = (slotInvCount + SLOT_INV_COLUMNS - 1) / SLOT_INV_COLUMNS;
	int selRow = slotInvSelection / SLOT_INV_COLUMNS;
	int end;
	int i;

	// Keep the selected row in view.
	if (selRow < slotInvFirstRow)
		slotInvFirstRow = selRow;
	if (selRow >= slotInvFirstRow + SLOT_INV_ROWS)
		slotInvFirstRow = selRow - SLOT_INV_ROWS + 1;
	if (slotInvFirstRow > rows - SLOT_INV_ROWS)
		slotInvFirstRow = rows - SLOT_INV_ROWS;
	if (slotInvFirstRow < 0)
		slotInvFirstRow = 0;

	if (slotInvFirstRow > 0)
		drawText(&game_12, left + SLOT_INV_COLUMNS * step / 2,
			 top - 26 * hudScale, BIND_Z + 4, hudScale, 0xffffffc0,
			 CENTER_X | CENTER_Y, "More above");
	if (slotInvFirstRow + SLOT_INV_ROWS < rows)
		drawText(&game_12, left + SLOT_INV_COLUMNS * step / 2,
			 top + SLOT_INV_ROWS * step + 4 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_X | CENTER_Y,
			 "More below");

	end = MIN(slotInvCount, (slotInvFirstRow + SLOT_INV_ROWS) *
					SLOT_INV_COLUMNS);
	for (i = slotInvFirstRow * SLOT_INV_COLUMNS; i < end; i++) {
		F32 x = left + (i % SLOT_INV_COLUMNS) * step + step / 2;
		F32 y = top + (i / SLOT_INV_COLUMNS - slotInvFirstRow) * step +
			step / 2;
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

#define SLOT_MAX_TOTALS 24
#define SLOT_CAP_SLACK 0.005f

// A power some of whose enhancement is wasted to diminishing returns.
static int powerOverCap(const Power *power)
{
	SpecTotal totals[SLOT_MAX_TOTALS];
	int count = spec_PowerTotals(power, NULL, 0, totals, SLOT_MAX_TOTALS);
	int i;

	for (i = 0; i < count; i++) {
		if (totals[i].lost > SLOT_CAP_SLACK)
			return 1;
	}
	return 0;
}

// The value a power has now for the attribute name, or 0 if it has none.
static float totalBefore(const SpecTotal *now, int count, const char *name)
{
	int i;

	for (i = 0; i < count; i++) {
		if (now[i].name == name)
			return now[i].value;
	}
	return 0;
}

// A power's enhancement totals, one attribute a line. With boost, each line
// reads what it is now and what it would be with boost in slot column. Lines
// past the cap are yellow and say what is wasted. Returns the height used.
static F32 drawPowerTotals(const Power *power, const Boost *boost, int column,
			   F32 x, F32 y, F32 bottom, F32 hudScale)
{
	SpecTotal now[SLOT_MAX_TOTALS], after[SLOT_MAX_TOTALS];
	int nowCount = spec_PowerTotals(power, NULL, 0, now, SLOT_MAX_TOTALS);
	int afterCount = boost ? spec_PowerTotals(power, boost, column, after,
						  SLOT_MAX_TOTALS)
			       : 0;
	const SpecTotal *shown = boost ? after : now;
	int shownCount = boost ? afterCount : nowCount;
	F32 lineHeight = 22 * hudScale;
	F32 start = y;
	int i;

	if (y + lineHeight > bottom)
		return 0;
	drawText(&game_14, x, y, BIND_Z + 4, hudScale, CLR_WHITE, CENTER_Y,
		 textStd(power->ppowBase->pchDisplayName));
	y += 26 * hudScale;

	if (!shownCount && y + lineHeight <= bottom) {
		drawText(&game_12, x, y, BIND_Z + 4, hudScale, 0xffffffa0,
			 CENTER_Y, "Not enhanced yet");
		y += lineHeight;
	}

	for (i = 0; i < shownCount && y + lineHeight <= bottom; i++) {
		float before = totalBefore(now, nowCount, shown[i].name);
		int over = shown[i].lost > SLOT_CAP_SLACK;
		char line[160];

		if (boost && fabsf(before - shown[i].value) > 0.0005f)
			snprintf(line, sizeof(line), "%s: %+.1f%% then %+.1f%%",
				 textStd(shown[i].name), before * 100,
				 shown[i].value * 100);
		else
			snprintf(line, sizeof(line), "%s: %+.1f%%",
				 textStd(shown[i].name), shown[i].value * 100);
		if (over) {
			char wasted[48];

			snprintf(wasted, sizeof(wasted),
				 "  (%.1f%% over the cap)",
				 shown[i].lost * 100);
			strncat(line, wasted, sizeof(line) - strlen(line) - 1);
		}
		drawText(&game_12, x, y, BIND_Z + 4, hudScale,
			 over ? 0xffd060ff : CLR_WHITE, CENTER_Y, line);
		y += lineHeight;
	}

	// Anything the new one would take away entirely, like a different
	// enhancement it replaces.
	for (i = 0; boost && i < nowCount && y + lineHeight <= bottom; i++) {
		char line[160];
		int j, found = 0;

		for (j = 0; j < afterCount; j++)
			found |= after[j].name == now[i].name;
		if (found)
			continue;
		snprintf(line, sizeof(line), "%s: %+.1f%% then none",
			 textStd(now[i].name), now[i].value * 100);
		drawText(&game_12, x, y, BIND_Z + 4, hudScale, 0xff9080ff,
			 CENTER_Y, line);
		y += lineHeight;
	}
	return y - start;
}

// What an enhancement does, written under its name: its set and the levels the
// set comes in, then the game's own descriptions. Stops at bottom. Returns the
// height used.
static F32 drawBoostInfo(const Boost *boost, F32 x, F32 y, F32 width,
			 F32 bottom, F32 hudScale)
{
	const BasePower *base = boost ? boost->ppowBase : NULL;
	F32 lineHeight = 20 * hudScale;
	F32 start = y;
	int room;

	if (!base)
		return 0;

	if (base->pBoostSetMember && y + lineHeight <= bottom) {
		const BoostSet *set = base->pBoostSetMember;
		char line[160];

		if (set->iMaxLevel > set->iMinLevel)
			snprintf(line, sizeof(line), "%s set, levels %d to %d",
				 textStd(set->pchDisplayName),
				 set->iMinLevel + 1, set->iMaxLevel + 1);
		else
			snprintf(line, sizeof(line), "%s set",
				 textStd(set->pchDisplayName));
		drawText(&game_12, x, y, BIND_Z + 4, hudScale, 0x80d0ffff,
			 CENTER_Y, line);
		y += lineHeight + 4 * hudScale;
	}

	room = (int)((bottom - y) / lineHeight);
	if (base->pchDisplayShortHelp && base->pchDisplayShortHelp[0] &&
	    room > 0) {
		y += drawWrapped(&game_12, textStd(base->pchDisplayShortHelp),
				 x, y, BIND_Z + 4, width, lineHeight, hudScale,
				 CLR_WHITE, 0, room) *
		     lineHeight;
		y += 4 * hudScale;
	}

	// Some of the game's texts carry a placeholder for the number, like
	// Boost.Attrib.Damage.Scale, that nothing fills in. Those are left out:
	// the short line says the same, and the power's totals give the number.
	room = (int)((bottom - y) / lineHeight);
	if (base->pchDisplayHelp && base->pchDisplayHelp[0] && room > 0 &&
	    !(base->pchDisplayShortHelp &&
	      stricmp(base->pchDisplayShortHelp, base->pchDisplayHelp) == 0)) {
		const char *help = textStd(base->pchDisplayHelp);

		if (!strstriConst(help, "Attrib."))
			y += drawWrapped(&game_12, help, x, y, BIND_Z + 4,
					 width, lineHeight, hudScale,
					 0xffffffb0, 0, MIN(room, 6)) *
			     lineHeight;
	}
	return y - start;
}

// The game's own numbers for a power, under its totals: damage, healing,
// recharge and the rest, each enhanced with the unenhanced value in brackets.
// boost, if given, is counted in at column. When they run past bottom the
// right stick scrolls them.
static void drawPowerNumbers(const Power *power, const Boost *boost, int column,
			     F32 x, F32 y, F32 width, F32 bottom, F32 hudScale)
{
	static const Power *lastPower;
	static int lastColumn = -1;
	static const Boost *lastBoost;
	Boost temp;
	F32 room, full;

	if (power != lastPower || column != lastColumn || boost != lastBoost) {
		slotNumbersScroll = 0;
		lastPower = power;
		lastColumn = column;
		lastBoost = boost;
	}
	if (y + 60 * hudScale > bottom)
		return;

	drawText(&game_12, x, y, BIND_Z + 4, hudScale, 0xffffffa0, CENTER_Y,
		 boost ? "With it in. Unenhanced in brackets."
		       : "Unenhanced in brackets.");
	y += 14 * hudScale;
	room = bottom - y;

	if (boost) {
		temp = *boost;
		temp.idx = column;
	}
	full = powerInfoConsoleDraw(power, boost ? &temp : NULL, x, y,
				    BIND_Z + 4, width, room, hudScale,
				    slotNumbersScroll);

	if (full > room) {
		slotNumbersMost = full - room;
		if (slotNumbersScroll > slotNumbersMost)
			slotNumbersScroll = slotNumbersMost;
		drawText(&game_12, x, bottom + 16 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffa0, CENTER_Y,
			 slotNumbersScroll > 0 ? "Right stick scrolls"
					       : "Right stick down for more");
	} else {
		slotNumbersScroll = 0;
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
					// Looking the powers over only
					// moves the selection.
					if (slotViewing)
						continue;
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
		else if (powerOverCap(power))
			drawText(&game_12, rowLeft + width / 2 - 130 * hudScale,
				 y + 14 * hudScale, BIND_Z + 4, hudScale,
				 0xffd060ff, CENTER_Y, "Over the cap");
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
	static const Prompt viewPrompts[] = { { GP_Y, "Unslot" },
					      { GP_B, "Back to inventory" } };
	F32 panelWidth = SCREEN_PANEL_WIDTH * hudScale;
	F32 panelHeight = 760 * hudScale;
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 invLeft = left + 36 * hudScale;
	F32 invTop = top + 110 * hudScale;
	F32 invWidth = SLOT_POG_STEP * SLOT_INV_COLUMNS * hudScale;
	F32 powersLeft = invLeft + invWidth + 70 * hudScale;
	F32 powersWidth = left + panelWidth - 36 * hudScale - powersLeft;
	F32 infoBottom = top + panelHeight - 70 * hudScale;
	int mouseMoved =
		gMouseInpCur.x != slotMouseX || gMouseInpCur.y != slotMouseY;

	slotMouseX = gMouseInpCur.x;
	slotMouseY = gMouseInpCur.y;

	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	gatherSlotScreen(e);
	slotNumbersMost = 0;

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000090);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	drawText(&game_18, left + panelWidth / 2, top + 36 * hudScale,
		 BIND_Z + 4, hudScale, CLR_WHITE, CENTER_X | CENTER_Y,
		 "Enhancements");
	drawText(&game_14, invLeft, invTop - 26 * hudScale, BIND_Z + 4,
		 hudScale, CLR_WHITE, CENTER_Y,
		 slotViewing    ? "Power"
		 : slotChoosing ? "Enhancement"
				: "Inventory");
	drawText(&game_14, powersLeft, invTop - 26 * hudScale, BIND_Z + 4,
		 hudScale, CLR_WHITE, CENTER_Y, "Powers");

	if (slotViewing && slotRow >= 0 && slotRow < slotRowCount) {
		// Looking a power over: its totals, the slot the D-pad is on,
		// then the game's numbers.
		Power *power = e->pchar->ppPowerSets[slotRows[slotRow].set]
				       ->ppPowers[slotRows[slotRow].power];
		const Boost *held = slotFilled(e, slotRow, slotColumn)
					    ? power->ppBoosts[slotColumn]
					    : NULL;
		F32 y = invTop + 6 * hudScale;
		char slotLine[160];

		y += drawPowerTotals(power, NULL, 0, invLeft, y, infoBottom,
				     hudScale);
		if (held)
			snprintf(slotLine, sizeof(slotLine),
				 held->iNumCombines
					 ? "Slot %d: %s, level %d +%d"
					 : "Slot %d: %s, level %d",
				 slotColumn + 1,
				 textStd(held->ppowBase->pchDisplayName),
				 held->iLevel + 1, held->iNumCombines);
		else
			snprintf(slotLine, sizeof(slotLine), "Slot %d: empty",
				 slotColumn + 1);
		drawText(&game_12, invLeft, y + 4 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_Y, slotLine);
		drawPowerNumbers(power, NULL, 0, invLeft, y + 30 * hudScale,
				 invWidth, infoBottom, hudScale);
	} else if (slotUnslotting) {
		Power *power = e->pchar->ppPowerSets[slotRows[slotRow].set]
				       ->ppPowers[slotRows[slotRow].power];
		const Boost *boost = slotFilled(e, slotRow, slotColumn)
					     ? power->ppBoosts[slotColumn]
					     : NULL;
		int invRows = slotInvShownRows();
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
		if (boost) {
			F32 infoTop = nameY + 82 * hudScale;
			F32 used = drawBoostInfo(boost, invLeft, infoTop,
						 invWidth, infoBottom,
						 hudScale);

			drawPowerTotals(power, NULL, 0, invLeft,
					nameY + 96 * hudScale + used,
					infoBottom, hudScale);
		}
	} else if (slotInvCount > 0) {
		const Boost *boost =
			e->pchar->aBoosts[slotInv[slotInvSelection]];
		int invRows = slotInvShownRows();
		F32 nameY = invTop + invRows * SLOT_POG_STEP * hudScale +
			    30 * hudScale;
		char level[32];

		// Choosing a slot puts the grid away, so the power's numbers
		// have the room.
		if (slotChoosing)
			nameY = invTop + 6 * hudScale;
		else
			drawSlotInventory(e, invLeft, invTop, hudScale,
					  mouseMoved);

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
		nameY += 58 * hudScale;

		if (slotChoosing && slotRow >= 0 && slotRow < slotRowCount) {
			// Picking a slot: the power's totals now and with this
			// one in, what it would replace, then the numbers.
			Power *power =
				e->pchar->ppPowerSets[slotRows[slotRow].set]
					->ppPowers[slotRows[slotRow].power];

			nameY += drawPowerTotals(power, boost, slotColumn,
						 invLeft, nameY, infoBottom,
						 hudScale);
			nameY += 10 * hudScale;

			if (slotFilled(e, slotRow, slotColumn)) {
				const Boost *held = power->ppBoosts[slotColumn];
				const char *heldName =
					textStd(held->ppowBase->pchDisplayName);
				char replaces[160];

				snprintf(replaces, sizeof(replaces),
					 held->iNumCombines
						 ? "Replaces %s, level %d +%d"
						 : "Replaces %s, level %d",
					 heldName, held->iLevel + 1,
					 held->iNumCombines);
				if (nameY + 20 * hudScale <= infoBottom)
					drawText(&game_12, invLeft, nameY,
						 BIND_Z + 4, hudScale,
						 0xffd060ff, CENTER_Y,
						 replaces);
				nameY += 26 * hudScale;
			}
			drawPowerNumbers(power, boost, slotColumn, invLeft,
					 nameY, invWidth, infoBottom, hudScale);
		} else {
			drawBoostInfo(boost, invLeft, nameY, invWidth,
				      infoBottom, hudScale);
			if (slotRowCount > 0)
				drawText(&game_12, invLeft,
					 infoBottom + 16 * hudScale, BIND_Z + 4,
					 hudScale, 0xffffffa0, CENTER_Y,
					 "RB: look over your powers");
		}
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
	else if (slotViewing)
		drawPrompts(viewPrompts, ARRAY_SIZE(viewPrompts),
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
