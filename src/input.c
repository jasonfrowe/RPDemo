#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "usb_hid_keys.h"
#include "constants.h"
#include "input.h"

#define JOYSTICK_CONFIG_FILE "JOYSTICK_SH.DAT"

typedef struct {
    uint8_t action_id;
    uint8_t field;
    uint8_t mask;
} JoystickMapping;

// Button mapping storage
ButtonMapping button_mappings[GAMEPAD_COUNT][ACTION_COUNT];

// Gamepad state structure
gamepad_t gamepad[GAMEPAD_COUNT];

// Keyboard state
uint8_t keystates[KEYBOARD_BYTES] = {0};

static bool load_button_mappings(uint8_t player_id)
{
    FILE* fp = fopen(JOYSTICK_CONFIG_FILE, "rb");
    if (!fp) {
        return false;
    }

    int count = fgetc(fp);
    if (count <= 0 || count > ACTION_COUNT) {
        fclose(fp);
        return false;
    }

    for (int i = 0; i < count; i++) {
        JoystickMapping mapping;
        if (fread(&mapping, sizeof(JoystickMapping), 1, fp) != 1) {
            fclose(fp);
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

    fclose(fp);
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
    button_mappings[player_id][ACTION_MOVE_UP].gamepad_mask = GP_LSTICK_UP;
    button_mappings[player_id][ACTION_MOVE_UP].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_MOVE_UP].gamepad_mask2 = GP_DPAD_UP;

    // ACTION_MOVE_DOWN: Down Arrow, Left Stick Down, or D-Pad Down
    button_mappings[player_id][ACTION_MOVE_DOWN].keyboard_key = KEY_DOWN;
    button_mappings[player_id][ACTION_MOVE_DOWN].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_MOVE_DOWN].gamepad_mask = GP_LSTICK_DOWN;
    button_mappings[player_id][ACTION_MOVE_DOWN].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_MOVE_DOWN].gamepad_mask2 = GP_DPAD_DOWN;

    // ACTION_MOVE_LEFT: Left Arrow, Left Stick Left, or D-Pad Left
    button_mappings[player_id][ACTION_MOVE_LEFT].keyboard_key = KEY_LEFT;
    button_mappings[player_id][ACTION_MOVE_LEFT].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_MOVE_LEFT].gamepad_mask = GP_LSTICK_LEFT;
    button_mappings[player_id][ACTION_MOVE_LEFT].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_MOVE_LEFT].gamepad_mask2 = GP_DPAD_LEFT;

    // ACTION_MOVE_RIGHT: Right Arrow, Left Stick Right, or D-Pad Right
    button_mappings[player_id][ACTION_MOVE_RIGHT].keyboard_key = KEY_RIGHT;
    button_mappings[player_id][ACTION_MOVE_RIGHT].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_MOVE_RIGHT].gamepad_mask = GP_LSTICK_RIGHT;
    button_mappings[player_id][ACTION_MOVE_RIGHT].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_MOVE_RIGHT].gamepad_mask2 = GP_DPAD_RIGHT;

    // ACTION_BTN_A: Z key or A button
    button_mappings[player_id][ACTION_BTN_A].keyboard_key = KEY_Z;
    button_mappings[player_id][ACTION_BTN_A].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_A].gamepad_mask = GP_BTN_A;

    // ACTION_BTN_B: X key or B button
    button_mappings[player_id][ACTION_BTN_B].keyboard_key = KEY_X;
    button_mappings[player_id][ACTION_BTN_B].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_B].gamepad_mask = GP_BTN_B;

    // ACTION_BTN_X: C key or X button
    button_mappings[player_id][ACTION_BTN_X].keyboard_key = KEY_C;
    button_mappings[player_id][ACTION_BTN_X].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_X].gamepad_mask = GP_BTN_X;

    // ACTION_BTN_Y: V key or Y button
    button_mappings[player_id][ACTION_BTN_Y].keyboard_key = KEY_V;
    button_mappings[player_id][ACTION_BTN_Y].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_Y].gamepad_mask = GP_BTN_Y;

    // ACTION_BTN_LT: A key or L1/L2
    button_mappings[player_id][ACTION_BTN_LT].keyboard_key = KEY_A;
    button_mappings[player_id][ACTION_BTN_LT].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_LT].gamepad_mask = GP_BTN_L1;
    button_mappings[player_id][ACTION_BTN_LT].gamepad_button2 = GP_FIELD_BTN1;
    button_mappings[player_id][ACTION_BTN_LT].gamepad_mask2 = GP_BTN_L2;

    // ACTION_BTN_RT: S key or R1/R2
    button_mappings[player_id][ACTION_BTN_RT].keyboard_key = KEY_S;
    button_mappings[player_id][ACTION_BTN_RT].gamepad_button = GP_FIELD_BTN0;
    button_mappings[player_id][ACTION_BTN_RT].gamepad_mask = GP_BTN_R1;
    button_mappings[player_id][ACTION_BTN_RT].gamepad_button2 = GP_FIELD_BTN1;
    button_mappings[player_id][ACTION_BTN_RT].gamepad_mask2 = GP_BTN_R2;

    // ACTION_BTN_SELECT: Backspace or Select
    button_mappings[player_id][ACTION_BTN_SELECT].keyboard_key = KEY_BACKSPACE;
    button_mappings[player_id][ACTION_BTN_SELECT].gamepad_button = GP_FIELD_BTN1;
    button_mappings[player_id][ACTION_BTN_SELECT].gamepad_mask = GP_BTN_SELECT;

    // ACTION_BTN_START: Enter or Start
    button_mappings[player_id][ACTION_BTN_START].keyboard_key = KEY_ENTER;
    button_mappings[player_id][ACTION_BTN_START].gamepad_button = GP_FIELD_BTN1;
    button_mappings[player_id][ACTION_BTN_START].gamepad_mask = GP_BTN_START;
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
    RIA.addr0 = KEYBOARD_INPUT;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < KEYBOARD_BYTES; i++) {
        keystates[i] = RIA.rw0;
    }
    
    // Read gamepad data
    RIA.addr0 = GAMEPAD_INPUT;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < GAMEPAD_COUNT; i++) {
        gamepad[i].dpad = RIA.rw0;
        gamepad[i].sticks = RIA.rw0;
        gamepad[i].btn0 = RIA.rw0;
        gamepad[i].btn1 = RIA.rw0;
        gamepad[i].lx = RIA.rw0;
        gamepad[i].ly = RIA.rw0;
        gamepad[i].rx = RIA.rw0;
        gamepad[i].ry = RIA.rw0;
        gamepad[i].l2 = RIA.rw0;
        gamepad[i].r2 = RIA.rw0;
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
        if (key(mapping->keyboard_key)) {
            return true;
        }
    }
    
    // Only check gamepad if one is connected
    if (!(gamepad[player_id].dpad & GP_CONNECTED)) {
        return false;
    }
    
    // Check primary gamepad mapping
    uint8_t gamepad_value = 0;
    switch (mapping->gamepad_button) {
        case 0: gamepad_value = gamepad[player_id].dpad; break;
        case 1: gamepad_value = gamepad[player_id].sticks; break;
        case 2: gamepad_value = gamepad[player_id].btn0; break;
        case 3: gamepad_value = gamepad[player_id].btn1; break;
    }
    if (gamepad_value & mapping->gamepad_mask) return true;

    // Check secondary gamepad mapping (e.g. D-pad alongside analog stick)
    if (mapping->gamepad_mask2 != 0) {
        uint8_t gamepad_value2 = 0;
        switch (mapping->gamepad_button2) {
            case 0: gamepad_value2 = gamepad[player_id].dpad; break;
            case 1: gamepad_value2 = gamepad[player_id].sticks; break;
            case 2: gamepad_value2 = gamepad[player_id].btn0; break;
            case 3: gamepad_value2 = gamepad[player_id].btn1; break;
        }
        return (gamepad_value2 & mapping->gamepad_mask2) != 0;
    }

    return false;
}
