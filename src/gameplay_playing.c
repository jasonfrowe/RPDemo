#include <stdint.h>
#include <stdbool.h>

#include "constants.h"
#include "enemy.h"
#include "game_state.h"
#include "level_bonus.h"
#include "music.h"
#include "player_controller.h"
#include "projectile.h"
#include "score.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "gameplay_internal.h"
#include "gameplay_boss.h"

void gameplay_update_playing_state(gameplay_runtime_t *state)
{
    int16_t player_x;
    int16_t player_y;
    int16_t hitbox_x;
    int16_t hitbox_y;
    uint8_t player_health;
    bool took_damage = false;

    if (state->player_script != PLAYER_SCRIPT_NONE) {
        gameplay_update_player_script(state);
        tile_mode2_update_health_fx(false, player_controller_is_low_health());
        return;
    }

    if (state->bonus_entry_pending) {
        sprite_mode5_set_frame(0);
        sprite_mode5_update_engine(false);
        if (state->bonus_entry_hold_timer > 0) {
            state->bonus_entry_hold_timer--;
        } else {
            state->bonus_entry_pending = false;
            if (game_state_enter_level_bonus() == GAME_TRANSITION_ENTER_LEVEL_BONUS) {
                level_bonus_begin(state->current_level, false);
            }
        }
        tile_mode2_update_health_fx(false, player_controller_is_low_health());
        return;
    }

    player_controller_update();
    projectile_update();
    enemy_update();

    if (state->level_banner_visible && enemy_has_active()) {
        tile_mode2_set_level_banner(state->current_level, false);
        state->level_banner_visible = false;
    }

    player_controller_get_position(&player_x, &player_y);
    hitbox_x = (int16_t)(player_x + PLAYER_HITBOX_OFFSET);
    hitbox_y = (int16_t)(player_y + PLAYER_HITBOX_OFFSET);

    if (player_controller_can_take_damage()) {
        if (projectile_hit_test_player(hitbox_x, hitbox_y, PLAYER_HITBOX_SIZE, PLAYER_HITBOX_SIZE)) {
            player_controller_apply_damage(PLAYER_BULLET_DAMAGE);
            took_damage = true;
        }
    }

    if (player_controller_can_take_damage()) {
        if (enemy_hit_test_player(hitbox_x, hitbox_y, PLAYER_HITBOX_SIZE, PLAYER_HITBOX_SIZE)) {
            player_controller_apply_damage(PLAYER_CONTACT_DAMAGE);
            took_damage = true;
        }
    }

    if (took_damage) {
        score_reset_multiplier();
    }

    player_health = player_controller_get_health();
    if (player_health != state->hud_health_last) {
        state->hud_health_last = player_health;
        tile_mode2_set_health(player_health);
    }
    tile_mode2_update_health_fx(
        player_controller_is_damage_flash_active(),
        player_controller_is_low_health()
    );

    if (player_controller_is_destroyed()) {
        if (game_state_enter_game_over() == GAME_TRANSITION_ENTER_GAME_OVER) {
            projectile_init();
            enemy_init();
            enemy_hide_bonus_icons();
            music_stop();
            state->game_over_timer = GAME_OVER_TIMEOUT_FRAMES;
            state->game_over_is_victory = false;
            state->game_over_letters_started = false;
            state->game_over_scroll_started = false;
            state->game_over_scroll_delay_timer = 0;
            level_bonus_reset();
            gameplay_clear_bonus_entry_state(state);
        }
    }

    if (!player_controller_is_destroyed() && enemy_is_level_complete() && state->player_script == PLAYER_SCRIPT_NONE) {
        player_controller_reset_damage_state();
        projectile_init();
        enemy_clear_all();
        game_state_enter_boss();
        gameplay_boss_begin(state);
    }
}
