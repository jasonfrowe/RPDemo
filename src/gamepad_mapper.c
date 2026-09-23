/*
 * Gamepad Button Mapping Tool for RPStarHopper
 */

#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"

#define JOYSTICK_CONFIG_FILE "JOYSTICK_SH.DAT"

static const char* prompt_labels[] = {
    "MOVE UP",
    "MOVE DOWN",
    "MOVE LEFT",
    "MOVE RIGHT",
    "BUTTON A",
    "BUTTON B",
    "BUTTON X",
    "BUTTON Y",
    "SELECT",
    "START",
    NULL
};

static const uint8_t action_map[] = {
    GP_CONTROL_UP,
    GP_CONTROL_DOWN,
    GP_CONTROL_LEFT,
    GP_CONTROL_RIGHT,
    GP_CONTROL_A,
    GP_CONTROL_B,
    GP_CONTROL_X,
    GP_CONTROL_Y,
    GP_CONTROL_SELECT,
    GP_CONTROL_START,
};

_Static_assert(sizeof(action_map) == sizeof(prompt_labels) / sizeof(prompt_labels[0]) - 1,
               "one GP_CONTROL_* per prompt");

static JoystickMapping mappings[GP_CONTROL_COUNT];
static uint8_t num_mappings = 0;

static void wait_for_all_released(void)
{
    uint8_t vsync_last = RIA.vsync;
    while (true) {
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        uint8_t pad[4];
        gamepad_read_raw(pad);
        uint8_t d  = pad[GP_FIELD_DPAD] & GP_DPAD_MASK;
        uint8_t s  = pad[GP_FIELD_STICKS];
        uint8_t b0 = pad[GP_FIELD_BTN0];
        uint8_t b1 = pad[GP_FIELD_BTN1];
        if (d == 0 && s == 0 && b0 == 0 && b1 == 0) return;
    }
}

static bool wait_for_any_button(uint8_t* field, uint8_t* mask)
{
    uint8_t vsync_last = RIA.vsync;
    uint8_t prev_dpad = 0, prev_sticks = 0, prev_btn0 = 0, prev_btn1 = 0;

    while (true) {
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        uint8_t pad[4];
        gamepad_read_raw(pad);
        uint8_t d = pad[GP_FIELD_DPAD] & GP_DPAD_MASK;
        uint8_t s = pad[GP_FIELD_STICKS];
        uint8_t b0 = pad[GP_FIELD_BTN0];
        uint8_t b1 = pad[GP_FIELD_BTN1];

        if ((d & (uint8_t)~prev_dpad) != 0) {
            *field = GP_FIELD_DPAD;
            *mask = d & (uint8_t)~prev_dpad;
            return true;
        }
        if ((s & (uint8_t)~prev_sticks) != 0) {
            *field = GP_FIELD_STICKS;
            *mask = s & (uint8_t)~prev_sticks;
            return true;
        }
        if ((b0 & (uint8_t)~prev_btn0) != 0) {
            *field = GP_FIELD_BTN0;
            *mask = b0 & (uint8_t)~prev_btn0;
            return true;
        }
        if ((b1 & (uint8_t)~prev_btn1) != 0) {
            *field = GP_FIELD_BTN1;
            *mask = b1 & (uint8_t)~prev_btn1;
            return true;
        }

        prev_dpad = d;
        prev_sticks = s;
        prev_btn0 = b0;
        prev_btn1 = b1;
    }
}

int main(void)
{
    printf("\f");
    printf("=== RPStarHopper Gamepad Mapper ===\n\n");

    xreg_ria_keyboard(XRAM_KEYBOARD);
    xreg_ria_gamepad(XRAM_GAMEPAD);

    printf("Press any button to begin...\n");
    uint8_t f, m;
    wait_for_any_button(&f, &m);
    wait_for_all_released();

    for (uint8_t i = 0; prompt_labels[i] != NULL; i++) {
        printf("PRESS: %s\n", prompt_labels[i]);

        wait_for_any_button(&f, &m);

        mappings[num_mappings].action_id = action_map[i];
        mappings[num_mappings].field = f;
        mappings[num_mappings].mask = m;
        num_mappings++;

        while (true) {
            uint8_t pad[4];
            gamepad_read_raw(pad);
            pad[GP_FIELD_DPAD] &= GP_DPAD_MASK;
            if ((pad[f] & m) == 0) break;
        }
    }

    FILE* fp = fopen(JOYSTICK_CONFIG_FILE, "wb");
    if (fp) {
        fputc(num_mappings, fp);
        fwrite(mappings, sizeof(JoystickMapping), num_mappings, fp);
        fclose(fp);
        printf("\nSaved to %s\n", JOYSTICK_CONFIG_FILE);
    } else {
        printf("\nError: Save failed\n");
    }

    printf("\nDone.\n");
    return 0;
}
