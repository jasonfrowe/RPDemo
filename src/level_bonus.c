#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "constants.h"
#include "enemy.h"
#include "music.h"
#include "player_controller.h"
#include "projectile.h"
#include "score.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "level_bonus.h"

typedef enum {
    BONUS_PHASE_IDLE = 0,
    BONUS_PHASE_ICON_FLY_IN,
    BONUS_PHASE_COUNT_KILLS,
    BONUS_PHASE_ROW_HOLD,
    BONUS_PHASE_COUNTDOWN_TOTAL,
    BONUS_PHASE_REFILL_HEALTH,
    BONUS_PHASE_DONE,
} bonus_phase_t;

#define BONUS_COUNT_STEP_FRAMES 6
#define BONUS_ROW_HOLD_FRAMES 36
#define BONUS_PAYOUT_STEP_FRAMES 3
#define BONUS_HEALTH_STEP_FRAMES 6
#define BONUS_BOSS_POINTS 10000
#define BONUS_BOSS_HEALTH_BONUS 6
#define BONUS_BASE_HEALTH_RECOVERY_CAP 24

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
static uint16_t bonus_boss_points = 0;
static bool level_bonus_complete = false;

void level_bonus_reset(void)
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
    bonus_boss_points = 0;
    level_bonus_complete = false;
    tile_mode2_set_bonus_continue_prompt(false);
}

void level_bonus_begin(uint8_t current_level, bool boss_defeated)
{
    uint16_t total_kills = 0;
    uint16_t base_health_recovery;

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
    base_health_recovery = (uint16_t)(total_kills / 3u);
    if (base_health_recovery > BONUS_BASE_HEALTH_RECOVERY_CAP) {
        base_health_recovery = BONUS_BASE_HEALTH_RECOVERY_CAP;
    }
    bonus_health_pending = base_health_recovery;
    if (boss_defeated) {
        bonus_health_pending = (uint16_t)(bonus_health_pending + BONUS_BOSS_HEALTH_BONUS);
    }
    bonus_health_tick = 0;
    bonus_payout_tick = 0;
    bonus_pending_total = 0;
    bonus_boss_points = boss_defeated ? BONUS_BOSS_POINTS : 0;
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

void level_bonus_update(uint8_t *hud_health_last)
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
            } else if (bonus_row_index == ENEMY_TYPE_COUNT) {
                tile_mode2_set_bonus_boss_row(bonus_boss_points);
                bonus_pending_total += bonus_boss_points;
                tile_mode2_set_bonus_pending_total(bonus_pending_total);
                bonus_row_hold_timer = BONUS_ROW_HOLD_FRAMES;
                bonus_phase = BONUS_PHASE_ROW_HOLD;
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
                    if (hud_health_last != NULL) {
                        *hud_health_last = player_controller_get_health();
                    }
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

bool level_bonus_is_complete(void)
{
    return level_bonus_complete;
}
