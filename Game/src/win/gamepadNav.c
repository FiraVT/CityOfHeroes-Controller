#include "win/gamepadNav.h"

#include <math.h>
#include <stdlib.h>

#define NAV_MAX_BOXES 768

// A target more than this far off the line of travel, relative to its distance
// along it, is not "in that direction". Sideways distance also costs more than
// distance along the line.
#define NAV_MAX_SPREAD 2.5f
#define NAV_SPREAD_COST 2.5f

#define NAV_MIN_BOX_SIZE 6.0f
#define NAV_MAX_BOX_WIDTH_SHARE 0.6f
#define NAV_MAX_BOX_HEIGHT_SHARE 0.5f

// Reading order for first focus: rows count for far more than columns.
#define NAV_FIRST_ROW_WEIGHT 4.0f

typedef struct NavBox {
	NavRect ui;
	NavRect screen;
	int window;
	int blocked;
} NavBox;

static NavBox frames[2][NAV_MAX_BOXES];
static int frameCounts[2];
static int writing;
static int currentWindow = NAV_OVERLAY;

void gamepadNavBeginFrame(void)
{
	writing = !writing;
	frameCounts[writing] = 0;
	currentWindow = NAV_OVERLAY;
}

void gamepadNavSetWindow(int window)
{
	currentWindow = window;
}

int gamepadNavGetWindow(void)
{
	return currentWindow;
}

void gamepadNavAddBox(const NavRect *ui, const NavRect *screen, int blocked)
{
	NavBox *box;

	if (screen->right - screen->left < NAV_MIN_BOX_SIZE ||
	    screen->bottom - screen->top < NAV_MIN_BOX_SIZE ||
	    frameCounts[writing] >= NAV_MAX_BOXES)
		return;

	box = &frames[writing][frameCounts[writing]++];
	box->ui = *ui;
	box->screen = *screen;
	box->window = currentWindow;
	box->blocked = blocked;
}

static int boxContainsPoint(const NavRect *box, float x, float y)
{
	return x >= box->left && x <= box->right && y >= box->top &&
	       y <= box->bottom;
}

static int sameBox(const NavRect *a, const NavRect *b)
{
	return a->left == b->left && a->top == b->top && a->right == b->right &&
	       a->bottom == b->bottom;
}

static float boxArea(const NavRect *box)
{
	return (box->right - box->left) * (box->bottom - box->top);
}

int gamepadNavHasWindow(int window)
{
	const NavBox *boxes = frames[!writing];
	int count = frameCounts[!writing];
	int i;

	for (i = 0; i < count; i++) {
		if (boxes[i].window == window)
			return 1;
	}
	return 0;
}

#define NAV_MAX_SCOPE_RECTS 16
static NavRect scopeRects[NAV_MAX_SCOPE_RECTS];
static int scopeRectCount;

void gamepadNavSetScopeRects(const NavRect *rects, int count)
{
	int i;

	scopeRectCount =
		count < NAV_MAX_SCOPE_RECTS ? count : NAV_MAX_SCOPE_RECTS;
	for (i = 0; i < scopeRectCount; i++)
		scopeRects[i] = rects[i];
}

static int inScope(const NavBox *box, const int *windows, int windowCount)
{
	float centerX = (box->screen.left + box->screen.right) / 2;
	float centerY = (box->screen.top + box->screen.bottom) / 2;
	int i;

	if (!windowCount)
		return 1;

	for (i = 0; i < windowCount; i++) {
		if (box->window == windows[i])
			return 1;
	}

	// Some windows are drawn by another, the way the chat box draws its
	// Team and Friends tabs, so their buttons carry the other window's tag.
	// Where an open window sits on screen settles it. A blocked box belongs
	// to a window under the one in scope, so the area does not bring it in.
	// Nor does it bring in a control the pad works another way.
	if (box->blocked || box->window == NAV_IGNORED)
		return 0;
	for (i = 0; i < scopeRectCount; i++) {
		if (boxContainsPoint(&scopeRects[i], centerX, centerY))
			return 1;
	}
	return 0;
}

// A box with a smaller box inside it is a panel, not something to land on. A
// window asks about its whole body as well as about each of its buttons.
static int isContainer(const NavBox *boxes, int count, int index)
{
	const NavRect *box = &boxes[index].screen;
	int i;

	for (i = 0; i < count; i++) {
		const NavRect *other = &boxes[i].screen;

		// A tooltip drawn over a row is not a button inside it.
		if (boxes[i].window == NAV_OVERLAY &&
		    boxes[index].window != NAV_OVERLAY)
			continue;

		// Nor is a blocked button of a window underneath.
		if (boxes[i].blocked && boxes[i].window != boxes[index].window)
			continue;

		// Wholly inside: a column's resize handle straddling the edge
		// between two headers does not make either header a panel.
		if (i != index && !sameBox(box, other) &&
		    boxArea(other) < boxArea(box) && other->left >= box->left &&
		    other->right <= box->right && other->top >= box->top &&
		    other->bottom <= box->bottom) {
			return 1;
		}
	}
	return 0;
}

static int isTarget(const NavBox *boxes, int count, int index,
		    const int *windows, int windowCount, int screenWidth,
		    int screenHeight)
{
	const NavRect *box = &boxes[index].screen;

	float centerX = (box->left + box->right) / 2;
	float centerY = (box->top + box->bottom) / 2;

	// A button may run past the edge, like the front end's Next arrow and
	// its tail, and still be one.
	return inScope(&boxes[index], windows, windowCount) &&
	       box->right - box->left <=
		       screenWidth * NAV_MAX_BOX_WIDTH_SHARE &&
	       box->bottom - box->top <=
		       screenHeight * NAV_MAX_BOX_HEIGHT_SHARE &&
	       centerX >= 0 && centerY >= 0 && centerX <= screenWidth &&
	       centerY <= screenHeight && !isContainer(boxes, count, index);
}

static void boxCenter(const NavRect *box, NavPoint *center)
{
	center->x = (int)((box->left + box->right) / 2);
	center->y = (int)((box->top + box->bottom) / 2);
}

static int pick(const int *windows, int windowCount, int x, int y,
		int directionX, int directionY, int screenWidth,
		int screenHeight, NavPoint *target, int allowWide);

int gamepadNavPick(const int *windows, int windowCount, int x, int y,
		   int directionX, int directionY, int screenWidth,
		   int screenHeight, NavPoint *target)
{
	return pick(windows, windowCount, x, y, directionX, directionY,
		    screenWidth, screenHeight, target, 0);
}

int gamepadNavPickWide(const int *windows, int windowCount, int x, int y,
		       int directionX, int directionY, int screenWidth,
		       int screenHeight, NavPoint *target)
{
	return pick(windows, windowCount, x, y, directionX, directionY,
		    screenWidth, screenHeight, target, 1);
}

static int pick(const int *windows, int windowCount, int x, int y,
		int directionX, int directionY, int screenWidth,
		int screenHeight, NavPoint *target, int allowWide)
{
	const NavBox *boxes = frames[!writing];
	int count = frameCounts[!writing];
	float bestScore = 0, wideScore = 0, alignedScore = 0, bestAlong = 0;
	int best = -1, wide = -1, aligned = -1, current = -1;
	int i;

	// The target the pointer is on now. Moving sideways, anything whose
	// middle is within its width is above or below it rather than beside
	// it, however little its middle is off to that side; the same going up
	// or down with its height.
	for (i = 0; i < count; i++) {
		if (boxContainsPoint(&boxes[i].screen, (float)x, (float)y) &&
		    isTarget(boxes, count, i, windows, windowCount, screenWidth,
			     screenHeight) &&
		    (current < 0 || boxArea(&boxes[i].screen) <
					    boxArea(&boxes[current].screen))) {
			current = i;
		}
	}

	for (i = 0; i < count; i++) {
		const NavRect *box = &boxes[i].screen;
		float centerX, centerY, along, sideways, score;

		if (boxContainsPoint(box, (float)x, (float)y))
			continue;

		if (current >= 0) {
			const NavRect *here = &boxes[current].screen;
			float middleX = (box->left + box->right) / 2;
			float middleY = (box->top + box->bottom) / 2;

			// Something further along the same row still counts as
			// beside it, like the arrow at the end of a wide part
			// selector in the tailor.
			if (directionX && middleX >= here->left &&
			    middleX <= here->right &&
			    !(middleY >= here->top && middleY <= here->bottom &&
			      (middleX - x) * directionX > 0))
				continue;
			if (directionY && middleY >= here->top &&
			    middleY <= here->bottom)
				continue;
		}

		centerX = (box->left + box->right) / 2;
		centerY = (box->top + box->bottom) / 2;

		along = (centerX - x) * directionX + (centerY - y) * directionY;
		// How far off the line of travel the box is, from its nearest
		// edge. A wide row straight below is straight below however far
		// its middle is off to one side.
		if (directionY)
			sideways = x < box->left    ? box->left - x
				   : x > box->right ? x - box->right
						    : 0;
		else
			sideways = y < box->top	     ? box->top - y
				   : y > box->bottom ? y - box->bottom
						     : 0;

		if (along <= 0)
			continue;

		score = along + sideways * NAV_SPREAD_COST;

		if (!isTarget(boxes, count, i, windows, windowCount,
			      screenWidth, screenHeight))
			continue;

		// Something straight along the line, in the same row or column,
		// beats anything off to one side, however near: left from one
		// column header goes to the next header, not up to a button
		// above.
		if (sideways == 0 && (aligned < 0 || along < alignedScore)) {
			aligned = i;
			alignedScore = along;
		}

		if (sideways <= along * NAV_MAX_SPREAD) {
			if (best < 0 || score < bestScore) {
				best = i;
				bestScore = score;
				bestAlong = along;
			}
		}
		// Anything at all on that side, for when nothing lines up: a
		// popup's lone OK below a word off to one side must still be
		// reachable. Not in windows, where nothing lined up means
		// scroll the list.
		else if (allowWide && (wide < 0 || score < wideScore)) {
			wide = i;
			wideScore = score;
		}
	}

	// Not when it is much further than the best of the others, so slightly
	// offset rows are not skipped.
	if (aligned >= 0 && (best < 0 || alignedScore <= bestAlong * 2.5f))
		best = aligned;
	if (best < 0)
		best = wide;
	if (best < 0)
		return 0;

	boxCenter(&boxes[best].screen, target);

	// Landing in a wide row from straight above or below keeps the pointer
	// in its column, so the next press carries on down the same line rather
	// than from the row's far middle.
	{
		const NavRect *box = &boxes[best].screen;

		if (directionY && x > box->left + 4 && x < box->right - 4)
			target->x = x;
		else if (directionX && y > box->top + 4 && y < box->bottom - 4)
			target->y = y;
	}
	return 1;
}

// The target on one half of the screen nearest the pointer's height, for
// jumping across a screen laid out in two sides, like the tailor's parts list
// and its colors.
int gamepadNavPickSide(const int *windows, int windowCount, int x, int y,
		       int side, int screenWidth, int screenHeight,
		       NavPoint *target)
{
	const NavBox *boxes = frames[!writing];
	int count = frameCounts[!writing];
	float bestScore = 0;
	float sideMiddle = side > 0 ? screenWidth * 0.75f : screenWidth * 0.25f;
	int best = -1;
	int i;

	for (i = 0; i < count; i++) {
		const NavRect *box = &boxes[i].screen;
		float centerX = (box->left + box->right) / 2;
		float centerY = (box->top + box->bottom) / 2;
		float score;

		if (side > 0 ? centerX <= screenWidth / 2
			     : centerX >= screenWidth / 2)
			continue;
		if (boxContainsPoint(box, (float)x, (float)y))
			continue;
		score = fabsf(centerY - y) +
			0.25f * fabsf(centerX - sideMiddle);
		if ((best < 0 || score < bestScore) &&
		    isTarget(boxes, count, i, windows, windowCount, screenWidth,
			     screenHeight)) {
			best = i;
			bestScore = score;
		}
	}

	if (best < 0)
		return 0;

	boxCenter(&boxes[best].screen, target);
	return 1;
}

int gamepadNavFirst(const int *windows, int windowCount, int screenWidth,
		    int screenHeight, NavPoint *target)
{
	const NavBox *boxes = frames[!writing];
	int count = frameCounts[!writing];
	float bestScore = 0;
	int best = -1;
	int i;

	for (i = 0; i < count; i++) {
		float score = boxes[i].screen.top * NAV_FIRST_ROW_WEIGHT +
			      boxes[i].screen.left;

		if ((best < 0 || score < bestScore) &&
		    isTarget(boxes, count, i, windows, windowCount, screenWidth,
			     screenHeight)) {
			best = i;
			bestScore = score;
		}
	}

	if (best < 0)
		return 0;

	boxCenter(&boxes[best].screen, target);
	return 1;
}

int gamepadNavFirstBelow(const int *windows, int windowCount, float minTop,
			 int screenWidth, int screenHeight, NavPoint *target)
{
	const NavBox *boxes = frames[!writing];
	int count = frameCounts[!writing];
	float bestScore = 0;
	int best = -1;
	int i;

	for (i = 0; i < count; i++) {
		float score = boxes[i].screen.top * NAV_FIRST_ROW_WEIGHT +
			      boxes[i].screen.left;

		if (boxes[i].screen.top >= minTop &&
		    (best < 0 || score < bestScore) &&
		    isTarget(boxes, count, i, windows, windowCount, screenWidth,
			     screenHeight)) {
			best = i;
			bestScore = score;
		}
	}

	if (best < 0)
		return gamepadNavFirst(windows, windowCount, screenWidth,
				       screenHeight, target);

	boxCenter(&boxes[best].screen, target);
	return 1;
}

#define NAV_SAME_ROW 6.0f

// How many targets share a row with this box.
static int rowSize(const NavBox *boxes, int count, int index,
		   const int *windows, int windowCount, int screenWidth,
		   int screenHeight)
{
	float middle =
		(boxes[index].screen.top + boxes[index].screen.bottom) / 2;
	int i, size = 0;

	for (i = 0; i < count; i++) {
		float other =
			(boxes[i].screen.top + boxes[i].screen.bottom) / 2;

		if (fabsf(other - middle) <= NAV_SAME_ROW &&
		    !sameBox(&boxes[i].screen, &boxes[index].screen) &&
		    isTarget(boxes, count, i, windows, windowCount, screenWidth,
			     screenHeight)) {
			size++;
		}
	}
	return size + 1;
}

// The leftmost box on the lowest row of buttons, where a popup keeps its
// answers. A row of two or more wins over a lone box below it, which is usually
// a "don't ask again" checkbox.
int gamepadNavLastRow(const int *windows, int windowCount, int screenWidth,
		      int screenHeight, NavPoint *target)
{
	const NavBox *boxes = frames[!writing];
	int count = frameCounts[!writing];
	float bestScore = 0, loneScore = 0;
	int best = -1, lone = -1;
	int i;

	for (i = 0; i < count; i++) {
		float score = -boxes[i].screen.bottom * NAV_FIRST_ROW_WEIGHT +
			      boxes[i].screen.left;

		if (!isTarget(boxes, count, i, windows, windowCount,
			      screenWidth, screenHeight))
			continue;

		if (rowSize(boxes, count, i, windows, windowCount, screenWidth,
			    screenHeight) >= 2) {
			if (best < 0 || score < bestScore) {
				best = i;
				bestScore = score;
			}
		} else if (lone < 0 || score < loneScore) {
			lone = i;
			loneScore = score;
		}
	}

	if (best < 0)
		best = lone;
	if (best < 0)
		return 0;

	boxCenter(&boxes[best].screen, target);
	return 1;
}

int gamepadNavFocused(const int *windows, int windowCount, int x, int y,
		      int screenWidth, int screenHeight, NavRect *screenBox)
{
	const NavBox *boxes = frames[!writing];
	int count = frameCounts[!writing];
	float bestArea = 0;
	int best = -1;
	int i;

	for (i = 0; i < count; i++) {
		const NavRect *box = &boxes[i].screen;

		if (boxContainsPoint(box, (float)x, (float)y) &&
		    (best < 0 || boxArea(box) < bestArea) &&
		    isTarget(boxes, count, i, windows, windowCount, screenWidth,
			     screenHeight)) {
			best = i;
			bestArea = boxArea(box);
		}
	}

	if (best < 0)
		return 0;

	*screenBox = boxes[best].screen;
	return 1;
}

int gamepadNavTargets(const int *windows, int windowCount, int screenWidth,
		      int screenHeight, NavRect *out, int max)
{
	const NavBox *boxes = frames[!writing];
	int count = frameCounts[!writing];
	int found = 0;
	int i;

	for (i = 0; i < count && found < max; i++) {
		if (isTarget(boxes, count, i, windows, windowCount, screenWidth,
			     screenHeight))
			out[found++] = boxes[i].screen;
	}
	return found;
}

#define NAV_SAME_EDGE 2.0f

int gamepadNavFollowScroll(const NavRect *focused, const NavRect *after,
			   int afterCount, int directionY, float shift,
			   NavPoint *target)
{
	float moved = focused->top - shift;
	float bestTop = 0;
	int j, found = 0;

	if (!directionY || shift == 0)
		return 0;

	// Where the focused row went, then the first row of its column past it.
	for (j = 0; j < afterCount; j++) {
		if (fabsf(after[j].left - focused->left) >= NAV_SAME_EDGE ||
		    fabsf(after[j].right - focused->right) >= NAV_SAME_EDGE)
			continue;

		if (directionY > 0
			    ? after[j].top > moved + 1 &&
				      (!found || after[j].top < bestTop)
			    : after[j].top < moved - 1 &&
				      (!found || after[j].top > bestTop)) {
			bestTop = after[j].top;
			boxCenter(&after[j], target);
			found = 1;
		}
	}
	return found;
}
