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
    "BUTTON LT",
    "BUTTON RT",
    "SELECT",
    "START",
    NULL
};

static uint8_t action_map[] = {
    ACTION_MOVE_UP,
    ACTION_MOVE_DOWN,
    ACTION_MOVE_LEFT,
    ACTION_MOVE_RIGHT,
    ACTION_BTN_A,
    ACTION_BTN_B,
    ACTION_BTN_X,
    ACTION_BTN_Y,
    ACTION_BTN_LT,
    ACTION_BTN_RT,
    ACTION_BTN_SELECT,
    ACTION_BTN_START,
    ACTION_COUNT
};

typedef struct {
    uint8_t action_id;
    uint8_t field;  // GP_FIELD_DPAD, GP_FIELD_STICKS, GP_FIELD_BTN0 or GP_FIELD_BTN1
    uint8_t mask;
} JoystickMapping;

// The direction bits of the dpad byte, without its type, sticks and connected bits.
#define DPAD_MASK (GAMEPAD_DPAD_UP | GAMEPAD_DPAD_DOWN | GAMEPAD_DPAD_LEFT | GAMEPAD_DPAD_RIGHT)

static JoystickMapping mappings[ACTION_COUNT];
static uint8_t num_mappings = 0;

static void wait_for_all_released(void)
{
    uint8_t vsync_last = RIA.vsync;
    while (true) {
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        RIA.addr0 = XRAM_GAMEPAD;
        RIA.step0 = 1;
        uint8_t d  = RIA.rw0 & DPAD_MASK;
        uint8_t s  = RIA.rw0;
        uint8_t b0 = RIA.rw0;
        uint8_t b1 = RIA.rw0;
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

        RIA.addr0 = XRAM_GAMEPAD;
        RIA.step0 = 1;
        uint8_t d = RIA.rw0 & DPAD_MASK;
        uint8_t s = RIA.rw0;
        uint8_t b0 = RIA.rw0;
        uint8_t b1 = RIA.rw0;

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
            RIA.addr0 = XRAM_GAMEPAD;
            RIA.step0 = 1;
            uint8_t d = RIA.rw0 & DPAD_MASK;
            uint8_t s = RIA.rw0;
            uint8_t b0 = RIA.rw0;
            uint8_t b1 = RIA.rw0;
            bool held = false;
            if (f == GP_FIELD_DPAD && (d & m)) held = true;
            if (f == GP_FIELD_STICKS && (s & m)) held = true;
            if (f == GP_FIELD_BTN0 && (b0 & m)) held = true;
            if (f == GP_FIELD_BTN1 && (b1 & m)) held = true;
            if (!held) break;
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
