/*
 * The console Incarnates screen: equipping, creating and converting incarnate
 * abilities.
 */
#include "UI/uiConsoleHudInternal.h"

void drawTextRight(TTDrawContext *context, F32 right, F32 y, F32 z, F32 scale,
		   int color, const char *text)
{
	drawText(context,
		 right - str_wd_notranslate(context, scale, scale, text), y, z,
		 scale, color, CENTER_Y, text);
}

// Text that shrinks to fit when it is longer than the room it has.
void drawFitted(TTDrawContext *context, F32 x, F32 y, F32 z, F32 scale,
		F32 width, int color, const char *text)
{
	F32 textWidth = str_wd_notranslate(context, scale, scale, text);

	// Measured text runs a little short of drawn text, so it is fitted with
	// some room to spare.
	width *= 0.92f;
	drawText(context, x, y, z,
		 textWidth > width && textWidth > 0 ? scale * width / textWidth
						    : scale,
		 color, CENTER_Y, text);
}

int incScreenUp(void)
{
	return incOpen && consoleHudActive() &&
	       window_getMode(WDW_DIALOG) != WINDOW_DISPLAYING;
}

static char *incTrim(char *text)
{
	char *end;

	while (*text == ' ')
		text++;
	end = text + strlen(text);
	while (end > text && end[-1] == ' ')
		*--end = 0;
	return text;
}

static void incBuildCatalog(Entity *e)
{
	char workshop[] = "Worktable_Incarnate";
	const DetailRecipe **recipes;
	int i;

	if (incEntries && incEntriesFor == e)
		return;

	for (i = 0; i < incEntryCount; i++)
		free(incEntries[i].text);
	free(incEntries);
	incEntries = NULL;
	incEntryCount = 0;
	incEntriesFor = e;

	recipes = entity_GetValidDetailRecipes(e, NULL, workshop);
	incEntries = calloc(eaSize(&recipes) + 1, sizeof(IncEntry));

	for (i = 0; i < eaSize(&recipes); i++) {
		const DetailRecipe *recipe = recipes[i];
		IncEntry *entry = &incEntries[incEntryCount];
		char *part;

		if (!recipe || !recipe->pchDisplayTabName)
			continue;

		memset(entry, 0, sizeof(*entry));
		entry->recipe = recipe;
		entry->text = strdup(textStd(recipe->pchDisplayTabName));
		part = entry->text;
		while (part && entry->depth < 4) {
			char *bar = strchr(part, '|');

			if (bar)
				*bar = 0;
			entry->path[entry->depth++] = incTrim(part);
			part = bar ? bar + 1 : NULL;
		}

		if (entry->depth == 3 || entry->depth == 4)
			incEntryCount++;
		else
			free(entry->text);
	}

	eaDestroyConst(&recipes);
}

// The distinct names at one level of the tab names, among the recipes whose
// earlier levels match. Every conversion sits under the same first level, so
// those are matched from the second.
static int incNames(const char **out, int depth, int level, const char *first,
		    const char *second)
{
	int skip = depth == 4 ? 1 : 0;
	int count = 0;
	int i, j;

	for (i = 0; i < incEntryCount; i++) {
		const IncEntry *entry = &incEntries[i];
		const char *name;

		if (entry->depth != depth)
			continue;
		if (level > skip && stricmp(entry->path[skip], first))
			continue;
		if (level > skip + 1 && stricmp(entry->path[skip + 1], second))
			continue;

		name = entry->path[level];
		for (j = 0; j < count && stricmp(out[j], name); j++)
			;
		if (j == count && count < INC_MAX_NAMES)
			out[count++] = name;
	}
	return count;
}

int incClamp(int value, int count)
{
	return value >= count ? (count > 0 ? count - 1 : 0)
	       : value < 0    ? 0
			      : value;
}

static int incRarityRow(int rarity)
{
	switch (rarity) {
	case kRecipeRarity_VeryRare:
		return 0;
	case kRecipeRarity_Rare:
		return 1;
	case kRecipeRarity_Uncommon:
		return 2;
	}
	return 3;
}

int incCreateAllowed(Entity *e, const DetailRecipe *recipe)
{
	return !recipe->ppchCreateRequires ||
	       chareval_Eval(e->pchar, recipe->ppchCreateRequires,
			     recipe->pchSourceFile);
}

// The components alone. The game's own check folds in the level rule as well,
// which would make a level problem read as a missing component.
static int incHasComponents(Entity *e, const DetailRecipe *recipe)
{
	int found = 0, missing = 0, i;

	for (i = 0; i < eaSize(&recipe->ppSalvagesRequired); i++) {
		const SalvageRequired *required = recipe->ppSalvagesRequired[i];

		if (required && required->pSalvage &&
		    character_SalvageCount(e->pchar, required->pSalvage) >=
			    (int)required->amount)
			found++;
		else
			missing++;
	}
	for (i = 0; i < eaSize(&recipe->ppPowersRequired); i++) {
		const PowerRequired *required = recipe->ppPowersRequired[i];

		if (required && required->pPower &&
		    character_OwnsPower(e->pchar, required->pPower))
			found++;
		else
			missing++;
	}

	// This engine has no "any one component" recipes, so every one is
	// needed.
	(void)found;
	return missing == 0;
}

static int incOwnsReward(Entity *e, const DetailRecipe *recipe)
{
	const BasePower *reward = recipe->pchIncarnateReward
					  ? powerdict_GetBasePowerByFullName(
						    &g_PowerDictionary,
						    recipe->pchIncarnateReward)
					  : NULL;

	return reward && character_OwnsPower(e->pchar, reward);
}

// Everything the game checks before it will make it: components, the level
// rule, and not owning it already.
static int incRecipeReady(Entity *e, const DetailRecipe *recipe)
{
	return character_DetailRecipeCreatable(e->pchar, recipe, NULL, false, 0,
					       -1) &&
	       !incOwnsReward(e, recipe);
}

static int incSelectedSlot(void)
{
	int slot = incSlotRows[incSlotRow][incSlotColumn];

	return slot >= 0 ? slot : incSlotRows[incSlotRow][0];
}

static void gatherIncOwned(Entity *e)
{
	const BasePowerSet *set =
		IncarnateSlot_getBasePowerSet(incSelectedSlot());
	int i;

	incOwnedCount = 0;
	for (i = 0;
	     set && i < eaSize(&set->ppPowers) && incOwnedCount < INC_MAX_OWNED;
	     i++) {
		if (character_OwnsPower(e->pchar, set->ppPowers[i]))
			incOwned[incOwnedCount++] = set->ppPowers[i];
	}

	incListSelection = incClamp(incListSelection, incOwnedCount);
	if (incOwnedCount == 0)
		incInList = 0;
}

static int incNodeFor(const char *reward)
{
	int i;

	for (i = 0; i < incNodeCount; i++) {
		if (!stricmp(incNodes[i].recipe->pchIncarnateReward, reward))
			return i;
	}
	return -1;
}

static void gatherIncCreate(Entity *e)
{
	int i, j, pass;

	incCreateSlotCount = incNames(incCreateSlots, 3, 0, NULL, NULL);
	incCreateSlot = incClamp(incCreateSlot, incCreateSlotCount);
	incCreateTreeCount =
		incCreateSlotCount
			? incNames(incCreateTrees, 3, 1,
				   incCreateSlots[incCreateSlot], NULL)
			: 0;
	incCreateTree = incClamp(incCreateTree, incCreateTreeCount);

	incNodeCount = 0;
	incRouteCount = 0;
	incReadyRoutes = 0;
	memset(incRowCounts, 0, sizeof(incRowCounts));
	if (incCreateTreeCount == 0)
		return;

	for (i = 0; i < incEntryCount; i++) {
		const IncEntry *entry = &incEntries[i];
		const DetailRecipe *recipe = entry->recipe;

		if (entry->depth != 3 ||
		    stricmp(entry->path[0], incCreateSlots[incCreateSlot]) ||
		    stricmp(entry->path[1], incCreateTrees[incCreateTree]) ||
		    !recipe->pchIncarnateReward)
			continue;

		for (j = 0; j < incNodeCount &&
			    stricmp(incNodes[j].recipe->pchIncarnateReward,
				    recipe->pchIncarnateReward);
		     j++)
			;
		if (j == incNodeCount && incNodeCount < INC_MAX_NODES) {
			IncNode *node = &incNodes[incNodeCount++];

			node->recipe = recipe;
			node->reward = powerdict_GetBasePowerByFullName(
				&g_PowerDictionary, recipe->pchIncarnateReward);
			node->row = incRarityRow(recipe->Rarity);
			node->column = incRowCounts[node->row]++;
		}
	}

	// Which abilities each one is made from, for the lines of the graph.
	memset(incFeeds, 0, sizeof(incFeeds));
	for (i = 0; i < incEntryCount; i++) {
		const IncEntry *entry = &incEntries[i];
		const DetailRecipe *recipe = entry->recipe;
		int made, k;

		if (entry->depth != 3 ||
		    stricmp(entry->path[0], incCreateSlots[incCreateSlot]) ||
		    stricmp(entry->path[1], incCreateTrees[incCreateTree]) ||
		    !recipe->pchIncarnateReward)
			continue;

		made = incNodeFor(recipe->pchIncarnateReward);
		for (k = 0; made >= 0 && k < eaSize(&recipe->ppPowersRequired);
		     k++) {
			const PowerRequired *required =
				recipe->ppPowersRequired[k];

			for (j = 0; required && j < incNodeCount; j++) {
				if (incNodes[j].reward == required->pPower)
					incFeeds[made] |= 1u << j;
			}
		}
	}

	incNodeSelection = incClamp(incNodeSelection, incNodeCount);
	if (incNodeCount == 0)
		return;

	// The recipes for the chosen ability, the ones that can be made now
	// first.
	for (pass = 0; pass < 2; pass++) {
		for (i = 0;
		     i < incEntryCount && incRouteCount < INC_MAX_RECIPES;
		     i++) {
			const IncEntry *entry = &incEntries[i];
			int ready;

			if (entry->depth != 3 ||
			    stricmp(entry->path[0],
				    incCreateSlots[incCreateSlot]) ||
			    stricmp(entry->path[1],
				    incCreateTrees[incCreateTree]) ||
			    !entry->recipe->pchIncarnateReward ||
			    stricmp(entry->recipe->pchIncarnateReward,
				    incNodes[incNodeSelection]
					    .recipe->pchIncarnateReward))
				continue;

			ready = incRecipeReady(e, entry->recipe);
			if ((pass == 0) == (ready != 0)) {
				incRoutes[incRouteCount++] = entry->recipe;
				incReadyRoutes += ready;
			}
		}
	}
	incRouteSelection = incClamp(incRouteSelection, incRouteCount);
}

static void gatherIncConvert(void)
{
	int i;

	incConvertSourceCount = incNames(incConvertSources, 4, 1, NULL, NULL);
	incConvertSource = incClamp(incConvertSource, incConvertSourceCount);
	incConvertKindCount =
		incConvertSourceCount
			? incNames(incConvertKinds, 4, 2,
				   incConvertSources[incConvertSource], NULL)
			: 0;
	incConvertKind = incClamp(incConvertKind, incConvertKindCount);

	incConvertRecipeCount = 0;
	for (i = 0; i < incEntryCount && incConvertKindCount &&
		    incConvertRecipeCount < INC_MAX_RECIPES;
	     i++) {
		const IncEntry *entry = &incEntries[i];

		if (entry->depth == 4 &&
		    !stricmp(entry->path[1],
			     incConvertSources[incConvertSource]) &&
		    !stricmp(entry->path[2], incConvertKinds[incConvertKind]))
			incConvertRecipes[incConvertRecipeCount++] =
				entry->recipe;
	}
	incConvertSelection =
		incClamp(incConvertSelection, incConvertRecipeCount);
}

static void incEquip(const BasePower *ability, int slot, int equip)
{
	char command[256];

	snprintf(command, sizeof(command), "%s %s %s",
		 equip ? "incarnate_equip" : "incarnate_unequip",
		 StaticDefineIntRevLookup(ParseIncarnateSlotNames, slot),
		 ability->pchName);
	cmdParse(command);
	sndPlay("Grab4", SOUND_GAME);
}

static void incAskCreate(Entity *e, const DetailRecipe *recipe)
{
	if (!recipe)
		return;

	if (!incCreateAllowed(e, recipe))
		addSystemChatMsg(
			recipe->pchDisplayCreateRequiresFail
				? textStd(recipe->pchDisplayCreateRequiresFail)
				: "You can't make this yet.",
			INFO_USER_ERROR, 0);
	else if (!incHasComponents(e, recipe))
		addSystemChatMsg(
			"You don't have everything this recipe needs yet.",
			INFO_USER_ERROR, 0);
	else if (incOwnsReward(e, recipe))
		addSystemChatMsg("You already have this.", INFO_USER_ERROR, 0);
	else if (!incRecipeReady(e, recipe))
		addSystemChatMsg("You can't make this right now.",
				 INFO_USER_ERROR, 0);
	else {
		incAsking = 1;
		incAskRecipe = recipe;
		sndPlay("select", SOUND_GAME);
	}
}

static void incCreateNow(void)
{
	incAsking = 0;
	if (incAskRecipe && uiIncarnateCreate(incAskRecipe))
		sndPlay("Grab4", SOUND_GAME);
}

// Moves across a row of the ability graph, or to the nearest ability in the
// next row up or down that has any. Returns 0 when there is nowhere to go that
// way.
static int incStepNode(int dx, int dy)
{
	const IncNode *at;
	F32 atPlace;
	int row, i;

	if (incNodeCount == 0)
		return 0;

	at = &incNodes[incNodeSelection];
	if (dx) {
		for (i = 0; i < incNodeCount; i++) {
			if (incNodes[i].row == at->row &&
			    incNodes[i].column == at->column + dx) {
				incNodeSelection = i;
				incRouteSelection = 0;
				return 1;
			}
		}
		return 0;
	}

	atPlace = at->column - (incRowCounts[at->row] - 1) * 0.5f;
	for (row = at->row + dy; row >= 0 && row < INC_RARITY_ROWS; row += dy) {
		int best = -1;
		F32 bestDistance = 0;

		for (i = 0; i < incNodeCount; i++) {
			F32 distance;

			if (incNodes[i].row != row)
				continue;
			distance =
				fabsf(incNodes[i].column -
				      (incRowCounts[row] - 1) * 0.5f - atPlace);
			if (best < 0 || distance < bestDistance) {
				best = i;
				bestDistance = distance;
			}
		}
		if (best >= 0) {
			incNodeSelection = best;
			incRouteSelection = 0;
			return 1;
		}
	}
	return 0;
}

static void incEquipPress(Entity *e, int button)
{
	int slot = incSelectedSlot();

	gatherIncOwned(e);

	if (incInList) {
		switch (button) {
		case GP_DPAD_UP:
			if (incListSelection > 0)
				incListSelection--;
			break;
		case GP_DPAD_DOWN:
			if (incListSelection + 1 < incOwnedCount)
				incListSelection++;
			break;
		case GP_A: {
			const BasePower *ability = incOwned[incListSelection];

			incEquip(ability, slot,
				 !Incarnate_isEquippedByBasePower(e, ability));
			incInList = 0;
			break;
		}
		case GP_B:
		case GP_DPAD_LEFT:
			incInList = 0;
			break;
		}
		return;
	}

	switch (button) {
	case GP_DPAD_UP:
		if (incSlotRow > 0)
			incSlotRow--;
		break;
	case GP_DPAD_DOWN:
		if (incSlotRow < INC_SLOT_ROWS - 1)
			incSlotRow++;
		break;
	case GP_DPAD_LEFT:
		if (incSlotRows[incSlotRow][1] >= 0)
			incSlotColumn = 0;
		break;
	case GP_DPAD_RIGHT:
		if (incSlotRows[incSlotRow][1] >= 0)
			incSlotColumn = 1;
		break;
	case GP_A:
		if (!IncarnateSlot_isActivated(e, slot))
			addSystemChatMsg("This slot is still locked.",
					 INFO_USER_ERROR, 0);
		else if (incOwnedCount == 0)
			addSystemChatMsg("You have no abilities for this slot "
					 "yet. The Create tab makes them.",
					 INFO_USER_ERROR, 0);
		else {
			Power *equipped = Incarnate_getEquipped(e, slot);
			int i;

			incListSelection = 0;
			for (i = 0; equipped && i < incOwnedCount; i++) {
				if (incOwned[i] == equipped->ppowBase)
					incListSelection = i;
			}
			incInList = 1;
			sndPlay("select", SOUND_GAME);
		}
		break;
	case GP_Y: {
		Power *equipped = Incarnate_getEquipped(e, slot);

		if (equipped)
			incEquip(equipped->ppowBase, slot, 0);
		break;
	}
	case GP_B:
		incOpen = 0;
		break;
	}
}

static void incCreatePress(Entity *e, int button)
{
	gatherIncCreate(e);

	switch (incCreateColumn) {
	case 0:
		if (button == GP_DPAD_UP && incCreateSlot > 0)
			incCreateSlot--, incCreateTree = 0,
					 incNodeSelection = 0;
		else if (button == GP_DPAD_DOWN &&
			 incCreateSlot + 1 < incCreateSlotCount)
			incCreateSlot++, incCreateTree = 0,
					 incNodeSelection = 0;
		else if ((button == GP_A || button == GP_DPAD_RIGHT) &&
			 incCreateTreeCount > 0)
			incCreateColumn = 1;
		else if (button == GP_B)
			incOpen = 0;
		break;
	case 1:
		if (button == GP_DPAD_UP && incCreateTree > 0)
			incCreateTree--, incNodeSelection = 0;
		else if (button == GP_DPAD_DOWN &&
			 incCreateTree + 1 < incCreateTreeCount)
			incCreateTree++, incNodeSelection = 0;
		else if ((button == GP_A || button == GP_DPAD_RIGHT) &&
			 incNodeCount > 0) {
			incCreateColumn = 2;
			// Start on the Common ability, where every tree begins.
			for (incNodeSelection = 0;
			     incNodeSelection < incNodeCount - 1 &&
			     incNodes[incNodeSelection].row !=
				     INC_RARITY_ROWS - 1;
			     incNodeSelection++)
				;
			incRouteSelection = 0;
		} else if (button == GP_B || button == GP_DPAD_LEFT)
			incCreateColumn = 0;
		break;
	case 2:
		if (button == GP_DPAD_UP)
			incStepNode(0, -1);
		else if (button == GP_DPAD_DOWN)
			incStepNode(0, 1);
		else if (button == GP_DPAD_RIGHT)
			incStepNode(1, 0);
		else if (button == GP_DPAD_LEFT && !incStepNode(-1, 0))
			incCreateColumn = 1;
		else if (button == GP_A && incRouteCount > 0) {
			incCreateColumn = 3;
			incRouteSelection = 0;
		} else if (button == GP_B)
			incCreateColumn = 1;
		break;
	case 3:
		if (button == GP_DPAD_UP && incRouteSelection > 0)
			incRouteSelection--;
		else if (button == GP_DPAD_DOWN &&
			 incRouteSelection + 1 < incRouteCount)
			incRouteSelection++;
		else if (button == GP_A && incRouteCount > 0)
			incAskCreate(e, incRoutes[incRouteSelection]);
		else if (button == GP_B || button == GP_DPAD_LEFT)
			incCreateColumn = 2;
		break;
	}
}

static void incConvertPress(Entity *e, int button)
{
	gatherIncConvert();

	switch (incConvertColumn) {
	case 0:
		if (button == GP_DPAD_UP && incConvertSource > 0)
			incConvertSource--, incConvertKind = 0,
					    incConvertSelection = 0;
		else if (button == GP_DPAD_DOWN &&
			 incConvertSource + 1 < incConvertSourceCount)
			incConvertSource++, incConvertKind = 0,
					    incConvertSelection = 0;
		else if ((button == GP_A || button == GP_DPAD_RIGHT) &&
			 incConvertKindCount > 0)
			incConvertColumn = 1;
		else if (button == GP_B)
			incOpen = 0;
		break;
	case 1:
		if (button == GP_DPAD_UP && incConvertKind > 0)
			incConvertKind--, incConvertSelection = 0;
		else if (button == GP_DPAD_DOWN &&
			 incConvertKind + 1 < incConvertKindCount)
			incConvertKind++, incConvertSelection = 0;
		else if ((button == GP_A || button == GP_DPAD_RIGHT) &&
			 incConvertRecipeCount > 0)
			incConvertColumn = 2;
		else if (button == GP_B || button == GP_DPAD_LEFT)
			incConvertColumn = 0;
		break;
	case 2:
		if (button == GP_DPAD_UP && incConvertSelection > 0)
			incConvertSelection--;
		else if (button == GP_DPAD_DOWN &&
			 incConvertSelection + 1 < incConvertRecipeCount)
			incConvertSelection++;
		else if (button == GP_A && incConvertRecipeCount > 0)
			incAskCreate(e, incConvertRecipes[incConvertSelection]);
		else if (button == GP_B || button == GP_DPAD_LEFT)
			incConvertColumn = 1;
		break;
	}
}

void incPress(int modifiers, int button)
{
	Entity *e = playerPtr();

	if (!e || !e->pchar || modifiers)
		return;

	incBuildCatalog(e);

	if (incAsking) {
		if (button == GP_A)
			incCreateNow();
		else if (button == GP_B)
			incAsking = 0;
		else if (button == GP_START)
			incAsking = 0, incOpen = 0;
		return;
	}

	if (button == GP_LB || button == GP_RB) {
		incTab = (incTab + bumperDirection(button) + INC_TAB_COUNT) %
			 INC_TAB_COUNT;
		incInList = 0;
		sndPlay("select", SOUND_GAME);
		return;
	}

	if (button == GP_START) {
		incOpen = 0;
		return;
	}

	switch (incTab) {
	case INC_TAB_EQUIP:
		incEquipPress(e, button);
		break;
	case INC_TAB_CREATE:
		incCreatePress(e, button);
		break;
	case INC_TAB_CONVERT:
		incConvertPress(e, button);
		break;
	}
}

int consoleIncarnateIsOpen(void)
{
	return incScreenUp();
}

void consoleIncarnateToggle(void)
{
	char standardIncarnate[] = "window_toggle incarnate";

	if (!consoleHudActive()) {
		cmdParse(standardIncarnate);
		return;
	}

	hudEditDiscard();
	menuOpen = 0;
	bindOpen = 0;
	slotOpen = 0;
	invOpen = 0;
	if (navOpen)
		navClose();
	incOpen = !incOpen;
	incInList = 0;
	incAsking = 0;
	incCreateColumn = 0;
	incConvertColumn = 0;
	// A character earns more recipes as it goes, so the list is read again
	// each time the screen opens.
	incEntriesFor = NULL;
}

// A list down one column of the screen, with a header. Returns nothing; the
// selection is drawn in gold when the column has the D-pad, and in white when
// it only shows where the next column comes from.
void drawIncColumn(const char *header, const char *const *names, int count,
		   int selection, int focused, F32 left, F32 top, F32 width,
		   F32 hudScale)
{
	F32 rowStep = INC_ROW_STEP * hudScale;
	int first =
		selection >= INC_LIST_ROWS ? selection - INC_LIST_ROWS + 1 : 0;
	int i;

	drawText(&game_14, left, top, BIND_Z + 4, hudScale,
		 focused ? FRAME_COLOR_HELD : CLR_WHITE, CENTER_Y, header);

	for (i = first; i < count && i < first + INC_LIST_ROWS; i++) {
		F32 y = top + 26 * hudScale + (i - first) * rowStep;
		int selected = i == selection;

		drawFlatFrame(
			PIX2, 8, left, y, BIND_Z + 2, width,
			rowStep - 6 * hudScale, hudScale,
			selected ? (focused ? FRAME_COLOR_HELD : FRAME_COLOR)
				 : FRAME_COLOR_DIM,
			selected ? (focused ? 0x405878f0 : 0x203040e0)
				 : 0x101820e0);
		drawFitted(&game_12, left + 12 * hudScale,
			   y + (rowStep - 6 * hudScale) / 2, BIND_Z + 4,
			   hudScale, width - 24 * hudScale,
			   selected ? (focused ? FRAME_COLOR_HELD : CLR_WHITE)
				    : 0xffffffc0,
			   names[i]);
	}
}

// What a recipe takes, with what the character has. Returns the height used.
F32 drawIncRequirements(Entity *e, const DetailRecipe *recipe, F32 left,
			F32 top, F32 width, F32 hudScale)
{
	F32 needX = left + width - 110 * hudScale;
	F32 haveX = left + width - 30 * hudScale;
	F32 rowStep = 26 * hudScale;
	F32 y = top;
	char number[16];
	int i;

	drawText(&game_12, needX, y, BIND_Z + 6, hudScale, 0xffffffa0,
		 CENTER_X | CENTER_Y, "Need");
	drawText(&game_12, haveX, y, BIND_Z + 6, hudScale, 0xffffffa0,
		 CENTER_X | CENTER_Y, "Have");
	y += rowStep;

	for (i = 0; i < eaSize(&recipe->ppPowersRequired); i++) {
		const PowerRequired *required = recipe->ppPowersRequired[i];
		int have;

		if (!required || !required->pPower)
			continue;
		have = character_OwnsPower(e->pchar, required->pPower) ? 1 : 0;
		drawText(&game_12, left, y, BIND_Z + 6, hudScale,
			 have ? CLR_WHITE : 0xff6060ff, CENTER_Y,
			 textStd(required->pPower->pchDisplayName));
		drawText(&game_12, needX, y, BIND_Z + 6, hudScale, CLR_WHITE,
			 CENTER_X | CENTER_Y, "1");
		snprintf(number, sizeof(number), "%d", have);
		drawText(&game_12, haveX, y, BIND_Z + 6, hudScale,
			 have ? CLR_WHITE : 0xff6060ff, CENTER_X | CENTER_Y,
			 number);
		y += rowStep;
	}

	for (i = 0; i < eaSize(&recipe->ppSalvagesRequired); i++) {
		const SalvageRequired *required = recipe->ppSalvagesRequired[i];
		int have;

		if (!required || !required->pSalvage)
			continue;
		have = character_SalvageCount(e->pchar, required->pSalvage);
		drawText(&game_12, left, y, BIND_Z + 6, hudScale,
			 have >= (int)required->amount ? CLR_WHITE : 0xff6060ff,
			 CENTER_Y,
			 textStd(required->pSalvage->ui.pchDisplayName));
		snprintf(number, sizeof(number), "%d", required->amount);
		drawText(&game_12, needX, y, BIND_Z + 6, hudScale, CLR_WHITE,
			 CENTER_X | CENTER_Y, number);
		snprintf(number, sizeof(number), "%d", have);
		drawText(&game_12, haveX, y, BIND_Z + 6, hudScale,
			 have >= (int)required->amount ? CLR_WHITE : 0xff6060ff,
			 CENTER_X | CENTER_Y, number);
		y += rowStep;
	}

	if (!incCreateAllowed(e, recipe) &&
	    recipe->pchDisplayCreateRequiresFail) {
		y += 6 * hudScale;
		y += drawWrapped(&game_12,
				 textStd(recipe->pchDisplayCreateRequiresFail),
				 left, y, BIND_Z + 6, width, 22 * hudScale,
				 hudScale, 0xff6060ff, 0, 3) *
		     22 * hudScale;
	}

	return y - top;
}

static void drawIncEquip(Entity *e, F32 left, F32 top, F32 right, F32 hudScale)
{
	F32 tileWidth = 260 * hudScale;
	F32 tileHeight = 76 * hudScale;
	F32 tileStep = 88 * hudScale;
	F32 pairGap = 12 * hudScale;
	F32 listLeft = left + tileWidth * 2 + pairGap + 40 * hudScale;
	F32 listWidth = 360 * hudScale;
	F32 infoLeft = listLeft + listWidth + 36 * hudScale;
	F32 infoWidth = right - infoLeft;
	F32 rowStep = INC_ROW_STEP * hudScale;
	int slot = incSelectedSlot();
	Power *equipped = Incarnate_getEquipped(e, slot);
	const BasePower *shown = NULL;
	int row, column, i;

	gatherIncOwned(e);

	for (row = 0; row < INC_SLOT_ROWS; row++) {
		for (column = 0; column < 2; column++) {
			int tileSlot = incSlotRows[row][column];
			int pair = incSlotRows[row][1] >= 0;
			F32 x = pair ? left + column * (tileWidth + pairGap)
				     : left + (tileWidth + pairGap) / 2;
			F32 y = top + row * tileStep;
			int selected = tileSlot == slot;
			int unlocked;
			Power *inSlot;

			if (tileSlot < 0)
				continue;

			unlocked = IncarnateSlot_isActivated(e, tileSlot);
			inSlot = unlocked ? Incarnate_getEquipped(e, tileSlot)
					  : NULL;

			drawFlatFrame(PIX3, R10, x, y, BIND_Z + 2, tileWidth,
				      tileHeight, hudScale,
				      selected	 ? (incInList ? CLR_WHITE
							      : FRAME_COLOR_HELD)
				      : unlocked ? FRAME_COLOR
						 : FRAME_COLOR_DIM,
				      selected ? 0x405878f0 : 0x101820e0);
			drawText(&game_14, x + 14 * hudScale, y + 20 * hudScale,
				 BIND_Z + 4, hudScale,
				 selected && !incInList ? FRAME_COLOR_HELD
				 : unlocked		? CLR_WHITE
							: 0xffffff70,
				 CENTER_Y,
				 textStd(incarnateSlotDisplayNames[tileSlot]));

			if (inSlot) {
				AtlasTex *icon =
					basepower_GetIcon(inSlot->ppowBase);
				F32 iconSize = 48 * hudScale;

				display_sprite(
					icon,
					x + tileWidth - iconSize -
						12 * hudScale,
					y + (tileHeight - iconSize) / 2,
					BIND_Z + 4, iconSize / icon->width,
					iconSize / icon->height, CLR_WHITE);
				drawWrapped(&game_9,
					    textStd(inSlot->ppowBase
							    ->pchDisplayName),
					    x + 14 * hudScale,
					    y + 44 * hudScale, BIND_Z + 4,
					    tileWidth - iconSize -
						    40 * hudScale,
					    16 * hudScale, hudScale, 0xffffffd0,
					    0, 2);
			} else if (unlocked) {
				drawText(&game_12, x + 14 * hudScale,
					 y + 52 * hudScale, BIND_Z + 4,
					 hudScale, 0xffffff80, CENTER_Y,
					 "Empty");
			} else {
				F32 percent = IncarnateSlot_getActivatePercent(
					e, tileSlot);
				F32 barWidth = tileWidth - 28 * hudScale;
				char text[32];

				drawFlatFrame(PIX2, 6, x + 14 * hudScale,
					      y + 44 * hudScale, BIND_Z + 3,
					      barWidth, 16 * hudScale, hudScale,
					      FRAME_COLOR_DIM, 0x00000080);
				if (percent > 0)
					drawFlatFrame(
						PIX2, 6, x + 14 * hudScale,
						y + 44 * hudScale, BIND_Z + 3,
						barWidth *
							MIN(percent, 100.0f) /
							100.0f,
						16 * hudScale, hudScale, 0,
						0x6080c0c0);
				snprintf(text, sizeof(text), "Locked  %d%%",
					 (int)percent);
				drawText(&game_9,
					 x + 14 * hudScale + barWidth / 2,
					 y + 52 * hudScale, BIND_Z + 5,
					 hudScale, 0xffffffb0,
					 CENTER_X | CENTER_Y, text);
			}
		}
	}

	// The abilities the character owns for the chosen slot.
	{
		char header[96];
		int first = incListSelection >= INC_LIST_ROWS
				    ? incListSelection - INC_LIST_ROWS + 1
				    : 0;

		snprintf(header, sizeof(header), "%s abilities",
			 textStd(incarnateSlotDisplayNames[slot]));
		drawText(&game_14, listLeft, top + 10 * hudScale, BIND_Z + 4,
			 hudScale, incInList ? FRAME_COLOR_HELD : CLR_WHITE,
			 CENTER_Y, header);

		if (!IncarnateSlot_isActivated(e, slot))
			drawWrapped(&game_12,
				    "This slot is still locked. Incarnate "
				    "experience from the trials and task "
				    "forces unlocks it.",
				    listLeft, top + 44 * hudScale, BIND_Z + 4,
				    listWidth, 22 * hudScale, hudScale,
				    0xffffffa0, 0, 4);
		else if (incOwnedCount == 0)
			drawWrapped(&game_12,
				    "You have none yet. The Create tab makes "
				    "them from Threads and other components.",
				    listLeft, top + 44 * hudScale, BIND_Z + 4,
				    listWidth, 22 * hudScale, hudScale,
				    0xffffffa0, 0, 4);

		incListTop = first;
		for (i = first; i < incOwnedCount && i < first + INC_LIST_ROWS;
		     i++) {
			const BasePower *ability = incOwned[i];
			F32 y = top + 36 * hudScale + (i - first) * rowStep;
			int selected = incInList && i == incListSelection;
			int isEquipped =
				Incarnate_isEquippedByBasePower(e, ability);
			AtlasTex *icon = basepower_GetIcon(ability);
			F32 iconSize = 28 * hudScale;

			drawFlatFrame(PIX2, 8, listLeft, y, BIND_Z + 2,
				      listWidth, rowStep - 6 * hudScale,
				      hudScale,
				      selected	   ? FRAME_COLOR_HELD
				      : isEquipped ? FRAME_COLOR
						   : FRAME_COLOR_DIM,
				      selected ? 0x405878f0 : 0x101820e0);
			display_sprite(icon, listLeft + 8 * hudScale,
				       y + (rowStep - 6 * hudScale - iconSize) /
						       2,
				       BIND_Z + 4, iconSize / icon->width,
				       iconSize / icon->height,
				       isEquipped || selected ? CLR_WHITE
							      : 0xffffffa0);
			drawText(&game_12, listLeft + 46 * hudScale,
				 y + (rowStep - 6 * hudScale) / 2, BIND_Z + 4,
				 hudScale,
				 selected ? FRAME_COLOR_HELD : CLR_WHITE,
				 CENTER_Y, textStd(ability->pchDisplayName));
			if (isEquipped)
				drawTextRight(&game_9,
					      listLeft + listWidth -
						      12 * hudScale,
					      y + (rowStep - 6 * hudScale) / 2,
					      BIND_Z + 4, hudScale, 0x80ff80ff,
					      "Equipped");
		}

		if (incOwnedCount > INC_LIST_ROWS) {
			char more[48];

			snprintf(more, sizeof(more), "%d to %d of %d",
				 first + 1,
				 MIN(incOwnedCount, first + INC_LIST_ROWS),
				 incOwnedCount);
			drawText(&game_9, listLeft,
				 top + 36 * hudScale + INC_LIST_ROWS * rowStep +
					 8 * hudScale,
				 BIND_Z + 4, hudScale, 0xffffffa0, CENTER_Y,
				 more);
		}
	}

	// What the highlighted ability does: the one in the list while
	// choosing, otherwise the one in the slot.
	if (incInList && incOwnedCount > 0)
		shown = incOwned[incListSelection];
	else if (equipped)
		shown = equipped->ppowBase;

	drawFlatFrame(PIX3, R10, infoLeft, top, BIND_Z + 1, infoWidth,
		      INC_SLOT_ROWS * tileStep - 12 * hudScale, hudScale,
		      FRAME_COLOR_DIM, 0x0c141cf0);
	if (shown) {
		const char *help =
			shown->pchDisplayHelp && shown->pchDisplayHelp[0]
				? shown->pchDisplayHelp
				: shown->pchDisplayShortHelp;
		int isEquipped = Incarnate_isEquippedByBasePower(e, shown);
		F32 y = top + 30 * hudScale;

		y += drawWrapped(&game_14, textStd(shown->pchDisplayName),
				 infoLeft + 20 * hudScale, y, BIND_Z + 4,
				 infoWidth - 40 * hudScale, 26 * hudScale,
				 hudScale, FRAME_COLOR_HELD, 0, 2) *
		     26 * hudScale;
		drawText(&game_12, infoLeft + 20 * hudScale, y + 4 * hudScale,
			 BIND_Z + 4, hudScale,
			 isEquipped ? 0x80ff80ff : 0xffffffa0, CENTER_Y,
			 isEquipped ? "Equipped" : "Not equipped");
		y += 36 * hudScale;
		drawWrapped(&game_12, help ? textStd(help) : "",
			    infoLeft + 20 * hudScale, y, BIND_Z + 4,
			    infoWidth - 40 * hudScale, 22 * hudScale, hudScale,
			    CLR_WHITE, 0, 18);
	} else {
		drawWrapped(
			&game_12,
			IncarnateSlot_isActivated(e, slot)
				? "Nothing equipped in this slot. Press A to "
				  "choose an ability for it."
				: "Unlock this slot to equip an ability in it.",
			infoLeft + 20 * hudScale, top + 30 * hudScale,
			BIND_Z + 4, infoWidth - 40 * hudScale, 22 * hudScale,
			hudScale, 0xffffffa0, 0, 4);
	}
}

static void drawIncCreate(Entity *e, F32 left, F32 top, F32 right, F32 hudScale)
{
	F32 slotsWidth = 160 * hudScale;
	F32 treesLeft = left + slotsWidth + 20 * hudScale;
	F32 treesWidth = 200 * hudScale;
	F32 graphLeft = treesLeft + treesWidth + 30 * hudScale;
	F32 graphWidth = 400 * hudScale;
	F32 infoLeft = graphLeft + graphWidth + 24 * hudScale;
	F32 infoWidth = right - infoLeft;
	F32 nodeSize = 64 * hudScale;
	F32 nodeStep = 84 * hudScale;
	F32 rowStep = 112 * hudScale;
	F32 labelWidth = 86 * hudScale;
	int i;

	gatherIncCreate(e);

	drawIncColumn("Slot", incCreateSlots, incCreateSlotCount, incCreateSlot,
		      incCreateColumn == 0, left, top + 10 * hudScale,
		      slotsWidth, hudScale);
	if (incCreateSlotCount == 0) {
		drawText(&game_12, left, top + 50 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffa0, CENTER_Y, "No recipes found.");
		return;
	}
	drawIncColumn("Tree", incCreateTrees, incCreateTreeCount, incCreateTree,
		      incCreateColumn == 1, treesLeft, top + 10 * hudScale,
		      treesWidth, hudScale);

	// The tree's abilities by rarity, Very Rare on top, as the game's graph
	// has them.
	drawText(&game_14, graphLeft, top + 10 * hudScale, BIND_Z + 4, hudScale,
		 incCreateColumn >= 2 ? FRAME_COLOR_HELD : CLR_WHITE, CENTER_Y,
		 incCreateTreeCount ? incCreateTrees[incCreateTree] : "");
	for (i = 0; i < INC_RARITY_ROWS; i++) {
		if (incRowCounts[i])
			drawText(&game_9, graphLeft,
				 top + 70 * hudScale + i * rowStep, BIND_Z + 4,
				 hudScale, 0xffffffa0, CENTER_Y,
				 incRarityNames[i]);
	}

	// Where each ability sits, then the lines from what a recipe uses up to
	// what it makes. Each line leaves the top of the lower ability, turns
	// halfway between the rows and climbs into the one above, so lines
	// sharing a row run together, the way the game's graph splits Common
	// into Uncommon and on up.
	{
		F32 nodeX[INC_MAX_NODES], nodeY[INC_MAX_NODES];
		F32 thick = 3 * hudScale;
		U32 lit = 0;
		int pass, from, to;

		for (i = 0; i < incNodeCount; i++) {
			F32 rowWidth =
				(incRowCounts[incNodes[i].row] - 1) * nodeStep;

			nodeX[i] = graphLeft + labelWidth +
				   (graphWidth - labelWidth) / 2 -
				   rowWidth / 2 + incNodes[i].column * nodeStep;
			nodeY[i] =
				top + 70 * hudScale + incNodes[i].row * rowStep;
		}

		// The lines into the chosen ability are gold: all of them while
		// choosing an ability, and only the chosen recipe's while
		// choosing a recipe.
		if (incCreateColumn == 2 && incNodeCount > 0)
			lit = incFeeds[incNodeSelection];
		else if (incCreateColumn == 3 && incRouteCount > 0) {
			const DetailRecipe *route =
				incRoutes[incRouteSelection];

			for (i = 0; i < eaSize(&route->ppPowersRequired); i++) {
				for (from = 0; route->ppPowersRequired[i] &&
					       from < incNodeCount;
				     from++) {
					if (incNodes[from].reward ==
					    route->ppPowersRequired[i]->pPower)
						lit |= 1u << from;
				}
			}
		}

		for (pass = 0; pass < 2; pass++) {
			for (to = 0; to < incNodeCount; to++) {
				for (from = 0; from < incNodeCount; from++) {
					int gold = incCreateColumn >= 2 &&
						   to == incNodeSelection &&
						   (lit & (1u << from));
					int owned =
						incNodes[from].reward &&
						character_OwnsPower(
							e->pchar,
							incNodes[from].reward);
					int color = gold    ? FRAME_COLOR_HELD
						    : owned ? 0xa8c0e0c0
							    : 0x60708090;
					F32 bottom, upper, middle, lowX, highX;

					if (!(incFeeds[to] & (1u << from)) ||
					    gold != pass)
						continue;

					bottom = nodeY[from] - nodeSize / 2;
					upper = nodeY[to] + nodeSize / 2;
					middle = (bottom + upper) / 2;
					lowX = MIN(nodeX[from], nodeX[to]);
					highX = MAX(nodeX[from], nodeX[to]);

					display_sprite(
						white_tex_atlas,
						nodeX[from] - thick / 2, middle,
						BIND_Z + 1 + pass,
						thick / white_tex_atlas->width,
						(bottom - middle) /
							white_tex_atlas->height,
						color);
					display_sprite(
						white_tex_atlas,
						lowX - thick / 2,
						middle - thick / 2,
						BIND_Z + 1 + pass,
						(highX - lowX + thick) /
							white_tex_atlas->width,
						thick / white_tex_atlas->height,
						color);
					display_sprite(
						white_tex_atlas,
						nodeX[to] - thick / 2, upper,
						BIND_Z + 1 + pass,
						thick / white_tex_atlas->width,
						(middle - upper) /
							white_tex_atlas->height,
						color);
				}
			}
		}
	}

	for (i = 0; i < incNodeCount; i++) {
		const IncNode *node = &incNodes[i];
		F32 rowWidth = (incRowCounts[node->row] - 1) * nodeStep;
		F32 x = graphLeft + labelWidth + (graphWidth - labelWidth) / 2 -
			rowWidth / 2 + node->column * nodeStep;
		F32 y = top + 70 * hudScale + node->row * rowStep;
		int owned = node->reward &&
			    character_OwnsPower(e->pchar, node->reward);
		int selected = incCreateColumn >= 2 && i == incNodeSelection;
		AtlasTex *icon =
			node->recipe->ui.pchIcon
				? atlasLoadTexture(node->recipe->ui.pchIcon)
				: NULL;

		drawFlatFrame(PIX3, R10, x - nodeSize / 2, y - nodeSize / 2,
			      BIND_Z + 2, nodeSize, nodeSize, hudScale,
			      selected ? FRAME_COLOR_HELD
			      : owned  ? 0x80ff80c0
				       : FRAME_COLOR_DIM,
			      selected ? 0x405878f0 : 0x101820e0);
		if (icon)
			display_sprite(
				icon, x - nodeSize * 0.4f, y - nodeSize * 0.4f,
				BIND_Z + 4, nodeSize * 0.8f / icon->width,
				nodeSize * 0.8f / icon->height,
				owned || selected ? CLR_WHITE : 0xffffff70);
	}

	drawFlatFrame(PIX3, R10, infoLeft, top, BIND_Z + 1, infoWidth,
		      520 * hudScale, hudScale, FRAME_COLOR_DIM, 0x0c141cf0);
	if (incNodeCount > 0 && incCreateColumn >= 2) {
		const IncNode *node = &incNodes[incNodeSelection];
		const char *help = node->recipe->ui.pchDisplayHelp;
		int owned = node->reward &&
			    character_OwnsPower(e->pchar, node->reward);
		F32 x = infoLeft + 20 * hudScale;
		F32 width = infoWidth - 40 * hudScale;
		F32 y = top + 30 * hudScale;
		char line[96];

		y += drawWrapped(&game_14,
				 textStd(node->recipe->ui.pchDisplayName), x, y,
				 BIND_Z + 4, width, 26 * hudScale, hudScale,
				 FRAME_COLOR_HELD, 0, 2) *
		     26 * hudScale;
		drawText(&game_12, x, y + 2 * hudScale, BIND_Z + 4, hudScale,
			 owned ? 0x80ff80ff : 0xffffffa0, CENTER_Y,
			 owned ? "You have this" : incRarityNames[node->row]);
		y += 30 * hudScale;
		y += drawWrapped(&game_12, help ? textStd(help) : "", x, y,
				 BIND_Z + 4, width, 22 * hudScale, hudScale,
				 CLR_WHITE, 0, 5) *
		     22 * hudScale;
		y += 14 * hudScale;

		if (incRouteCount > 0) {
			const DetailRecipe *route =
				incRoutes[incRouteSelection];
			int ready = incRecipeReady(e, route);
			const char *status = ready ? "Ready to make"
					     : incOwnsReward(e, route)
						     ? "You have this"
					     : !incHasComponents(e, route)
						     ? "Missing components"
						     : "Not yet available";

			if (incRouteCount > 1)
				snprintf(line, sizeof(line), "Recipe %d of %d",
					 incRouteSelection + 1, incRouteCount);
			else
				snprintf(line, sizeof(line), "Recipe");
			drawText(&game_14, x, y, BIND_Z + 4, hudScale,
				 incCreateColumn == 3 ? FRAME_COLOR_HELD
						      : CLR_WHITE,
				 CENTER_Y, line);
			drawTextRight(&game_12, x + width, y, BIND_Z + 4,
				      hudScale, ready ? 0x80ff80ff : 0xff8080ff,
				      status);
			y += 30 * hudScale;
			if (incRouteCount > 1) {
				snprintf(line, sizeof(line),
					 "You can make %d of them now",
					 incReadyRoutes);
				drawText(&game_9, x, y, BIND_Z + 4, hudScale,
					 0xffffffa0, CENTER_Y, line);
				y += 24 * hudScale;
			}
			drawIncRequirements(e, route, x, y, width, hudScale);
		}
	} else {
		drawWrapped(&game_12,
			    "Pick a slot and a tree, then an ability in it to "
			    "see how it is made.",
			    infoLeft + 20 * hudScale, top + 30 * hudScale,
			    BIND_Z + 4, infoWidth - 40 * hudScale,
			    22 * hudScale, hudScale, 0xffffffa0, 0, 4);
	}
}

static void drawIncConvert(Entity *e, F32 left, F32 top, F32 right,
			   F32 hudScale)
{
	F32 sourcesWidth = 200 * hudScale;
	F32 kindsLeft = left + sourcesWidth + 20 * hudScale;
	F32 kindsWidth = 160 * hudScale;
	F32 recipesLeft = kindsLeft + kindsWidth + 30 * hudScale;
	F32 recipesWidth = 380 * hudScale;
	F32 infoLeft = recipesLeft + recipesWidth + 24 * hudScale;
	F32 infoWidth = right - infoLeft;
	F32 rowStep = INC_ROW_STEP * hudScale;
	int first, i;

	gatherIncConvert();

	drawIncColumn("Convert", incConvertSources, incConvertSourceCount,
		      incConvertSource, incConvertColumn == 0, left,
		      top + 10 * hudScale, sourcesWidth, hudScale);
	if (incConvertSourceCount == 0)
		return;
	drawIncColumn("How", incConvertKinds, incConvertKindCount,
		      incConvertKind, incConvertColumn == 1, kindsLeft,
		      top + 10 * hudScale, kindsWidth, hudScale);

	drawText(&game_14, recipesLeft, top + 10 * hudScale, BIND_Z + 4,
		 hudScale, incConvertColumn == 2 ? FRAME_COLOR_HELD : CLR_WHITE,
		 CENTER_Y, "Recipes");
	first = incConvertSelection >= INC_LIST_ROWS
			? incConvertSelection - INC_LIST_ROWS + 1
			: 0;
	for (i = first; i < incConvertRecipeCount && i < first + INC_LIST_ROWS;
	     i++) {
		const DetailRecipe *recipe = incConvertRecipes[i];
		F32 y = top + 36 * hudScale + (i - first) * rowStep;
		int selected =
			incConvertColumn == 2 && i == incConvertSelection;
		int ready = incRecipeReady(e, recipe);
		AtlasTex *icon = recipe->ui.pchIcon
					 ? atlasLoadTexture(recipe->ui.pchIcon)
					 : NULL;
		F32 iconSize = 28 * hudScale;

		drawFlatFrame(PIX2, 8, recipesLeft, y, BIND_Z + 2, recipesWidth,
			      rowStep - 6 * hudScale, hudScale,
			      selected ? FRAME_COLOR_HELD
			      : ready  ? FRAME_COLOR
				       : FRAME_COLOR_DIM,
			      selected ? 0x405878f0 : 0x101820e0);
		if (icon)
			display_sprite(
				icon, recipesLeft + 8 * hudScale,
				y + (rowStep - 6 * hudScale - iconSize) / 2,
				BIND_Z + 4, iconSize / icon->width,
				iconSize / icon->height,
				ready || selected ? CLR_WHITE : 0xffffff90);
		drawText(&game_12, recipesLeft + 46 * hudScale,
			 y + (rowStep - 6 * hudScale) / 2, BIND_Z + 4, hudScale,
			 selected ? FRAME_COLOR_HELD
			 : ready  ? CLR_WHITE
				  : 0xffffffa0,
			 CENTER_Y, textStd(recipe->ui.pchDisplayName));
	}

	drawFlatFrame(PIX3, R10, infoLeft, top, BIND_Z + 1, infoWidth,
		      520 * hudScale, hudScale, FRAME_COLOR_DIM, 0x0c141cf0);
	if (incConvertRecipeCount > 0 && incConvertColumn == 2) {
		const DetailRecipe *recipe =
			incConvertRecipes[incConvertSelection];
		F32 x = infoLeft + 20 * hudScale;
		F32 width = infoWidth - 40 * hudScale;
		F32 y = top + 30 * hudScale;

		y += drawWrapped(&game_14, textStd(recipe->ui.pchDisplayName),
				 x, y, BIND_Z + 4, width, 26 * hudScale,
				 hudScale, FRAME_COLOR_HELD, 0, 2) *
		     26 * hudScale;
		y += 8 * hudScale;
		if (recipe->ui.pchDisplayHelp)
			y += drawWrapped(&game_12,
					 textStd(recipe->ui.pchDisplayHelp), x,
					 y, BIND_Z + 4, width, 22 * hudScale,
					 hudScale, CLR_WHITE, 0, 6) *
			     22 * hudScale;
		y += 14 * hudScale;
		drawIncRequirements(e, recipe, x, y, width, hudScale);
	} else {
		drawWrapped(&game_12,
			    "Pick what to convert and how, then a recipe.",
			    infoLeft + 20 * hudScale, top + 30 * hudScale,
			    BIND_Z + 4, infoWidth - 40 * hudScale,
			    22 * hudScale, hudScale, 0xffffffa0, 0, 4);
	}
}

static void drawIncQuestion(int width, int height, F32 centerX, F32 centerY,
			    F32 hudScale)
{
	static const Prompt askPrompts[] = { { GP_A, "Make it" },
					     { GP_B, "Cancel" } };
	F32 boxWidth = 620 * hudScale;
	F32 boxHeight = 170 * hudScale;
	F32 left = centerX - boxWidth / 2;
	F32 top = centerY - boxHeight / 2;
	const char *title = textStd(incAskRecipe->ui.pchDisplayName);

	display_sprite(white_tex_atlas, 0, 0, BIND_Z + 10,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000060);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z + 11, boxWidth, boxHeight,
		      hudScale, FRAME_COLOR_HELD, 0x101820f8);
	drawText(&game_14, centerX, top + 42 * hudScale, BIND_Z + 14, hudScale,
		 CLR_WHITE, CENTER_X | CENTER_Y, title);
	drawText(&game_12, centerX, top + 78 * hudScale, BIND_Z + 14, hudScale,
		 0xffffffc0, CENTER_X | CENTER_Y,
		 "The components it lists are used up.");
	drawPrompts(askPrompts, ARRAY_SIZE(askPrompts), centerX,
		    top + boxHeight - 36 * hudScale, BIND_Z + 14, hudScale);
}

void drawIncScreen(Entity *e, int width, int height, F32 hudScale)
{
	static const Prompt slotPrompts[] = { { GP_A, "Choose" },
					      { GP_Y, "Take out" },
					      { GP_B, "Close" },
					      { -1, "LB  RB  Tabs" } };
	static const Prompt equipPrompts[] = { { GP_A, "Equip" },
					       { GP_B, "Back" } };
	static const Prompt unequipPrompts[] = { { GP_A, "Take it out" },
						 { GP_B, "Back" } };
	static const Prompt firstColumnPrompts[] = { { GP_A, "Open" },
						     { GP_B, "Close" },
						     { -1, "LB  RB  Tabs" } };
	static const Prompt openPrompts[] = { { GP_A, "Open" },
					      { GP_B, "Back" } };
	static const Prompt graphPrompts[] = { { GP_A, "Recipes" },
					       { GP_B, "Back" } };
	static const Prompt makePrompts[] = {
		{ GP_A, "Make it" },
		{ GP_B, "Back" },
		{ -1, "Up and down for the other recipes" }
	};
	static const Prompt convertPrompts[] = { { GP_A, "Convert" },
						 { GP_B, "Back" } };
	F32 panelWidth = SCREEN_PANEL_WIDTH * hudScale;
	F32 panelHeight = 760 * hudScale;
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 contentLeft = left + 36 * hudScale;
	F32 contentRight = left + panelWidth - 36 * hudScale;
	F32 contentTop = top + 118 * hudScale;
	F32 tabY = top + 78 * hudScale;
	F32 promptY = top + panelHeight - 30 * hudScale;
	int i;

	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	incBuildCatalog(e);

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000090);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	drawText(&game_18, left + panelWidth / 2, top + 36 * hudScale,
		 BIND_Z + 4, hudScale, CLR_WHITE, CENTER_X | CENTER_Y,
		 "Incarnates");

	// The tabs, with the bumpers either side, as on the ring's pages.
	{
		F32 tabStep = 130 * hudScale;
		F32 firstX = left + panelWidth / 2 -
			     (INC_TAB_COUNT - 1) * tabStep / 2;

		drawText(&game_12, firstX - 100 * hudScale, tabY, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_X | CENTER_Y,
			 gamepadButtonLabel(GP_LB));
		for (i = 0; i < INC_TAB_COUNT; i++) {
			F32 x = firstX + i * tabStep;

			drawFlatFrame(PIX2, 8, x - 55 * hudScale,
				      tabY - 15 * hudScale, BIND_Z + 2,
				      110 * hudScale, 30 * hudScale, hudScale,
				      i == incTab ? FRAME_COLOR_HELD
						  : FRAME_COLOR_DIM,
				      i == incTab ? 0x405878f0 : 0x101820e0);
			drawText(&game_12, x, tabY, BIND_Z + 4, hudScale,
				 i == incTab ? FRAME_COLOR_HELD : 0xffffffa0,
				 CENTER_X | CENTER_Y, incTabNames[i]);
		}
		drawText(&game_12,
			 firstX + (INC_TAB_COUNT - 1) * tabStep +
				 100 * hudScale,
			 tabY, BIND_Z + 4, hudScale, 0xffffffc0,
			 CENTER_X | CENTER_Y, gamepadButtonLabel(GP_RB));
	}

	switch (incTab) {
	case INC_TAB_EQUIP:
		drawIncEquip(e, contentLeft, contentTop, contentRight,
			     hudScale);
		if (incInList && incOwnedCount > 0) {
			if (Incarnate_isEquippedByBasePower(
				    e, incOwned[incListSelection]))
				drawPrompts(unequipPrompts,
					    ARRAY_SIZE(unequipPrompts),
					    left + panelWidth / 2, promptY,
					    BIND_Z + 4, hudScale);
			else
				drawPrompts(equipPrompts,
					    ARRAY_SIZE(equipPrompts),
					    left + panelWidth / 2, promptY,
					    BIND_Z + 4, hudScale);
		} else
			drawPrompts(slotPrompts, ARRAY_SIZE(slotPrompts),
				    left + panelWidth / 2, promptY, BIND_Z + 4,
				    hudScale);
		break;

	case INC_TAB_CREATE:
		drawIncCreate(e, contentLeft, contentTop, contentRight,
			      hudScale);
		if (incCreateColumn == 0)
			drawPrompts(firstColumnPrompts,
				    ARRAY_SIZE(firstColumnPrompts),
				    left + panelWidth / 2, promptY, BIND_Z + 4,
				    hudScale);
		else if (incCreateColumn == 1)
			drawPrompts(openPrompts, ARRAY_SIZE(openPrompts),
				    left + panelWidth / 2, promptY, BIND_Z + 4,
				    hudScale);
		else if (incCreateColumn == 2)
			drawPrompts(graphPrompts, ARRAY_SIZE(graphPrompts),
				    left + panelWidth / 2, promptY, BIND_Z + 4,
				    hudScale);
		else
			drawPrompts(makePrompts,
				    incRouteCount > 1 ? ARRAY_SIZE(makePrompts)
						      : 2,
				    left + panelWidth / 2, promptY, BIND_Z + 4,
				    hudScale);
		break;

	case INC_TAB_CONVERT:
		drawIncConvert(e, contentLeft, contentTop, contentRight,
			       hudScale);
		if (incConvertColumn == 0)
			drawPrompts(firstColumnPrompts,
				    ARRAY_SIZE(firstColumnPrompts),
				    left + panelWidth / 2, promptY, BIND_Z + 4,
				    hudScale);
		else if (incConvertColumn == 1)
			drawPrompts(openPrompts, ARRAY_SIZE(openPrompts),
				    left + panelWidth / 2, promptY, BIND_Z + 4,
				    hudScale);
		else
			drawPrompts(convertPrompts, ARRAY_SIZE(convertPrompts),
				    left + panelWidth / 2, promptY, BIND_Z + 4,
				    hudScale);
		break;
	}

	if (incAsking && incAskRecipe)
		drawIncQuestion(width, height, left + panelWidth / 2,
				top + panelHeight / 2, hudScale);

	// Nothing behind the screen hears this click, including the world.
	collisions_off_for_rest_of_frame = 1;
}
#define INV_TAB_SALVAGE 2


const char *const invTabNames[INV_TAB_COUNT] = { "Build", "Recipes",
						 "Salvage" };
const char *const invCategoryNames[INV_CAT_COUNT] = {
	"Set enhancements", "Common enhancements", "Powers",
	"Costume pieces",   "Salvage conversions", "Other"
};
// What the middle column holds in each category.
const char *const invGroupHeaders[INV_CAT_COUNT] = { "Set",	"Enhancement",
						     "Power",	"Piece",
						     "Salvage", "Item" };

int invTab;
int invAsking;
const DetailRecipe *invAskRecipe;
const SalvageItem *invAskSalvage;
int invAskAmount;

// The server says a table opened before the character's update says which one,
// so the screen waits for the name. While it waits, and while the screen is up
// at the table, the game's recipe window must not close it.
int invTablePending;
int invTablePendingFrames;
int invAtTable;

InvEntry *invEntries;
int invEntryCount;
Entity *invEntriesFor;
int invRefresh;

// Build: column 0 is the categories, 1 the groups in one, 2 the recipes in a
// group. What is chosen is kept by name and recipe, not by place, because the
// lists are read again as recipes are built and bought.
int invBuildableOnly;
int invColumn;
int invCategory = -1;
char invGroup[256];
const DetailRecipe *invRecipe;

int invCategoryIds[INV_CAT_COUNT];
char invCategoryLabels[INV_CAT_COUNT][64];
const char *invCategoryLabelList[INV_CAT_COUNT];
int invCategoryCount;
int invCategoryIndex;
const char *invGroups[INV_MAX_GROUPS];
int invGroupCount;
int invGroupIndex;
const InvEntry *invRecipes[INV_MAX_ITEMS];
int invRecipeCount;
int invRecipeIndex;

// Recipes and Salvage share one list, since only one of them is up at a time.
InvItem invItems[INV_MAX_ITEMS];
int invItemCount;
int invItemIndex;
const void *invItemChosen;
