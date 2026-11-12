/*
 * The console Powers screen: putting powers on the pad buttons and reading
 * their details.
 */
#include "UI/uiConsoleHudInternal.h"

static void moveBindSelection(int step)
{
	int target = bindSelection + step;

	if (target >= 0 && target < bindPowerCount)
		bindSelection = target;
	else if (step > 1 && bindSelection / BIND_COLUMNS <
				     (bindPowerCount - 1) / BIND_COLUMNS)
		bindSelection = bindPowerCount - 1;
}

int slotOpen;
int incOpen;
int invOpen;

// The enhancement and Incarnate screens are driven the same way as this one, so
// the pad sees any of them as "a console screen".
int consoleBindIsOpen(void)
{
	return (bindOpen && consoleHudActive()) || slotScreenUp() ||
	       incScreenUp() || invScreenUp() || hudEditUp() || gameMenuUp() ||
	       inspScreenUp() || tradeScreenUp();
}

void consoleBindToggle(void)
{
	char standardPowers[] = "window_toggle powerlist";

	if (!consoleHudActive()) {
		cmdParse(standardPowers);
		return;
	}

	hudEditDiscard();
	gameMenuOpen = 0;
	inspOpen = 0;
	menuOpen = 0;
	slotOpen = 0;
	incOpen = 0;
	invOpen = 0;
	if (navOpen)
		navClose();
	bindOpen = !bindOpen;
	bindPlacing = 0;
	bindInfoOpen = 0;
}

int consoleBindLook(F32 y, F32 seconds)
{
	F32 most;

	if (hudEditUp())
		return hudEditLook(y, seconds);

	if (!bindOpen || !bindInfoOpen || slotScreenUp() || incScreenUp() ||
	    invScreenUp())
		return 0;

	most = (F32)(bindInfoLines > bindInfoVisibleLines
			     ? bindInfoLines - bindInfoVisibleLines
			     : 0);
	bindInfoScroll += y * seconds * 12.0f;
	if (bindInfoScroll < 0)
		bindInfoScroll = 0;
	if (bindInfoScroll > most)
		bindInfoScroll = most;
	return 1;
}

// The game's own Info window, with every number the power has.
static void openFullPowerInfo(void)
{
	if (bindSelection >= bindPowerCount)
		return;

	info_SpecificPower(bindPowers[bindSelection].set,
			   bindPowers[bindSelection].power);
	bindOpen = 0;
	bindInfoOpen = 0;
	gamepadFocusOnWindow(WDW_INFO);
}

void consoleBindBlockWindows(void)
{
	if (consoleBindIsOpen() || navCovers()) {
		bindCollisionsBefore = collisions_off_for_rest_of_frame;
		collisions_off_for_rest_of_frame = 1;
	}
}

void consoleBindPress(int modifiers, int button)
{
	Entity *e = playerPtr();

	if (tradeScreenUp()) {
		tradePress(modifiers, button);
		return;
	}

	if (hudEditUp()) {
		hudEditPress(modifiers, button);
		return;
	}

	if (gameMenuUp()) {
		gameMenuPress(button);
		return;
	}

	if (inspScreenUp()) {
		inspPress(modifiers, button);
		return;
	}

	if (slotScreenUp()) {
		slotPress(modifiers, button);
		return;
	}

	if (incScreenUp()) {
		incPress(modifiers, button);
		return;
	}

	if (invScreenUp()) {
		invPress(modifiers, button);
		return;
	}

	if (!e || !e->pchar || button == GP_LT || button == GP_RT ||
	    button == GP_LB || button == GP_RB)
		return;

	gatherBindPowers(e);

	if (bindPlacing) {
		CurTrayType tray;
		int slot;

		if (bindSelection < bindPowerCount &&
		    slotForCommand(gamepadBoundCommand(
					   modifiers & (GP_MOD_LT | GP_MOD_RT),
					   button),
				   &tray, &slot)) {
			TrayObj object;

			buildPowerTrayObj(&object,
					  bindPowers[bindSelection].set,
					  bindPowers[bindSelection].power, 0);
			trayslot_Place(e, tray, slot, &object);
			bindPlacing = 0;
		} else if (button == GP_B || button == GP_START) {
			bindPlacing = 0;
		}
		return;
	}

	if (modifiers)
		return;

	switch (button) {
	case GP_DPAD_LEFT:
		moveBindSelection(-1);
		break;
	case GP_DPAD_RIGHT:
		moveBindSelection(1);
		break;
	case GP_DPAD_UP:
		moveBindSelection(-BIND_COLUMNS);
		break;
	case GP_DPAD_DOWN:
		moveBindSelection(BIND_COLUMNS);
		break;
	case GP_A:
		bindPlacing = bindPowerCount > 0;
		break;
	case GP_Y:
		bindInfoOpen = !bindInfoOpen && bindPowerCount > 0;
		bindInfoScroll = 0;
		break;
	case GP_X:
		if (bindInfoOpen)
			openFullPowerInfo();
		break;
	case GP_B:
		// B puts the description away first, and closes the screen from
		// there.
		if (bindInfoOpen)
			bindInfoOpen = 0;
		else
			bindOpen = 0;
		break;
	case GP_START:
		bindOpen = 0;
		break;
	}
}

// Holding a direction or pushing a stick walks the list, and must not bind the
// slot on that direction.
void consoleBindSteer(int dpadButton)
{
	if (tradeScreenUp())
		tradePress(0, dpadButton);
	else if (hudEditUp())
		hudEditPress(0, dpadButton);
	else if (gameMenuUp())
		gameMenuPress(dpadButton);
	else if (inspScreenUp())
		inspPress(0, dpadButton);
	else if (slotScreenUp())
		slotPress(0, dpadButton);
	else if (incScreenUp())
		incPress(0, dpadButton);
	else if (invScreenUp())
		invPress(0, dpadButton);
	else if (!bindPlacing)
		consoleBindPress(0, dpadButton);
}

static void drawBindList(Entity *e, F32 left, F32 top, F32 hudScale)
{
	F32 step = BIND_ICON_STEP * hudScale;
	F32 iconSize = BIND_ICON_SIZE * hudScale;
	int mouseMoved =
		gMouseInpCur.x != bindMouseX || gMouseInpCur.y != bindMouseY;
	int i;

	bindMouseX = gMouseInpCur.x;
	bindMouseY = gMouseInpCur.y;

	if (bindSelection / BIND_COLUMNS < bindTopRow)
		bindTopRow = bindSelection / BIND_COLUMNS;
	else if (bindSelection / BIND_COLUMNS >= bindTopRow + BIND_ROWS)
		bindTopRow = bindSelection / BIND_COLUMNS - BIND_ROWS + 1;

	for (i = bindTopRow * BIND_COLUMNS;
	     i < bindPowerCount && i < (bindTopRow + BIND_ROWS) * BIND_COLUMNS;
	     i++) {
		const BasePower *base = e->pchar->ppPowerSets[bindPowers[i].set]
						->ppPowers[bindPowers[i].power]
						->ppowBase;
		AtlasTex *icon = basepower_GetIcon(base);
		F32 x = left + (i % BIND_COLUMNS) * step;
		F32 y = top + (i / BIND_COLUMNS - bindTopRow) * step;
		F32 iconScale = iconSize / icon->width;
		CBox box;

		BuildCBox(&box, x, y, iconSize, iconSize);

		if (!bindPlacing && mouseCollision(&box)) {
			if (mouseMoved)
				bindSelection = i;

			if (mouseLeftDrag(&box))
				tray_dragPower(bindPowers[i].set,
					       bindPowers[i].power, icon);
		}

		display_sprite(icon, x, y, BIND_Z + 4, iconScale, iconScale,
			       bindPlacing && i != bindSelection ? 0xffffff50
								 : CLR_WHITE);

		if (i == bindSelection)
			drawFlatFrame(PIX2, 8, x - 3 * hudScale,
				      y - 3 * hudScale, BIND_Z + 3,
				      iconSize + 6 * hudScale,
				      iconSize + 6 * hudScale, hudScale,
				      FRAME_COLOR_HELD, 0x405878f0);
	}
}

static int emitWrappedLine(TTDrawContext *context, const char *line, F32 x,
			   F32 y, F32 z, F32 lineHeight, F32 scale, int color,
			   int lineNumber, int firstLine, int maxLines)
{
	if (lineNumber >= firstLine && lineNumber < firstLine + maxLines)
		drawText(context, x, y + (lineNumber - firstLine) * lineHeight,
			 z, scale, color, CENTER_Y, line);
	return lineNumber + 1;
}

// Breaks text into lines that fit the width, without the game's formatting
// tags. <br> and new lines start a new line. Draws from firstLine on, at most
// maxLines of them, and returns how many lines the text needs.
int drawWrapped(TTDrawContext *context, const char *text, F32 x, F32 y, F32 z,
		F32 width, F32 lineHeight, F32 scale, int color, int firstLine,
		int maxLines)
{
	char clean[4096];
	char line[512];
	char candidate[512];
	int length = 0;
	int lines = 0;
	const char *p;

	if (!text)
		return 0;

	for (p = text; *p && length < (int)sizeof(clean) - 2; p++) {
		if (*p == '<') {
			const char *end = strchr(p, '>');

			if (end) {
				if (!strnicmp(p, "<br", 3))
					clean[length++] = '\n';
				p = end;
				continue;
			}
		}
		if (*p != '\r')
			clean[length++] = *p == '\t' ? ' ' : *p;
	}
	clean[length] = 0;

	line[0] = 0;
	p = clean;
	while (*p) {
		const char *start;
		int wordLength;

		if (*p == '\n') {
			lines = emitWrappedLine(context, line, x, y, z,
						lineHeight, scale, color, lines,
						firstLine, maxLines);
			line[0] = 0;
			p++;
			continue;
		}
		if (*p == ' ') {
			p++;
			continue;
		}

		start = p;
		while (*p && *p != ' ' && *p != '\n')
			p++;
		wordLength = (int)(p - start);
		if (wordLength > 200)
			wordLength = 200;

		snprintf(candidate, sizeof(candidate), "%s%s%.*s", line,
			 line[0] ? " " : "", wordLength, start);
		if (line[0] && str_wd_notranslate(context, scale, scale,
						  candidate) > width) {
			lines = emitWrappedLine(context, line, x, y, z,
						lineHeight, scale, color, lines,
						firstLine, maxLines);
			snprintf(line, sizeof(line), "%.*s", wordLength, start);
		} else {
			strcpy(line, candidate);
		}
	}

	if (line[0])
		lines = emitWrappedLine(context, line, x, y, z, lineHeight,
					scale, color, lines, firstLine,
					maxLines);
	return lines;
}

static const char *powerTypeLabel(const BasePower *base)
{
	switch (base->eType) {
	case kPowerType_Toggle:
		return "Toggle";
	case kPowerType_Auto:
		return "Always on";
	default:
		return "Click";
	}
}

static const char *powerTargetLabel(const BasePower *base)
{
	switch (base->eTargetType) {
	case kTargetType_Caster:
		return "Yourself";
	case kTargetType_Foe:
	case kTargetType_DeadOrAliveFoe:
	case kTargetType_Villain:
		return "A foe";
	case kTargetType_DeadFoe:
	case kTargetType_DeadVillain:
		return "A defeated foe";
	case kTargetType_Friend:
	case kTargetType_Teammate:
	case kTargetType_Player:
	case kTargetType_PlayerHero:
	case kTargetType_PlayerVillain:
	case kTargetType_Leaguemate:
		return "An ally";
	case kTargetType_DeadFriend:
	case kTargetType_DeadPlayer:
	case kTargetType_DeadPlayerFriend:
	case kTargetType_DeadTeammate:
	case kTargetType_DeadLeaguemate:
		return "A defeated ally";
	case kTargetType_DeadOrAliveFriend:
	case kTargetType_DeadOrAliveTeammate:
	case kTargetType_DeadOrAliveLeaguemate:
		return "Any ally";
	case kTargetType_Location:
	case kTargetType_Position:
		return "A location";
	case kTargetType_Teleport:
		return "Where you teleport";
	case kTargetType_MyPet:
	case kTargetType_DeadMyPet:
	case kTargetType_DeadOrAliveMyPet:
	case kTargetType_MyCreation:
	case kTargetType_DeadMyCreation:
	case kTargetType_DeadOrAliveMyCreation:
		return "Your pet";
	default:
		return "Anyone";
	}
}

// The panel Y opens beside the power list: what the power does, and its base
// numbers before enhancements.
static void drawPowerInfo(const PowerSet *set, const BasePower *base, F32 left,
			  F32 top, F32 width, F32 height, F32 hudScale)
{
	static const Prompt infoPrompts[] = { { GP_X, "Full details" },
					      { GP_Y, "Hide" } };
	static int lastSelection = -1;
	F32 pad = 22 * hudScale;
	F32 x = left + pad;
	F32 textWidth = width - pad * 2;
	F32 y = top + pad + 10 * hudScale;
	F32 lineHeight = 24 * hudScale;
	F32 rowHeight = 24 * hudScale;
	F32 valueX = x + 150 * hudScale;
	const char *help = base->pchDisplayHelp && base->pchDisplayHelp[0]
				   ? base->pchDisplayHelp
				   : base->pchDisplayShortHelp;
	char value[96];
	int visible;

	if (bindSelection != lastSelection) {
		lastSelection = bindSelection;
		bindInfoScroll = 0;
	}

	drawFlatFrame(PIX3, R10, left, top, BIND_Z + 2, width, height, hudScale,
		      FRAME_COLOR_HELD, 0x0c141cf8);

	drawText(&game_18, x, y, BIND_Z + 6, hudScale, FRAME_COLOR_HELD,
		 CENTER_Y, textStd(base->pchDisplayName));
	y += 28 * hudScale;
	drawText(&game_12, x, y, BIND_Z + 6, hudScale, 0xffffffc0, CENTER_Y,
		 textStd(set->psetBase->pchDisplayName));
	y += 36 * hudScale;

	drawText(&game_12, x, y, BIND_Z + 6, hudScale, 0xffffffa0, CENTER_Y,
		 "Type");
	drawText(&game_12, valueX, y, BIND_Z + 6, hudScale, CLR_WHITE, CENTER_Y,
		 powerTypeLabel(base));
	y += rowHeight;

	drawText(&game_12, x, y, BIND_Z + 6, hudScale, 0xffffffa0, CENTER_Y,
		 "Target");
	if (base->fRadius > 0)
		snprintf(value, sizeof(value), "%s, %.0f ft area",
			 powerTargetLabel(base), base->fRadius);
	else
		snprintf(value, sizeof(value), "%s", powerTargetLabel(base));
	drawText(&game_12, valueX, y, BIND_Z + 6, hudScale, CLR_WHITE, CENTER_Y,
		 value);
	y += rowHeight;

	if (base->fRange > 0) {
		drawText(&game_12, x, y, BIND_Z + 6, hudScale, 0xffffffa0,
			 CENTER_Y, "Range");
		snprintf(value, sizeof(value), "%.0f ft", base->fRange);
		drawText(&game_12, valueX, y, BIND_Z + 6, hudScale, CLR_WHITE,
			 CENTER_Y, value);
		y += rowHeight;
	}

	drawText(&game_12, x, y, BIND_Z + 6, hudScale, 0xffffffa0, CENTER_Y,
		 "Endurance");
	if (base->eType == kPowerType_Toggle && base->fActivatePeriod > 0)
		snprintf(value, sizeof(value), "%.2f every %.1f s",
			 base->fEnduranceCost, base->fActivatePeriod);
	else
		snprintf(value, sizeof(value), "%.2f", base->fEnduranceCost);
	drawText(&game_12, valueX, y, BIND_Z + 6, hudScale, CLR_WHITE, CENTER_Y,
		 value);
	y += rowHeight;

	drawText(&game_12, x, y, BIND_Z + 6, hudScale, 0xffffffa0, CENTER_Y,
		 "Recharge");
	snprintf(value, sizeof(value), "%.1f s", base->fRechargeTime);
	drawText(&game_12, valueX, y, BIND_Z + 6, hudScale, CLR_WHITE, CENTER_Y,
		 value);
	y += rowHeight;

	drawText(&game_12, x, y, BIND_Z + 6, hudScale, 0xffffffa0, CENTER_Y,
		 "Activation");
	snprintf(value, sizeof(value), "%.2f s", base->fTimeToActivate);
	drawText(&game_12, valueX, y, BIND_Z + 6, hudScale, CLR_WHITE, CENTER_Y,
		 value);
	y += rowHeight;

	drawText(&game_9, x, y, BIND_Z + 6, hudScale, 0xffffff80, CENTER_Y,
		 "Base values, before enhancements");
	y += 30 * hudScale;

	visible = (int)((top + height - 56 * hudScale - y) / lineHeight);
	if (visible < 1)
		visible = 1;
	bindInfoVisibleLines = visible;
	bindInfoLines = drawWrapped(&game_14, help ? textStd(help) : "", x, y,
				    BIND_Z + 6, textWidth, lineHeight, hudScale,
				    CLR_WHITE, (int)bindInfoScroll, visible);

	if (bindInfoLines > visible) {
		snprintf(value, sizeof(value), "Right stick scrolls  %d / %d",
			 (int)bindInfoScroll + visible < bindInfoLines
				 ? (int)bindInfoScroll + visible
				 : bindInfoLines,
			 bindInfoLines);
		drawText(&game_9, x, top + height - 46 * hudScale, BIND_Z + 6,
			 hudScale, 0xffffff80, CENTER_Y, value);
	}

	drawPrompts(infoPrompts, ARRAY_SIZE(infoPrompts), left + width / 2,
		    top + height - 22 * hudScale, BIND_Z + 6, hudScale);
}

void drawBindScreen(Entity *e, int width, int height, F32 hudScale)
{
	static const Prompt pickPrompts[] = {
		{ GP_A, "Bind" },
		{ GP_Y, "Details" },
		{ GP_B, "Close" },
		{ -1, "Or drag a power with the mouse" }
	};
	static const Prompt infoOpenPrompts[] = {
		{ GP_A, "Bind" },
		{ GP_Y, "Hide details" },
		{ GP_B, "Back" },
		{ -1, "Or drag a power with the mouse" }
	};
	static const Prompt placePrompts[] = {
		{ GP_B, "Cancel" }, { -1, "Hold LT or RT for the other layers" }
	};
	F32 panelWidth = SCREEN_PANEL_WIDTH * hudScale;
	F32 panelHeight = 760 * hudScale;
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 listLeft = left + 36 * hudScale;
	F32 listTop = top + 84 * hudScale;
	F32 listWidth = BIND_ICON_STEP * BIND_COLUMNS * hudScale;
	F32 clusterLeft = listLeft + listWidth + 190 * hudScale;
	F32 clusterRight = clusterLeft + 350 * hudScale;
	F32 clusterTop = listTop + 90 * hudScale;
	int held = gamepadHeldModifiers() & (GP_MOD_LT | GP_MOD_RT);

	// Everything on the screen answers the mouse, and the windows under it
	// were told not to.
	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	gatherBindPowers(e);

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000090);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	drawText(&game_18, left + panelWidth / 2, top + 36 * hudScale,
		 BIND_Z + 4, hudScale, CLR_WHITE, CENTER_X | CENTER_Y,
		 "Powers");

	if (bindPowerCount > 0) {
		const PowerSet *set =
			e->pchar->ppPowerSets[bindPowers[bindSelection].set];
		const BasePower *base =
			set->ppPowers[bindPowers[bindSelection].power]
				->ppowBase;
		F32 nameY = listTop + BIND_ICON_STEP * BIND_ROWS * hudScale +
			    30 * hudScale;

		drawBindList(e, listLeft, listTop, hudScale);

		// The hover in the list may have moved the selection.
		set = e->pchar->ppPowerSets[bindPowers[bindSelection].set];
		base = set->ppPowers[bindPowers[bindSelection].power]->ppowBase;

		drawText(&game_14, listLeft, nameY, BIND_Z + 4, hudScale,
			 bindPlacing ? FRAME_COLOR_HELD : CLR_WHITE, CENTER_Y,
			 textStd(base->pchDisplayName));
		drawText(&game_12, listLeft, nameY + 26 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_Y,
			 bindPlacing ? "Press the button to bind it to"
				     : textStd(set->psetBase->pchDisplayName));
	} else {
		drawText(&game_14, listLeft, listTop + 20 * hudScale,
			 BIND_Z + 4, hudScale, CLR_WHITE, CENTER_Y,
			 "No powers to bind yet");
	}

	// The description takes the place of the button layout while it is
	// open. Binding brings the layout back.
	if (bindInfoOpen && !bindPlacing && bindPowerCount > 0) {
		F32 infoLeft = listLeft + listWidth + 40 * hudScale;

		drawPowerInfo(
			e->pchar->ppPowerSets[bindPowers[bindSelection].set],
			e->pchar->ppPowerSets[bindPowers[bindSelection].set]
				->ppPowers[bindPowers[bindSelection].power]
				->ppowBase,
			infoLeft, listTop - 20 * hudScale,
			left + panelWidth - 36 * hudScale - infoLeft,
			panelHeight - 150 * hudScale, hudScale);
	} else {
		bindScreenDrawing = 1;
		clusterBaseZ = BIND_Z + 10;
		drawCluster(e, 0, "Main", clusterLeft, clusterTop,
			    hudScale * 0.8f, held);
		drawCluster(e, GP_MOD_LT | GP_MOD_RT, "LT + RT", clusterRight,
			    clusterTop, hudScale * 0.8f, held);
		drawCluster(e, GP_MOD_LT, gamepadButtonLabel(GP_LT),
			    clusterLeft, clusterTop + 240 * hudScale,
			    hudScale * 0.8f, held);
		drawCluster(e, GP_MOD_RT, gamepadButtonLabel(GP_RT),
			    clusterRight, clusterTop + 240 * hudScale,
			    hudScale * 0.8f, held);
		clusterBaseZ = HUD_Z;
		bindScreenDrawing = 0;
	}

	if (bindPlacing)
		drawPrompts(placePrompts, ARRAY_SIZE(placePrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 30 * hudScale, BIND_Z + 4,
			    hudScale);
	else if (bindInfoOpen)
		drawPrompts(infoOpenPrompts, ARRAY_SIZE(infoOpenPrompts),
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

int slotOpen;
int slotChoosing;
// Picking a filled slot to take its enhancement out, instead of a slot to put
// one in.
int slotUnslotting;
int slotAsking;
int slotAskKind;
int slotInvSelection;
int slotRow;
int slotColumn;
int slotTopRow;
int slotInvCount;
int slotInv[CHAR_BOOST_MAX];
int slotRowCount;
SlotRow slotRows[SLOT_MAX_ROWS];
int slotMouseX, slotMouseY;
