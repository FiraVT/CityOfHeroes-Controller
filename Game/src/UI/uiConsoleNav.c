/*
 * The console Navigation and Social screens, which host the map, contacts,
 * missions and social windows.
 */
#include "UI/uiConsoleHudInternal.h"

int consoleNavIsOpen(void)
{
	return navOpen && consoleHudActive();
}

// A window a tab opens of its own, like the Contact Finder from Find Contact.
// It is drawn under the screen.
int navSideWindowUp(void)
{
	int mode = window_getMode(WDW_CONTACT_FINDER);

	return mode == WINDOW_DISPLAYING || mode == WINDOW_GROWING;
}

// A popup, or a window a tab opened, is under this screen, so while one is up
// the screen steps aside until it is answered or closed.
int navCovers(void)
{
	return consoleNavIsOpen() &&
	       window_getMode(WDW_DIALOG) != WINDOW_DISPLAYING &&
	       !navSideWindowUp();
}

static int navWindow(void)
{
	return navTabs[navTab].window;
}

int consoleNavOnMap(void)
{
	return consoleNavIsOpen() && navWindow() == WDW_MAP;
}

int consoleNavHostedWindow(void)
{
	return navCovers() ? navWindow() : -1;
}

int consoleNavShowsWindow(int window)
{
	return consoleNavIsOpen() && navWindow() == window;
}

// Tabs with nothing in them are left out: no Contacts before the first one, no
// Clues before the first clue, no League or Supergroup outside one. Team stays:
// alone it sets looking for a team.
static int navTabHasContent(int tab)
{
	Entity *e = playerPtr();

	switch (navTabs[tab].window) {
	case WDW_CONTACT:
		return eaSize(&contacts) > 0;
	case WDW_CLUE:
		return clue_HasAny();
	case WDW_LEAGUE:
		return e && e->league_id;
	case WDW_SUPERGROUP:
		return e && e->supergroup_id;
	}
	return 1;
}

static void navSelect(int tab)
{
	navTab = tab;
	navLastTab[navScreen] = tab;
	navFocusFrames = 2;
	if (navTabs[tab].window == WDW_MAP)
		automapPadStart();
}

static void navStep(int direction)
{
	int tab = navTab;
	int i;

	for (i = 0; i < navTabCount; i++) {
		tab = (tab + direction + navTabCount) % navTabCount;
		if (navTabHasContent(tab)) {
			navSelect(tab);
			return;
		}
	}
}

// The windows put away for the screen come back as they were, without taking
// the pad.
static void navRestoreWindows(void)
{
	int i;

	for (i = 0; i < navTabCount; i++) {
		if (navReopen[i]) {
			navReopen[i] = 0;
			gamepadQuietOpen(navTabs[i].window);
			window_setMode(navTabs[i].window, WINDOW_DISPLAYING);
		}
	}
}

void navClose(void)
{
	navOpen = 0;
	navRestoreWindows();
}

static void navOpenScreen(int screen, int tab)
{
	int i;

	hudEditDiscard();
	menuOpen = 0;
	bindOpen = 0;
	slotOpen = 0;
	incOpen = 0;
	invOpen = 0;

	if (navOpen && screen != navScreen)
		navClose();

	navScreen = screen;

	if (!navOpen) {
		// A tab's window open by itself would be drawn twice, so it is
		// put away while the screen is up.
		for (i = 0; i < navTabCount; i++) {
			int mode = window_getMode(navTabs[i].window);

			navReopen[i] = mode == WINDOW_DISPLAYING ||
				       mode == WINDOW_GROWING;
			if (navReopen[i])
				window_setMode(navTabs[i].window,
					       WINDOW_DOCKED);
		}
		navOpen = 1;
	}

	if (tab < 0)
		tab = navLastTab[screen];
	if (!navTabHasContent(tab))
		tab = 0;
	navSelect(tab);
}

void consoleNavOpen(const char *tabName)
{
	int screen, i;

	for (screen = 0; screen < NAV_SCREEN_COUNT; screen++) {
		for (i = 0; tabName && i < navScreens[screen].count; i++) {
			const NavTab *tab = &navScreens[screen].tabs[i];

			if (stricmp(tabName, tab->name) &&
			    stricmp(tabName, tab->label))
				continue;

			// Without the console HUD, the tab's own window opens
			// as it always has.
			if (!consoleHudActive()) {
				window_setMode(tab->window, WINDOW_GROWING);
				gamepadFocusOnWindow(tab->window);
				return;
			}

			navOpenScreen(screen, i);
			return;
		}
	}

	// No tab by that name: the Navigation screen where it was left.
	if (!consoleHudActive()) {
		window_setMode(WDW_MAP, WINDOW_GROWING);
		gamepadFocusOnWindow(WDW_MAP);
		return;
	}
	navOpenScreen(0, -1);
}

static void navToggleScreen(int screen)
{
	if (consoleNavIsOpen() && navScreen == screen)
		navClose();
	else if (!consoleHudActive())
		consoleNavOpen(
			navScreens[screen].tabs[navLastTab[screen]].name);
	else
		navOpenScreen(screen, -1);
}

void consoleNavToggle(void)
{
	navToggleScreen(0);
}

void consoleSocialToggle(void)
{
	navToggleScreen(1);
}

int consoleNavPress(int button)
{
	// A menu or popup from inside a tab answers to the pad the usual way,
	// and nothing else gets through.
	if (!navCovers() || contextMenu_IsActive()) {
		switch (button) {
		case GP_A:
		case GP_X:
		case GP_B:
		case GP_R3:
		case GP_LB:
		case GP_RB:
		case GP_LT:
		case GP_RT:
		case GP_DPAD_UP:
		case GP_DPAD_DOWN:
		case GP_DPAD_LEFT:
		case GP_DPAD_RIGHT:
			return 0;
		}
		return !uiFocusIsEditable() || button != GP_Y;
	}

	switch (button) {
	case GP_LB:
	case GP_RB:
		navStep(bumperDirection(button));
		return 1;
	case GP_B:
		// An open drop down list shuts first, as a context menu does.
		if (!consoleCloseContextMenu())
			navClose();
		return 1;
	case GP_START:
		navClose();
		return 1;
	case GP_Y:
		// A text field in a tab, like the search box, gets the keyboard
		// as anywhere else.
		if (uiFocusIsEditable())
			return 0;
		if (navWindow() == WDW_MAP)
			automapPadSwitchView();
		else if (navWindow() == WDW_LFG)
			lfg_padInvite();
		else if (navWindow() == WDW_FRIENDS)
			friend_padInvite();
		return 1;
	case GP_A:
	case GP_X:
	case GP_R3:
	case GP_DPAD_UP:
	case GP_DPAD_DOWN:
	case GP_DPAD_LEFT:
	case GP_DPAD_RIGHT:
		return 0;
	}

	// Nothing else reaches the game behind the screen.
	return 1;
}

void consoleNavMapControl(F32 lookX, F32 lookY, F32 lookMagnitude, int zoomOut,
			  int zoomIn, F32 seconds)
{
	if (!consoleNavOnMap() || !navCovers())
		return;

	if (lookMagnitude > 0) {
		F32 speed = 1000.0f * navMapScale * lookMagnitude * seconds;

		automapPadPan(-lookX * speed, -lookY * speed);
	}

	if (zoomIn != zoomOut)
		automapPadZoom(powf(2.0f, (zoomIn ? 1.4f : -1.4f) * seconds));
}

void consoleNavMapNudge(int directionX, int directionY)
{
	if (!consoleNavOnMap() || !navCovers())
		return;

	automapPadPan(-directionX * navContentWidth / 3,
		      -directionY * navContentHeight / 3);
}

void drawNavScreen(int width, int height, F32 hudScale)
{
	static const Prompt mapPrompts[] = {
		{ GP_A, "Waypoint" },	 { GP_X, "Pin" },
		{ GP_Y, "City / Zone" }, { GP_B, "Close" },
		{ -1, "LT / RT Zoom" },	 { -1, "Right stick Move map" }
	};
	static const Prompt searchPrompts[] = { { GP_A, "Select" },
						{ GP_Y, "Invite" },
						{ GP_X, "Options" },
						{ GP_B, "Close" },
						{ -1, "Right stick Scroll" } };
	static const Prompt listPrompts[] = { { GP_A, "Select" },
					      { GP_X, "Options" },
					      { GP_B, "Close" },
					      { -1, "Right stick Scroll" } };
	F32 panelWidth = MIN(1560 * hudScale, width - 32.0f);
	F32 panelHeight = MIN(960 * hudScale, height - 32.0f);
	F32 left = (width - panelWidth) / 2;
	F32 top = (height - panelHeight) / 2;
	F32 tabY = top + 34 * hudScale;
	// Whole pixels: windows keep their place in whole pixels, and the map
	// creeps a pixel a frame when its window sits between two.
	F32 contentLeft = floorf(left + 24 * hudScale);
	F32 contentTop = floorf(top + 74 * hudScale);
	F32 contentWidth = floorf(panelWidth - 48 * hudScale);
	F32 contentHeight = floorf(panelHeight - 74 * hudScale - 60 * hudScale);
	F32 tabWidth = NAV_TAB_WIDTH * hudScale;
	F32 tabHeight = NAV_TAB_HEIGHT * hudScale;
	F32 tabStep = tabWidth + 8 * hudScale;
	F32 tabX;
	int shown = 0;
	int i;

	navMapScale = hudScale;
	navContentWidth = contentWidth;
	navContentHeight = contentHeight;

	// The tab in use may have emptied, like the last clue being used or
	// leaving the league.
	if (!navTabHasContent(navTab))
		navStep(1);

	for (i = 0; i < navTabCount; i++)
		shown += navTabHasContent(i);

	// Everything on the screen answers the mouse, and the windows under it
	// were told not to.
	collisions_off_for_rest_of_frame = bindCollisionsBefore;

	display_sprite(white_tex_atlas, 0, 0, BIND_Z - 2,
		       (F32)width / white_tex_atlas->width,
		       (F32)height / white_tex_atlas->height, 0x000000a0);
	drawFlatFrame(PIX3, R10, left, top, BIND_Z, panelWidth, panelHeight,
		      hudScale, FRAME_COLOR, 0x101820f0);
	drawText(&game_18, left + 28 * hudScale, tabY, BIND_Z + 4, hudScale,
		 CLR_WHITE, CENTER_Y, navScreens[navScreen].title);

	// The tabs, centred, with the bumpers either side of them.
	tabX = left + panelWidth / 2 - (shown * tabStep - 8 * hudScale) / 2;
	drawText(&game_12, tabX - 30 * hudScale, tabY, BIND_Z + 4, hudScale,
		 0xffffffc0, CENTER_X | CENTER_Y, "LB");
	for (i = 0; i < navTabCount; i++) {
		int selected = i == navTab;
		CBox box;

		if (!navTabHasContent(i))
			continue;

		BuildCBox(&box, tabX, tabY - tabHeight / 2, tabWidth,
			  tabHeight);
		if (mouseClickHit(&box, MS_LEFT))
			navSelect(i);

		drawFlatFrame(PIX3, R10, tabX, tabY - tabHeight / 2, BIND_Z + 2,
			      tabWidth, tabHeight, hudScale,
			      selected ? FRAME_COLOR_HELD : FRAME_COLOR,
			      selected ? 0x405878f0 : 0x101820e0);
		drawText(&game_14, tabX + tabWidth / 2, tabY, BIND_Z + 4,
			 hudScale, selected ? FRAME_COLOR_HELD : CLR_WHITE,
			 CENTER_X | CENTER_Y, navTabs[i].label);
		tabX += tabStep;
	}
	drawText(&game_12, tabX + 22 * hudScale, tabY, BIND_Z + 4, hudScale,
		 0xffffffc0, CENTER_X | CENTER_Y, "RB");

	// The League tab opens on the grid of every team; its list view showed
	// one team of eight at a time.
	{
		static int lastHosted = -1;

		if (navWindow() == WDW_LEAGUE && lastHosted != WDW_LEAGUE)
			league_useGridView();
		lastHosted = navWindow();
	}
	// Its grid of six teams used a quarter of the room at the usual size,
	// so it is drawn larger.
	window_drawHosted(navWindow(), contentLeft, contentTop, BIND_Z + 6,
			  contentWidth, contentHeight,
			  navWindow() == WDW_LEAGUE ? hudScale * 1.6f
						    : hudScale,
			  FRAME_COLOR, 0x0c141cf0);

	// Once the tab has drawn, the pad starts on it: on the map at the
	// player, elsewhere on the first entry.
	if (navFocusFrames > 0 && --navFocusFrames == 0) {
		if (navWindow() == WDW_MAP) {
			float x, y;

			if (automapPadPlayerPos(&x, &y))
				gamepadPointAt((int)x, (int)y);
			else
				gamepadCenterPointer();
		} else if (navTabs[navTab].subTabs) {
			gamepadRequestFocusBelow(contentTop + 30 * hudScale);
		} else {
			gamepadRequestFocus();
		}
	}

	if (navWindow() == WDW_MAP)
		drawPrompts(mapPrompts, ARRAY_SIZE(mapPrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 30 * hudScale, BIND_Z + 4,
			    hudScale);
	else if (navWindow() == WDW_LFG || navWindow() == WDW_FRIENDS)
		drawPrompts(searchPrompts, ARRAY_SIZE(searchPrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 30 * hudScale, BIND_Z + 4,
			    hudScale);
	else
		drawPrompts(listPrompts, ARRAY_SIZE(listPrompts),
			    left + panelWidth / 2,
			    top + panelHeight - 30 * hudScale, BIND_Z + 4,
			    hudScale);

	// Nothing behind the screen hears this click, including the world.
	collisions_off_for_rest_of_frame = 1;
}

#undef navTabs
#undef navTabCount
