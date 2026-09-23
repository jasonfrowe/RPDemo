#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "xram.h"

// ============================================================================
// GAME ACTIONS
// ============================================================================
//
// Keyboard:
//   Move   W A S D, arrow keys, or keypad 8/4/6/2 (7/9/1/3 move diagonally)
//   Fire   any other key except P, Pause and Enter
//   Pause  P, Pause, or Enter
//   Start  any key ("PRESS BUTTON")
//
// Gamepad:
//   Move   D-Pad or left stick
//   Fire   A, B, X or Y
//   Pause  Start or Select
//   Start  A, B, X, Y, Start or Select ("PRESS BUTTON")

typedef enum {
    ACTION_MOVE_UP,
    ACTION_MOVE_DOWN,
    ACTION_MOVE_LEFT,
    ACTION_MOVE_RIGHT,
    ACTION_FIRE,
    ACTION_PAUSE,
    ACTION_START,
    ACTION_COUNT  // Total number of actions
} GameAction;

// ============================================================================
// GAMEPAD MAPPING (JOYSTICK_SH.DAT, written by GamepadMapper)
// ============================================================================

// Gamepad controls a saved mapping can bind. The values are stored in
// JOYSTICK_SH.DAT, so they never change. LT and RT are no longer used,
// but keep their ids so files written by older mappers still load.
typedef enum {
    GP_CONTROL_UP,
    GP_CONTROL_DOWN,
    GP_CONTROL_LEFT,
    GP_CONTROL_RIGHT,
    GP_CONTROL_A,
    GP_CONTROL_B,
    GP_CONTROL_X,
    GP_CONTROL_Y,
    GP_CONTROL_LT,
    GP_CONTROL_RT,
    GP_CONTROL_SELECT,
    GP_CONTROL_START,
    GP_CONTROL_COUNT
} GamepadControl;

_Static_assert(GP_CONTROL_Y == 7 && GP_CONTROL_SELECT == 10 && GP_CONTROL_START == 11,
               "GP_CONTROL_* values are stored in JOYSTICK_SH.DAT");

// Gamepad field offsets, also stored in JOYSTICK_SH.DAT
#define GP_FIELD_DPAD    0  // D-Pad and Status
#define GP_FIELD_STICKS  1  // Digital Sticks
#define GP_FIELD_BTN0    2  // Face Buttons
#define GP_FIELD_BTN1    3  // Triggers/Select/Start

// The direction bits of the dpad field, without its type, sticks and connected bits.
#define GP_DPAD_MASK (GAMEPAD_DPAD_UP | GAMEPAD_DPAD_DOWN | GAMEPAD_DPAD_LEFT | GAMEPAD_DPAD_RIGHT)

// A gamepad button mapping as stored in JOYSTICK_SH.DAT. The definitive
// on-disk layout, shared by input.c (reader) and gamepad_mapper.c (writer)
// so the two can't silently drift apart.
typedef struct {
    uint8_t action_id;
    uint8_t field;  // GP_FIELD_DPAD, GP_FIELD_STICKS, GP_FIELD_BTN0 or GP_FIELD_BTN1
    uint8_t mask;
} JoystickMapping;

// Reads the first gamepad's raw digital state into pad[GP_FIELD_DPAD..GP_FIELD_BTN1].
// pad[GP_FIELD_DPAD] still carries the pad's type/connected bits; mask with
// GP_DPAD_MASK for direction bits only. The definitive byte order for
// XRAM_GAMEPAD, shared by input.c and gamepad_mapper.c.
static inline void gamepad_read_raw(uint8_t pad[4])
{
    RIA.addr0 = XRAM_GAMEPAD;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < 4; i++) {
        pad[i] = RIA.rw0;
    }
}

extern void init_input_system(void);
extern void handle_input(void);
extern bool is_action_pressed(GameAction action);

#endif // INPUT_H
