#ifndef UIKEYBIND_H
#define UIKEYBIND_H

#include <utilitieslib/stdtypes.h>
#include "entity/keybindModifier.h"

#define BIND_MAX 256

// temporary struct used to hold data parsed in
typedef struct defaultKeyBind
{
    int keys;
    char *key;
    char *command;
} defaultKeyBind;

// A list of all the keybindings
typedef struct KeyBindList
{
    char            *displayName;
    char            *name;
    defaultKeyBind **binds;
} KeyBindList;

// a list of profiles
typedef struct KeyBindProfiles
{
    KeyBindList **profiles;
}KeyBindProfiles;

// holder for parsed data list
extern KeyBindProfiles data_kbp;

//----------------------------------------------------------------------

typedef enum ModTypes
{
    MODIFIER_NONE,
    MODIFIER_CTRL,
    MODIFIER_SHIFT,
    MODIFIER_ALT,
    // Gamepad modifier mixes follow; see entity/keybindModifier.h.
    MODIFIER_PAD_FIRST = KEYBIND_MODIFIER_PAD_FIRST,
    MOD_TOTAL = KEYBIND_MODIFIER_COUNT,
}ModTypes;

typedef struct KeyBind
{
    // used for data recieved form server
    int        key;
    int        modifier;
    int        secondary;
    U32        uID;

    // used for bind exec
    int        pressed[MOD_TOTAL];
    char    *command[MOD_TOTAL];

} KeyBind;

/* Function type Command parser
*    Parameters:
*        str - a string that contains the command name to invoke as well as any parameters
*
*    Returns:
*        0 - if the command passed in str was not handled
*        1 - if the command passed in str was handled
*/
typedef int (*CommandParser)(char *str, int x, int y);
typedef struct KeyBindProfile
{
    unsigned int        havePressed    : 1;    // are any keys in the binding "pressed" right now?
    unsigned int        trickleKeys : 1;    // allow unhandled keys to be handled by other profiles?

    void (*allOtherKeyCallback)();
    KeyBind binds[BIND_MAX];
    CommandParser parser;
    char* name;

} KeyBindProfile;

char *keyName(int num);
int keybind_modifier( int keyvalue );

/*
 * Modifier number for a mix of gamepad modifier bits (KEYBIND_PADMOD_*).
 * Returns MODIFIER_NONE for 0. Bits outside KEYBIND_PADMOD_ALL are ignored.
 */
int keybind_padModifier(int mask);

/*
 * Gamepad modifier bits of a modifier number, or 0 for MODIFIER_NONE, Ctrl,
 * Shift, Alt and out of range numbers.
 */
int keybind_padMask(int mod);

/*
 * Write the bind prefix of a modifier number, such as "SHIFT+" or "LT+RB+",
 * into @out, @outSize bytes, at least 16. Writes "" for MODIFIER_NONE.
 */
void keybind_modifierPrefix(int mod, char *out, size_t outSize);

/*
 * Set the gamepad modifiers for the key events that follow.
 *
 * The gamepad code calls this before each pad key event it sends through
 * inpUpdateKey(), with the mix that was held when that button went down, so
 * a release finds the same bind as its press, and sets it back to 0 after.
 * A bind for the mix wins over the key's plain bind. Global state: read by
 * keybind_findCommand() until changed again.
 */
void keybind_setPadModifiers(int mask);

/*
 * Set the gamepad modifiers held right now. The gamepad code keeps this up
 * to date whenever a trigger or bumper goes down or up. A +hold command
 * bound to a pad mix stays pressed while every modifier of its mix is held,
 * as Shift+key does for Shift; bindCheckKeysReleased() releases it once one
 * is let go. Global state, read by bindCheckKeysReleased().
 */
void keybind_setPadModifiersHeld(int mask);

void ParseBindings();

void keybinds_init( const char *profile, KeyBindProfile *kbp, KeyBind *kb );
void keybind_reset( const char *profile, KeyBindProfile *kbp );

void keybind_set( const char *profile, int key, int mod, int secondary, const char *command );
void keybind_setClear( const char *profile, int key, int mod );

void displayKeybind(const char *key);

void bindKey( const char *keyname, const char *command, int save );
void keybind_setkey( KeyBindProfile *kbp, const char *key, const char *command, int save, int secondary );

void bindKeyExec(int keyScanCode, int state, U32 timeStamp);
void bindKeyExecEx(int keyScanCode, int state, U32 timeStamp, int x, int y );
void bindCheckKeysReleased();
int isMovementKeybind(void);
int isGameReturnKeybind(int keyScanCode);

void bindKeyProfile(KeyBindProfile *profile);
void unbindKeyProfile(KeyBindProfile *profile);

void bindListBinds(void);
void bindListSave(char *pch, int silent);
void bindListLoad(char *pch, int silent);

U32 keybindGetNextID();

extern KeyBindProfile **kbProfiles; // key binding profile stack

#endif