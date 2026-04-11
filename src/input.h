#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

#define GAMEPAD_COUNT 1       // Support 1 gamepad for now
#define KEYBOARD_BYTES  32    // 32 bytes for 256 key states

// Hardware button bit masks - DPAD
#define GP_DPAD_UP        0x01
#define GP_DPAD_DOWN      0x02
#define GP_DPAD_LEFT      0x04
#define GP_DPAD_RIGHT     0x08
#define GP_SONY           0x40  // Sony button faces (Circle/Cross/Square/Triangle)
#define GP_CONNECTED      0x80  // Gamepad is connected

// Hardware button bit masks - ANALOG STICKS
#define GP_LSTICK_UP      0x01
#define GP_LSTICK_DOWN    0x02
#define GP_LSTICK_LEFT    0x04
#define GP_LSTICK_RIGHT   0x08
#define GP_RSTICK_UP      0x10
#define GP_RSTICK_DOWN    0x20
#define GP_RSTICK_LEFT    0x40
#define GP_RSTICK_RIGHT   0x80

// Hardware button bit masks - BTN0
#define GP_BTN_A          0x01
#define GP_BTN_B          0x02
#define GP_BTN_C          0x04
#define GP_BTN_X          0x08
#define GP_BTN_Y          0x10
#define GP_BTN_Z          0x20
#define GP_BTN_L1         0x40
#define GP_BTN_R1         0x80

// Hardware button bit masks - BTN1
#define GP_BTN_L2         0x01
#define GP_BTN_R2         0x02
#define GP_BTN_SELECT     0x04
#define GP_BTN_START      0x08
#define GP_BTN_HOME       0x10
#define GP_BTN_L3         0x20
#define GP_BTN_R3         0x40

// ============================================================================
// BUTTON MAPPING SYSTEM
// ============================================================================

// Keyboard state array and macro
extern uint8_t keystates[KEYBOARD_BYTES];

// Macro to check if a key is pressed
#define key(code) (keystates[code >> 3] & (1 << (code & 7)))

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

// Gamepad structure (10 bytes per gamepad)
typedef struct {
    uint8_t dpad;      // Direction pad + status bits
    uint8_t sticks;    // Left and right stick digital directions
    uint8_t btn0;      // Face buttons and shoulders
    uint8_t btn1;      // L2/R2/Select/Start/Home/L3/R3
    int8_t lx;         // Left stick X analog (-128 to 127)
    int8_t ly;         // Left stick Y analog (-128 to 127)
    int8_t rx;         // Right stick X analog (-128 to 127)
    int8_t ry;         // Right stick Y analog (-128 to 127)
    uint8_t l2;        // Left trigger analog (0-255)
    uint8_t r2;        // Right trigger analog (0-255)
} gamepad_t;

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

extern gamepad_t gamepad[GAMEPAD_COUNT];

extern void init_input_system(void);
extern void handle_input(void);
extern bool is_action_pressed(uint8_t player_id, GameAction action);

#endif // INPUT_H
