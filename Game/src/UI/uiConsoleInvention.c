/*
 * The console Invention screen: building recipes at an invention table and
 * browsing salvage.
 */
#include "UI/uiConsoleHudInternal.h"

int invScreenUp(void)
{
	return invOpen && consoleHudActive() &&
	       window_getMode(WDW_DIALOG) != WINDOW_DISPLAYING;
}

static int invTableHere(Entity *e)
{
	return invAtTable && e && e->pl &&
	       !stricmp(e->pl->workshopInteracting, INV_TABLE);
}

// Recipes and salvage number their rarities the same way, Common 1 to Very
// Rare 4.
static int invRarityColor(int rarity)
{
	switch (rarity) {
	case kRecipeRarity_Uncommon:
		return 0xffe060ff;
	case kRecipeRarity_Rare:
		return 0xffa040ff;
	case kRecipeRarity_VeryRare:
	case kRecipeRarity_VeryRare + 1:
		return 0xc890ffff;
	}
	return 0xd8d8d8ff;
}

static const char *invRarityName(int rarity)
{
	switch (rarity) {
	case kRecipeRarity_Uncommon:
		return "Uncommon";
	case kRecipeRarity_Rare:
		return "Rare";
	case kRecipeRarity_VeryRare:
		return "Very Rare";
	case kRecipeRarity_VeryRare + 1:
		return "Unique";
	}
	return "Common";
}

static void invCommas(int value, char *out, int size)
{
	char digits[16];
	int length, i, o = 0;

	snprintf(digits, sizeof(digits), "%d", value < 0 ? 0 : value);
	length = (int)strlen(digits);
	for (i = 0; i < length && o < size - 2; i++) {
		if (i && (length - i) % 3 == 0)
			out[o++] = ',';
		out[o++] = digits[i];
	}
	out[o] = 0;
}

static const BasePower *invEnhancementOf(const DetailRecipe *recipe)
{
	return recipe->pchEnhancementReward
		       ? powerdict_GetBasePowerByFullName(
				 &g_PowerDictionary,
				 recipe->pchEnhancementReward)
		       : NULL;
}

static const BasePower *invPowerOf(const DetailRecipe *recipe)
{
	return recipe->pchPowerReward
		       ? powerdict_GetBasePowerByFullName(
				 &g_PowerDictionary, recipe->pchPowerReward)
		       : NULL;
}

static int invMadeHere(const DetailRecipe *recipe)
{
	int i;

	for (i = 0; i < eaSize(&recipe->ppWorkshops); i++) {
		if (recipe->ppWorkshops[i] && recipe->ppWorkshops[i]->pchName &&
		    !stricmp(recipe->ppWorkshops[i]->pchName, INV_TABLE))
			return 1;
	}
	return 0;
}

// What a recipe makes decides where it goes: set and common enhancements,
// powers such as the temporary travel and attack ones, then what the table
// makes from reward tables. The client is not sent those tables, so they are
// told apart by kind: the costume pieces are recipes that drop, and the salvage
// conversions are the table's own.
static int invCategoryOf(const DetailRecipe *recipe,
			 const BasePower *enhancement)
{
	if (enhancement)
		return enhancement->pBoostSetMember ? INV_CAT_SETS
						    : INV_CAT_COMMON;
	if (recipe->pchPowerReward)
		return INV_CAT_POWERS;
	if (!invMadeHere(recipe) || recipe->pchRecipeReward ||
	    recipe->creates_inspiration || recipe->pDetailReward)
		return INV_CAT_OTHER;
	if (recipe->creates_salvage || recipe->Type == kRecipeType_Workshop)
		return INV_CAT_CONVERT;
	return INV_CAT_COSTUME;
}

// Recipe names end in " (Recipe)", which every row of this screen would repeat.
static void invDropRecipeWord(char *name)
{
	size_t length = strlen(name);
	const char *word = " (Recipe)";
	size_t wordLength = strlen(word);

	if (length > wordLength && !stricmp(name + length - wordLength, word))
		name[length - wordLength] = 0;
}

// Set recipes group under the set, and drop the set's name from the front of
// their own. The rest group under their own name, so the levels of one common
// enhancement sit together.
static void invNameEntry(InvEntry *entry, const BasePower *enhancement)
{
	entry->label = strdup(textStd(entry->recipe->ui.pchDisplayName));
	invDropRecipeWord(entry->label);

	if (entry->category == INV_CAT_SETS) {
		const char *set =
			textStd(enhancement->pBoostSetMember->pchDisplayName);
		size_t length = strlen(set);

		entry->group = strdup(set);
		if (length && !strnicmp(entry->label, set, length) &&
		    entry->label[length] == ':') {
			char *piece = entry->label + length + 1;

			while (*piece == ' ')
				piece++;
			memmove(entry->label, piece, strlen(piece) + 1);
		}
	} else if (entry->category == INV_CAT_CONVERT &&
		   eaSize(&entry->recipe->ppSalvagesRequired) > 0 &&
		   entry->recipe->ppSalvagesRequired[0] &&
		   entry->recipe->ppSalvagesRequired[0]->pSalvage &&
		   entry->recipe->ppSalvagesRequired[0]
			   ->pSalvage->ui.pchDisplayName) {
		// A conversion is named for the salvage it takes, which is
		// shorter than its own name.
		entry->group =
			strdup(textStd(entry->recipe->ppSalvagesRequired[0]
					       ->pSalvage->ui.pchDisplayName));
	} else {
		entry->group = strdup(entry->label);
		// The group already names the enhancement, so its rows only
		// need the level.
		if (entry->category == INV_CAT_COMMON &&
		    entry->recipe->level > 0 &&
		    !(entry->recipe->flags & RECIPE_NO_LEVEL_DISPLAYED)) {
			char level[32];

			snprintf(level, sizeof(level), "Level %d",
				 entry->recipe->level);
			free(entry->label);
			entry->label = strdup(level);
		}
	}
}

// Everything but the table: the recipe, its components, the influence and
// anything else it asks for.
static int invRecipeReady(Entity *e, const DetailRecipe *recipe)
{
	return incCreateAllowed(e, recipe) &&
	       character_DetailRecipeCreatable(e->pchar, recipe, NULL, true,
					       false, -1);
}

static void invFreeCatalog(void)
{
	int i;

	for (i = 0; i < invEntryCount; i++) {
		free(invEntries[i].group);
		free(invEntries[i].label);
	}
	free(invEntries);
	invEntries = NULL;
	invEntryCount = 0;
}

static int invCompareEntries(const void *a, const void *b)
{
	const InvEntry *x = (const InvEntry *)a;
	const InvEntry *y = (const InvEntry *)b;
	int result;

	if (x->category != y->category)
		return x->category - y->category;
	if ((result = stricmp(x->group, y->group)) != 0)
		return result;
	if (x->recipe->level != y->recipe->level)
		return x->recipe->level - y->recipe->level;
	return stricmp(x->label, y->label);
}

// At the table this is what the table makes. Anywhere else it is the recipes
// the character has, and the memorized ones. Read again every few frames, so
// building and buying show up without reopening.
static void invBuildCatalog(Entity *e)
{
	char workshop[] = INV_TABLE;
	const DetailRecipe **recipes;
	int i;

	if (invEntries && invEntriesFor == e && invRefresh > 0) {
		invRefresh--;
		return;
	}

	invFreeCatalog();
	invEntriesFor = e;
	invRefresh = INV_REFRESH_FRAMES;

	recipes = entity_GetValidDetailRecipes(e, NULL, workshop);
	invEntries = calloc(eaSize(&recipes) + 1, sizeof(InvEntry));

	for (i = 0; i < eaSize(&recipes); i++) {
		const DetailRecipe *recipe = recipes[i];
		InvEntry *entry = &invEntries[invEntryCount];
		const BasePower *enhancement;
		int owned, buy;

		// Incarnate abilities have their own screen.
		if (!recipe || !recipe->ui.pchDisplayName ||
		    recipe->pchIncarnateReward)
			continue;

		owned = character_RecipeCount(e->pchar, recipe);
		buy = invTableHere(e) && recipe->Type == kRecipeType_Drop &&
		      owned <= 0 && recipe->BuyFromVendor > 0;
		if (recipe->Type == kRecipeType_Drop && owned <= 0 && !buy)
			continue;

		enhancement = invEnhancementOf(recipe);
		memset(entry, 0, sizeof(*entry));
		entry->recipe = recipe;
		entry->owned = owned;
		entry->buy = buy;
		entry->category = invCategoryOf(recipe, enhancement);
		invNameEntry(entry, enhancement);
		entry->ready = !buy && invRecipeReady(e, recipe);
		invEntryCount++;
	}

	eaDestroyConst(&recipes);
	qsort(invEntries, invEntryCount, sizeof(InvEntry), invCompareEntries);
}

static int invShown(const InvEntry *entry)
{
	return !invBuildableOnly || entry->ready;
}

static const InvEntry *invEntryFor(const DetailRecipe *recipe)
{
	int i;

	for (i = 0; i < invEntryCount; i++) {
		if (invEntries[i].recipe == recipe)
			return &invEntries[i];
	}
	return NULL;
}

// The three Build columns, from the catalog and what is chosen.
static void gatherInvBuild(void)
{
	int counts[INV_CAT_COUNT] = { 0 };
	int i;

	for (i = 0; i < invEntryCount; i++) {
		if (invShown(&invEntries[i]))
			counts[invEntries[i].category]++;
	}

	// A category or group emptied by building or discarding drops the D-pad
	// back to the column above it, rather than leaving it in a list it
	// never chose.
	if (invCategory >= 0 && invCategory < INV_CAT_COUNT &&
	    !counts[invCategory] && invColumn > 0)
		invColumn = 0;

	invCategoryCount = 0;
	invCategoryIndex = 0;
	for (i = 0; i < INV_CAT_COUNT; i++) {
		if (!counts[i])
			continue;
		if (i == invCategory)
			invCategoryIndex = invCategoryCount;
		snprintf(invCategoryLabels[invCategoryCount],
			 sizeof(invCategoryLabels[0]), "%s (%d)",
			 invCategoryNames[i], counts[i]);
		invCategoryLabelList[invCategoryCount] =
			invCategoryLabels[invCategoryCount];
		invCategoryIds[invCategoryCount++] = i;
	}

	invGroupCount = 0;
	invGroupIndex = 0;
	invRecipeCount = 0;
	invRecipeIndex = 0;
	if (invCategoryCount == 0) {
		invRecipe = NULL;
		return;
	}
	invCategory = invCategoryIds[invCategoryIndex];

	for (i = 0; i < invEntryCount; i++) {
		const InvEntry *entry = &invEntries[i];

		if (entry->category != invCategory || !invShown(entry))
			continue;
		if (invGroupCount > 0 &&
		    !stricmp(invGroups[invGroupCount - 1], entry->group))
			continue;
		if (invGroupCount >= INV_MAX_GROUPS)
			break;
		if (!stricmp(entry->group, invGroup))
			invGroupIndex = invGroupCount;
		invGroups[invGroupCount++] = entry->group;
	}
	if (invGroupCount == 0) {
		invRecipe = NULL;
		return;
	}
	if (invGroup[0] && stricmp(invGroups[invGroupIndex], invGroup) &&
	    invColumn > 1)
		invColumn = 1;
	snprintf(invGroup, sizeof(invGroup), "%s", invGroups[invGroupIndex]);

	for (i = 0; i < invEntryCount && invRecipeCount < INV_MAX_ITEMS; i++) {
		const InvEntry *entry = &invEntries[i];

		if (entry->category != invCategory || !invShown(entry) ||
		    stricmp(entry->group, invGroup))
			continue;
		if (entry->recipe == invRecipe)
			invRecipeIndex = invRecipeCount;
		invRecipes[invRecipeCount++] = entry;
	}
	invRecipe = invRecipeCount ? invRecipes[invRecipeIndex]->recipe : NULL;
}

static int invCompareItems(const void *a, const void *b)
{
	const InvItem *x = (const InvItem *)a;
	const InvItem *y = (const InvItem *)b;

	if (x->rarity != y->rarity)
		return y->rarity - x->rarity;
	return stricmp(x->name, y->name);
}

// The recipe or salvage inventory, rarest first.
static void gatherInvItems(Entity *e)
{
	int i;

	invItemCount = 0;
	if (invTab == INV_TAB_RECIPES) {
		for (i = 0; i < eaSize(&e->pchar->recipeInv) &&
			    invItemCount < INV_MAX_ITEMS;
		     i++) {
			const RecipeInventoryItem *stack =
				e->pchar->recipeInv[i];
			InvItem *item = &invItems[invItemCount];

			if (!stack || !stack->recipe || stack->amount == 0)
				continue;
			memset(item, 0, sizeof(*item));
			item->recipe = stack->recipe;
			item->amount = stack->amount;
			item->rarity = stack->recipe->Rarity;
			snprintf(item->name, sizeof(item->name), "%s",
				 stack->recipe->ui.pchDisplayName
					 ? textStd(stack->recipe->ui
							   .pchDisplayName)
					 : stack->recipe->pchName);
			invDropRecipeWord(item->name);
			invItemCount++;
		}
	} else {
		for (i = 0; i < eaSize(&e->pchar->salvageInv) &&
			    invItemCount < INV_MAX_ITEMS;
		     i++) {
			const SalvageInventoryItem *stack =
				e->pchar->salvageInv[i];
			InvItem *item = &invItems[invItemCount];

			if (!stack || !stack->salvage || stack->amount == 0)
				continue;
			memset(item, 0, sizeof(*item));
			item->salvage = stack->salvage;
			item->amount = stack->amount;
			item->rarity = stack->salvage->rarity;
			snprintf(item->name, sizeof(item->name), "%s",
				 stack->salvage->ui.pchDisplayName
					 ? textStd(stack->salvage->ui
							   .pchDisplayName)
					 : stack->salvage->pchName);
			invItemCount++;
		}
	}

	qsort(invItems, invItemCount, sizeof(InvItem), invCompareItems);

	for (i = 0; i < invItemCount; i++) {
		if ((invItems[i].recipe &&
		     invItems[i].recipe == invItemChosen) ||
		    (invItems[i].salvage &&
		     invItems[i].salvage == invItemChosen)) {
			invItemIndex = i;
			break;
		}
	}
	invItemIndex = incClamp(invItemIndex, invItemCount);
	invItemChosen =
		invItemCount
			? (invItems[invItemIndex].recipe
				   ? (const void *)invItems[invItemIndex].recipe
				   : (const void *)invItems[invItemIndex]
					     .salvage)
			: NULL;
}

static int invCreationCost(Entity *e, const DetailRecipe *recipe)
{
	return recipe->CreationCost
		       ? chareval_Eval(e->pchar, recipe->CreationCost,
				       recipe->pchSourceFile)
		       : 0;
}

void invClose(void)
{
	char emoteNone[] = "emote none";

	invOpen = 0;
	invAsking = INV_ASK_NONE;
	if (invAtTable) {
		invAtTable = 0;
		workshop_SendClose();
		cmdParse(emoteNone);
	}
}

static void invAskBuild(Entity *e, const InvEntry *entry)
{
	const DetailRecipe *recipe = entry ? entry->recipe : NULL;

	if (!recipe)
		return;

	if (entry->buy) {
		if (e->pchar->iInfluencePoints < recipe->BuyFromVendor)
			addSystemChatMsg("You don't have enough influence to "
					 "buy this recipe.",
					 INFO_USER_ERROR, 0);
		else {
			invAsking = INV_ASK_BUY;
			invAskRecipe = recipe;
			sndPlay("select", SOUND_GAME);
		}
		return;
	}

	if (!invTableHere(e))
		addSystemChatMsg(invMadeHere(recipe)
					 ? "Building needs an invention table."
					 : "This recipe is built somewhere "
					   "else, not at an invention table.",
				 INFO_USER_ERROR, 0);
	else if (!incCreateAllowed(e, recipe))
		addSystemChatMsg(
			recipe->pchDisplayCreateRequiresFail
				? textStd(recipe->pchDisplayCreateRequiresFail)
				: "You can't build this.",
			INFO_USER_ERROR, 0);
	else if (!invRecipeReady(e, recipe))
		addSystemChatMsg(
			"You don't have everything this recipe needs yet.",
			INFO_USER_ERROR, 0);
	else {
		invAsking = INV_ASK_BUILD;
		invAskRecipe = recipe;
		sndPlay("select", SOUND_GAME);
	}
}

static void invAskDiscardRecipe(const DetailRecipe *recipe)
{
	invAsking = INV_ASK_DISCARD_RECIPE;
	invAskRecipe = recipe;
	sndPlay("Reject5", SOUND_GAME);
}

static void invAnswer(Entity *e, int button)
{
	int ask = invAsking;

	invAsking = INV_ASK_NONE;
	if (button == GP_START) {
		invClose();
		return;
	}
	if (button != GP_A &&
	    !(button == GP_Y && ask == INV_ASK_DISCARD_SALVAGE))
		return;

	// The lists are read again soon after, once the server has answered.
	invRefresh = 0;
	switch (ask) {
	case INV_ASK_BUILD:
		if (invAskRecipe &&
		    character_WorkshopSendDetailRecipeBuild(
			    e->pchar, invAskRecipe->pchName,
			    DETAILRECIPE_USE_RECIPE_LEVEL, false))
			sndPlay("Grab4", SOUND_GAME);
		break;
	case INV_ASK_BUY:
		if (invAskRecipe && character_WorkshopSendDetailRecipeBuy(
					    e->pchar, invAskRecipe->pchName))
			sndPlay("Grab4", SOUND_GAME);
		else
			addSystemChatMsg("Your recipe inventory is full.",
					 INFO_USER_ERROR, 0);
		break;
	case INV_ASK_DISCARD_RECIPE:
		if (invAskRecipe)
			sendRecipeDelete(invAskRecipe->pchName, 1);
		break;
	case INV_ASK_DISCARD_SALVAGE:
		if (invAskSalvage)
			sendSalvageDelete(invAskSalvage->pchName,
					  button == GP_Y ? invAskAmount : 1,
					  false);
		break;
	}
}

static void invToggleBuildableOnly(void)
{
	invBuildableOnly = !invBuildableOnly;
	sndPlay("select", SOUND_GAME);
	gatherInvBuild();
	if (invColumn == 2 && invRecipeCount == 0)
		invColumn = 1;
	if (invColumn == 1 && invGroupCount == 0)
		invColumn = 0;
}

static void invBuildPress(Entity *e, int button)
{
	gatherInvBuild();

	if (button == GP_Y) {
		invToggleBuildableOnly();
		return;
	}

	switch (invColumn) {
	case 0:
		if (button == GP_DPAD_UP && invCategoryIndex > 0)
			invCategory = invCategoryIds[invCategoryIndex - 1],
			invGroup[0] = 0, invRecipe = NULL;
		else if (button == GP_DPAD_DOWN &&
			 invCategoryIndex + 1 < invCategoryCount)
			invCategory = invCategoryIds[invCategoryIndex + 1],
			invGroup[0] = 0, invRecipe = NULL;
		else if ((button == GP_A || button == GP_DPAD_RIGHT) &&
			 invGroupCount > 0)
			invColumn = 1;
		else if (button == GP_B)
			invClose();
		break;
	case 1:
		if (button == GP_DPAD_UP && invGroupIndex > 0)
			snprintf(invGroup, sizeof(invGroup), "%s",
				 invGroups[invGroupIndex - 1]),
				invRecipe = NULL;
		else if (button == GP_DPAD_DOWN &&
			 invGroupIndex + 1 < invGroupCount)
			snprintf(invGroup, sizeof(invGroup), "%s",
				 invGroups[invGroupIndex + 1]),
				invRecipe = NULL;
		else if ((button == GP_A || button == GP_DPAD_RIGHT) &&
			 invRecipeCount > 0)
			invColumn = 2;
		else if (button == GP_B || button == GP_DPAD_LEFT)
			invColumn = 0;
		break;
	case 2:
		if (button == GP_DPAD_UP && invRecipeIndex > 0)
			invRecipe = invRecipes[invRecipeIndex - 1]->recipe;
		else if (button == GP_DPAD_DOWN &&
			 invRecipeIndex + 1 < invRecipeCount)
			invRecipe = invRecipes[invRecipeIndex + 1]->recipe;
		else if (button == GP_A && invRecipeCount > 0)
			invAskBuild(e, invRecipes[invRecipeIndex]);
		else if (button == GP_X && invRecipeCount > 0 &&
			 invRecipes[invRecipeIndex]->owned > 0)
			invAskDiscardRecipe(invRecipes[invRecipeIndex]->recipe);
		else if (button == GP_B || button == GP_DPAD_LEFT)
			invColumn = 1;
		break;
	}
}

// From the Recipes tab to the same recipe on the Build tab.
static void invShowInBuild(const DetailRecipe *recipe)
{
	const InvEntry *entry = invEntryFor(recipe);

	if (!entry) {
		addSystemChatMsg("This recipe is built somewhere else, not at "
				 "an invention table.",
				 INFO_USER_ERROR, 0);
		return;
	}

	invTab = INV_TAB_BUILD;
	invBuildableOnly = 0;
	invCategory = entry->category;
	snprintf(invGroup, sizeof(invGroup), "%s", entry->group);
	invRecipe = recipe;
	invColumn = 2;
	sndPlay("select", SOUND_GAME);
}

static void invItemsPress(Entity *e, int button)
{
	const InvItem *item;

	gatherInvItems(e);
	item = invItemCount ? &invItems[invItemIndex] : NULL;

	switch (button) {
	case GP_DPAD_UP:
		if (invItemIndex > 0)
			invItemIndex--, invItemChosen = NULL;
		break;
	case GP_DPAD_DOWN:
		if (invItemIndex + 1 < invItemCount)
			invItemIndex++, invItemChosen = NULL;
		break;
	case GP_A:
		if (item && item->recipe)
			invShowInBuild(item->recipe);
		break;
	case GP_X:
		if (item && item->recipe)
			invAskDiscardRecipe(item->recipe);
		else if (item && item->salvage) {
			invAsking = INV_ASK_DISCARD_SALVAGE;
			invAskSalvage = item->salvage;
			invAskAmount = item->amount;
			sndPlay("Reject5", SOUND_GAME);
		}
		break;
	case GP_B:
		invClose();
		break;
	}

	// Keep the place just moved to, by what is there.
	if (!invItemChosen && invItemCount)
		invItemChosen =
			invItems[invItemIndex].recipe
				? (const void *)invItems[invItemIndex].recipe
				: (const void *)invItems[invItemIndex].salvage;
}

void invPress(int modifiers, int button)
{
	Entity *e = playerPtr();

	if (!e || !e->pchar || modifiers)
		return;

	invBuildCatalog(e);

	if (invAsking) {
		invAnswer(e, button);
		return;
	}

	if (button == GP_LB || button == GP_RB) {
		invTab = (invTab + bumperDirection(button) + INV_TAB_COUNT) %
			 INV_TAB_COUNT;
		invItemChosen = NULL;
		invItemIndex = 0;
		sndPlay("select", SOUND_GAME);
		return;
	}

	if (button == GP_START) {
		invClose();
		return;
	}

	if (invTab == INV_TAB_BUILD)
		invBuildPress(e, button);
	else
		invItemsPress(e, button);
}

int consoleInventionIsOpen(void)
{
	return invScreenUp();
}

static void invOpenScreen(void)
{
	hudEditDiscard();
	menuOpen = 0;
	bindOpen = 0;
	slotOpen = 0;
	incOpen = 0;
	if (navOpen)
		navClose();
	invOpen = 1;
	invAsking = INV_ASK_NONE;
	invColumn = 0;
	// A character gains recipes as it goes, so the catalog is read again
	// each time the screen opens.
	invEntriesFor = NULL;
}

void consoleInventionToggle(void)
{
	char standardRecipes[] = "show recipes";

	if (!consoleHudActive()) {
		cmdParse(standardRecipes);
		return;
	}

	// Opened behind a message box, the screen waits for it, so asking again
	// leaves it open.
	if (invOpen && invScreenUp())
		invClose();
	else
		invOpenScreen();
}

int consoleInventionTableOpened(void)
{
	if (!consoleHudActive())
		return 0;

	invTablePending = 1;
	invTablePendingFrames = 0;
	return 1;
}

void consoleInventionTableClosed(void)
{
	char emoteNone[] = "emote none";

	invTablePending = 0;
	if (invAtTable) {
		invAtTable = 0;
		invOpen = 0;
		invAsking = INV_ASK_NONE;
		cmdParse(emoteNone);
	}
}

int consoleInventionHoldsTable(void)
{
	return invTablePending || (invAtTable && invOpen);
}

// Settles a table the server has opened, and lets go of one the screen no
// longer shows.
void invTick(void)
{
	Entity *e = playerPtr();

	if (invTablePending) {
		int stock = 0;

		if (!e || !e->pl || !consoleHudActive())
			invTablePending = 0, stock = 1;
		else if (e->pl->workshopInteracting[0]) {
			invTablePending = 0;
			if (!stricmp(e->pl->workshopInteracting, INV_TABLE)) {
				char emoteInvent[] = "emote invent";

				invOpenScreen();
				invAtTable = 1;
				invTab = INV_TAB_BUILD;
				invBuildableOnly = 0;
				cmdParse(emoteInvent);
			} else
				stock = 1;
		} else if (++invTablePendingFrames > INV_TABLE_WAIT_FRAMES)
			invTablePending = 0, stock = 1;

		// Every other table, the merit and ticket vendors among them,
		// keeps the game's own window.
		if (stock) {
			requestArchitectInventory();
			window_setMode(WDW_RECIPEINVENTORY, WINDOW_GROWING);
		}
	}

	if (invAtTable && !invOpen)
		invClose();
}

// A recipe's own column on the Build tab, with level, how many are held, and
// its rarity down the side.
static void drawInvRecipeColumn(F32 left, F32 top, F32 width, F32 hudScale)
{
	F32 rowStep = INC_ROW_STEP * hudScale;
	F32 rowHeight = rowStep - 6 * hudScale;
	F32 iconSize = 28 * hudScale;
	int focused = invColumn == 2;
	int first = invRecipeIndex >= INC_LIST_ROWS
			    ? invRecipeIndex - INC_LIST_ROWS + 1
			    : 0;
	int i;

	drawText(&game_14, left, top + 10 * hudScale, BIND_Z + 4, hudScale,
		 focused ? FRAME_COLOR_HELD : CLR_WHITE, CENTER_Y, "Recipes");

	for (i = first; i < invRecipeCount && i < first + INC_LIST_ROWS; i++) {
		const InvEntry *entry = invRecipes[i];
		const DetailRecipe *recipe = entry->recipe;
		F32 y = top + 36 * hudScale + (i - first) * rowStep;
		int selected = i == invRecipeIndex;
		AtlasTex *icon = recipe->ui.pchIcon
					 ? atlasLoadTexture(recipe->ui.pchIcon)
					 : NULL;
		int textColor = selected && focused ? FRAME_COLOR_HELD
				: entry->ready	    ? 0xa0ffa0ff
				: entry->buy	    ? 0xffffff90
						    : CLR_WHITE;
		char right[32] = "";
		char amount[16] = "";

		drawFlatFrame(PIX2, 8, left, y, BIND_Z + 2, width, rowHeight,
			      hudScale,
			      selected
				      ? (focused ? FRAME_COLOR_HELD : CLR_WHITE)
				      : FRAME_COLOR_DIM,
			      selected ? (focused ? 0x405878f0 : 0x203040e0)
				       : 0x101820e0);
		display_sprite(
			white_tex_atlas, left + 5 * hudScale, y + 7 * hudScale,
			BIND_Z + 3, 4 * hudScale / white_tex_atlas->width,
			(rowHeight - 14 * hudScale) / white_tex_atlas->height,
			invRarityColor(recipe->Rarity));
		if (icon)
			display_sprite(icon, left + 14 * hudScale,
				       y + (rowHeight - iconSize) / 2,
				       BIND_Z + 4, iconSize / icon->width,
				       iconSize / icon->height, CLR_WHITE);

		// Common rows are named for their level already, and a level of
		// 1 says nothing.
		if (!(recipe->flags & RECIPE_NO_LEVEL_DISPLAYED) &&
		    recipe->level > 1 && entry->category != INV_CAT_COMMON)
			snprintf(right, sizeof(right), "Lv %d", recipe->level);
		if (entry->buy)
			snprintf(amount, sizeof(amount), "Buy");
		else if (entry->owned > 1)
			snprintf(amount, sizeof(amount), "x%d", entry->owned);

		drawFitted(&game_12, left + 50 * hudScale, y + rowHeight / 2,
			   BIND_Z + 4, hudScale, width - 150 * hudScale,
			   textColor, entry->label);
		if (amount[0])
			drawTextRight(&game_9, left + width - 62 * hudScale,
				      y + rowHeight / 2, BIND_Z + 4, hudScale,
				      entry->buy ? 0x80c0ffff : 0xffffffc0,
				      amount);
		if (right[0])
			drawTextRight(&game_9, left + width - 12 * hudScale,
				      y + rowHeight / 2, BIND_Z + 4, hudScale,
				      0xffffffc0, right);
	}

	if (invRecipeCount > INC_LIST_ROWS) {
		char more[48];

		snprintf(more, sizeof(more), "%d to %d of %d", first + 1,
			 MIN(invRecipeCount, first + INC_LIST_ROWS),
			 invRecipeCount);
		drawText(&game_9, left,
			 top + 36 * hudScale + INC_LIST_ROWS * rowStep +
				 8 * hudScale,
			 BIND_Z + 4, hudScale, 0xffffffa0, CENTER_Y, more);
	}
}

// Everything about one recipe: what it makes, what it takes, and whether it can
// be built here and now.
static void drawInvRecipeInfo(Entity *e, const DetailRecipe *recipe, int owned,
			      int buy, F32 left, F32 top, F32 width, F32 height,
			      F32 hudScale)
{
	const BasePower *enhancement = invEnhancementOf(recipe);
	const BasePower *power = invPowerOf(recipe);
	const char *help = recipe->ui.pchDisplayHelp;
	F32 x = left + 20 * hudScale;
	F32 w = width - 40 * hudScale;
	F32 y = top + 30 * hudScale;
	int cost = invCreationCost(e, recipe);
	int ready = !buy && invRecipeReady(e, recipe);
	const char *status;
	int statusColor;
	char line[128];
	char number[32];

	drawFlatFrame(PIX3, R10, left, top, BIND_Z + 1, width, height, hudScale,
		      FRAME_COLOR_DIM, 0x0c141cf0);

	snprintf(line, sizeof(line), "%s", textStd(recipe->ui.pchDisplayName));
	invDropRecipeWord(line);
	y += drawWrapped(&game_14, line, x, y, BIND_Z + 4, w, 26 * hudScale,
			 hudScale, FRAME_COLOR_HELD, 0, 2) *
	     26 * hudScale;

	if (!(recipe->flags & RECIPE_NO_LEVEL_DISPLAYED) && recipe->level > 1)
		snprintf(line, sizeof(line), "%s, level %d",
			 invRarityName(recipe->Rarity), recipe->level);
	else
		snprintf(line, sizeof(line), "%s",
			 invRarityName(recipe->Rarity));
	drawText(&game_12, x, y + 2 * hudScale, BIND_Z + 4, hudScale,
		 invRarityColor(recipe->Rarity), CENTER_Y, line);

	if (buy) {
		invCommas(recipe->BuyFromVendor, number, sizeof(number));
		snprintf(line, sizeof(line), "Buy for %s", number);
	} else if (owned > 0)
		snprintf(line, sizeof(line), "You have %d", owned);
	else if (recipe->Type == kRecipeType_Memorized)
		snprintf(line, sizeof(line), "Memorized");
	else
		line[0] = 0;
	if (line[0])
		drawTextRight(&game_12, x + w, y + 2 * hudScale, BIND_Z + 4,
			      hudScale, buy ? 0x80c0ffff : 0xffffffc0, line);
	y += 30 * hudScale;

	if (enhancement && enhancement->pBoostSetMember) {
		snprintf(line, sizeof(line), "Part of %s",
			 textStd(enhancement->pBoostSetMember->pchDisplayName));
		drawFitted(&game_12, x, y, BIND_Z + 4, hudScale, w, 0x80c0ffff,
			   line);
		y += 26 * hudScale;
	}

	// Some recipes still carry a placeholder for their help, so what they
	// make speaks instead.
	if (help && help[0] && !strnicmp(textStd(help), "TODO", 4))
		help = NULL;
	if ((!help || !help[0]) && enhancement)
		help = enhancement->pchDisplayShortHelp;
	if ((!help || !help[0]) && power)
		help = power->pchDisplayShortHelp;
	// Enhancement help has its numbers filled in where the game shows it
	// with an enhancement in hand. Bare, it would show the names of the
	// numbers instead, so it is left out.
	if (help && help[0] && strchr(textStd(help), '{'))
		help = NULL;
	if (help && help[0])
		y += drawWrapped(&game_12, textStd(help), x, y, BIND_Z + 4, w,
				 22 * hudScale, hudScale, CLR_WHITE, 0, 5) *
		     22 * hudScale;
	y += 14 * hudScale;

	if (buy)
		status = "Buy the recipe to build it", statusColor = 0x80c0ffff;
	else if (!incCreateAllowed(e, recipe))
		status = "Can't be built", statusColor = 0xff8080ff;
	else if (!ready)
		status = "Missing components", statusColor = 0xff8080ff;
	else if (!invMadeHere(recipe))
		status = "Built somewhere else", statusColor = 0xffe060ff;
	else if (!invTableHere(e))
		status = "Needs an invention table", statusColor = 0xffe060ff;
	else
		status = "Ready to build", statusColor = 0x80ff80ff;
	drawText(&game_14, x, y, BIND_Z + 4, hudScale, CLR_WHITE, CENTER_Y,
		 "Components");
	drawTextRight(&game_12, x + w, y, BIND_Z + 4, hudScale, statusColor,
		      status);
	y += 30 * hudScale;

	y += drawIncRequirements(e, recipe, x, y, w, hudScale);

	if (cost > 0) {
		invCommas(cost, number, sizeof(number));
		snprintf(line, sizeof(line), "Costs %s influence", number);
		drawText(&game_12, x, y + 6 * hudScale, BIND_Z + 6, hudScale,
			 e->pchar->iInfluencePoints >= cost ? CLR_WHITE
							    : 0xff6060ff,
			 CENTER_Y, line);
	}
}

// The Recipes and Salvage lists: one row a stack, rarest first.
static void drawInvItemList(const char *header, F32 left, F32 top, F32 width,
			    F32 hudScale)
{
	F32 rowStep = INC_ROW_STEP * hudScale;
	F32 rowHeight = rowStep - 6 * hudScale;
	F32 iconSize = 28 * hudScale;
	int first = invItemIndex >= INC_LIST_ROWS
			    ? invItemIndex - INC_LIST_ROWS + 1
			    : 0;
	int i;

	drawText(&game_14, left, top + 10 * hudScale, BIND_Z + 4, hudScale,
		 FRAME_COLOR_HELD, CENTER_Y, header);

	for (i = first; i < invItemCount && i < first + INC_LIST_ROWS; i++) {
		const InvItem *item = &invItems[i];
		const char *iconName = item->recipe ? item->recipe->ui.pchIcon
						    : item->salvage->ui.pchIcon;
		AtlasTex *icon = iconName ? atlasLoadTexture(iconName) : NULL;
		F32 y = top + 36 * hudScale + (i - first) * rowStep;
		int selected = i == invItemIndex;
		char amount[16];

		drawFlatFrame(PIX2, 8, left, y, BIND_Z + 2, width, rowHeight,
			      hudScale,
			      selected ? FRAME_COLOR_HELD : FRAME_COLOR_DIM,
			      selected ? 0x405878f0 : 0x101820e0);
		display_sprite(
			white_tex_atlas, left + 5 * hudScale, y + 7 * hudScale,
			BIND_Z + 3, 4 * hudScale / white_tex_atlas->width,
			(rowHeight - 14 * hudScale) / white_tex_atlas->height,
			invRarityColor(item->rarity));
		if (icon)
			display_sprite(icon, left + 14 * hudScale,
				       y + (rowHeight - iconSize) / 2,
				       BIND_Z + 4, iconSize / icon->width,
				       iconSize / icon->height, CLR_WHITE);
		drawFitted(&game_12, left + 50 * hudScale, y + rowHeight / 2,
			   BIND_Z + 4, hudScale, width - 120 * hudScale,
			   selected ? FRAME_COLOR_HELD : CLR_WHITE, item->name);
		snprintf(amount, sizeof(amount), "x%d", item->amount);
		drawTextRight(&game_12, left + width - 12 * hudScale,
			      y + rowHeight / 2, BIND_Z + 4, hudScale,
			      0xffffffc0, amount);
	}

	if (invItemCount > INC_LIST_ROWS) {
		char more[48];

		snprintf(more, sizeof(more), "%d to %d of %d", first + 1,
			 MIN(invItemCount, first + INC_LIST_ROWS),
			 invItemCount);
		drawText(&game_9, left,
			 top + 36 * hudScale + INC_LIST_ROWS * rowStep +
				 8 * hudScale,
			 BIND_Z + 4, hudScale, 0xffffffa0, CENTER_Y, more);
	}
}

static void drawInvSalvageInfo(Entity *e, const InvItem *item, F32 left,
			       F32 top, F32 width, F32 height, F32 hudScale)
{
	const SalvageItem *salvage = item->salvage;
	F32 x = left + 20 * hudScale;
	F32 w = width - 40 * hudScale;
	F32 y = top + 30 * hudScale;
	int shown = 0, more = 0, i, j;
	char line[160];

	drawFlatFrame(PIX3, R10, left, top, BIND_Z + 1, width, height, hudScale,
		      FRAME_COLOR_DIM, 0x0c141cf0);

	y += drawWrapped(&game_14, item->name, x, y, BIND_Z + 4, w,
			 26 * hudScale, hudScale, FRAME_COLOR_HELD, 0, 2) *
	     26 * hudScale;
	drawText(&game_12, x, y + 2 * hudScale, BIND_Z + 4, hudScale,
		 invRarityColor(item->rarity), CENTER_Y,
		 invRarityName(item->rarity));
	snprintf(line, sizeof(line), "You have %d", item->amount);
	drawTextRight(&game_12, x + w, y + 2 * hudScale, BIND_Z + 4, hudScale,
		      0xffffffc0, line);
	y += 30 * hudScale;

	if (salvage->ui.pchDisplayHelp)
		y += drawWrapped(&game_12, textStd(salvage->ui.pchDisplayHelp),
				 x, y, BIND_Z + 4, w, 22 * hudScale, hudScale,
				 CLR_WHITE, 0, 5) *
		     22 * hudScale;
	y += 14 * hudScale;

	drawText(&game_14, x, y, BIND_Z + 4, hudScale, CLR_WHITE, CENTER_Y,
		 "Your recipes that use it");
	y += 30 * hudScale;

	for (i = 0; i < eaSize(&e->pchar->recipeInv); i++) {
		const RecipeInventoryItem *stack = e->pchar->recipeInv[i];

		if (!stack || !stack->recipe || stack->amount == 0)
			continue;
		for (j = 0; j < eaSize(&stack->recipe->ppSalvagesRequired);
		     j++) {
			const SalvageRequired *required =
				stack->recipe->ppSalvagesRequired[j];

			if (!required || required->pSalvage != salvage)
				continue;
			if (shown < INV_USED_BY_ROWS) {
				snprintf(line, sizeof(line), "%s  (needs %d)",
					 textStd(stack->recipe->ui
							 .pchDisplayName),
					 required->amount);
				drawFitted(&game_12, x, y, BIND_Z + 4, hudScale,
					   w,
					   character_SalvageCount(e->pchar,
								  salvage) >=
							   (int)required->amount
						   ? 0xa0ffa0ff
						   : CLR_WHITE,
					   line);
				y += 24 * hudScale;
				shown++;
			} else
				more++;
			break;
		}
	}

	if (shown == 0)
		drawText(&game_12, x, y, BIND_Z + 4, hudScale, 0xffffffa0,
			 CENTER_Y, "None of your recipes use it.");
	else if (more > 0) {
		snprintf(line, sizeof(line), "and %d more", more);
		drawText(&game_12, x, y, BIND_Z + 4, hudScale, 0xffffffa0,
			 CENTER_Y, line);
	}
}

static void drawInvBuild(Entity *e, F32 left, F32 top, F32 right, F32 hudScale)
{
	F32 categoriesWidth = 240 * hudScale;
	F32 groupsLeft = left + categoriesWidth + 20 * hudScale;
	F32 groupsWidth = 290 * hudScale;
	F32 recipesLeft = groupsLeft + groupsWidth + 20 * hudScale;
	F32 recipesWidth = 360 * hudScale;
	F32 infoLeft = recipesLeft + recipesWidth + 24 * hudScale;
	F32 infoWidth = right - infoLeft;

	gatherInvBuild();

	if (invCategoryCount == 0) {
		const char *empty =
			invBuildableOnly ? "Nothing you can build right now. "
					   "Press Y to show every recipe."
			: invTableHere(e)
				? "This table has nothing for you to build yet."
				: "You have no recipes yet. They drop from "
				  "enemies, and an invention table sells the "
				  "common ones.";

		drawWrapped(&game_12, empty, left, top + 30 * hudScale,
			    BIND_Z + 4, right - left, 22 * hudScale, hudScale,
			    0xffffffa0, 0, 3);
		return;
	}

	drawIncColumn("Category", invCategoryLabelList, invCategoryCount,
		      invCategoryIndex, invColumn == 0, left,
		      top + 10 * hudScale, categoriesWidth, hudScale);
	drawIncColumn(invGroupHeaders[invCategory], invGroups, invGroupCount,
		      invGroupIndex, invColumn == 1, groupsLeft,
		      top + 10 * hudScale, groupsWidth, hudScale);
	drawInvRecipeColumn(recipesLeft, top, recipesWidth, hudScale);

	if (invRecipeCount > 0 && invColumn >= 1) {
		const InvEntry *entry = invRecipes[invRecipeIndex];

		drawInvRecipeInfo(e, entry->recipe, entry->owned, entry->buy,
				  infoLeft, top, infoWidth, 560 * hudScale,
				  hudScale);
	} else {
		drawFlatFrame(PIX3, R10, infoLeft, top, BIND_Z + 1, infoWidth,
			      560 * hudScale, hudScale, FRAME_COLOR_DIM,
			      0x0c141cf0);
		drawWrapped(
			&game_12,
			"Pick a category and a recipe to see what it makes and "
			"what it takes. Green recipes can be built now.",
			infoLeft + 20 * hudScale, top + 30 * hudScale,
			BIND_Z + 4, infoWidth - 40 * hudScale, 22 * hudScale,
			hudScale, 0xffffffa0, 0, 5);
	}
}

static void drawInvItems(Entity *e, F32 left, F32 top, F32 right, F32 hudScale)
{
	F32 listWidth = 620 * hudScale;
	F32 infoLeft = left + listWidth + 36 * hudScale;
	F32 infoWidth = right - infoLeft;
	InventoryType type = invTab == INV_TAB_RECIPES ? kInventoryType_Recipe
						       : kInventoryType_Salvage;
	char header[64];

	gatherInvItems(e);

	snprintf(header, sizeof(header), "%s  %d of %d",
		 invTab == INV_TAB_RECIPES ? "Recipes" : "Salvage",
		 character_GetInvSize(e->pchar, type),
		 character_GetInvTotalSize(e->pchar, type));
	drawInvItemList(header, left, top, listWidth, hudScale);

	if (invItemCount == 0) {
		drawText(&game_12, left, top + 56 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffa0, CENTER_Y,
			 invTab == INV_TAB_RECIPES ? "You have no recipes."
						   : "You have no salvage.");
		return;
	}

	if (invItems[invItemIndex].recipe) {
		const InvEntry *entry =
			invEntryFor(invItems[invItemIndex].recipe);

		drawInvRecipeInfo(e, invItems[invItemIndex].recipe,
				  invItems[invItemIndex].amount,
				  entry ? entry->buy : 0, infoLeft, top,
				  infoWidth, 560 * hudScale, hudScale);
	} else
		drawInvSalvageInfo(e, &invItems[invItemIndex], infoLeft, top,
				   infoWidth, 560 * hudScale, hudScale);
}

static void drawInvQuestion(Entity *e, int width, int height, F32 centerX,
			    F32 centerY, F32 hudScale)
{
	static const Prompt buildPrompts[] = { { GP_A, "Build it" },
					       { GP_B, "Cancel" } };
	static const Prompt buyPrompts[] = { { GP_A, "Buy it" },
					     { GP_B, "Cancel" } };
	static const Prompt discardPrompts[] = { { GP_A, "Discard" },
						 { GP_B, "Cancel" } };
	static const Prompt salvagePrompts[] = { { GP_A, "Discard one" },
						 { GP_Y, "Discard all" },
						 { GP_B, "Cancel" } };
	F32 boxWidth = 640 * hudScale;
	F32 boxHeight = 170 * hudScale;
	F32 left = centerX - boxWidth / 2;
	F32 top = centerY - boxHeight / 2;
	const Prompt *prompts = discardPrompts;
	int promptCount = ARRAY_SIZE(discardPrompts);
	const char *title = "";
	char line[160] = "";
	char number[32];

	switch (invAsking) {
	case INV_ASK_BUILD: {
		int cost = invCreationCost(e, invAskRecipe);

		title = textStd(invAskRecipe->ui.pchDisplayName);
		if (cost > 0) {
			invCommas(cost, number, sizeof(number));
			snprintf(line, sizeof(line),
				 "Uses up the components and %s influence.",
				 number);
		} else
			snprintf(line, sizeof(line),
				 "Uses up the components it lists.");
		prompts = buildPrompts;
		promptCount = ARRAY_SIZE(buildPrompts);
		break;
	}
	case INV_ASK_BUY:
		title = textStd(invAskRecipe->ui.pchDisplayName);
		invCommas(invAskRecipe->BuyFromVendor, number, sizeof(number));
		snprintf(line, sizeof(line),
			 "Buy this recipe for %s influence?", number);
		prompts = buyPrompts;
		promptCount = ARRAY_SIZE(buyPrompts);
		break;
	case INV_ASK_DISCARD_RECIPE:
		title = textStd(invAskRecipe->ui.pchDisplayName);
		snprintf(line, sizeof(line),
			 "Discard one of this recipe? It is gone for good.");
		break;
	case INV_ASK_DISCARD_SALVAGE:
		title = textStd(invAskSalvage->ui.pchDisplayName);
		if (invAskAmount > 1) {
			snprintf(line, sizeof(line),
				 "Discard one, or all %d? Discarded salvage is "
				 "gone for good.",
				 invAskAmount);
			prompts = salvagePrompts;
			promptCount = ARRAY_SIZE(salvagePrompts);
		} else
			snprintf(line, sizeof(line),
				 "Discard it? Discarded salvage is gone for "
				 "good.");
		break;
	}

	display_sprite(white_tex_atlas, 0, 0, BIND_Z + 10,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000060);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z + 11, boxWidth, boxHeight,
		      hudScale, FRAME_COLOR_HELD, 0x101820f8);
	drawText(&game_14, centerX, top + 42 * hudScale, BIND_Z + 14, hudScale,
		 CLR_WHITE, CENTER_X | CENTER_Y, title);
	drawText(&game_12, centerX, top + 78 * hudScale, BIND_Z + 14, hudScale,
		 0xffffffc0, CENTER_X | CENTER_Y, line);
	drawPrompts(prompts, promptCount, centerX,
		    top + boxHeight - 36 * hudScale, BIND_Z + 14, hudScale);
}

void drawInvScreen(Entity *e, int width, int height, F32 hudScale)
{
	F32 panelWidth = SCREEN_PANEL_WIDTH * hudScale;
	F32 panelHeight = 760 * hudScale;
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 contentLeft = left + 36 * hudScale;
	F32 contentRight = left + panelWidth - 36 * hudScale;
	F32 contentTop = top + 118 * hudScale;
	F32 tabY = top + 78 * hudScale;
	F32 promptY = top + panelHeight - 30 * hudScale;
	Prompt prompts[5];
	int promptCount = 0;
	int i;

	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	invBuildCatalog(e);

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x00000090);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	drawText(&game_18, left + panelWidth / 2, top + 36 * hudScale,
		 BIND_Z + 4, hudScale, CLR_WHITE, CENTER_X | CENTER_Y,
		 "Invention");
	drawTextRight(&game_12, contentRight, top + 36 * hudScale, BIND_Z + 4,
		      hudScale, invTableHere(e) ? 0x80ff80ff : 0xffffffa0,
		      invTableHere(e) ? "At an invention table"
				      : "Building needs an invention table");

	// The tabs, with the bumpers either side, as on the Incarnates screen.
	{
		F32 tabStep = 130 * hudScale;
		F32 firstX = left + panelWidth / 2 -
			     (INV_TAB_COUNT - 1) * tabStep / 2;

		drawText(&game_12, firstX - 100 * hudScale, tabY, BIND_Z + 4,
			 hudScale, 0xffffffc0, CENTER_X | CENTER_Y, "LB");
		for (i = 0; i < INV_TAB_COUNT; i++) {
			F32 x = firstX + i * tabStep;

			drawFlatFrame(PIX2, 8, x - 55 * hudScale,
				      tabY - 15 * hudScale, BIND_Z + 2,
				      110 * hudScale, 30 * hudScale, hudScale,
				      i == invTab ? FRAME_COLOR_HELD
						  : FRAME_COLOR_DIM,
				      i == invTab ? 0x405878f0 : 0x101820e0);
			drawText(&game_12, x, tabY, BIND_Z + 4, hudScale,
				 i == invTab ? FRAME_COLOR_HELD : 0xffffffa0,
				 CENTER_X | CENTER_Y, invTabNames[i]);
		}
		drawText(&game_12,
			 firstX + (INV_TAB_COUNT - 1) * tabStep +
				 100 * hudScale,
			 tabY, BIND_Z + 4, hudScale, 0xffffffc0,
			 CENTER_X | CENTER_Y, "RB");
	}

	if (invTab == INV_TAB_BUILD) {
		drawInvBuild(e, contentLeft, contentTop, contentRight,
			     hudScale);

		if (invColumn == 2 && invRecipeCount > 0) {
			const InvEntry *entry = invRecipes[invRecipeIndex];

			prompts[promptCount].button = GP_A,
			prompts[promptCount++].text =
				entry->buy ? "Buy" : "Build";
			if (entry->owned > 0)
				prompts[promptCount].button = GP_X,
				prompts[promptCount++].text = "Discard";
		} else if (invCategoryCount > 0)
			prompts[promptCount].button = GP_A,
			prompts[promptCount++].text = "Open";
		prompts[promptCount].button = GP_Y,
		prompts[promptCount++].text =
			invBuildableOnly ? "Show all" : "Buildable only";
		prompts[promptCount].button = GP_B,
		prompts[promptCount++].text = invColumn == 0 ? "Close" : "Back";
		if (invColumn == 0)
			prompts[promptCount].button = -1,
			prompts[promptCount++].text = "LB  RB  Tabs";
	} else {
		drawInvItems(e, contentLeft, contentTop, contentRight,
			     hudScale);

		if (invItemCount > 0) {
			if (invTab == INV_TAB_RECIPES)
				prompts[promptCount].button = GP_A,
				prompts[promptCount++].text = "Show in Build";
			prompts[promptCount].button = GP_X,
			prompts[promptCount++].text = "Discard";
		}
		prompts[promptCount].button = GP_B,
		prompts[promptCount++].text = "Close";
		prompts[promptCount].button = -1,
		prompts[promptCount++].text = "LB  RB  Tabs";
	}
	drawPrompts(prompts, promptCount, left + panelWidth / 2, promptY,
		    BIND_Z + 4, hudScale);

	if (invAsking && (invAskRecipe || invAskSalvage))
		drawInvQuestion(e, width, height, left + panelWidth / 2,
				top + panelHeight / 2, hudScale);

	// Nothing behind the screen hears this click, including the world.
	collisions_off_for_rest_of_frame = 1;
}
