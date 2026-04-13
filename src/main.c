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
static bool game_over_letters_started = false;
static bool game_over_scroll_started = false;
static uint16_t game_over_scroll_delay_timer = 0;
static uint8_t current_level = 1;
static bool level_banner_visible = false;
static bool bonus_entry_pending = false;
static uint8_t bonus_entry_hold_timer = 0;

typedef enum {
    BONUS_PHASE_IDLE = 0,
    BONUS_PHASE_ICON_FLY_IN,
    BONUS_PHASE_COUNT_KILLS,
    BONUS_PHASE_ROW_HOLD,
    BONUS_PHASE_COUNTDOWN_TOTAL,
    BONUS_PHASE_REFILL_HEALTH,
    BONUS_PHASE_DONE,
} bonus_phase_t;

typedef enum {
    PLAYER_SCRIPT_NONE = 0,
    PLAYER_SCRIPT_TO_BONUS,
    PLAYER_SCRIPT_FROM_BONUS,
} player_script_t;

static bonus_phase_t bonus_phase = BONUS_PHASE_IDLE;
static uint16_t bonus_kills[ENEMY_TYPE_COUNT];
static uint8_t bonus_multiplier = 1;
static uint8_t bonus_row_index = 0;
static uint16_t bonus_row_target_kills = 0;
static uint16_t bonus_row_count_display = 0;
static uint16_t bonus_row_points_each = 0;
static uint16_t bonus_row_subtotal = 0;
static uint8_t bonus_count_tick = 0;
static uint8_t bonus_row_hold_timer = 0;
static uint16_t bonus_health_pending = 0;
static uint8_t bonus_health_tick = 0;
static uint8_t bonus_payout_tick = 0;
static uint32_t bonus_pending_total = 0;
static bool level_bonus_complete = false;
static player_script_t player_script = PLAYER_SCRIPT_NONE;

#define BONUS_COUNT_STEP_FRAMES 6
#define PLAYER_SCRIPT_STEP_PX 1
#define PLAYER_BONUS_TARGET_X 240
#define PLAYER_BONUS_TARGET_Y 120
#define PLAYER_START_X ((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2)
#define PLAYER_START_Y (((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2) / 3)
#define BONUS_ROW_HOLD_FRAMES 36
#define BONUS_PAYOUT_STEP_FRAMES 3
#define BONUS_HEALTH_STEP_FRAMES 6
#define BONUS_ENTRY_HOLD_FRAMES 60

static void begin_level_bonus_sequence(void);

static const char *track_for_level(uint8_t level)
{
    switch (level) {
        case 1:
            return "music/RESOURCE.005.vgm";
        case 2:
            return "music/RESOURCE.003.vgm";
        case 3:
            return "music/RESOURCE.008.vgm";
        default:
            return "music/RESOURCE.009.vgm";
    }
}

static void reset_level_bonus_sequence(void)
{
    for (uint8_t i = 0; i < ENEMY_TYPE_COUNT; ++i) {
        bonus_kills[i] = 0;
    }

    bonus_phase = BONUS_PHASE_IDLE;
    bonus_multiplier = 1;
    bonus_row_index = 0;
    bonus_row_target_kills = 0;
    bonus_row_count_display = 0;
    bonus_row_points_each = 0;
    bonus_row_subtotal = 0;
    bonus_count_tick = 0;
    bonus_row_hold_timer = 0;
    bonus_health_pending = 0;
    bonus_health_tick = 0;
    bonus_payout_tick = 0;
    bonus_pending_total = 0;
    level_bonus_complete = false;
    player_script = PLAYER_SCRIPT_NONE;
    bonus_entry_pending = false;
    bonus_entry_hold_timer = 0;
    tile_mode2_set_bonus_continue_prompt(false);
}

static void update_player_script(void)
{
    int16_t x;
    int16_t y;
    int16_t target_x;
    int16_t target_y;

    if (player_script == PLAYER_SCRIPT_NONE) {
        return;
    }

        // During autopilot: show rest frame and continue engine animation
        sprite_mode5_set_frame(0);
        sprite_mode5_update_engine(false);

    player_controller_get_position(&x, &y);

    if (player_script == PLAYER_SCRIPT_TO_BONUS) {
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
        if (player_script == PLAYER_SCRIPT_TO_BONUS) {
            player_script = PLAYER_SCRIPT_NONE;
            bonus_entry_pending = true;
            bonus_entry_hold_timer = BONUS_ENTRY_HOLD_FRAMES;
        } else {
            player_script = PLAYER_SCRIPT_NONE;
        }
    }
}

static void begin_level_bonus_sequence(void)
{
    uint16_t total_kills = 0;

    bonus_multiplier = current_level;
    if (bonus_multiplier == 0) {
        bonus_multiplier = 1;
    }

    for (uint8_t i = 0; i < ENEMY_TYPE_COUNT; ++i) {
        bonus_kills[i] = score_get_level_kills(i);
        total_kills = (uint16_t)(total_kills + bonus_kills[i]);
    }

    bonus_row_index = 0;
    bonus_row_target_kills = 0;
    bonus_row_count_display = 0;
    bonus_row_points_each = 0;
    bonus_row_subtotal = 0;
    bonus_count_tick = 0;
    bonus_row_hold_timer = 0;
    bonus_health_pending = (uint16_t)(total_kills / 3u);
    bonus_health_tick = 0;
    bonus_payout_tick = 0;
    bonus_pending_total = 0;
    level_bonus_complete = false;

    projectile_init();
    enemy_clear_all();
    enemy_prepare_bonus_icons();
    sprite_mode5_show_player();

    tile_mode2_start_level_bonus_transition();
    music_set_track("music/RESOURCE.006.vgm");
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_begin_level_bonus(current_level, bonus_multiplier);
    tile_mode2_set_bonus_pending_total(0);
    tile_mode2_set_bonus_continue_prompt(false);

    enemy_start_bonus_icon_fly_in(0);
    bonus_phase = BONUS_PHASE_ICON_FLY_IN;
}

static void update_level_bonus_sequence(void)
{
    switch (bonus_phase) {
        case BONUS_PHASE_IDLE:
            break;

        case BONUS_PHASE_ICON_FLY_IN:
            if (enemy_update_bonus_icon_fly_in()) {
                bonus_row_target_kills = bonus_kills[bonus_row_index];
                bonus_row_count_display = 0;
                bonus_row_points_each = (uint16_t)(score_points_for_enemy(bonus_row_index) * bonus_multiplier);
                bonus_row_subtotal = 0;
                bonus_count_tick = 0;

                tile_mode2_set_bonus_row(
                    bonus_row_index,
                    bonus_row_count_display,
                    bonus_row_points_each,
                    bonus_row_subtotal
                );

                bonus_phase = BONUS_PHASE_COUNT_KILLS;
            }
            break;

        case BONUS_PHASE_COUNT_KILLS:
            bonus_count_tick++;
            if (bonus_count_tick >= BONUS_COUNT_STEP_FRAMES) {
                bonus_count_tick = 0;

                if (bonus_row_count_display < bonus_row_target_kills) {
                    bonus_row_count_display++;
                }

                bonus_row_subtotal = (uint16_t)(bonus_row_count_display * bonus_row_points_each);
                tile_mode2_set_bonus_row(
                    bonus_row_index,
                    bonus_row_count_display,
                    bonus_row_points_each,
                    bonus_row_subtotal
                );
            }

            if (bonus_row_count_display >= bonus_row_target_kills) {
                bonus_pending_total += bonus_row_subtotal;
                tile_mode2_set_bonus_pending_total(bonus_pending_total);
                bonus_row_hold_timer = BONUS_ROW_HOLD_FRAMES;
                bonus_phase = BONUS_PHASE_ROW_HOLD;
            }
            break;

        case BONUS_PHASE_ROW_HOLD:
            if (bonus_row_hold_timer > 0) {
                bonus_row_hold_timer--;
                break;
            }

            bonus_row_index++;
            if (bonus_row_index < ENEMY_TYPE_COUNT) {
                enemy_start_bonus_icon_fly_in(bonus_row_index);
                bonus_phase = BONUS_PHASE_ICON_FLY_IN;
            } else {
                bonus_phase = BONUS_PHASE_COUNTDOWN_TOTAL;
            }
            break;

        case BONUS_PHASE_COUNTDOWN_TOTAL:
            if (bonus_pending_total > 0) {
                uint32_t payout_step;

                bonus_payout_tick++;
                if (bonus_payout_tick < BONUS_PAYOUT_STEP_FRAMES) {
                    break;
                }
                bonus_payout_tick = 0;

                if (bonus_pending_total >= 100u) {
                    payout_step = 100u;
                } else if (bonus_pending_total >= 40u) {
                    payout_step = 40u;
                } else if (bonus_pending_total >= 12u) {
                    payout_step = 12u;
                } else {
                    payout_step = bonus_pending_total;
                }

                bonus_pending_total -= payout_step;
                score_add_points(payout_step);
                tile_mode2_set_bonus_pending_total(bonus_pending_total);
            } else {
                bonus_phase = BONUS_PHASE_REFILL_HEALTH;
            }
            break;

        case BONUS_PHASE_REFILL_HEALTH:
            if (bonus_health_pending > 0 && player_controller_get_health() < PLAYER_MAX_HEALTH) {
                bonus_health_tick++;
                if (bonus_health_tick >= BONUS_HEALTH_STEP_FRAMES) {
                    bonus_health_tick = 0;
                    player_controller_heal(1);
                    bonus_health_pending--;
                    tile_mode2_set_health(player_controller_get_health());
                    hud_health_last = player_controller_get_health();
                    tile_mode2_update_health_fx(false, player_controller_is_low_health());
                }
            } else {
                bonus_phase = BONUS_PHASE_DONE;
                level_bonus_complete = true;
                tile_mode2_set_bonus_continue_prompt(true);
            }
            break;

        case BONUS_PHASE_DONE:
            break;
    }
}

static void reset_to_title_scene(void)
{
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
    tile_mode2_set_level_banner(current_level, false);
    level_banner_visible = false;
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_set_bonus_continue_prompt(false);
    tile_mode2_set_health(PLAYER_MAX_HEALTH);
    hud_health_last = PLAYER_MAX_HEALTH;
    tile_mode2_update_health_fx(false, false);
    sprite_mode5_show_player();
    music_set_track("music/RESOURCE.001.vgm");
    game_over_timer = 0;
    game_over_letters_started = false;
    game_over_scroll_started = false;
    game_over_scroll_delay_timer = 0;
    reset_level_bonus_sequence();
}

static void start_new_run(void)
{
    current_level = 1;
    enemy_stop_game_over_animation();
    enemy_hide_bonus_icons();
    projectile_init();
    enemy_start_level(current_level);
    score_init();
    score_reset_level_kills();
    player_controller_reset_for_new_run();
    tile_mode2_set_score(0);
    tile_mode2_set_multiplier(score_get_multiplier());
    tile_mode2_set_paused_banner(false);
    tile_mode2_set_level_banner(current_level, true);
    level_banner_visible = true;
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_set_bonus_continue_prompt(false);
    tile_mode2_set_health(PLAYER_MAX_HEALTH);
    hud_health_last = PLAYER_MAX_HEALTH;
    tile_mode2_update_health_fx(false, false);
    sprite_mode5_show_player();
    tile_mode2_clear_level_bonus();
    tile_mode2_start_gameplay_transition();
    music_set_track(track_for_level(current_level));
    game_over_letters_started = false;
    game_over_scroll_started = false;
    game_over_scroll_delay_timer = 0;
    reset_level_bonus_sequence();
}

static void start_next_level(void)
{
    if (current_level < 255) {
        current_level++;
    }

    enemy_hide_bonus_icons();
    projectile_init();
    enemy_start_level(current_level);
    score_reset_level_kills();
    tile_mode2_set_score(score_get());
    tile_mode2_set_multiplier(score_get_multiplier());
    tile_mode2_set_paused_banner(false);
    tile_mode2_set_level_banner(current_level, true);
    level_banner_visible = true;
    tile_mode2_set_level_complete_banner(false);
    tile_mode2_set_bonus_continue_prompt(false);
    tile_mode2_set_health(player_controller_get_health());
    hud_health_last = player_controller_get_health();
    tile_mode2_update_health_fx(false, player_controller_is_low_health());
    sprite_mode5_show_player();
    tile_mode2_clear_level_bonus();
    tile_mode2_start_gameplay_transition();
    music_set_track(track_for_level(current_level));
    reset_level_bonus_sequence();
    player_script = PLAYER_SCRIPT_FROM_BONUS;
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
                start_new_run();
            } else if (transition == GAME_TRANSITION_PAUSE_GAME) {
                tile_mode2_set_paused_banner(true);
            } else if (transition == GAME_TRANSITION_UNPAUSE_GAME) {
                tile_mode2_set_paused_banner(false);
            } else if (transition == GAME_TRANSITION_START_NEXT_LEVEL) {
                if (level_bonus_complete) {
                    start_next_level();
                } else {
                    game_state_enter_level_bonus();
                }
            } else if (transition == GAME_TRANSITION_RETURN_TO_TITLE) {
                game_state_init();
                reset_to_title_scene();
            }
        }

        // 3. UPDATE
        music_update();
        if (game_state_get() == GAME_STATE_TITLE) {
            sprite_mode5_update_engine(false);
            tile_mode2_update_title_palette();
        }
        if (game_state_get() != GAME_STATE_PAUSED) {
            tile_mode2_update_scroll();
        }
        if (game_state_get() == GAME_STATE_PLAYING) {
            int16_t player_x;
            int16_t player_y;
            int16_t hitbox_x;
            int16_t hitbox_y;
            uint8_t player_health;
            bool took_damage = false;

            if (player_script != PLAYER_SCRIPT_NONE) {
                update_player_script();
                tile_mode2_update_health_fx(false, player_controller_is_low_health());
                continue;
            }

            if (bonus_entry_pending) {
                sprite_mode5_set_frame(0);
                sprite_mode5_update_engine(false);
                if (bonus_entry_hold_timer > 0) {
                    bonus_entry_hold_timer--;
                } else {
                    bonus_entry_pending = false;
                    if (game_state_enter_level_bonus() == GAME_TRANSITION_ENTER_LEVEL_BONUS) {
                        begin_level_bonus_sequence();
                    }
                }
                tile_mode2_update_health_fx(false, player_controller_is_low_health());
                continue;
            }

            player_controller_update();
            projectile_update();
            enemy_update();

            if (level_banner_visible && enemy_has_active()) {
                tile_mode2_set_level_banner(current_level, false);
                level_banner_visible = false;
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
                    enemy_init();
                    enemy_hide_bonus_icons();
                    music_stop();
                    game_over_timer = GAME_OVER_TIMEOUT_FRAMES;
                    game_over_letters_started = false;
                    game_over_scroll_started = false;
                    game_over_scroll_delay_timer = 0;
                    reset_level_bonus_sequence();
                }
            }

            if (!player_controller_is_destroyed() && enemy_is_level_complete() && player_script == PLAYER_SCRIPT_NONE) {
                player_controller_reset_damage_state();
                projectile_init();
                enemy_clear_all();
                tile_mode2_set_level_complete_banner(true);
                player_script = PLAYER_SCRIPT_TO_BONUS;
            }
        } else if (game_state_get() == GAME_STATE_LEVEL_BONUS) {
            sprite_mode5_set_frame(0);
            sprite_mode5_update_engine(false);
            update_level_bonus_sequence();
        } else if (game_state_get() == GAME_STATE_GAME_OVER) {
            player_controller_update();

            if (!game_over_letters_started && player_controller_is_death_animation_complete()) {
                enemy_start_game_over_animation();
                music_set_track("music/RESOURCE.011.vgm");
                game_over_letters_started = true;
            }

            if (game_over_letters_started) {
                enemy_update();

                if (enemy_is_game_over_animation_complete()) {
                    sprite_mode5_hide_player();

                    if (!game_over_scroll_started) {
                        if (game_over_scroll_delay_timer < GAME_OVER_SCROLL_START_DELAY_FRAMES) {
                            game_over_scroll_delay_timer++;
                        } else {
                            tile_mode2_start_game_over_transition();
                            tile_mode2_restore_hud_from_rom();
                            tile_mode2_set_score(score_get());
                            tile_mode2_set_multiplier(score_get_multiplier());
                            tile_mode2_set_paused_banner(false);
                            tile_mode2_set_level_banner(current_level, false);
                            level_banner_visible = false;
                            tile_mode2_set_level_complete_banner(false);
                            tile_mode2_set_bonus_continue_prompt(false);
                            tile_mode2_set_health(0);
                            game_over_scroll_started = true;
                        }
                    }
                }
            }
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