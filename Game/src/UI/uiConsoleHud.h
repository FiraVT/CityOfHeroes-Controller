/* File uiConsoleHud.h
 *    A heads-up display arranged like the gamepad in the player's hands. Each
 * trigger layer gets a cluster shaped like the D-pad and face buttons, showing
 * the powers those buttons fire, with a radial main menu and button prompts.
 * /console_ui switches between this and the standard tray.
 */
#ifndef UICONSOLEHUD_H__
#define UICONSOLEHUD_H__

#include <utilitieslib/stdtypes.h>

C_DECLARATIONS_BEGIN

// True while the console HUD has replaced the standard tray and status bars.
int consoleHudActive(void);
int consoleHudHidesWindow(int window);
// Windows the console HUD replaces outright, whose code is not run from the
// window list at all.
int consoleHudSkipsWindow(int window);
void consoleHudDraw(void);

int consoleMenuIsOpen(void);
// Opens the radial menu when the console HUD is on, or the standard menu
// otherwise.
void consoleMenuToggle(void);
void consoleMenuClose(void);
void consoleMenuConfirm(void);
void consoleMenuSteer(F32 x, F32 y);
// LB or RB turns the radial menu to its other page.
void consoleMenuPage(int button);

// A screen listing the character's powers, where one is picked and then bound
// by pressing a button.
int consoleBindIsOpen(void);
// Opens it when the console HUD is on, or the standard powers window otherwise.
void consoleBindToggle(void);
void consoleBindPress(int modifiers, int button);
void consoleBindSteer(int dpadButton);
// The right stick, while a power's description is open beside the list. True
// when it scrolled it.
int consoleBindLook(F32 y, F32 seconds);
// The screen covers the windows, so they must not take the mouse while it is
// up.
void consoleBindBlockWindows(void);

// A screen for slotting enhancements: pick one from the inventory, then a power
// slot it fits. It takes the pad through the binding screen's calls above.
// Without the console HUD this opens Manage instead.
int consoleSlotIsOpen(void);
void consoleSlotToggle(void);

// A screen for Incarnates: equip the ten slots, then make and convert
// abilities, with LB and RB between the three. It takes the pad the same way.
// Without the console HUD this opens the game's Incarnate window.
int consoleIncarnateIsOpen(void);
void consoleIncarnateToggle(void);

// A screen for inventions: build recipes, look through the recipe inventory and
// clear out salvage, with LB and RB between the three. It takes the pad the
// same way. Without the console HUD this opens the game's recipe window
// instead.
int consoleInventionIsOpen(void);
void consoleInventionToggle(void);
// An invention table the server has opened. Returns 1 when the screen takes it,
// so the game's recipe window stays shut; tables of other kinds still get that
// window.
int consoleInventionTableOpened(void);
void consoleInventionTableClosed(void);
// True while the screen holds a table open, so the game's recipe window does
// not close it.
int consoleInventionHoldsTable(void);

// The Navigation screen: Map, Missions, Contacts, Clues and Badges as tabs, LB
// and RB between them. Each tab is the game's own window drawn inside it.
// tabName picks a tab by name, or NULL for the last one. Without the console
// HUD this opens that window instead.
void consoleNavOpen(const char *tabName);
void consoleNavToggle(void);
// The Social screen: Search, Team, Friends, League and Supergroup as tabs.
void consoleSocialToggle(void);
int consoleNavIsOpen(void);
int consoleNavOnMap(void);
// The window the pad's focus stays inside, or -1 when the screen is closed or a
// popup is in front of it.
int consoleNavHostedWindow(void);
// True while one of the console screens is open on this window's tab.
int consoleNavShowsWindow(int window);
// A button for the screen. True when it was used; false leaves it to the usual
// menu handling.
int consoleNavPress(int button);
// The right stick moves the map and the triggers zoom it.
void consoleNavMapControl(F32 lookX, F32 lookY, F32 lookMagnitude, int zoomOut,
			  int zoomIn, F32 seconds);
// Moves the map a third of the view in a D-pad direction, each of directionX
// and directionY -1, 0 or 1.
void consoleNavMapNudge(int directionX, int directionY);

// An edit mode for the play HUD: each piece can be dragged to move it and by
// its corner to size it, or moved with the D-pad and sized with Y and X. A and
// Save keep the layout in console_hud.txt; B and Cancel put back the one it
// started with. It takes the pad the same way as the screens above. Only for
// the console HUD.
void consoleHudEditToggle(void);
int consoleHudEditIsOpen(void);

// Big tiles in the middle of the screen for what the menu ring does not open:
// Options, Costumes, Quit and the rest of the game's own menu. Only for the
// console HUD.
void consoleGameMenuToggle(void);
int consoleGameMenuIsOpen(void);

// The inspiration tray as a console screen: use them, and combine three of the
// same into another.
void consoleInspirationsToggle(void);

// Moves the selection through the team and league members shown on the HUD:
// "up", "down", "left" or "right".
void consoleTeamStep(const char *direction);

// A highlight on whatever has D-pad focus, here and on the front end.
void consoleFocusDraw(void);
// True while a dialog box or context menu is on top, which the pad must deal
// with first.
int consoleOverlayActive(void);
int consoleCloseContextMenu(void);
// Opens the current target's right click menu in the middle of the screen, for
// the pad to work.
void consoleTargetMenu(void);
// Y: the target's menu with something targeted, the menu wheel without.
void consoleTargetOrMenu(void);
int consoleWorldTargetActive(void);
// True when the binding fires an attack on a foe and nothing it can use is
// targeted.
int consoleAttackNeedsTarget(const char *command);
// Where aiming a location power starts, in client pixels. False when that point
// is off screen.
int consoleWorldTargetAimPoint(int *x, int *y);

// An on-screen keyboard that types into whichever text field has focus, here
// and on the front end.
int gamepadKeyboardIsOpen(void);
void gamepadKeyboardOpen(void);
void gamepadKeyboardClose(void);
void gamepadKeyboardPress(int button);
void gamepadKeyboardDraw(void);

C_DECLARATIONS_END

#endif
