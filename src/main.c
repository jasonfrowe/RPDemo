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
#include "score.h"

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
static uint16_t game_over_timer = 0;
static uint8_t hud_health_last = 0xFF;

static void reset_to_title_scene(void)
{
    enemy_stop_game_over_animation();
    projectile_init();
    enemy_init();
    score_init();
    player_controller_reset_for_new_run();
    tile_mode2_start_game_over_transition();
    tile_mode2_restore_hud_from_rom();
    tile_mode2_set_score(0);
    tile_mode2_set_health(PLAYER_MAX_HEALTH);
    hud_health_last = PLAYER_MAX_HEALTH;
    tile_mode2_update_health_fx(false, false);
    music_set_track("music/RESOURCE.001.vgm");
    game_over_timer = 0;
}

static void start_playing_scene(void)
{
    projectile_init();
    enemy_init();
    score_init();
    player_controller_reset_for_new_run();
    tile_mode2_set_score(0);
    tile_mode2_set_health(PLAYER_MAX_HEALTH);
    hud_health_last = PLAYER_MAX_HEALTH;
    tile_mode2_update_health_fx(false, false);
    tile_mode2_start_gameplay_transition();
    music_set_track("music/RESOURCE.005.vgm");
}

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
    game_state_init();
    tile_mode2_set_health(player_controller_get_health());
    hud_health_last = player_controller_get_health();

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
                start_playing_scene();
            } else if (transition == GAME_TRANSITION_RETURN_TO_TITLE) {
                reset_to_title_scene();
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
            int16_t player_x;
            int16_t player_y;
            uint8_t player_health;

            player_controller_update();
            projectile_update();
            enemy_update();

            player_controller_get_position(&player_x, &player_y);

            if (player_controller_can_take_damage()) {
                if (projectile_hit_test_player(player_x, player_y, PLAYER_SPRITE_SIZE_PX, PLAYER_SPRITE_SIZE_PX)) {
                    player_controller_apply_damage(PLAYER_BULLET_DAMAGE);
                }
            }

            if (player_controller_can_take_damage()) {
                if (enemy_hit_test_player(player_x, player_y, PLAYER_SPRITE_SIZE_PX, PLAYER_SPRITE_SIZE_PX)) {
                    player_controller_apply_damage(PLAYER_CONTACT_DAMAGE);
                }
            }

            player_health = player_controller_get_health();
            if (player_health != hud_health_last) {
                hud_health_last = player_health;
                tile_mode2_set_health(player_health);
            }
            tile_mode2_update_health_fx(
                player_controller_is_damage_flash_active(),
                player_controller_is_low_health()
            );

            if (player_controller_is_destroyed()) {
                if (game_state_enter_game_over() == GAME_TRANSITION_ENTER_GAME_OVER) {
                    projectile_init();
                    enemy_start_game_over_animation();
                    tile_mode2_start_game_over_transition();
                    tile_mode2_restore_hud_from_rom();
                    tile_mode2_set_health(0);
                    tile_mode2_update_health_fx(false, true);
                    music_set_track("music/RESOURCE.011.vgm");
                    game_over_timer = GAME_OVER_TIMEOUT_FRAMES;
                }
            }
        } else if (game_state_get() == GAME_STATE_GAME_OVER) {
            enemy_update();
            tile_mode2_update_health_fx(false, true);

            if (game_over_timer > 0) {
                game_over_timer--;
            }

            if (game_over_timer == 0) {
                game_state_init();
                reset_to_title_scene();
            }
        }
    }

    return 0;
}