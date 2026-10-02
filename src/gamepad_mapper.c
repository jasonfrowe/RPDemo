/*
 * Gamepad Button Mapping Tool for RPStarHopper
 */

#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "input.h"

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

static void wait_frame(void)
{
    uint8_t vsync = ria_vsync();
    while (ria_vsync() == vsync) {
    }
}

static void read_pad(uint8_t pad[4])
{
    gamepad_read_raw(pad);
    pad[GP_FIELD_DPAD] &= GP_DPAD_MASK;
}

static void wait_for_all_released(void)
{
    uint8_t pad[4];
    do {
        wait_frame();
        read_pad(pad);
    } while (pad[GP_FIELD_DPAD] | pad[GP_FIELD_STICKS] | pad[GP_FIELD_BTN0] | pad[GP_FIELD_BTN1]);
}

static void wait_for_any_button(uint8_t* field, uint8_t* mask)
{
    uint8_t prev[4] = {0};

    while (true) {
        wait_frame();

        uint8_t pad[4];
        read_pad(pad);

        for (uint8_t f = GP_FIELD_DPAD; f <= GP_FIELD_BTN1; f++) {
            uint8_t pressed = pad[f] & (uint8_t)~prev[f];
            if (pressed != 0) {
                *field = f;
                *mask = pressed;
                return;
            }
        }

        memcpy(prev, pad, sizeof(prev));
    }
}

int main(void)
{
    printf("\f");
    printf("=== RPStarHopper Gamepad Mapper ===\n\n");

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

        uint8_t pad[4];
        do {
            read_pad(pad);
        } while (pad[f] & m);
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
