/*
 * Shared by the console HUD sources, uiConsoleHud*.c and uiConsole*.c.
 * Not for use outside them; uiConsoleHud.h is the public interface.
 */
#ifndef UICONSOLEHUDINTERNAL_H
#define UICONSOLEHUDINTERNAL_H

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <utilitieslib/stdtypes.h>
#include <utilitieslib/language/MessageStoreUtil.h>
#include "UI/uiConsoleHud.h"
#include "UI/uiAutomap.h"
#include "UI/uiChat.h"
#include "UI/uiClue.h"
#include "UI/uiContactDialog.h"
#include "UI/uiInfo.h"
#include "UI/uiLfg.h"
#include "UI/uiFriend.h"
#include "UI/uiBuff.h"
#include "UI/uiGroupWindow.h"
#include "UI/uiReticle.h"
#include "UI/uiTarget.h"
#include "entity/teamCommon.h"
#include "storyarc/contactClient.h"
#include "UI/uiComboBox.h"
#include "UI/uiContextMenu.h"
#include "UI/uiCursor.h"
#include "UI/uiFocus.h"
#include "UI/uiGame.h"
#include "UI/uiInput.h"
#include "UI/uiStatus.h"
#include "UI/uiTray.h"
#include "UI/uiUtil.h"
#include "UI/uiUtilGame.h"
#include "UI/uiWindows.h"
#include "UI/uiChatUtil.h"
#include "UI/uiInspiration.h"
#include "UI/uiTrade.h"
#include "UI/uiDialog.h"
#include "UI/uiCombineSpec.h"
#include "gameComm/chatClient.h"
#include "UI/sprite/sprite_base.h"
#include "UI/sprite/sprite_font.h"
#include "UI/sprite/sprite_text.h"
#include "bases/baseedit.h"
#include "cmdparse/cmdgame.h"
#include "entity/character_base.h"
#include "entity/character_level.h"
#include "entity/character_target.h"
#include "entity/entity.h"
#include "entity/entPlayer.h"
#include "entity/powers.h"
#include "gameComm/trayCommon.h"
#include "gameComm/wdwbase.h"
#include "graphics/textureatlas.h"
#include "UI/uiCombineSpec.h"
#include "entity/salvage.h"
#include "UI/uiOptions.h"
#include "entity/character_inventory.h"
#include "entity/character_eval.h"
#include "entity/incarnate.h"
#include "bases/DetailRecipe.h"
#include "bases/basedata.h"
#include "UI/uiIncarnate.h"
#include "UI/uiRecipeInventory.h"
#include "UI/uiNet.h"
#include "entity/character_workshop.h"
#include "entity/boostset.h"
#include <utilitieslib/components/earray.h>
#include <utilitieslib/utils/sysutil.h>
#include <utilitieslib/utils/structDefines.h>
#include "graphics/ttFontUtil.h"
#include "graphics/camera.h"
#include "graphics/gfxwindow.h"
#include "player/player.h"
#include "sound/sound.h"
#include "win/gamepad.h"
#include "win/gamepadNav.h"
#include "win/input.h"
#include "win/win_init.h"

#define HUD_Z 6.0f

#define FRAME_COLOR 0xa8c0e0a0

#define FRAME_COLOR_HELD 0xffe080ff

#define FRAME_COLOR_DIM 0x60708060

#define BACK_COLOR 0x101820b0

#define BACK_COLOR_HELD 0x283848e0

#define COLOR_ENEMY 0xf06060ff

// Over the windows and under their context menus, so right clicking a slot here
// still works.
#define BIND_Z ((MAX_WINDOW_COUNT + MAX_CUSTOM_WINDOW_COUNT) * 100 + 500.0f)

#define BIND_COLUMNS 9

#define BIND_ROWS 7

#define BIND_ICON_SIZE 60.0f

#define BIND_ICON_STEP 68.0f

#define BIND_MAX_POWERS 256

#define SCREEN_PANEL_WIDTH 1420.0f

typedef struct Prompt {
	int button;
	const char *text;
} Prompt;

typedef struct BindPower {
	int set;
	int power;
} BindPower;

// The enhancement screen: pick an enhancement from the inventory, then a slot
// it fits, without dragging.

#define SLOT_MAX_ROWS 128

// Two columns of nine, filled down the first column and then the second.
#define SLOT_ROWS_PER_COLUMN 9

#define SLOT_VISIBLE_ROWS (SLOT_ROWS_PER_COLUMN * 2)

#define SLOT_INV_COLUMNS 5

#define SLOT_POG_STEP 74.0f

#define SLOT_ROW_STEP 62.0f

#define SLOT_ROW_POG_STEP 54.0f

typedef struct SlotRow {
	int set;
	int power;
	int slots;
} SlotRow;

// The question asked inside the screen before placing or taking out, instead of
// the game's pop-up.
enum { SLOT_ASK_NONE, SLOT_ASK_PLACE, SLOT_ASK_UNSLOT };

// The Incarnates screen: equip the ten slots, make abilities and convert
// components, all from the pad. LB and RB turn between the three tabs. It sits
// beside the Enhancements screen and is drawn the same way.

#define INC_TAB_EQUIP 0

#define INC_TAB_CREATE 1

#define INC_TAB_CONVERT 2

#define INC_TAB_COUNT 3

#define INC_MAX_NAMES 32

#define INC_MAX_NODES 16

#define INC_MAX_RECIPES 48

#define INC_MAX_OWNED 192

#define INC_LIST_ROWS 12

#define INC_ROW_STEP 42.0f

#define INC_RARITY_ROWS 4

typedef struct IncEntry {
	const DetailRecipe *recipe;
	char *text;
	// The recipe's tab name in parts. Creation is slot, tree, rarity.
	// Conversion is Conversion, what is converted, how, and a last part
	// every conversion shares.
	const char *path[4];
	int depth;
} IncEntry;

// One ability in a tree, however many recipes make it.
typedef struct IncNode {
	const DetailRecipe *recipe;
	const BasePower *reward;
	int row;
	int column;
} IncNode;

// The slots as the game lays them out: Omega on top, Alpha at the bottom, the
// rest in pairs.
#define INC_SLOT_ROWS 6

// The Invention screen: build recipes, look through the recipe inventory and
// clear out salvage, all from the pad. LB and RB turn between Build, Recipes
// and Salvage. Building needs an invention table, as it does in the game's own
// window. Anywhere else the screen still shows what can be built and what each
// recipe takes.

#define INV_TAB_BUILD 0

#define INV_TAB_RECIPES 1

#define INV_TAB_COUNT 3

#define INV_CAT_SETS 0

#define INV_CAT_COMMON 1

#define INV_CAT_POWERS 2

#define INV_CAT_COSTUME 3

#define INV_CAT_CONVERT 4

#define INV_CAT_OTHER 5

#define INV_CAT_COUNT 6

#define INV_MAX_GROUPS 512

#define INV_MAX_ITEMS 1024

#define INV_REFRESH_FRAMES 15

#define INV_TABLE_WAIT_FRAMES 120

#define INV_USED_BY_ROWS 8

#define INV_ASK_NONE 0

#define INV_ASK_BUILD 1

#define INV_ASK_BUY 2

#define INV_ASK_DISCARD_RECIPE 3

#define INV_ASK_DISCARD_SALVAGE 4

#define INV_TABLE "Worktable_Invention"

// One recipe the Build tab can show, sorted into a category and a group within
// it.
typedef struct InvEntry {
	const DetailRecipe *recipe;
	char *group;
	char *label;
	int category;
	int owned;
	int buy;
	int ready;
} InvEntry;

// One stack in the recipe or salvage inventory.
typedef struct InvItem {
	const DetailRecipe *recipe;
	const SalvageItem *salvage;
	int amount;
	int rarity;
	char name[128];
} InvItem;

#define HUD_EDIT_HANDLE 18.0f

enum {
	HUD_PIECE_CHARACTER,
	HUD_PIECE_POWERS,
	HUD_PIECE_LT,
	HUD_PIECE_RT,
	HUD_PIECE_BOTH,
	HUD_PIECE_TARGETS,
	HUD_PIECE_TEAM,
	HUD_PIECE_LEAGUE_1,
	HUD_PIECE_LEAGUE_2,
	HUD_PIECE_LEAGUE_3,
	HUD_PIECE_LEAGUE_4,
	HUD_PIECE_LEAGUE_5,
	HUD_PIECE_LEAGUE_6,
	HUD_PIECE_HINTS,
	HUD_PIECE_COUNT
};

// How far a piece reaches from the point it is drawn from, in pixels of a 1080
// high screen at its own size.
typedef struct HudBox {
	F32 left, top, width, height;
} HudBox;

typedef struct HudPiece {
	const char *key;
	const char *label;
	HudBox box;
} HudPiece;

typedef struct HudPlace {
	F32 x, y, size;
	int hidden;
} HudPlace;

// Where a piece is drawn this frame: the point it is drawn from, in window
// pixels, and its scale.
typedef struct HudFrame {
	F32 x, y, scale;
} HudFrame;

typedef struct HudRect {
	F32 x, y, width, height;
} HudRect;

// A full team of frames ran most of the way down the left side at full size, so
// it starts smaller.
#define HUD_TEAM_DEFAULT_SIZE 0.8f

enum { TRADE_TAB_INSP, TRADE_TAB_ENH, TRADE_TAB_SALVAGE, TRADE_TAB_RECIPES };

// Team frames down the left side while on a team: one per teammate, with bars,
// buffs and the shortcut that selects them. The game's own Team window is
// hidden while the console HUD is up; the Social screen's Team tab still
// manages the team.

#define TEAM_FRAME_WIDTH 250.0f

#define TEAM_FRAME_HEIGHT 58.0f

#define TEAM_FRAME_GAP 8.0f

#define TEAM_FRAME_TOP 150.0f

#define TEAM_BUFF_SIZE 18.0f

#define TEAM_BUFF_MAX 10

// Every teammate and league member drawn this frame, where they are, so LB and
// the D-pad can step from one to the next the way they are laid out on screen.
#define HUD_MEMBER_MAX (MAX_LEAGUE_MEMBERS + MAX_TEAM_MEMBERS)

typedef struct HudMember {
	TeamMembers *members;
	int index;
	F32 x, y;
} HudMember;

// Tabbed screens: Navigation (map, missions, contacts, clues and badges) and
// Social (search, team, friends, league and supergroup). Each tab is the game's
// own window drawn inside the screen, so everything it does still works, with
// the pad's focus kept inside it and the bumpers changing tab.

#define NAV_TAB_MAX 6

#define NAV_TAB_WIDTH 150.0f

#define NAV_TAB_HEIGHT 40.0f

typedef struct NavTab {
	const char *label;
	const char *name;
	int window;
	// The window opens with its own strip of sub-tabs along the top, which
	// focus starts below.
	int subTabs;
} NavTab;

#define NAV_SCREEN_COUNT 2

typedef struct NavScreen {
	const char *title;
	const NavTab *tabs;
	int count;
} NavScreen;

#define navTabs (navScreens[navScreen].tabs)

#define navTabCount (navScreens[navScreen].count)

extern int menuOpen;
extern int navOpen;
extern int hudEditOpen;
extern int gameMenuOpen;
extern int inspOpen;
extern int cursorOverHud;
extern F32 clusterBaseZ;
extern int bindScreenDrawing;
int consoleHudActive(void);
int consoleCloseContextMenu(void);
int bumperDirection(int button);
int slotForCommand(const char *command, CurTrayType *tray, int *slot);
void drawText(TTDrawContext *context, F32 x, F32 y, F32 z, F32 scale, int color,
	      int flags, const char *text);
void drawCluster(Entity *e, int modifiers, const char *tag, F32 centerX,
		 F32 centerY, F32 hudScale, int heldLayer);
F32 drawPrompts(const Prompt *prompts, int count, F32 centerX, F32 y, F32 z,
		F32 hudScale);
extern BindPower bindPowers[BIND_MAX_POWERS];
extern int bindPowerCount;
extern int bindOpen;
extern int bindPlacing;
extern int bindSelection;
extern int bindTopRow;
extern int bindCollisionsBefore;
extern int bindMouseX, bindMouseY;
extern int bindInfoOpen;
extern F32 bindInfoScroll;
extern int bindInfoLines;
extern int bindInfoVisibleLines;
void gatherBindPowers(Entity *e);
extern int slotOpen;
extern int incOpen;
extern int invOpen;
int consoleBindIsOpen(void);
int drawWrapped(TTDrawContext *context, const char *text, F32 x, F32 y, F32 z,
		F32 width, F32 lineHeight, F32 scale, int color, int firstLine,
		int maxLines);
void drawBindScreen(Entity *e, int width, int height, F32 hudScale);
extern int slotOpen;
extern int slotChoosing;
extern int slotUnslotting;
extern int slotAsking;
extern int slotAskKind;
extern int slotInvSelection;
extern int slotRow;
extern int slotColumn;
extern int slotTopRow;
extern int slotInvCount;
extern int slotInv[CHAR_BOOST_MAX];
extern int slotRowCount;
extern SlotRow slotRows[SLOT_MAX_ROWS];
extern int slotMouseX, slotMouseY;
int slotScreenUp(void);
void slotPress(int modifiers, int button);
void drawSlotScreen(Entity *e, int width, int height, F32 hudScale);
extern const char *const incTabNames[INC_TAB_COUNT];
extern const char *const incRarityNames[INC_RARITY_ROWS];
extern const int incSlotRows[INC_SLOT_ROWS][2];
extern int incTab;
extern int incAsking;
extern const DetailRecipe *incAskRecipe;
extern int incSlotRow;
extern int incSlotColumn;
extern int incInList;
extern int incListSelection;
extern int incListTop;
extern const BasePower *incOwned[INC_MAX_OWNED];
extern int incOwnedCount;
extern IncEntry *incEntries;
extern int incEntryCount;
extern Entity *incEntriesFor;
extern int incCreateColumn;
extern int incCreateSlot;
extern int incCreateTree;
extern int incNodeSelection;
extern int incRouteSelection;
extern const char *incCreateSlots[INC_MAX_NAMES];
extern int incCreateSlotCount;
extern const char *incCreateTrees[INC_MAX_NAMES];
extern int incCreateTreeCount;
extern IncNode incNodes[INC_MAX_NODES];
extern int incNodeCount;
extern int incRowCounts[INC_RARITY_ROWS];
extern U32 incFeeds[INC_MAX_NODES];
extern const DetailRecipe *incRoutes[INC_MAX_RECIPES];
extern int incRouteCount;
extern int incReadyRoutes;
extern int incConvertColumn;
extern int incConvertSource;
extern int incConvertKind;
extern int incConvertSelection;
extern const char *incConvertSources[INC_MAX_NAMES];
extern int incConvertSourceCount;
extern const char *incConvertKinds[INC_MAX_NAMES];
extern int incConvertKindCount;
extern const DetailRecipe *incConvertRecipes[INC_MAX_RECIPES];
extern int incConvertRecipeCount;
void drawTextRight(TTDrawContext *context, F32 right, F32 y, F32 z, F32 scale,
		   int color, const char *text);
void drawFitted(TTDrawContext *context, F32 x, F32 y, F32 z, F32 scale,
		F32 width, int color, const char *text);
int incScreenUp(void);
int incClamp(int value, int count);
int incCreateAllowed(Entity *e, const DetailRecipe *recipe);
void incPress(int modifiers, int button);
void drawIncColumn(const char *header, const char *const *names, int count,
		   int selection, int focused, F32 left, F32 top, F32 width,
		   F32 hudScale);
F32 drawIncRequirements(Entity *e, const DetailRecipe *recipe, F32 left,
			F32 top, F32 width, F32 hudScale);
void drawIncScreen(Entity *e, int width, int height, F32 hudScale);
extern const char *const invTabNames[INV_TAB_COUNT];
extern const char *const invCategoryNames[INV_CAT_COUNT];
extern const char *const invGroupHeaders[INV_CAT_COUNT];
extern int invTab;
extern int invAsking;
extern const DetailRecipe *invAskRecipe;
extern const SalvageItem *invAskSalvage;
extern int invAskAmount;
extern int invTablePending;
extern int invTablePendingFrames;
extern int invAtTable;
extern InvEntry *invEntries;
extern int invEntryCount;
extern Entity *invEntriesFor;
extern int invRefresh;
extern int invBuildableOnly;
extern int invColumn;
extern int invCategory;
extern char invGroup[256];
extern const DetailRecipe *invRecipe;
extern int invCategoryIds[INV_CAT_COUNT];
extern char invCategoryLabels[INV_CAT_COUNT][64];
extern const char *invCategoryLabelList[INV_CAT_COUNT];
extern int invCategoryCount;
extern int invCategoryIndex;
extern const char *invGroups[INV_MAX_GROUPS];
extern int invGroupCount;
extern int invGroupIndex;
extern const InvEntry *invRecipes[INV_MAX_ITEMS];
extern int invRecipeCount;
extern int invRecipeIndex;
extern InvItem invItems[INV_MAX_ITEMS];
extern int invItemCount;
extern int invItemIndex;
extern const void *invItemChosen;
int invScreenUp(void);
void invClose(void);
void invPress(int modifiers, int button);
void invTick(void);
void drawInvScreen(Entity *e, int width, int height, F32 hudScale);
extern const HudPiece hudPieces[HUD_PIECE_COUNT];
extern HudPlace hudLayout[HUD_PIECE_COUNT];
extern HudBox hudBoxes[HUD_PIECE_COUNT];
extern HudRect hudDrawn[HUD_PIECE_COUNT];
extern F32 hudAnchorX[HUD_PIECE_COUNT];
extern int hudShown[HUD_PIECE_COUNT];
extern int hudEditPiece;
extern int hudDragPiece;
extern int hudDragSizing;
extern int hudDragX, hudDragY;
extern HudPlace hudDragFrom;
extern F32 hudDragReach;
extern const Prompt hudEditPrompts[];
extern const int hudEditPromptCount;
F32 hudClampSize(F32 size);
void hudResetPiece(HudPlace *place);
int hudHidden(int piece);
void hudLoadLayout(void);
HudFrame hudPlace(int piece, F32 x, F32 y, F32 hudScale, int screenWidth,
		  int screenHeight);
int hudEditUp(void);
void hudEditClose(int keep);
void hudEditDiscard(void);
void hudEditPress(int modifiers, int button);
int hudEditLook(F32 y, F32 seconds);
int drawHudEditButton(const char *label, F32 x, F32 y, F32 width, F32 height,
		      F32 z, F32 hudScale);
int gameMenuUp(void);
void gameMenuPress(int button);
void drawGameMenu(int width, int height, F32 hudScale);
int inspScreenUp(void);
void inspPress(int modifiers, int button);
void drawInspScreen(Entity *e, int width, int height, F32 hudScale);
extern int tradeTab;
extern int tradeSelection;
extern int tradeTopRow;
extern int tradeEditing;
extern int tradeWasUp;
extern int tradeConfirming;
int tradeScreenShown(void);
int tradeScreenUp(void);
void tradePress(int modifiers, int button);
void drawTradeScreen(Entity *e, int width, int height, F32 hudScale);
void drawHudEdit(int screenWidth, F32 hudScale);
void keepPetsOffTeam(HudFrame team, F32 teamHeight, int screenWidth,
		     F32 hudScale);
extern HudMember hudMembers[HUD_MEMBER_MAX];
extern int hudMemberCount;
extern HudMember hudMembersShown[HUD_MEMBER_MAX];
extern int hudMembersShownCount;
F32 drawMemberBlock(Entity *e, TeamMembers *members, const int *roster,
		    int count, int team, F32 left, F32 top, F32 scale);
void drawLeagueTeams(Entity *e, int screenWidth, int screenHeight,
		     F32 hudScale);
F32 drawTeamFrames(Entity *e, F32 left, F32 top, F32 hudScale);
extern const NavScreen navScreens[NAV_SCREEN_COUNT];
extern int navScreen;
extern int navTab;
extern int navLastTab[NAV_SCREEN_COUNT];
extern U8 navReopen[NAV_TAB_MAX];
extern int navFocusFrames;
extern F32 navMapScale;
extern F32 navContentWidth, navContentHeight;
int navSideWindowUp(void);
int navCovers(void);
void navClose(void);
void drawNavScreen(int width, int height, F32 hudScale);

#endif /* UICONSOLEHUDINTERNAL_H */
