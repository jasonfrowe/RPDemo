#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "input.h"
#include "player_controller.h"
#include "game_state.h"
#include "music.h"
#include "projectile.h"
#include "enemy.h"

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

    return true;
}

uint8_t vsync_last = 0;

int main(void)
{

    // Initialize input
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);

    // Initialise graphics
    if (!init_graphics()) {
        puts("Fatal: graphics initialization failed");
        return 1;
    }
    music_init();
    init_input_system();
    player_controller_init();
    game_state_init();

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. INPUT
        handle_input();

        {
            game_transition_t transition = game_state_handle_start_button(
                is_action_pressed(0, ACTION_BTN_START)
            );

            if (transition == GAME_TRANSITION_START_GAME) {
                tile_mode2_start_gameplay_transition();
                music_set_track("music/RESOURCE.005.vgm");
            }
        }

        // 3. UPDATE
        music_update();
        if (game_state_get() == GAME_STATE_TITLE) {
            tile_mode2_update_title_palette();
        }
        if (game_state_get() != GAME_STATE_PAUSED) {
            tile_mode2_update_scroll();
        }
        if (game_state_get() == GAME_STATE_PLAYING) {
            player_controller_update();
            projectile_update();
            enemy_update();
        }
    }

    return 0;
}