/*
 * Keybind modifier numbers shared by the client and the MapServer.
 *
 * A saved keybind is a key code plus a modifier number. 0 to 3 are no
 * modifier, Ctrl, Shift and Alt. The numbers after them are gamepad
 * modifier combinations: any mix of the left trigger, right trigger, left
 * bumper and right bumper held while another pad button is pressed, so one
 * pad button can carry many binds without moving the power trays.
 *
 * The modifier travels as 32 bits and is stored as an int4 column, so the
 * pad combinations need no packet or database change. Client and MapServer
 * must agree on KEYBIND_MODIFIER_COUNT, or the server drops the binds as
 * corrupt.
 */
#ifndef KEYBIND_MODIFIER_H
#define KEYBIND_MODIFIER_H

/* Gamepad modifier bits. A pad modifier number holds a non-zero mix. */
#define KEYBIND_PADMOD_LT 1 /* left trigger held */
#define KEYBIND_PADMOD_RT 2 /* right trigger held */
#define KEYBIND_PADMOD_LB 4 /* left bumper held */
#define KEYBIND_PADMOD_RB 8 /* right bumper held */
#define KEYBIND_PADMOD_ALL 15

/* Modifier number of pad mix 1; mix m is KEYBIND_MODIFIER_PAD_FIRST + m - 1. */
#define KEYBIND_MODIFIER_PAD_FIRST 4

/* One more than the highest valid modifier number. */
#define KEYBIND_MODIFIER_COUNT (KEYBIND_MODIFIER_PAD_FIRST + KEYBIND_PADMOD_ALL)

#endif /* KEYBIND_MODIFIER_H */
