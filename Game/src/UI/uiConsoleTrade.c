/*
 * The console Trade screen.
 */
#include "UI/uiConsoleHudInternal.h"

//------------------------------------------------------------------------------------------------------
// Trade
//------------------------------------------------------------------------------------------------------

// The trade window, as a screen the pad can use. The game's window takes items
// dragged in from four other windows and influence typed into a box; here your
// items are listed beside both offers, A adds one and Y takes it back, and
// influence is set a digit at a time with the D-pad. The window still runs
// underneath for its checks, but is not drawn.
#define TRADE_TAB_COUNT 4
#define TRADE_ROWS 11
#define TRADE_ROW_HEIGHT 48.0f
#define TRADE_OFFER_ROW_HEIGHT 40.0f
#define TRADE_MAX_ITEMS 160
#define TRADE_DIGITS 9


static const char *tradeTabNames[TRADE_TAB_COUNT] = { "Inspirations",
						      "Enhancements", "Salvage",
						      "Recipes" };

typedef struct TradeItem {
	int a, b;
	const BasePower *insp;
	const Boost *boost;
	const SalvageItem *salvage;
	const DetailRecipe *recipe;
	int have;
	int offered;
} TradeItem;

static TradeItem tradeItems[TRADE_MAX_ITEMS];
static int tradeItemCount;
int tradeTab;
int tradeSelection;
int tradeTopRow;
int tradeEditing;
static int tradeDigit;
static int tradeDigits[TRADE_DIGITS];
static int tradeMouseX, tradeMouseY;
int tradeWasUp;
// B asks first, on the screen itself: a popup would be drawn under it.
int tradeConfirming;

// On screen whenever a trade is open; it takes the pad unless a popup is asking
// something.
int tradeScreenShown(void)
{
	int mode = window_getMode(WDW_TRADE);

	return consoleHudActive() &&
	       (mode == WINDOW_DISPLAYING || mode == WINDOW_GROWING);
}

int tradeScreenUp(void)
{
	return tradeScreenShown() &&
	       window_getMode(WDW_DIALOG) != WINDOW_DISPLAYING;
}

static int tradeSalvageOffered(int index)
{
	int i;

	for (i = 0; i < trade.aNumSalv; i++)
		if (trade.aSalv[i].iCol == index)
			return trade.aSalv[i].iNumCombines;
	return 0;
}

static int tradeRecipeOffered(int id)
{
	int i;

	for (i = 0; i < trade.aNumReci; i++)
		if (trade.aReci[i].iCol == id)
			return trade.aReci[i].iNumCombines;
	return 0;
}

static int tradeBoostOffered(int index)
{
	int i;

	for (i = 0; i < trade.aNumSpec; i++)
		if (trade.aSpec[i].iCol == index)
			return 1;
	return 0;
}

static void tradeBuildItems(Entity *e)
{
	int i, j;

	tradeItemCount = 0;
	switch (tradeTab) {
	case TRADE_TAB_INSP:
		for (i = 0; i < e->pchar->iNumInspirationCols; i++) {
			for (j = 0; j < e->pchar->iNumInspirationRows &&
				    tradeItemCount < TRADE_MAX_ITEMS;
			     j++) {
				TradeItem *item = &tradeItems[tradeItemCount];

				if (!e->pchar->aInspirations[i][j])
					continue;
				memset(item, 0, sizeof(*item));
				item->a = i;
				item->b = j;
				item->insp = e->pchar->aInspirations[i][j];
				item->have = 1;
				item->offered = inspIsMarkedForTrade(e, i, j);
				tradeItemCount++;
			}
		}
		break;
	case TRADE_TAB_ENH:
		for (i = 0;
		     i < CHAR_BOOST_MAX && tradeItemCount < TRADE_MAX_ITEMS;
		     i++) {
			TradeItem *item = &tradeItems[tradeItemCount];

			if (!e->pchar->aBoosts[i] ||
			    !e->pchar->aBoosts[i]->ppowBase)
				continue;
			memset(item, 0, sizeof(*item));
			item->a = i;
			item->boost = e->pchar->aBoosts[i];
			item->have = 1;
			item->offered = tradeBoostOffered(i);
			tradeItemCount++;
		}
		break;
	case TRADE_TAB_SALVAGE:
		for (i = 0; i < eaSize(&e->pchar->salvageInv) &&
			    tradeItemCount < TRADE_MAX_ITEMS;
		     i++) {
			TradeItem *item = &tradeItems[tradeItemCount];
			const SalvageInventoryItem *stack =
				e->pchar->salvageInv[i];

			if (!stack || !stack->salvage || stack->amount == 0)
				continue;
			memset(item, 0, sizeof(*item));
			item->a = i;
			item->salvage = stack->salvage;
			item->have = stack->amount;
			item->offered = tradeSalvageOffered(i);
			tradeItemCount++;
		}
		break;
	case TRADE_TAB_RECIPES:
		for (i = 0; i < eaSize(&e->pchar->recipeInv) &&
			    tradeItemCount < TRADE_MAX_ITEMS;
		     i++) {
			TradeItem *item = &tradeItems[tradeItemCount];
			const RecipeInventoryItem *stack =
				e->pchar->recipeInv[i];

			if (!stack || !stack->recipe || stack->amount == 0)
				continue;
			memset(item, 0, sizeof(*item));
			item->a = stack->recipe->id;
			item->recipe = stack->recipe;
			item->have = stack->amount;
			item->offered = tradeRecipeOffered(stack->recipe->id);
			tradeItemCount++;
		}
		break;
	}

	if (tradeSelection >= tradeItemCount)
		tradeSelection = tradeItemCount - 1;
	if (tradeSelection < 0)
		tradeSelection = 0;
}

static const char *tradeItemName(const TradeItem *item)
{
	if (item->insp)
		return textStd(item->insp->pchDisplayName);
	if (item->boost)
		return textStd(item->boost->ppowBase->pchDisplayName);
	if (item->salvage)
		return textStd(item->salvage->ui.pchDisplayName);
	if (item->recipe)
		return item->recipe->ui.pchDisplayName
			       ? textStd(item->recipe->ui.pchDisplayName)
			       : item->recipe->pchName;
	return "";
}

// Adds one: an inspiration or enhancement goes in whole, salvage and recipes
// one at a time.
static void tradeAdd(Entity *e, const TradeItem *item)
{
	if (item->insp)
		inspMarkForTrade(e, item->a, item->b);
	else if (item->boost)
		specMarkForTrade(e, item->a);
	else if (item->salvage && item->offered < item->have)
		salvMarkForTrade(e, item->a, 1);
	else if (item->recipe && item->offered < item->have)
		recipeMarkForTrade(e, item->a, 1);
}

static void tradeTakeBack(const TradeItem *item)
{
	if (!item->offered)
		return;
	if (item->insp)
		consoleTradeRemoveInspiration(item->a, item->b);
	else if (item->boost)
		consoleTradeRemoveEnhancement(item->a);
	else if (item->salvage)
		salvRemoveFromTrade(item->a, 1);
	else if (item->recipe)
		recipeRemoveFromTrade(item->a, 1);
}

static void tradeStartEditing(void)
{
	int value = trade.aInfluence;
	int i;

	for (i = TRADE_DIGITS - 1; i >= 0; i--) {
		tradeDigits[i] = value % 10;
		value /= 10;
	}
	tradeDigit = TRADE_DIGITS - 1;
	tradeEditing = 1;
}

static int tradeEditedValue(void)
{
	int value = 0;
	int i;

	for (i = 0; i < TRADE_DIGITS; i++)
		value = value * 10 + tradeDigits[i];
	return value;
}

void tradePress(int modifiers, int button)
{
	Entity *e = playerPtr();

	(void)modifiers;
	if (!e || !e->pchar)
		return;

	if (tradeConfirming) {
		if (button == GP_A) {
			tradeConfirming = 0;
			consoleTradeCancel();
		} else if (button == GP_B) {
			tradeConfirming = 0;
		}
		return;
	}

	if (tradeEditing) {
		switch (button) {
		case GP_DPAD_LEFT:
			if (tradeDigit > 0)
				tradeDigit--;
			break;
		case GP_DPAD_RIGHT:
			if (tradeDigit < TRADE_DIGITS - 1)
				tradeDigit++;
			break;
		case GP_DPAD_UP:
			tradeDigits[tradeDigit] =
				(tradeDigits[tradeDigit] + 1) % 10;
			break;
		case GP_DPAD_DOWN:
			tradeDigits[tradeDigit] =
				(tradeDigits[tradeDigit] + 9) % 10;
			break;
		case GP_A:
		case GP_X:
			consoleTradeSetInfluence(tradeEditedValue());
			tradeEditing = 0;
			break;
		case GP_B:
			tradeEditing = 0;
			break;
		}
		return;
	}

	tradeBuildItems(e);
	switch (button) {
	case GP_LB:
	case GP_RB:
		tradeTab = (tradeTab +
			    (button == GP_RB ? 1 : TRADE_TAB_COUNT - 1)) %
			   TRADE_TAB_COUNT;
		tradeSelection = 0;
		tradeTopRow = 0;
		break;
	case GP_DPAD_UP:
		if (tradeSelection > 0)
			tradeSelection--;
		break;
	case GP_DPAD_DOWN:
		if (tradeSelection < tradeItemCount - 1)
			tradeSelection++;
		break;
	case GP_A:
		if (tradeSelection < tradeItemCount)
			tradeAdd(e, &tradeItems[tradeSelection]);
		break;
	case GP_Y:
		if (tradeSelection < tradeItemCount)
			tradeTakeBack(&tradeItems[tradeSelection]);
		break;
	case GP_X:
		tradeStartEditing();
		break;
	case GP_START:
		consoleTradeSetOffered(!trade.aAccepted);
		break;
	case GP_B:
		tradeConfirming = 1;
		break;
	}
}

static void drawTradeIcon(const BasePower *power, const Boost *boost,
			  const SalvageItem *salvage,
			  const DetailRecipe *recipe, F32 x, F32 y, F32 size,
			  F32 z, F32 hudScale)
{
	const char *iconName = NULL;

	if (boost) {
		drawMenuEnhancement(boost, x + size / 2, y + size / 2, z,
				    size / 64.0f, kBoostState_Normal, 0, true,
				    MENU_GAME, 0);
		return;
	}
	if (power)
		iconName = power->pchIconName;
	else if (salvage)
		iconName = salvage->ui.pchIcon;
	else if (recipe)
		iconName = recipe->ui.pchIcon;

	if (iconName) {
		AtlasTex *icon = atlasLoadTexture(iconName);

		if (icon && icon->width > 0)
			display_sprite(icon, x, y, z, size / icon->width,
				       size / icon->width, CLR_WHITE);
	}
}

// One line of an offer: icon, name and how many.
static F32 drawTradeOfferLine(const BasePower *power, const Boost *boost,
			      const SalvageItem *salvage,
			      const DetailRecipe *recipe, int count, F32 x,
			      F32 y, F32 width, F32 bottom, F32 hudScale)
{
	F32 rowHeight = TRADE_OFFER_ROW_HEIGHT * hudScale;
	const char *name = boost     ? boost->ppowBase->pchDisplayName
			   : power   ? power->pchDisplayName
			   : salvage ? salvage->ui.pchDisplayName
			   : recipe  ? recipe->ui.pchDisplayName
				     : NULL;
	char line[160];

	if (y + rowHeight > bottom)
		return y;

	drawTradeIcon(power, boost, salvage, recipe, x, y + 4 * hudScale,
		      rowHeight - 8 * hudScale, BIND_Z + 4, hudScale);
	if (count > 1)
		snprintf(line, sizeof(line), "%s  x%d",
			 name ? textStd(name) : "", count);
	else
		snprintf(line, sizeof(line), "%s", name ? textStd(name) : "");
	line[sizeof(line) - 1] = 0;
	drawText(&game_12, x + rowHeight + 4 * hudScale, y + rowHeight / 2,
		 BIND_Z + 4, hudScale, CLR_WHITE, CENTER_Y, line);
	return y + rowHeight;
}

static void drawTradeOffer(Entity *e, int partner, F32 x, F32 y, F32 width,
			   F32 bottom, F32 hudScale)
{
	char line[96];
	int i;

	snprintf(line, sizeof(line), "%s influence",
		 itoa_with_commas_static(partner ? trade.bInfluence
						 : trade.aInfluence));
	line[sizeof(line) - 1] = 0;
	drawText(&game_14, x, y + 14 * hudScale, BIND_Z + 4, hudScale,
		 0xffe060ff, CENTER_Y, line);
	y += 36 * hudScale;

	if (partner) {
		for (i = 0; i < trade.bNumInsp; i++)
			y = drawTradeOfferLine(trade.bInsp[i], NULL, NULL, NULL,
					       1, x, y, width, bottom,
					       hudScale);
		for (i = 0; i < trade.bNumSpec; i++)
			y = drawTradeOfferLine(NULL, &trade.bSpec[i], NULL,
					       NULL, 1, x, y, width, bottom,
					       hudScale);
		for (i = 0; i < trade.bNumSalv; i++)
			y = drawTradeOfferLine(NULL, NULL, trade.bSalv[i].sal,
					       NULL, trade.bSalv[i].count, x, y,
					       width, bottom, hudScale);
		for (i = 0; i < trade.bNumReci; i++)
			y = drawTradeOfferLine(NULL, NULL, NULL,
					       trade.bReci[i].pRec,
					       trade.bReci[i].count, x, y,
					       width, bottom, hudScale);
		for (i = 0; i < trade.bNumTpow; i++)
			y = drawTradeOfferLine(trade.bTpow[i].ppowBase, NULL,
					       NULL, NULL, 1, x, y, width,
					       bottom, hudScale);
	} else {
		for (i = 0; i < trade.aNumInsp; i++)
			y = drawTradeOfferLine(trade.aInsp[i].ppow, NULL, NULL,
					       NULL, 1, x, y, width, bottom,
					       hudScale);
		for (i = 0; i < trade.aNumSpec; i++) {
			int index = trade.aSpec[i].iCol;

			if (index >= 0 && index < CHAR_BOOST_MAX &&
			    e->pchar->aBoosts[index])
				y = drawTradeOfferLine(
					NULL, e->pchar->aBoosts[index], NULL,
					NULL, 1, x, y, width, bottom, hudScale);
		}
		for (i = 0; i < trade.aNumSalv; i++) {
			int index = trade.aSalv[i].iCol;

			if (index >= 0 &&
			    index < eaSize(&e->pchar->salvageInv) &&
			    e->pchar->salvageInv[index])
				y = drawTradeOfferLine(
					NULL, NULL,
					e->pchar->salvageInv[index]->salvage,
					NULL, trade.aSalv[i].iNumCombines, x, y,
					width, bottom, hudScale);
		}
		for (i = 0; i < trade.aNumReci; i++)
			y = drawTradeOfferLine(
				NULL, NULL, NULL,
				recipe_GetItemById(trade.aReci[i].iCol),
				trade.aReci[i].iNumCombines, x, y, width,
				bottom, hudScale);
		for (i = 0; i < trade.aNumTpow; i++)
			y = drawTradeOfferLine(trade.aTpow[i].ppow, NULL, NULL,
					       NULL, 1, x, y, width, bottom,
					       hudScale);
	}
}

void drawTradeScreen(Entity *e, int width, int height, F32 hudScale)
{
	static const Prompt listPrompts[] = {
		{ GP_A, "Add" },	  { GP_Y, "Take back" },
		{ GP_X, "Influence" },	  { -1, "Start Offer" },
		{ GP_B, "Cancel trade" }, { -1, "LB  RB  Tabs" }
	};
	static const Prompt offeredPrompts[] = {
		{ GP_A, "Add" },	  { GP_Y, "Take back" },
		{ GP_X, "Influence" },	  { -1, "Start Withdraw offer" },
		{ GP_B, "Cancel trade" }, { -1, "LB  RB  Tabs" }
	};
	static const Prompt editPrompts[] = { { -1, "Left  Right  Digit" },
					      { -1, "Up  Down  Change" },
					      { GP_A, "Done" },
					      { GP_B, "Back" } };
	Entity *partner = entFromDbId(trade.partnerDbid);
	F32 panelWidth = SCREEN_PANEL_WIDTH * hudScale;
	F32 panelHeight = 760 * hudScale;
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 listLeft = left + 36 * hudScale;
	F32 listWidth = 560 * hudScale;
	F32 listTop = top + 150 * hudScale;
	F32 rowHeight = TRADE_ROW_HEIGHT * hudScale;
	F32 offerWidth =
		(left + panelWidth - 36 * hudScale -
		 (listLeft + listWidth + 40 * hudScale) - 30 * hudScale) /
		2;
	F32 mineLeft = listLeft + listWidth + 40 * hudScale;
	F32 theirsLeft = mineLeft + offerWidth + 30 * hudScale;
	F32 offerTop = top + 110 * hudScale;
	F32 offerBottom = top + panelHeight - 150 * hudScale;
	F32 tabY = top + 96 * hudScale;
	int mouseMoved =
		gMouseInpCur.x != tradeMouseX || gMouseInpCur.y != tradeMouseY;
	char title[128];
	int i;

	tradeMouseX = gMouseInpCur.x;
	tradeMouseY = gMouseInpCur.y;
	collisions_off_for_rest_of_frame = bindCollisionsBefore;
	// While it asks about cancelling, the mouse cannot reach the screen
	// behind.
	if (tradeConfirming)
		collisions_off_for_rest_of_frame = 1;

	tradeBuildItems(e);
	if (tradeSelection < tradeTopRow)
		tradeTopRow = tradeSelection;
	else if (tradeSelection >= tradeTopRow + TRADE_ROWS)
		tradeTopRow = tradeSelection - TRADE_ROWS + 1;

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x000000b0);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	snprintf(title, sizeof(title), "Trade with %s",
		 partner ? partner->name : "...");
	title[sizeof(title) - 1] = 0;
	drawText(&game_18, left + panelWidth / 2, top + 44 * hudScale,
		 BIND_Z + 4, hudScale * 1.3f, CLR_WHITE, CENTER_X | CENTER_Y,
		 title);

	// Your items, a tab at a time.
	for (i = 0; i < TRADE_TAB_COUNT; i++) {
		F32 tabWidth = listWidth / TRADE_TAB_COUNT;
		F32 x = listLeft + i * tabWidth;
		CBox box;

		BuildCBox(&box, x + 2 * hudScale, tabY - 16 * hudScale,
			  tabWidth - 4 * hudScale, 32 * hudScale);
		if (!tradeEditing && mouseClickHit(&box, MS_LEFT)) {
			tradeTab = i;
			tradeSelection = 0;
			tradeTopRow = 0;
		}
		drawFlatFrame(PIX2, 8, x + 2 * hudScale, tabY - 16 * hudScale,
			      BIND_Z + 2, tabWidth - 4 * hudScale,
			      32 * hudScale, hudScale,
			      i == tradeTab ? FRAME_COLOR_HELD
					    : FRAME_COLOR_DIM,
			      i == tradeTab ? 0x405878f0 : 0x101820e0);
		drawText(&game_12, x + tabWidth / 2, tabY, BIND_Z + 4, hudScale,
			 i == tradeTab ? FRAME_COLOR_HELD : 0xffffffa0,
			 CENTER_X | CENTER_Y, tradeTabNames[i]);
	}
	drawText(&game_12, listLeft, listTop - 18 * hudScale, BIND_Z + 4,
		 hudScale, 0xffffffa0, CENTER_Y, "Your items");

	if (!tradeItemCount)
		drawText(&game_14, listLeft + 12 * hudScale,
			 listTop + rowHeight / 2, BIND_Z + 4, hudScale,
			 0xffffffa0, CENTER_Y, "Nothing here to trade.");

	for (i = tradeTopRow;
	     i < tradeItemCount && i < tradeTopRow + TRADE_ROWS; i++) {
		const TradeItem *item = &tradeItems[i];
		F32 y = listTop + (i - tradeTopRow) * rowHeight;
		int selected = i == tradeSelection && !tradeEditing;
		char count[48];
		CBox box;

		BuildCBox(&box, listLeft, y, listWidth,
			  rowHeight - 4 * hudScale);
		if (!tradeEditing && mouseCollision(&box)) {
			if (mouseMoved)
				tradeSelection = i;
			if (mouseClickHit(&box, MS_LEFT)) {
				tradeSelection = i;
				tradeAdd(e, item);
			} else if (mouseClickHit(&box, MS_RIGHT)) {
				tradeSelection = i;
				tradeTakeBack(item);
			}
		}

		drawFlatFrame(PIX2, 8, listLeft, y, BIND_Z + 2, listWidth,
			      rowHeight - 4 * hudScale, hudScale,
			      selected	      ? FRAME_COLOR_HELD
			      : item->offered ? 0x80ff80c0
					      : FRAME_COLOR_DIM,
			      selected	      ? 0x405878f0
			      : item->offered ? 0x20402080
					      : 0x101820e0);
		drawTradeIcon(item->insp, item->boost, item->salvage,
			      item->recipe, listLeft + 6 * hudScale,
			      y + 4 * hudScale, rowHeight - 12 * hudScale,
			      BIND_Z + 4, hudScale);
		drawText(&game_14, listLeft + rowHeight + 8 * hudScale,
			 y + (rowHeight - 4 * hudScale) / 2, BIND_Z + 4,
			 hudScale, selected ? FRAME_COLOR_HELD : CLR_WHITE,
			 CENTER_Y, tradeItemName(item));

		if (item->have > 1 || item->offered > 1)
			snprintf(count, sizeof(count),
				 item->offered ? "%d of %d offered" : "x%d",
				 item->offered ? item->offered : item->have,
				 item->have);
		else
			snprintf(count, sizeof(count),
				 item->offered ? "Offered" : "");
		count[sizeof(count) - 1] = 0;
		if (count[0])
			drawTextRight(
				&game_12, listLeft + listWidth - 12 * hudScale,
				y + (rowHeight - 4 * hudScale) / 2, BIND_Z + 4,
				hudScale,
				item->offered ? 0x80ff80ff : 0xffffffa0, count);
	}
	if (tradeItemCount > TRADE_ROWS) {
		char more[48];

		snprintf(more, sizeof(more), "%d of %d", tradeSelection + 1,
			 tradeItemCount);
		more[sizeof(more) - 1] = 0;
		drawTextRight(&game_12, listLeft + listWidth,
			      listTop - 18 * hudScale, BIND_Z + 4, hudScale,
			      0xffffffa0, more);
	}

	// Both offers, side by side.
	drawText(&game_14, mineLeft, offerTop, BIND_Z + 4, hudScale, CLR_WHITE,
		 CENTER_Y, "Your offer");
	snprintf(title, sizeof(title), "%s's offer",
		 partner ? partner->name : "Their");
	title[sizeof(title) - 1] = 0;
	drawText(&game_14, theirsLeft, offerTop, BIND_Z + 4, hudScale,
		 CLR_WHITE, CENTER_Y, title);
	drawFlatFrame(PIX2, 8, mineLeft - 10 * hudScale,
		      offerTop + 20 * hudScale, BIND_Z + 1,
		      offerWidth + 20 * hudScale,
		      offerBottom - offerTop - 10 * hudScale, hudScale,
		      FRAME_COLOR_DIM, 0x0a1018e0);
	drawFlatFrame(PIX2, 8, theirsLeft - 10 * hudScale,
		      offerTop + 20 * hudScale, BIND_Z + 1,
		      offerWidth + 20 * hudScale,
		      offerBottom - offerTop - 10 * hudScale, hudScale,
		      FRAME_COLOR_DIM, 0x0a1018e0);
	drawTradeOffer(e, 0, mineLeft, offerTop + 30 * hudScale, offerWidth,
		       offerBottom, hudScale);
	drawTradeOffer(e, 1, theirsLeft, offerTop + 30 * hudScale, offerWidth,
		       offerBottom, hudScale);

	// Where each side stands.
	{
		F32 statusY = offerBottom + 24 * hudScale;
		F32 statusHeight = 44 * hudScale;
		CBox box;

		BuildCBox(&box, mineLeft - 10 * hudScale, statusY,
			  offerWidth + 20 * hudScale, statusHeight);
		if (!tradeEditing && mouseClickHit(&box, MS_LEFT))
			consoleTradeSetOffered(!trade.aAccepted);
		drawFlatFrame(PIX3, R10, mineLeft - 10 * hudScale, statusY,
			      BIND_Z + 2, offerWidth + 20 * hudScale,
			      statusHeight, hudScale,
			      trade.aAccepted ? 0x80ff80ff : FRAME_COLOR,
			      trade.aAccepted ? 0x205020f0 : 0x203050f0);
		drawText(&game_14, mineLeft + offerWidth / 2,
			 statusY + statusHeight / 2, BIND_Z + 4, hudScale,
			 CLR_WHITE, CENTER_X | CENTER_Y,
			 trade.aAccepted ? "You offered this trade"
					 : "Start: offer this trade");

		drawFlatFrame(PIX3, R10, theirsLeft - 10 * hudScale, statusY,
			      BIND_Z + 2, offerWidth + 20 * hudScale,
			      statusHeight, hudScale,
			      !partner		? 0xff8080ff
			      : trade.bAccepted ? 0x80ff80ff
						: FRAME_COLOR_DIM,
			      !partner		? 0x502020f0
			      : trade.bAccepted ? 0x205020f0
						: 0x101820f0);
		drawText(&game_14, theirsLeft + offerWidth / 2,
			 statusY + statusHeight / 2, BIND_Z + 4, hudScale,
			 CLR_WHITE, CENTER_X | CENTER_Y,
			 !partner	   ? "They left"
			 : trade.bAccepted ? "They offered this trade"
					   : "Still deciding");
	}

	// Influence, a digit at a time.
	{
		F32 boxY = top + panelHeight - 112 * hudScale;
		F32 digitWidth = 30 * hudScale;
		F32 x = listLeft + 150 * hudScale;
		char influence[64];
		CBox box;

		drawText(&game_14, listLeft, boxY + 20 * hudScale, BIND_Z + 4,
			 hudScale, CLR_WHITE, CENTER_Y, "Influence");
		if (tradeEditing) {
			for (i = 0; i < TRADE_DIGITS; i++) {
				char digit[2];
				int chosen = i == tradeDigit;

				digit[0] = (char)('0' + tradeDigits[i]);
				digit[1] = 0;
				drawFlatFrame(
					PIX2, 6,
					x + i * (digitWidth + 4 * hudScale),
					boxY, BIND_Z + 2, digitWidth,
					40 * hudScale, hudScale,
					chosen ? FRAME_COLOR_HELD
					       : FRAME_COLOR_DIM,
					chosen ? 0x405878f0 : 0x101820e0);
				drawText(&game_18,
					 x + i * (digitWidth + 4 * hudScale) +
						 digitWidth / 2,
					 boxY + 20 * hudScale, BIND_Z + 4,
					 hudScale,
					 chosen ? FRAME_COLOR_HELD : CLR_WHITE,
					 CENTER_X | CENTER_Y, digit);
			}
			snprintf(influence, sizeof(influence), "You have %s",
				 itoa_with_commas_static(
					 e->pchar->iInfluencePoints));
		} else {
			BuildCBox(&box, x, boxY,
				  9 * (digitWidth + 4 * hudScale),
				  40 * hudScale);
			if (mouseClickHit(&box, MS_LEFT))
				tradeStartEditing();
			drawFlatFrame(PIX2, 6, x, boxY, BIND_Z + 2,
				      9 * (digitWidth + 4 * hudScale),
				      40 * hudScale, hudScale, FRAME_COLOR_DIM,
				      0x101820e0);
			drawText(&game_18, x + 12 * hudScale,
				 boxY + 20 * hudScale, BIND_Z + 4, hudScale,
				 0xffe060ff, CENTER_Y,
				 itoa_with_commas_static(trade.aInfluence));
			snprintf(influence, sizeof(influence),
				 "X to change. You have %s",
				 itoa_with_commas_static(
					 e->pchar->iInfluencePoints));
		}
		influence[sizeof(influence) - 1] = 0;
		drawText(&game_12, x, boxY + 56 * hudScale, BIND_Z + 4,
			 hudScale, 0xffffffa0, CENTER_Y, influence);
	}

	if (tradeConfirming) {
		static const Prompt confirmPrompts[] = {
			{ GP_A, "Yes, cancel it" }, { GP_B, "No" }
		};
		F32 boxWidth = 520 * hudScale;
		F32 boxHeight = 150 * hudScale;
		F32 boxLeft = left + (panelWidth - boxWidth) / 2;
		F32 boxTop = top + (panelHeight - boxHeight) / 2;

		display_sprite(white_tex_atlas, left, top, BIND_Z + 20,
			       panelWidth / white_tex_atlas->width,
			       panelHeight / white_tex_atlas->height,
			       0x00000090);
		drawFlatFrame(PIX3, R10, boxLeft, boxTop, BIND_Z + 22, boxWidth,
			      boxHeight, hudScale, FRAME_COLOR_HELD,
			      0x101820f8);
		drawText(&game_18, boxLeft + boxWidth / 2,
			 boxTop + 50 * hudScale, BIND_Z + 24, hudScale,
			 CLR_WHITE, CENTER_X | CENTER_Y, "Cancel this trade?");
		drawPrompts(confirmPrompts, ARRAY_SIZE(confirmPrompts),
			    boxLeft + boxWidth / 2, boxTop + 108 * hudScale,
			    BIND_Z + 24, hudScale);
	}

	if (tradeEditing)
		drawPrompts(editPrompts, ARRAY_SIZE(editPrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 26 * hudScale, BIND_Z + 6,
			    hudScale);
	else if (trade.aAccepted)
		drawPrompts(offeredPrompts, ARRAY_SIZE(offeredPrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 26 * hudScale, BIND_Z + 6,
			    hudScale);
	else
		drawPrompts(listPrompts, ARRAY_SIZE(listPrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 26 * hudScale, BIND_Z + 6,
			    hudScale);
}


// Outlines over every piece, and a panel at the top with Save, Cancel and Reset
// all. A piece is dragged by its body to move it and by its corner to size it.
// A right click puts it back where it started.
void drawHudEdit(int screenWidth, F32 hudScale)
{
	F32 z = BIND_Z;
	F32 panelWidth = 760 * hudScale;
	F32 panelHeight = 104 * hudScale;
	F32 panelX = (screenWidth - panelWidth) / 2;
	F32 panelY = 40 * hudScale;
	F32 buttonWidth = 130 * hudScale;
	F32 buttonHeight = 30 * hudScale;
	F32 buttonY = panelY + panelHeight - buttonHeight - 12 * hudScale;
	F32 handle = HUD_EDIT_HANDLE * hudScale;
	int hovered = -1;
	int overPanel;
	CBox panel;
	int n;

	BuildCBox(&panel, panelX, panelY, panelWidth, panelHeight);
	overPanel = mouseCollision(&panel);

	// A drag follows the mouse until the button is let go.
	if (hudDragPiece >= 0) {
		if (!isDown(MS_LEFT)) {
			hudDragPiece = -1;
		} else {
			HudPlace *place = &hudLayout[hudDragPiece];
			F32 dx = (F32)(gMouseInpCur.x - hudDragX);
			F32 dy = (F32)(gMouseInpCur.y - hudDragY);

			// The corner stays under the mouse: the size grows by
			// how far it has been pulled past the point the piece
			// is drawn from.
			if (hudDragSizing)
				place->size = hudClampSize(
					hudDragReach > 0
						? hudDragFrom.size *
							  (hudDragReach + dx) /
							  hudDragReach
						: hudDragFrom.size);
			else {
				place->x = hudDragFrom.x + dx / hudScale;
				place->y = hudDragFrom.y + dy / hudScale;
			}
			hovered = hudDragPiece;
		}
	} else if (!overPanel) {
		// The chosen piece is tried first, since pieces can overlap.
		for (n = 0; n < HUD_PIECE_COUNT; n++) {
			int i = (hudEditPiece + n) % HUD_PIECE_COUNT;
			HudRect *r = &hudDrawn[i];
			CBox box, corner;

			if (!hudShown[i])
				continue;

			BuildCBox(&box, r->x, r->y, r->width, r->height);
			BuildCBox(&corner, r->x + r->width - handle,
				  r->y + r->height - handle, handle, handle);

			if (mouseDownHit(&box, MS_LEFT)) {
				hudEditPiece = i;
				hudDragPiece = i;
				hudDragSizing = mouseDownHit(&corner, MS_LEFT);
				hudDragX = gMouseInpCur.x;
				hudDragY = gMouseInpCur.y;
				hudDragFrom = hudLayout[i];
				hudDragReach = r->x + r->width - hudAnchorX[i];
				hovered = i;
				break;
			}
			if (mouseClickHit(&box, MS_RIGHT)) {
				hudEditPiece = i;
				hudResetPiece(&hudLayout[i]);
				break;
			}
			if (hovered < 0 && mouseCollision(&box))
				hovered = i;
		}
	}

	for (n = 0; n < HUD_PIECE_COUNT; n++) {
		HudRect *r = &hudDrawn[n];
		int chosen = n == hudEditPiece;
		int frameColor = chosen		? FRAME_COLOR_HELD
				 : n == hovered ? CLR_WHITE
						: 0x80b8ffd0;
		char label[64];
		F32 labelWidth;

		if (!hudShown[n])
			continue;

		drawFlatFrame(PIX2, 6, r->x, r->y, z, r->width, r->height,
			      hudScale, frameColor,
			      hudLayout[n].hidden ? 0x00000090
			      : chosen		  ? 0xffe08024
						  : 0x4080ff22);
		drawFlatFrame(PIX2, 2, r->x + r->width - handle,
			      r->y + r->height - handle, z + 1, handle, handle,
			      hudScale, frameColor,
			      chosen ? 0xffe080a0 : 0x80b8ff70);

		if (hudLayout[n].hidden)
			snprintf(label, sizeof(label), "%s (hidden)",
				 gamepadLabelText(hudPieces[n].label));
		else if (n == HUD_PIECE_TEAM &&
			 hudBoxes[n].height == hudPieces[n].box.height &&
			 !(playerPtr()->teamup &&
			   playerPtr()->teamup->members.count > 1))
			snprintf(label, sizeof(label), "%s (shown on a team)",
				 gamepadLabelText(hudPieces[n].label));
		else if (hudLayout[n].size < 0.995f ||
			 hudLayout[n].size > 1.005f)
			snprintf(label, sizeof(label), "%s  %d%%",
				 gamepadLabelText(hudPieces[n].label),
				 (int)(hudLayout[n].size * 100 + 0.5f));
		else
			snprintf(label, sizeof(label), "%s",
				 gamepadLabelText(hudPieces[n].label));

		labelWidth = str_wd_notranslate(&game_12, hudScale, hudScale,
						label) +
			     20 * hudScale;
		drawFlatFrame(PIX2, 6, r->x + (r->width - labelWidth) / 2,
			      r->y - 11 * hudScale, z + 2, labelWidth,
			      22 * hudScale, hudScale, frameColor, 0x101820f0);
		drawText(&game_12, r->x + r->width / 2, r->y, z + 3, hudScale,
			 frameColor, CENTER_X | CENTER_Y, label);
	}

	drawFlatFrame(PIX3, R10, panelX, panelY, z + 10, panelWidth,
		      panelHeight, hudScale, FRAME_COLOR_HELD, 0x0c141cf0);
	drawText(&game_18, panelX + panelWidth / 2, panelY + 20 * hudScale,
		 z + 12, hudScale, FRAME_COLOR_HELD, CENTER_X | CENTER_Y,
		 "Edit HUD");
	drawText(&game_12, panelX + panelWidth / 2, panelY + 44 * hudScale,
		 z + 12, hudScale, 0xffffffc0, CENTER_X | CENTER_Y,
		 "Drag a piece to move it, or its corner to size it. Right "
		 "click puts it back.");

	if (drawHudEditButton(
		    "Save",
		    panelX + panelWidth / 2 - buttonWidth * 2 - 15 * hudScale,
		    buttonY, buttonWidth, buttonHeight, z + 12, hudScale))
		hudEditClose(1);
	else if (drawHudEditButton(
			 "Cancel",
			 panelX + panelWidth / 2 - buttonWidth - 5 * hudScale,
			 buttonY, buttonWidth, buttonHeight, z + 12, hudScale))
		hudEditClose(0);
	else if (drawHudEditButton(
			 hudLayout[hudEditPiece].hidden ? "Show piece"
							: "Hide piece",
			 panelX + panelWidth / 2 + 5 * hudScale, buttonY,
			 buttonWidth, buttonHeight, z + 12, hudScale))
		hudLayout[hudEditPiece].hidden =
			!hudLayout[hudEditPiece].hidden;
	else if (drawHudEditButton("Reset all",
				   panelX + panelWidth / 2 + buttonWidth +
					   15 * hudScale,
				   buttonY, buttonWidth, buttonHeight, z + 12,
				   hudScale)) {
		for (n = 0; n < HUD_PIECE_COUNT; n++)
			hudResetPiece(&hudLayout[n]);
	}
}
