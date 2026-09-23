#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "xram.h"

#define GAMEPAD_COUNT 1       // Support 1 gamepad for now

// ============================================================================
// BUTTON MAPPING SYSTEM
// ============================================================================

// Game actions - movement and buttons
typedef enum {
    // Movement
    ACTION_MOVE_UP,
    ACTION_MOVE_DOWN,
    ACTION_MOVE_LEFT,
    ACTION_MOVE_RIGHT,
    // Face buttons
    ACTION_BTN_A,
    ACTION_BTN_B,
    ACTION_BTN_X,
    ACTION_BTN_Y,
    // Triggers
    ACTION_BTN_LT,
    ACTION_BTN_RT,
    // System
    ACTION_BTN_SELECT,
    ACTION_BTN_START,
    ACTION_COUNT  // Total number of actions
} GameAction;

// Button mapping structure
typedef struct {
    uint8_t keyboard_key;     // USB HID keycode
    uint8_t gamepad_button;   // Which gamepad field (0=dpad, 1=sticks, 2=btn0, 3=btn1)
    uint8_t gamepad_mask;     // Bit mask for the button
    uint8_t gamepad_button2;  // Secondary gamepad field (same encoding)
    uint8_t gamepad_mask2;    // Secondary bit mask (0 = no secondary mapping)
} ButtonMapping;

// Gamepad Field Offsets
#define GP_FIELD_DPAD    0  // D-Pad and Status
#define GP_FIELD_STICKS  1  // Digital Sticks
#define GP_FIELD_BTN0    2  // Face Buttons
#define GP_FIELD_BTN1    3  // Triggers/Select/Start

extern gamepad_t gamepad;

extern void init_input_system(void);
extern void handle_input(void);
extern bool is_action_pressed(uint8_t player_id, GameAction action);

#endif // INPUT_H
