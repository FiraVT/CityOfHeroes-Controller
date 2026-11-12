#ifndef UICOMBINESPEC_H
#define UICOMBINESPEC_H

#include <utilitieslib/stdtypes.h>

void combineSpecMenu();

// Takes an enhancement out of a power into the first free inventory spot, as
// dragging it there does: it asks first unless that question is turned off, and
// uses an Enhancement Unslotter.
enum { UNSLOT_STARTED, UNSLOT_NO_UNSLOTTER, UNSLOT_INVENTORY_FULL };
// ask 0 is for a screen that already asked its own question.
int combo_unslotToInventory(int iset, int ipow, int ispec, int ask);

#define FRAME_WIDTH        320
#define FRAME_SPACE (DEFAULT_SCRN_WD - 3*FRAME_WIDTH)/4
#define POWER_SPEC_HT   65
#define SKILL_SPEC_HT    55
#define POWER_OFFSET    50
#define SKILL_OFF        25
#define POWER_WIDTH        240
#define SPEC_WD         36

#define SKILL_OFFSET 16
#define SKILL_FRAME_WIDTH (DEFAULT_SCRN_WD-5*SKILL_OFFSET)/4
#define SKILL_FRAME_SPACE (DEFAULT_SCRN_WD - 4*SKILL_FRAME_WIDTH)/5
#define FRAME_X(x)  (SKILL_OFFSET*(x+1) + x*SKILL_FRAME_WIDTH)

typedef enum EBoostState
{
    kBoostState_Normal,
    kBoostState_Disabled,
    kBoostState_Selected,
    kBoostState_Hidden,
    kBoostState_Later,
    kBoostState_BeyondMaxSize,
} EBoostState;

typedef struct Power Power;
typedef struct BasePower BasePower;
typedef struct Character Character;
typedef struct Boost Boost;
typedef struct uiEnhancement uiEnhancement;
typedef struct CharacterAttributes CharacterAttributes;
void ClientCalcBoosts(const Power *ppow, CharacterAttributes *pattrStr, Boost *pboostReplace);

int drawPowerAndSpecializations( Character *pchar, Power * pow, int x, int y, int z, float scale, int available, int combine, int selectable, int isCombining, float screenScaleX, float screenScaleY);
int drawMenuPowerEnhancement( Character *pchar, int iset, int ipow, int ispec, float x, float y, float z, float sc, int color, int isCombining, float screenScaleX, float screenScaleY);

AtlasTex *drawMenuEnhancement( const Boost *spec, float centerX, float centerY, float z, float sc, int state, int level, bool bDrawOrigin, int menu, int showToolTip);
AtlasTex *drawEnhancementOriginLevelEx( const BasePower *spec, int level, int iNumCombines, bool bDrawOrigin, float centerX, float centerY, float z, float sc, float textSc, int clr, bool bDrawCentered, bool bDrawRecipe );
#define drawEnhancementOriginLevel( spec, level, iNumCombines, bDrawOrigin, centerX, centerY, z, sc, textSc, clr, bRecipe ) drawEnhancementOriginLevelEx( spec, level, iNumCombines, bDrawOrigin, centerX, centerY, z, sc, textSc, clr, 0, bRecipe )
#define drawEnhancementOriginLevelCentered( spec, level, iNumCombines, bDrawOrigin, centerX, centerY, z, sc, textSc, clr, bRecipe ) drawEnhancementOriginLevelEx( spec, level, iNumCombines, bDrawOrigin, centerX, centerY, z, sc, textSc, clr, 1, bRecipe )

void combo_ActOnResult( int success, int slot );

// For the gamepad's enhancement screen, which places enhancements without
// dragging.
int spec_CanPlace(Character *pchar, int iset, int ipow, int ispec, int idx);
void spec_PlaceFromInventory(int iset, int ipow, int ispec, int idx, float x,
			     float y, float z);
// What a drop of inventory enhancement idx into this slot would ask before
// placing it.
enum {
	PLACE_INVALID,
	PLACE_NO_QUESTION,
	PLACE_NEW,
	PLACE_REPLACE,
	PLACE_REPLACE_WORSE
};
int spec_PlaceQuestion(int iset, int ipow, int ispec, int idx);
// Places it without asking, for a screen that asked its own question.
void spec_PlaceFromInventoryNow(int iset, int ipow, int ispec, int idx, float x,
				float y, float z);
char *getEffectiveString(Power *ppow, Boost *oldboost, Boost *newboost, int idx);


#endif