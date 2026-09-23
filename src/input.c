#include <rp6502.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "usb_hid_keys.h"
#include "input.h"

#define JOYSTICK_CONFIG_FILE "JOYSTICK_SH.DAT"

// Keycodes 0-3 of the keyboard bitmap are state, not keys: no key pressed,
// and the Num, Caps and Scroll Lock lamps.
#define KEYBOARD_STATE_BITS ((1 << KEYBOARD_NO_KEY) | (1 << KEYBOARD_NUM_LOCK) | \
                             (1 << KEYBOARD_CAPS_LOCK) | (1 << KEYBOARD_SCROLL_LOCK))

// A mask tested against one field of a gamepad's state. A zero mask is unbound.
typedef struct {
    uint8_t field;
    uint8_t mask;
} GamepadBinding;

// The standard bindings. Moves read both the left stick and the D-Pad.
static const GamepadBinding default_bindings[GP_CONTROL_COUNT][2] = {
    [GP_CONTROL_UP]     = {{GP_FIELD_STICKS, GAMEPAD_LSTICK_UP},    {GP_FIELD_DPAD, GAMEPAD_DPAD_UP}},
    [GP_CONTROL_DOWN]   = {{GP_FIELD_STICKS, GAMEPAD_LSTICK_DOWN},  {GP_FIELD_DPAD, GAMEPAD_DPAD_DOWN}},
    [GP_CONTROL_LEFT]   = {{GP_FIELD_STICKS, GAMEPAD_LSTICK_LEFT},  {GP_FIELD_DPAD, GAMEPAD_DPAD_LEFT}},
    [GP_CONTROL_RIGHT]  = {{GP_FIELD_STICKS, GAMEPAD_LSTICK_RIGHT}, {GP_FIELD_DPAD, GAMEPAD_DPAD_RIGHT}},
    [GP_CONTROL_A]      = {{GP_FIELD_BTN0, GAMEPAD_BTN0_A}},
    [GP_CONTROL_B]      = {{GP_FIELD_BTN0, GAMEPAD_BTN0_B}},
    [GP_CONTROL_X]      = {{GP_FIELD_BTN0, GAMEPAD_BTN0_X}},
    [GP_CONTROL_Y]      = {{GP_FIELD_BTN0, GAMEPAD_BTN0_Y}},
    [GP_CONTROL_SELECT] = {{GP_FIELD_BTN1, GAMEPAD_BTN1_SELECT}},
    [GP_CONTROL_START]  = {{GP_FIELD_BTN1, GAMEPAD_BTN1_START}},
};

// Bindings loaded from JOYSTICK_SH.DAT. They add to the standard bindings,
// so the D-Pad, the left stick and the usual buttons always work.
static GamepadBinding saved_bindings[GP_CONTROL_COUNT];

static const uint8_t up_keys[]    = {KEY_W, KEY_UP,    KEY_KP8, KEY_KP7, KEY_KP9};
static const uint8_t down_keys[]  = {KEY_S, KEY_DOWN,  KEY_KP2, KEY_KP1, KEY_KP3};
static const uint8_t left_keys[]  = {KEY_A, KEY_LEFT,  KEY_KP4, KEY_KP7, KEY_KP1};
static const uint8_t right_keys[] = {KEY_D, KEY_RIGHT, KEY_KP6, KEY_KP9, KEY_KP3};
static const uint8_t pause_keys[] = {KEY_P, KEY_PAUSE, KEY_ENTER};

// Keyboard state, copied from XRAM each frame
static keyboard_t keyboard;

// Every key that fires: all keys except the move and pause keys.
static uint8_t fire_keys[sizeof(keyboard.keys)];

// One bit per GameAction, computed once per frame by handle_input()
static uint8_t actions;

static void load_button_mappings(void)
{
    int fd = open(JOYSTICK_CONFIG_FILE, O_RDONLY);
    if (fd < 0) {
        return;
    }

    uint8_t count;
    if (read(fd, &count, 1) == 1 && count != 0 && count <= GP_CONTROL_COUNT) {
        for (uint8_t i = 0; i < count; i++) {
            JoystickMapping mapping;
            if (read(fd, &mapping, sizeof(JoystickMapping)) != sizeof(JoystickMapping)) {
                break;
            }
            if (mapping.action_id >= GP_CONTROL_COUNT || mapping.field > GP_FIELD_BTN1) {
                continue;
            }
            // The upper bits of the dpad field are the pad's type and status.
            if (mapping.field == GP_FIELD_DPAD) {
                mapping.mask &= GP_DPAD_MASK;
            }
            saved_bindings[mapping.action_id].field = mapping.field;
            saved_bindings[mapping.action_id].mask = mapping.mask;
        }
    }

    close(fd);
}

static void clear_fire_keys(const uint8_t *codes, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        fire_keys[codes[i] >> 3] &= (uint8_t)~(1 << (codes[i] & 7));
    }
}

/**
 * Initialize input system with default button mappings
 */
void init_input_system(void)
{
    memset(fire_keys, 0xFF, sizeof(fire_keys));
    fire_keys[0] &= (uint8_t)~KEYBOARD_STATE_BITS;
    clear_fire_keys(up_keys, sizeof(up_keys));
    clear_fire_keys(down_keys, sizeof(down_keys));
    clear_fire_keys(left_keys, sizeof(left_keys));
    clear_fire_keys(right_keys, sizeof(right_keys));
    clear_fire_keys(pause_keys, sizeof(pause_keys));

    memset(saved_bindings, 0, sizeof(saved_bindings));
    // Add saved gamepad mappings if present.
    load_button_mappings();

    actions = 0;
}

static bool any_key_pressed(const uint8_t *codes, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        if (KEYBOARD_PRESSED(keyboard.keys, codes[i])) {
            return true;
        }
    }
    return false;
}

static bool binding_pressed(const uint8_t *pad, const GamepadBinding *binding)
{
    return (pad[binding->field] & binding->mask) != 0;
}

static bool control_pressed(const uint8_t *pad, GamepadControl control)
{
    return binding_pressed(pad, &default_bindings[control][0]) ||
           binding_pressed(pad, &default_bindings[control][1]) ||
           binding_pressed(pad, &saved_bindings[control]);
}

static void set_action(GameAction action, bool pressed)
{
    if (pressed) {
        actions |= (uint8_t)(1 << action);
    }
}

/**
 * Read keyboard and gamepad input
 */
void handle_input(void)
{
    actions = 0;

    // Read all keyboard state bytes
    RIA.addr0 = XRAM_KEYBOARD;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < sizeof(keyboard.keys); i++) {
        keyboard.keys[i] = RIA.rw0;
    }

    if (!KEYBOARD_PRESSED(keyboard.keys, KEYBOARD_NO_KEY)) {
        bool fire_key = false;
        for (uint8_t i = 0; i < sizeof(keyboard.keys); i++) {
            if (keyboard.keys[i] & fire_keys[i]) {
                fire_key = true;
            }
        }

        bool move_up = any_key_pressed(up_keys, sizeof(up_keys));
        bool move_down = any_key_pressed(down_keys, sizeof(down_keys));
        bool move_left = any_key_pressed(left_keys, sizeof(left_keys));
        bool move_right = any_key_pressed(right_keys, sizeof(right_keys));
        bool pause_key = any_key_pressed(pause_keys, sizeof(pause_keys));

        set_action(ACTION_MOVE_UP, move_up);
        set_action(ACTION_MOVE_DOWN, move_down);
        set_action(ACTION_MOVE_LEFT, move_left);
        set_action(ACTION_MOVE_RIGHT, move_right);
        set_action(ACTION_FIRE, fire_key);
        set_action(ACTION_PAUSE, pause_key);
        // Any key that fires, moves or pauses also counts as a START press;
        // this covers every key but the state bits, without a second scan.
        set_action(ACTION_START, fire_key || move_up || move_down || move_left || move_right || pause_key);
    }

    // Read the first gamepad's digital state: dpad, sticks, btn0, btn1,
    // indexed by GP_FIELD_*.
    uint8_t pad[4];
    gamepad_read_raw(pad);

    if (pad[GP_FIELD_DPAD] & GAMEPAD_FEAT_CONNECTED) {
        pad[GP_FIELD_DPAD] &= GP_DPAD_MASK;

        bool fire = control_pressed(pad, GP_CONTROL_A) || control_pressed(pad, GP_CONTROL_B) ||
                    control_pressed(pad, GP_CONTROL_X) || control_pressed(pad, GP_CONTROL_Y);
        bool pause = control_pressed(pad, GP_CONTROL_SELECT) || control_pressed(pad, GP_CONTROL_START);

        set_action(ACTION_MOVE_UP, control_pressed(pad, GP_CONTROL_UP));
        set_action(ACTION_MOVE_DOWN, control_pressed(pad, GP_CONTROL_DOWN));
        set_action(ACTION_MOVE_LEFT, control_pressed(pad, GP_CONTROL_LEFT));
        set_action(ACTION_MOVE_RIGHT, control_pressed(pad, GP_CONTROL_RIGHT));
        set_action(ACTION_FIRE, fire);
        set_action(ACTION_PAUSE, pause);
        set_action(ACTION_START, fire || pause);
    }
}

/**
 * Check if a game action is active this frame
 */
bool is_action_pressed(GameAction action)
{
    if (action >= ACTION_COUNT) {
        return false;
    }
    return (actions & (uint8_t)(1 << action)) != 0;
}
