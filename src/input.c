#include <rp6502.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "usb_hid_keys.h"
#include "input.h"

#define JOYSTICK_CONFIG_FILE "JOYSTICK_SH.DAT"

typedef struct {
    uint8_t action_id;
    uint8_t field;
    uint8_t mask;
} JoystickMapping;

// Button mapping storage
ButtonMapping button_mappings[GAMEPAD_COUNT][ACTION_COUNT];

// Gamepad state, copied from XRAM each frame
gamepad_t gamepad;

// Keyboard state, copied from XRAM each frame
static keyboard_t keyboard;

static bool load_button_mappings(uint8_t player_id)
{
    int fd = open(JOYSTICK_CONFIG_FILE, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    uint8_t count_byte;
    if (read(fd, &count_byte, 1) != 1) {
        close(fd);
        return false;
    }
    int count = count_byte;
    if (count <= 0 || count > ACTION_COUNT) {
        close(fd);
        return false;
    }

    for (int i = 0; i < count; i++) {
        JoystickMapping mapping;
        if (read(fd, &mapping, sizeof(JoystickMapping)) != sizeof(JoystickMapping)) {
            close(fd);
            return false;
        }

        if (mapping.action_id >= ACTION_COUNT || mapping.field > GP_FIELD_BTN1) {
            continue;
        }

        button_mappings[player_id][mapping.action_id].gamepad_button = mapping.field;
        button_mappings[player_id][mapping.action_id].gamepad_mask = mapping.mask;
        button_mappings[player_id][mapping.action_id].gamepad_button2 = 0;
        button_mappings[player_id][mapping.action_id].gamepad_mask2 = 0;
    }

    close(fd);
    return true;
}

/**
 * Reset to default button mappings for a specific player
 */
void reset_button_mappings(uint8_t player_id)
{
    if (player_id >= GAMEPAD_COUNT) return;

    // Zero out all mappings
    memset(&button_mappings[player_id], 0, sizeof(button_mappings[player_id]));

    // ACTION_MOVE_UP: Up Arrow, Left Stick Up, or D-Pad Up
    button_mappings[player_id][ACTION_MOVE_UP].keyboard_key = KEY_UP;
    button_mappings[player_id][ACTION_MOVE_UP].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_MOVE_UP].gamepad_mask = GAMEPAD_LSTICK_UP;
    button_mappings[player_id][ACTION_MOVE_UP].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_MOVE_UP].gamepad_mask2 = GAMEPAD_DPAD_UP;

    // ACTION_MOVE_DOWN: Down Arrow, Left Stick Down, or D-Pad Down
    button_mappings[player_id][ACTION_MOVE_DOWN].keyboard_key = KEY_DOWN;
    button_mappings[player_id][ACTION_MOVE_DOWN].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_MOVE_DOWN].gamepad_mask = GAMEPAD_LSTICK_DOWN;
    button_mappings[player_id][ACTION_MOVE_DOWN].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_MOVE_DOWN].gamepad_mask2 = GAMEPAD_DPAD_DOWN;

    // ACTION_MOVE_LEFT: Left Arrow, Left Stick Left, or D-Pad Left
    button_mappings[player_id][ACTION_MOVE_LEFT].keyboard_key = KEY_LEFT;
    button_mappings[player_id][ACTION_MOVE_LEFT].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_MOVE_LEFT].gamepad_mask = GAMEPAD_LSTICK_LEFT;
    button_mappings[player_id][ACTION_MOVE_LEFT].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_MOVE_LEFT].gamepad_mask2 = GAMEPAD_DPAD_LEFT;

    // ACTION_MOVE_RIGHT: Right Arrow, Left Stick Right, or D-Pad Right
    button_mappings[player_id][ACTION_MOVE_RIGHT].keyboard_key = KEY_RIGHT;
    button_mappings[player_id][ACTION_MOVE_RIGHT].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_MOVE_RIGHT].gamepad_mask = GAMEPAD_LSTICK_RIGHT;
    button_mappings[player_id][ACTION_MOVE_RIGHT].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_MOVE_RIGHT].gamepad_mask2 = GAMEPAD_DPAD_RIGHT;

    // ACTION_BTN_A: Z key or A button
    button_mappings[player_id][ACTION_BTN_A].keyboard_key = KEY_Z;
    button_mappings[player_id][ACTION_BTN_A].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_A].gamepad_mask = GAMEPAD_BTN0_A;

    // ACTION_BTN_B: X key or B button
    button_mappings[player_id][ACTION_BTN_B].keyboard_key = KEY_X;
    button_mappings[player_id][ACTION_BTN_B].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_B].gamepad_mask = GAMEPAD_BTN0_B;

    // ACTION_BTN_X: C key or X button
    button_mappings[player_id][ACTION_BTN_X].keyboard_key = KEY_C;
    button_mappings[player_id][ACTION_BTN_X].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_X].gamepad_mask = GAMEPAD_BTN0_X;

    // ACTION_BTN_Y: V key or Y button
    button_mappings[player_id][ACTION_BTN_Y].keyboard_key = KEY_V;
    button_mappings[player_id][ACTION_BTN_Y].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_Y].gamepad_mask = GAMEPAD_BTN0_Y;

    // ACTION_BTN_LT: A key or L1/L2
    button_mappings[player_id][ACTION_BTN_LT].keyboard_key = KEY_A;
    button_mappings[player_id][ACTION_BTN_LT].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_LT].gamepad_mask = GAMEPAD_BTN0_L1;
    button_mappings[player_id][ACTION_BTN_LT].gamepad_button2 = GP_FIELD_BTN1;
    button_mappings[player_id][ACTION_BTN_LT].gamepad_mask2 = GAMEPAD_BTN1_L2;

    // ACTION_BTN_RT: S key or R1/R2
    button_mappings[player_id][ACTION_BTN_RT].keyboard_key = KEY_S;
    button_mappings[player_id][ACTION_BTN_RT].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_RT].gamepad_mask = GAMEPAD_BTN0_R1;
    button_mappings[player_id][ACTION_BTN_RT].gamepad_button2 = GP_FIELD_BTN1;
    button_mappings[player_id][ACTION_BTN_RT].gamepad_mask2 = GAMEPAD_BTN1_R2;

    // ACTION_BTN_SELECT: Backspace or Select
    button_mappings[player_id][ACTION_BTN_SELECT].keyboard_key = KEY_BACKSPACE;
    button_mappings[player_id][ACTION_BTN_SELECT].gamepad_button = GP_FIELD_BTN1;
    button_mappings[player_id][ACTION_BTN_SELECT].gamepad_mask = GAMEPAD_BTN1_SELECT;

    // ACTION_BTN_START: Enter or Start
    button_mappings[player_id][ACTION_BTN_START].keyboard_key = KEY_ENTER;
    button_mappings[player_id][ACTION_BTN_START].gamepad_button = GP_FIELD_BTN1;
    button_mappings[player_id][ACTION_BTN_START].gamepad_mask = GAMEPAD_BTN1_START;
}

/**
 * Initialize input system with default button mappings
 */
void init_input_system(void)
{
    // Initialize with default button mappings
    for (uint8_t player = 0; player < GAMEPAD_COUNT; player++) {
        reset_button_mappings(player);
    }

    // Override defaults with saved gamepad mappings if present.
    (void)load_button_mappings(0);
}

/**
 * Read keyboard and gamepad input
 */
void handle_input(void)
{
    // Read all keyboard state bytes
    RIA.addr0 = XRAM_KEYBOARD;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < sizeof(keyboard.keys); i++) {
        keyboard.keys[i] = RIA.rw0;
    }
    
    // Read gamepad data
    RIA.addr0 = XRAM_GAMEPAD;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < GAMEPAD_COUNT; i++) {
        gamepad.player[i].dpad = RIA.rw0;
        gamepad.player[i].sticks = RIA.rw0;
        gamepad.player[i].btn0 = RIA.rw0;
        gamepad.player[i].btn1 = RIA.rw0;
        gamepad.player[i].lx = RIA.rw0;
        gamepad.player[i].ly = RIA.rw0;
        gamepad.player[i].rx = RIA.rw0;
        gamepad.player[i].ry = RIA.rw0;
        gamepad.player[i].l2 = RIA.rw0;
        gamepad.player[i].r2 = RIA.rw0;
    }
}

/**
 * Check if a game action is active for a specific player
 */
bool is_action_pressed(uint8_t player_id, GameAction action)
{
    if (player_id >= GAMEPAD_COUNT || action >= ACTION_COUNT) {
        return false;
    }
    
    ButtonMapping* mapping = &button_mappings[player_id][action];
    
    // Check keyboard (player 0 only for now)
    if (player_id == 0) {
        if (KEYBOARD_PRESSED(keyboard.keys, mapping->keyboard_key)) {
            return true;
        }
    }
    
    // Only check gamepad if one is connected
    if (!(gamepad.player[player_id].dpad & GAMEPAD_FEAT_CONNECTED)) {
        return false;
    }
    
    // Check primary gamepad mapping
    uint8_t gamepad_value = 0;
    switch (mapping->gamepad_button) {
        case 0: gamepad_value = gamepad.player[player_id].dpad; break;
        case 1: gamepad_value = gamepad.player[player_id].sticks; break;
        case 2: gamepad_value = gamepad.player[player_id].btn0; break;
        case 3: gamepad_value = gamepad.player[player_id].btn1; break;
    }
    if (gamepad_value & mapping->gamepad_mask) return true;

    // Check secondary gamepad mapping (e.g. D-pad alongside analog stick)
    if (mapping->gamepad_mask2 != 0) {
        uint8_t gamepad_value2 = 0;
        switch (mapping->gamepad_button2) {
            case 0: gamepad_value2 = gamepad.player[player_id].dpad; break;
            case 1: gamepad_value2 = gamepad.player[player_id].sticks; break;
            case 2: gamepad_value2 = gamepad.player[player_id].btn0; break;
            case 3: gamepad_value2 = gamepad.player[player_id].btn1; break;
        }
        return (gamepad_value2 & mapping->gamepad_mask2) != 0;
    }

    return false;
}
