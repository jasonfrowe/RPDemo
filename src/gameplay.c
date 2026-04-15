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

#define PLAYER_SCRIPT_STEP_PX 1
#define PLAYER_BONUS_TARGET_X 240
#define PLAYER_BONUS_TARGET_Y 120
#define PLAYER_START_X ((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2)
#define PLAYER_START_Y (((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2) / 3)
#define BONUS_ENTRY_HOLD_FRAMES 60
#define PLAYER_START_EXTRA_LIVES 2

static gameplay_runtime_t runtime_state = {
    .game_over_timer = 0,
    .hud_health_last = 0xFF,
    .extra_lives = PLAYER_START_EXTRA_LIVES,
    .game_over_is_victory = false,
    .game_over_letters_started = false,
    .game_over_scroll_started = false,
    .game_over_scroll_delay_timer = 0,
    .current_level = 1,
    .level_banner_visible = false,
    .bonus_entry_pending = false,
    .bonus_entry_hold_timer = 0,
    .player_script = PLAYER_SCRIPT_NONE,
};

static const char *track_for_level(uint8_t level)
{
    switch (level) {
        case 1:
            return "ROM:RESOURCE.005.vgm";
        case 2:
            return "ROM:RESOURCE.003.vgm";
        case 3:
            return "ROM:RESOURCE.008.vgm";
        case 4:
            return "ROM:RESOURCE.002.vgm";
        case 5:
            return "ROM:RESOURCE.005.vgm";
        case 6:
            return "ROM:RESOURCE.003.vgm";
        case 7:
            return "ROM:RESOURCE.010.vgm";
        default:
            return "ROM:RESOURCE.009.vgm";
    }
}

void gameplay_clear_bonus_entry_state(gameplay_runtime_t *state)
{
    state->player_script = PLAYER_SCRIPT_NONE;
    state->bonus_entry_pending = false;
    state->bonus_entry_hold_timer = 0;
}

void gameplay_update_player_script(gameplay_runtime_t *state)
{
    int16_t x;
    int16_t y;
    int16_t target_x;
    int16_t target_y;

    if (state->player_script == PLAYER_SCRIPT_NONE) {
        return;
    }

    sprite_mode5_set_frame(0);
    sprite_mode5_update_engine(false);

    player_controller_get_position(&x, &y);

    if (state->player_script == PLAYER_SCRIPT_TO_BONUS) {
        target_x = PLAYER_BONUS_TARGET_X;
        target_y = PLAYER_BONUS_TARGET_Y;
    } else {
        target_x = PLAYER_START_X;
        target_y = PLAYER_START_Y;
    }

    if (x < target_x) {
        x = (int16_t)(x + PLAYER_SCRIPT_STEP_PX);
        if (x > target_x) {
            x = target_x;
        }
    } else if (x > target_x) {
        x = (int16_t)(x - PLAYER_SCRIPT_STEP_PX);
        if (x < target_x) {
            x = target_x;
        }
    }

    if (y < target_y) {
        y = (int16_t)(y + PLAYER_SCRIPT_STEP_PX);
        if (y > target_y) {
            y = target_y;
        }
    } else if (y > target_y) {
        y = (int16_t)(y - PLAYER_SCRIPT_STEP_PX);
        if (y < target_y) {
            y = target_y;
        }
    }

    player_controller_set_position(x, y);

    if (x == target_x && y == target_y) {
        if (state->player_script == PLAYER_SCRIPT_TO_BONUS) {
            state->player_script = PLAYER_SCRIPT_NONE;
            state->bonus_entry_pending = true;
            state->bonus_entry_hold_timer = BONUS_ENTRY_HOLD_FRAMES;
        } else {
            state->player_script = PLAYER_SCRIPT_NONE;
        }
    }
}

void gameplay_reset_to_title_scene(gameplay_runtime_t *state)
{
    gameplay_boss_reset();
    enemy_stop_game_over_animation();
    enemy_hide_bonus_icons();
    projectile_init();
    enemy_init();
    player_controller_reset_for_new_run();
    tile_mode2_start_game_over_transition();
    tile_mode2_restore_hud_from_rom();
    tile_mode2_set_score(score_get());
    tile_mode2_set_multiplier(score_get_multiplier());
    tile_mode2_set_paused_banner(false);
    tile_mode2_set_level_banner(state->current_level, false);
    state->level_banner_visible = false;
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_set_bonus_continue_prompt(false);
    tile_mode2_set_health(PLAYER_MAX_HEALTH);
    tile_mode2_set_lives(0);
    state->hud_health_last = PLAYER_MAX_HEALTH;
    tile_mode2_update_health_fx(false, false);
    sprite_mode5_show_player();
    music_set_track("ROM:RESOURCE.001.vgm");
    state->game_over_timer = 0;
    state->game_over_is_victory = false;
    state->game_over_letters_started = false;
    state->game_over_scroll_started = false;
    state->game_over_scroll_delay_timer = 0;
    level_bonus_reset();
    gameplay_clear_bonus_entry_state(state);
    projectile_reset_pickup_sequence();
}

static void start_new_run(void)
{
    gameplay_runtime_t *state = &runtime_state;

    state->current_level = 1;
    state->extra_lives = PLAYER_START_EXTRA_LIVES;
    gameplay_boss_reset();
    enemy_stop_game_over_animation();
    enemy_hide_bonus_icons();
    projectile_init();
    enemy_start_level(state->current_level);
    score_init();
    score_reset_level_kills();
    player_controller_reset_for_new_run();
    tile_mode2_set_score(0);
    tile_mode2_set_multiplier(score_get_multiplier());
    tile_mode2_set_paused_banner(false);
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_set_bonus_continue_prompt(false);
    tile_mode2_set_health(PLAYER_MAX_HEALTH);
    tile_mode2_set_lives(state->extra_lives);
    state->hud_health_last = PLAYER_MAX_HEALTH;
    tile_mode2_update_health_fx(false, false);
    sprite_mode5_show_player();
    tile_mode2_clear_level_bonus();
    tile_mode2_start_gameplay_transition();
    tile_mode2_set_level_banner(state->current_level, true);
    state->level_banner_visible = true;
    music_set_track(track_for_level(state->current_level));
    state->game_over_is_victory = false;
    state->game_over_letters_started = false;
    state->game_over_scroll_started = false;
    state->game_over_scroll_delay_timer = 0;
    level_bonus_reset();
    gameplay_clear_bonus_entry_state(state);
    projectile_reset_pickup_sequence();
}

static void start_next_level(void)
{
    gameplay_runtime_t *state = &runtime_state;

    if (state->current_level < 255) {
        state->current_level++;
    }

    gameplay_boss_reset();
    enemy_hide_bonus_icons();
    projectile_init();
    enemy_start_level(state->current_level);
    score_reset_level_kills();
    tile_mode2_set_score(score_get());
    tile_mode2_set_multiplier(score_get_multiplier());
    tile_mode2_set_paused_banner(false);
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_set_bonus_continue_prompt(false);
    tile_mode2_set_health(player_controller_get_health());
    tile_mode2_set_lives(state->extra_lives);
    state->hud_health_last = player_controller_get_health();
    tile_mode2_update_health_fx(false, player_controller_is_low_health());
    sprite_mode5_show_player();
    tile_mode2_clear_level_bonus();
    tile_mode2_start_gameplay_transition();
    tile_mode2_set_level_banner(state->current_level, true);
    state->level_banner_visible = true;
    music_set_track(track_for_level(state->current_level));
    state->game_over_is_victory = false;
    level_bonus_reset();
    gameplay_clear_bonus_entry_state(state);
    state->player_script = PLAYER_SCRIPT_FROM_BONUS;
}

static void restart_current_level(void)
{
    gameplay_runtime_t *state = &runtime_state;

    gameplay_boss_reset();
    enemy_hide_bonus_icons();
    enemy_stop_game_over_animation();
    projectile_init();
    enemy_start_level(state->current_level);
    score_reset_level_kills();
    tile_mode2_set_score(score_get());
    tile_mode2_set_multiplier(score_get_multiplier());
    tile_mode2_set_paused_banner(false);
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_set_level_failed_banner(false);
    tile_mode2_set_push_start_prompt(false);
    tile_mode2_set_bonus_continue_prompt(false);
    tile_mode2_set_health(player_controller_get_health());
    tile_mode2_set_lives(state->extra_lives);
    state->hud_health_last = player_controller_get_health();
    tile_mode2_update_health_fx(false, player_controller_is_low_health());
    sprite_mode5_show_player();
    tile_mode2_clear_level_bonus();
    tile_mode2_start_gameplay_transition();
    tile_mode2_set_level_banner(state->current_level, true);
    state->level_banner_visible = true;
    music_set_track(track_for_level(state->current_level));
    state->game_over_is_victory = false;
    level_bonus_reset();
    gameplay_clear_bonus_entry_state(state);
}

static void start_victory_ending(void)
{
    gameplay_runtime_t *state = &runtime_state;

    if (game_state_enter_game_over() != GAME_TRANSITION_ENTER_GAME_OVER) {
        return;
    }

    gameplay_boss_reset();
    projectile_init();
    enemy_init();
    enemy_hide_bonus_icons();
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_set_bonus_continue_prompt(false);
    state->level_banner_visible = false;
    state->game_over_timer = GAME_OVER_TIMEOUT_FRAMES;
    state->game_over_is_victory = true;
    state->game_over_letters_started = false;
    state->game_over_scroll_started = false;
    state->game_over_scroll_delay_timer = 0;
    level_bonus_reset();
    gameplay_clear_bonus_entry_state(state);
}

static void handle_start_transition(game_transition_t transition)
{
    if (transition == GAME_TRANSITION_START_GAME) {
        start_new_run();
    } else if (transition == GAME_TRANSITION_PAUSE_GAME) {
        tile_mode2_set_paused_banner(true);
    } else if (transition == GAME_TRANSITION_UNPAUSE_GAME) {
        tile_mode2_set_paused_banner(false);
    } else if (transition == GAME_TRANSITION_START_NEXT_LEVEL) {
        if (level_bonus_is_complete()) {
            if (runtime_state.current_level >= 7) {
                start_victory_ending();
            } else {
                start_next_level();
            }
        } else {
            game_state_enter_level_bonus();
        }
    } else if (transition == GAME_TRANSITION_RETRY_LEVEL) {
        restart_current_level();
    } else if (transition == GAME_TRANSITION_RETURN_TO_TITLE) {
        game_state_init();
        gameplay_reset_to_title_scene(&runtime_state);
    }
}

void gameplay_init(void)
{
    game_state_init();
    tile_mode2_set_health(player_controller_get_health());
    tile_mode2_set_lives(0);
    runtime_state.hud_health_last = player_controller_get_health();
}

void gameplay_frame(bool start_pressed)
{
    game_transition_t transition = game_state_handle_start_button(start_pressed);
    game_state_t state;

    handle_start_transition(transition);

    music_update();

    state = game_state_get();
    if (state == GAME_STATE_TITLE) {
        sprite_mode5_update_engine(false);
        tile_mode2_update_title_palette();
    }

    if (state != GAME_STATE_PAUSED) {
        tile_mode2_update_scroll();
    }

    if (state == GAME_STATE_PLAYING) {
        gameplay_update_playing_state(&runtime_state);
    } else if (state == GAME_STATE_BOSS) {
        gameplay_boss_update(&runtime_state);
    } else if (state == GAME_STATE_LEVEL_BONUS) {
        gameplay_update_bonus_state(&runtime_state);
    } else if (state == GAME_STATE_LEVEL_FAILED) {
        sprite_mode5_set_frame(0);
        sprite_mode5_update_engine(false);
    } else if (state == GAME_STATE_GAME_OVER) {
        gameplay_update_game_over_state(&runtime_state);
    }
}
