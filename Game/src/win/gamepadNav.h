/* File gamepadNav.h
 *    Lets the D-pad move focus between things on screen in any window. The UI
 * is immediate mode and every button asks the mouse functions whether the
 * cursor is over it, so those functions report each box here, tagged with the
 * window being drawn, and no window needs code of its own.
 */
#ifndef _GAMEPADNAV_H
#define _GAMEPADNAV_H

#ifdef __cplusplus
extern "C" {
#endif

// The tag for boxes drawn outside the window list, such as context menus. Popup
// dialogs carry WDW_DIALOG.
#define NAV_OVERLAY -1
// The tag for a context menu's items, so nothing else on screen competes with
// them.
#define NAV_CONTEXT_MENU -2
// A tag nothing navigates to, for a control the pad works another way, like the
// map's zoom slider.
#define NAV_IGNORED -3
// The tag for everything a window draws inside another screen's box, like a tab
// of the console Navigation screen, so nothing that window reports from
// anywhere else is mistaken for it.
#define NAV_HOSTED -4

typedef struct NavPoint {
	int x;
	int y;
} NavPoint;

typedef struct NavRect {
	float left;
	float top;
	float right;
	float bottom;
} NavRect;

// Starts collecting a new frame's boxes. The last complete frame stays
// available for picking.
void gamepadNavBeginFrame(void);

// Which window the next boxes belong to.
void gamepadNavSetWindow(int window);
int gamepadNavGetWindow(void);

// Reports a box the UI hit tested this frame. ui is in the UI's own
// coordinates, for drawing, and screen is in client pixels, for moving the
// cursor. They differ on the front end menus. blocked is nonzero when mouse
// collisions were already off for the rest of the frame, so the mouse could
// not have hit it: a window above has the mouse, or a control earlier in the
// same window, such as a hovered help button, took it. A blocked box is only
// a target for the window that drew it, never through the screen area rule.
void gamepadNavAddBox(const NavRect *ui, const NavRect *screen, int blocked);

// True when the last frame drew anything for this window. A popup can be open
// but not drawn, as on the full screen menus, and must not trap the D-pad then.
int gamepadNavHasWindow(int window);

// Where the windows being navigated sit on screen, in client pixels. A box
// inside one of these counts as theirs whatever window drew it.
void gamepadNavSetScopeRects(const NavRect *rects, int count);

// All of these look only at boxes from the given windows, or at every box if
// windowCount is 0.

// Finds the best target from (x, y) in direction (directionX, directionY), each
// of them -1, 0 or 1. screenWidth and screenHeight let it ignore boxes that are
// really whole windows. Returns 0 if there is nothing in that direction.
int gamepadNavPick(const int *windows, int windowCount, int x, int y,
		   int directionX, int directionY, int screenWidth,
		   int screenHeight, NavPoint *target);

// The same, but with nothing lined up it takes the nearest target anywhere on
// that side. For popups, which have no lists to scroll.
int gamepadNavPickWide(const int *windows, int windowCount, int x, int y,
		       int directionX, int directionY, int screenWidth,
		       int screenHeight, NavPoint *target);

// Finds the top left target, for focus to land on when a window opens.
int gamepadNavPickSide(const int *windows, int windowCount, int x, int y,
		       int side, int screenWidth, int screenHeight,
		       NavPoint *target);
int gamepadNavFirst(const int *windows, int windowCount, int screenWidth,
		    int screenHeight, NavPoint *target);

// The same, but the first target whose top is at or below minTop, in client
// pixels, so a window's strip of sub-tabs is passed over. Falls back to the
// first target anywhere.
int gamepadNavFirstBelow(const int *windows, int windowCount, float minTop,
			 int screenWidth, int screenHeight, NavPoint *target);

// Finds the leftmost target on the lowest row, where a popup keeps its buttons.
int gamepadNavLastRow(const int *windows, int windowCount, int screenWidth,
		      int screenHeight, NavPoint *target);

// The last frame's targets, in client pixels, for following a list as it
// scrolls. Returns how many.
int gamepadNavTargets(const int *windows, int windowCount, int screenWidth,
		      int screenHeight, NavRect *out, int max);

// After a list scrolled by shift pixels (positive when its rows moved up),
// finds the row next to the one that had focus, in that row's column. One wheel
// notch can move several rows, and none is skipped.
int gamepadNavFollowScroll(const NavRect *focused, const NavRect *after,
			   int afterCount, int directionY, float shift,
			   NavPoint *target);

// Finds the target under (x, y), and gives its box in client pixels for a focus
// highlight. Menus draw in differently scaled spaces, so the highlight is drawn
// in pixels with scaling off.
int gamepadNavFocused(const int *windows, int windowCount, int x, int y,
		      int screenWidth, int screenHeight, NavRect *screenBox);

#ifdef __cplusplus
}
#endif

#endif
