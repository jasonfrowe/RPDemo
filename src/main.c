#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "input.h"
#include "player_controller.h"
#include "music.h"
#include "projectile.h"
#include "enemy.h"
#include "score.h"
#include "gameplay.h"

static bool init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(1);
    if (rc < 0) {
        puts("Error: xreg_vga_canvas(1) failed");
        return false;
    }

    sprite_mode5_init();
    tile_mode2_init();
    sprite_mode5_init_projectiles();
    sprite_mode5_init_enemies();
    projectile_init();
    enemy_init();
    score_init();

    return true;
}

uint8_t vsync_last = 0;

int main(void)
{

    // Initialize input
    xreg(0, 0, 0, KEYBOARD_INPUT);
    xreg(0, 0, 2, GAMEPAD_INPUT);

    // Initialise graphics
    if (!init_graphics()) {
        puts("Fatal: graphics initialization failed");
        return 1;
    }
    music_init();
    init_input_system();
    player_controller_init();
    gameplay_init();

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. INPUT
        handle_input();

        gameplay_frame(is_action_pressed(0, ACTION_BTN_START));
    }

    return 0;
}