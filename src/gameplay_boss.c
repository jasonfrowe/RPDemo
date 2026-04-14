#include <stdint.h>
#include <stdbool.h>

#include "constants.h"
#include "enemy.h"
#include "game_state.h"
#include "level_bonus.h"
#include "music.h"
#include "player_controller.h"
#include "projectile.h"
#include "rng.h"
#include "score.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "gameplay_boss.h"

#define BOSS_ENTRY_SPEED_PX 2
#define BOSS_ARENA_MARGIN_X 8
#define BOSS_ARENA_TOP_Y 24
#define BOSS_ARENA_BOTTOM_Y ((SCREEN_HEIGHT / 3) - (BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX))
#define BOSS_PIVOT_STEP_X 3
#define BOSS_PIVOT_STEP_Y 2
#define BOSS_PIVOT_COUNT 8
#define BOSS_PIVOT_LEFT_X BOSS_ARENA_MARGIN_X
#define BOSS_PIVOT_CENTER_X BOSS_START_X
#define BOSS_PIVOT_RIGHT_X (SCREEN_WIDTH - (BOSS_GRID_COLS * ENEMY_SPRITE_SIZE_PX) - BOSS_ARENA_MARGIN_X)
#define BOSS_PIVOT_TOP_Y BOSS_ARENA_TOP_Y
#define BOSS_PIVOT_MID_Y (BOSS_ARENA_TOP_Y + ((BOSS_ARENA_BOTTOM_Y - BOSS_ARENA_TOP_Y) / 2))
#define BOSS_PIVOT_BOTTOM_Y BOSS_ARENA_BOTTOM_Y
#define BOSS_ANIM_STEP_FRAMES 24
#define BOSS_ATTACK_CYCLE_FRAMES 180
#define BOSS_ATTACK_WINDOW_FRAMES 54
#define BOSS_ATTACK_FIRE_INTERVAL_FRAMES 8
#define BOSS_ATTACK_VOLLEYS_PER_WINDOW 3
#define BOSS_SHOT_ALIGN_TOLERANCE_PX 18
#define BOSS_PROJECTILE_VX_Q8 0
#define BOSS_PROJECTILE_VY_Q8 (3 << 8)
#define BOSS_DAMAGE_PER_HIT 4
#define BOSS_HIT_SCORE_POINTS 100
#define BOSS_HIT_FLASH_FRAMES 12
#define BOSS_WAVE_INITIAL_DELAY_FRAMES 180
#define BOSS_WAVE_INTERVAL_FRAMES 420
#define BOSS_WAVE_SPAWN_COUNT (ENEMY_WAVE_SIZE)
#define BOSS_WAVE_INTER_SPAWN_FRAMES 12
#define BOSS_WAVE_ASTEROID_COUNT 2
#define BOSS_ASTEROID_WAVE_THIRD 3
#define BOSS_ASTEROID_WAVE_SIXTH 6
#define BOSS_VARIANT_ONE   1
#define BOSS_VARIANT_TWO   2
#define BOSS_VARIANT_THREE 3
#define BOSS_VARIANT_FOUR  4
#define BOSS_VARIANT_FIVE  5
#define BOSS_VARIANT_SIX   6
#define BOSS_VARIANT_SEVEN 7
#define BOSS_VARIANT_FOUR_MIN_ENEMIES_FOR_DAMAGE 3
#define BOSS_LEVEL_TWO_FRAME_SET_A_BASE 68
#define BOSS_LEVEL_TWO_FRAME_SET_B_BASE 74
#define BOSS_LEVEL_TWO_FRAME_SET_ATTACK_BASE 80
#define BOSS_LEVEL_TWO_COLLISION_X_MIN 11
#define BOSS_LEVEL_TWO_COLLISION_X_MAX 36
#define BOSS_LEVEL_THREE_FRAME_SET_A_BASE 86
#define BOSS_LEVEL_THREE_FRAME_SET_B_BASE 92
#define BOSS_LEVEL_THREE_FRAME_SET_ATTACK_BASE 98
#define BOSS_LEVEL_FOUR_FRAME_SET_A_BASE 104
#define BOSS_LEVEL_FOUR_FRAME_SET_B_BASE 110
#define BOSS_LEVEL_FOUR_FRAME_SET_ATTACK_BASE 116
#define BOSS_LEVEL_FIVE_FRAME_SET_A_BASE 122
#define BOSS_LEVEL_FIVE_FRAME_SET_B_BASE 128
#define BOSS_LEVEL_FIVE_FRAME_SET_ATTACK_BASE 134
#define BOSS_LEVEL_SIX_FRAME_SET_A_BASE 140
#define BOSS_LEVEL_SIX_FRAME_SET_B_BASE 146
#define BOSS_LEVEL_SIX_FRAME_SET_ATTACK_BASE 152
#define BOSS_LEVEL_SEVEN_FRAME_SET_A_BASE 158
#define BOSS_LEVEL_SEVEN_FRAME_SET_B_BASE 164
#define BOSS_LEVEL_SEVEN_FRAME_SET_ATTACK_BASE 170
#define BOSS_WAVE_DIVE_EXIT_SPEED_PX 3
#define BOSS_WAVE_DIVE_RISE_SPEED_PX 3
#define BOSS_WAVE_DIVE_UNDER_PLAYER_OFFSET_PX 2
#define BOSS_WAVE_DIVE_PHASE_EXIT_TOP 0
#define BOSS_WAVE_DIVE_PHASE_RISE_FROM_BOTTOM 1
#define BOSS_WAVE_SLIDE_PHASE_SINK_TO_BOTTOM 0
#define BOSS_WAVE_SLIDE_PHASE_SLIDE_X 1
#define BOSS_WAVE_SLIDE_PHASE_RISE_TO_ARENA 2
#define BOSS_WAVE_SLIDE_SINK_SPEED_PX 3
#define BOSS_WAVE_SLIDE_SPEED_PX 3
#define BOSS_WAVE_SLIDE_RISE_SPEED_PX 3
#define BOSS_WAVE_SLIDE_BOTTOM_Y (SCREEN_HEIGHT - (BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX))
#define BOSS_DEFEAT_SEQUENCE_FRAMES (3 * 60)
#define BOSS_DEFEAT_EXPLOSION_INTERVAL_FRAMES 3
#define BOSS_DEFEAT_EXPLOSIONS_PER_BURST 3
#define BOSS_DEFEAT_EXPLOSION_MARGIN 4
#define PLAYER_SCRIPT_STEP_PX 1
#define PLAYER_START_X ((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2)
#define PLAYER_START_Y (((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2) / 3)

static bool boss_active = false;
static int16_t boss_x = BOSS_START_X;
static int16_t boss_y = BOSS_START_Y;
static uint8_t boss_anim_tick = 0;
static uint8_t boss_frame_set = BOSS_FRAME_SET_A_BASE;
static uint16_t boss_attack_cycle_timer = 0;
static uint8_t boss_attack_fire_timer = 0;
static uint8_t boss_attack_volleys_fired = 0;
static uint8_t boss_health = BOSS_MAX_HEALTH;
static uint32_t boss_fight_timer = 0;
static uint8_t boss_hit_flash_timer = 0;
static uint8_t boss_pivot_index = 0;
static bool boss_entering = true;
static bool boss_retreating = false;
static uint16_t boss_wave_timer = 0;
static uint8_t boss_wave_spawned = 0;
static uint8_t boss_wave_type = 0;
static uint8_t boss_wave_inter_spawn_timer = 0;
static bool boss_wave_active = false;
static uint8_t boss_wave_count = 0;
static uint8_t boss_variant = BOSS_VARIANT_ONE;
static uint8_t boss_frame_set_a_base = BOSS_FRAME_SET_A_BASE;
static uint8_t boss_frame_set_b_base = BOSS_FRAME_SET_B_BASE;
static uint8_t boss_frame_set_attack_base = BOSS_FRAME_SET_ATTACK_BASE;
static uint8_t boss_collision_x_min = BOSS_WEAKSPOT_X_MIN;
static uint8_t boss_collision_x_max = BOSS_WEAKSPOT_X_MAX;
static bool boss_wave_dive_active = false;
static uint8_t boss_wave_dive_phase = BOSS_WAVE_DIVE_PHASE_EXIT_TOP;
static bool boss_wave_slide_active = false;
static uint8_t boss_wave_slide_phase = BOSS_WAVE_SLIDE_PHASE_SINK_TO_BOTTOM;
static int16_t boss_wave_slide_target_x = BOSS_START_X;
static bool boss_defeat_sequence_active = false;
static uint16_t boss_defeat_timer = 0;
static uint8_t boss_defeat_spawn_timer = 0;
static uint16_t boss_defeat_rng = 0xC35Fu;

static const int16_t boss_pivot_x[BOSS_PIVOT_COUNT] = {
    BOSS_PIVOT_LEFT_X,
    BOSS_PIVOT_CENTER_X,
    BOSS_PIVOT_RIGHT_X,
    BOSS_PIVOT_RIGHT_X,
    BOSS_PIVOT_CENTER_X,
    BOSS_PIVOT_LEFT_X,
    BOSS_PIVOT_LEFT_X,
    BOSS_PIVOT_CENTER_X,
};

static const int16_t boss_pivot_y[BOSS_PIVOT_COUNT] = {
    BOSS_PIVOT_TOP_Y,
    BOSS_PIVOT_TOP_Y,
    BOSS_PIVOT_MID_Y,
    BOSS_PIVOT_BOTTOM_Y,
    BOSS_PIVOT_BOTTOM_Y,
    BOSS_PIVOT_MID_Y,
    BOSS_PIVOT_BOTTOM_Y,
    BOSS_PIVOT_TOP_Y,
};

static void boss_move_player_to_start(void)
{
    int16_t x;
    int16_t y;

    player_controller_get_position(&x, &y);

    if (x < PLAYER_START_X) {
        x = (int16_t)(x + PLAYER_SCRIPT_STEP_PX);
        if (x > PLAYER_START_X) {
            x = PLAYER_START_X;
        }
    } else if (x > PLAYER_START_X) {
        x = (int16_t)(x - PLAYER_SCRIPT_STEP_PX);
        if (x < PLAYER_START_X) {
            x = PLAYER_START_X;
        }
    }

    if (y < PLAYER_START_Y) {
        y = (int16_t)(y + PLAYER_SCRIPT_STEP_PX);
        if (y > PLAYER_START_Y) {
            y = PLAYER_START_Y;
        }
    } else if (y > PLAYER_START_Y) {
        y = (int16_t)(y - PLAYER_SCRIPT_STEP_PX);
        if (y < PLAYER_START_Y) {
            y = PLAYER_START_Y;
        }
    }

    player_controller_set_position(x, y);
    sprite_mode5_set_frame(0);
    sprite_mode5_update_engine(false);
}

static void boss_spawn_defeat_explosions(void)
{
    int16_t boss_left = (int16_t)(boss_x - BOSS_DEFEAT_EXPLOSION_MARGIN);
    int16_t boss_top = (int16_t)(boss_y - BOSS_DEFEAT_EXPLOSION_MARGIN);
    int16_t boss_right = (int16_t)(boss_x + (BOSS_GRID_COLS * ENEMY_SPRITE_SIZE_PX) - PROJECTILE_SPRITE_SIZE_PX + BOSS_DEFEAT_EXPLOSION_MARGIN);
    int16_t boss_bottom = (int16_t)(boss_y + (BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX) - PROJECTILE_SPRITE_SIZE_PX + BOSS_DEFEAT_EXPLOSION_MARGIN);

    if (boss_left < 0) {
        boss_left = 0;
    }
    if (boss_top < HUD_TOP_PX) {
        boss_top = HUD_TOP_PX;
    }
    if (boss_right > (int16_t)(SCREEN_WIDTH - PROJECTILE_SPRITE_SIZE_PX)) {
        boss_right = (int16_t)(SCREEN_WIDTH - PROJECTILE_SPRITE_SIZE_PX);
    }
    if (boss_bottom > (int16_t)(SCREEN_HEIGHT - PROJECTILE_SPRITE_SIZE_PX)) {
        boss_bottom = (int16_t)(SCREEN_HEIGHT - PROJECTILE_SPRITE_SIZE_PX);
    }

    for (uint8_t i = 0; i < BOSS_DEFEAT_EXPLOSIONS_PER_BURST; ++i) {
        int16_t ex = rng_range(&boss_defeat_rng, boss_left, boss_right);
        int16_t ey = rng_range(&boss_defeat_rng, boss_top, boss_bottom);
        projectile_spawn_explosion(ex, ey);
    }
}

static void boss_update_defeat_sequence(gameplay_runtime_t *state)
{
    (void)state;

    boss_move_player_to_start();

    if (boss_defeat_spawn_timer == 0) {
        boss_spawn_defeat_explosions();
        boss_defeat_spawn_timer = BOSS_DEFEAT_EXPLOSION_INTERVAL_FRAMES;
    } else {
        boss_defeat_spawn_timer--;
    }

    projectile_update();
    tile_mode2_update_health_fx(false, player_controller_is_low_health());
    sprite_mode5_set_boss_weakspot_flash(false);
    sprite_mode5_show_boss(boss_x, boss_y, boss_frame_set_a_base);

    if (boss_defeat_timer > 0) {
        boss_defeat_timer--;
    }

    if (boss_defeat_timer == 0) {
        projectile_init();
        boss_defeat_sequence_active = false;
        boss_retreating = true;
    }
}

static void boss_fire_dual_projectiles(void)
{
    int16_t boss_width = (int16_t)(BOSS_GRID_COLS * ENEMY_SPRITE_SIZE_PX);
    int16_t boss_height = (int16_t)(BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX);
    int16_t center_x = (int16_t)(boss_x + (boss_width / 2));
    int16_t spawn_y = (int16_t)(boss_y + boss_height);
    int16_t left_x = (int16_t)(center_x - (BOSS_PROJECTILE_SEPARATION_PX / 2));
    int16_t right_x = (int16_t)(center_x + (BOSS_PROJECTILE_SEPARATION_PX / 2));

    projectile_fire_enemy(left_x, spawn_y, BOSS_PROJECTILE_VX_Q8, BOSS_PROJECTILE_VY_Q8, BOSS_PROJECTILE_LEFT_FRAME);
    projectile_fire_enemy(right_x, spawn_y, BOSS_PROJECTILE_VX_Q8, BOSS_PROJECTILE_VY_Q8, BOSS_PROJECTILE_RIGHT_FRAME);
}

void gameplay_boss_begin(gameplay_runtime_t *state)
{
    int16_t boss_height = (int16_t)(BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX);

    if (state->current_level >= 7) {
        boss_variant = BOSS_VARIANT_SEVEN;
        boss_frame_set_a_base = BOSS_LEVEL_SEVEN_FRAME_SET_A_BASE;
        boss_frame_set_b_base = BOSS_LEVEL_SEVEN_FRAME_SET_B_BASE;
        boss_frame_set_attack_base = BOSS_LEVEL_SEVEN_FRAME_SET_ATTACK_BASE;
        boss_collision_x_min = BOSS_WEAKSPOT_X_MIN;
        boss_collision_x_max = BOSS_WEAKSPOT_X_MAX;
    } else if (state->current_level >= 6) {
        boss_variant = BOSS_VARIANT_SIX;
        boss_frame_set_a_base = BOSS_LEVEL_SIX_FRAME_SET_A_BASE;
        boss_frame_set_b_base = BOSS_LEVEL_SIX_FRAME_SET_B_BASE;
        boss_frame_set_attack_base = BOSS_LEVEL_SIX_FRAME_SET_ATTACK_BASE;
        boss_collision_x_min = BOSS_WEAKSPOT_X_MIN;
        boss_collision_x_max = BOSS_WEAKSPOT_X_MAX;
    } else if (state->current_level >= 5) {
        boss_variant = BOSS_VARIANT_FIVE;
        boss_frame_set_a_base = BOSS_LEVEL_FIVE_FRAME_SET_A_BASE;
        boss_frame_set_b_base = BOSS_LEVEL_FIVE_FRAME_SET_B_BASE;
        boss_frame_set_attack_base = BOSS_LEVEL_FIVE_FRAME_SET_ATTACK_BASE;
        boss_collision_x_min = BOSS_WEAKSPOT_X_MIN;
        boss_collision_x_max = BOSS_WEAKSPOT_X_MAX;
    } else if (state->current_level >= 4) {
        boss_variant = BOSS_VARIANT_FOUR;
        boss_frame_set_a_base = BOSS_LEVEL_FOUR_FRAME_SET_A_BASE;
        boss_frame_set_b_base = BOSS_LEVEL_FOUR_FRAME_SET_B_BASE;
        boss_frame_set_attack_base = BOSS_LEVEL_FOUR_FRAME_SET_ATTACK_BASE;
        boss_collision_x_min = BOSS_WEAKSPOT_X_MIN;
        boss_collision_x_max = BOSS_WEAKSPOT_X_MAX;
    } else if (state->current_level >= 3) {
        boss_variant = BOSS_VARIANT_THREE;
        boss_frame_set_a_base = BOSS_LEVEL_THREE_FRAME_SET_A_BASE;
        boss_frame_set_b_base = BOSS_LEVEL_THREE_FRAME_SET_B_BASE;
        boss_frame_set_attack_base = BOSS_LEVEL_THREE_FRAME_SET_ATTACK_BASE;
        boss_collision_x_min = BOSS_WEAKSPOT_X_MIN;
        boss_collision_x_max = BOSS_WEAKSPOT_X_MAX;
    } else if (state->current_level >= 2) {
        boss_variant = BOSS_VARIANT_TWO;
        boss_frame_set_a_base = BOSS_LEVEL_TWO_FRAME_SET_A_BASE;
        boss_frame_set_b_base = BOSS_LEVEL_TWO_FRAME_SET_B_BASE;
        boss_frame_set_attack_base = BOSS_LEVEL_TWO_FRAME_SET_ATTACK_BASE;
        boss_collision_x_min = BOSS_LEVEL_TWO_COLLISION_X_MIN;
        boss_collision_x_max = BOSS_LEVEL_TWO_COLLISION_X_MAX;
    } else {
        boss_variant = BOSS_VARIANT_ONE;
        boss_frame_set_a_base = BOSS_FRAME_SET_A_BASE;
        boss_frame_set_b_base = BOSS_FRAME_SET_B_BASE;
        boss_frame_set_attack_base = BOSS_FRAME_SET_ATTACK_BASE;
        boss_collision_x_min = BOSS_WEAKSPOT_X_MIN;
        boss_collision_x_max = BOSS_WEAKSPOT_X_MAX;
    }

    boss_active = true;
    boss_x = BOSS_START_X;
    boss_y = (int16_t)(-boss_height);
    boss_anim_tick = 0;
    boss_frame_set = boss_frame_set_a_base;
    boss_attack_cycle_timer = 0;
    boss_attack_fire_timer = 0;
    boss_attack_volleys_fired = 0;
    boss_health = BOSS_MAX_HEALTH;
    boss_fight_timer = 0;
    boss_hit_flash_timer = 0;
    boss_pivot_index = 0;
    boss_entering = true;
    boss_retreating = false;
    boss_wave_timer = BOSS_WAVE_INITIAL_DELAY_FRAMES;
    boss_wave_spawned = 0;
    boss_wave_type = 0;
    boss_wave_inter_spawn_timer = 0;
    boss_wave_active = false;
    boss_wave_count = 0;
    boss_wave_dive_active = false;
    boss_wave_dive_phase = BOSS_WAVE_DIVE_PHASE_EXIT_TOP;
    boss_wave_slide_active = false;
    boss_wave_slide_phase = BOSS_WAVE_SLIDE_PHASE_SINK_TO_BOTTOM;
    boss_wave_slide_target_x = BOSS_START_X;
    boss_defeat_sequence_active = false;
    boss_defeat_timer = 0;
    boss_defeat_spawn_timer = 0;
    boss_defeat_rng = 0xC35Fu;
    state->level_banner_visible = false;
    state->hud_health_last = player_controller_get_health();
    music_set_track(BOSS_STAGE_MUSIC_TRACK);
    tile_mode2_set_boss_hud_visible(true);
    tile_mode2_set_boss_health(boss_health);
    sprite_mode5_set_boss_palette_active(true);
    sprite_mode5_show_boss(boss_x, boss_y, boss_frame_set);
}

void gameplay_boss_update(gameplay_runtime_t *state)
{
    int16_t player_x;
    int16_t player_y;
    int16_t hitbox_x;
    int16_t hitbox_y;
    uint8_t player_health;
    bool took_damage = false;
    int16_t weakspot_x;
    int16_t weakspot_y;
    int16_t weakspot_w;
    int16_t weakspot_h;
    bool boss_can_take_damage = true;

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
                level_bonus_begin(state->current_level, state->level_banner_visible);
                state->level_banner_visible = false;
            }
        }
        tile_mode2_update_health_fx(false, player_controller_is_low_health());
        return;
    }

    if (!boss_active) {
        return;
    }

    if (boss_defeat_sequence_active) {
        boss_update_defeat_sequence(state);
        return;
    }

    if (boss_retreating) {
        boss_y = (int16_t)(boss_y - BOSS_ENTRY_SPEED_PX);
        if (boss_y <= (int16_t)(-(BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX))) {
            gameplay_boss_reset();
            projectile_init();
            enemy_clear_all();
            player_controller_reset_damage_state();
            tile_mode2_set_level_complete_banner(true);
            state->player_script = PLAYER_SCRIPT_TO_BONUS;
            return;
        }
        sprite_mode5_show_boss(boss_x, boss_y, boss_frame_set);
        return;
    }

    boss_fight_timer++;

    player_controller_update();
    projectile_update();
    enemy_update();

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
        int16_t boss_collision_left = (int16_t)(boss_x + boss_collision_x_min);
        int16_t boss_collision_right = (int16_t)(boss_x + boss_collision_x_max + 1);

        if (hitbox_x < boss_collision_right &&
            (int16_t)(hitbox_x + PLAYER_HITBOX_SIZE) > boss_collision_left &&
            hitbox_y < (int16_t)(boss_y + (BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX)) &&
            (int16_t)(hitbox_y + PLAYER_HITBOX_SIZE) > boss_y) {
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
            gameplay_boss_reset();
            projectile_init();
            enemy_init();
            enemy_hide_bonus_icons();
            music_stop();
            state->game_over_timer = GAME_OVER_TIMEOUT_FRAMES;
            state->game_over_letters_started = false;
            state->game_over_scroll_started = false;
            state->game_over_scroll_delay_timer = 0;
            level_bonus_reset();
            gameplay_clear_bonus_entry_state(state);
        }
        return;
    }

    if (boss_entering) {
        boss_y = (int16_t)(boss_y + BOSS_ENTRY_SPEED_PX);
        if (boss_y >= BOSS_ARENA_TOP_Y) {
            boss_y = BOSS_ARENA_TOP_Y;
            boss_entering = false;
            boss_pivot_index = 0;
        }
    } else if (boss_wave_dive_active) {
        int16_t boss_width = (int16_t)(BOSS_GRID_COLS * ENEMY_SPRITE_SIZE_PX);
        int16_t boss_height = (int16_t)(BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX);

        if (boss_wave_dive_phase == BOSS_WAVE_DIVE_PHASE_EXIT_TOP) {
            boss_y = (int16_t)(boss_y - BOSS_WAVE_DIVE_EXIT_SPEED_PX);
            if (boss_y <= (int16_t)(-boss_height)) {
                int16_t player_center_x = (int16_t)(player_x + (PLAYER_SPRITE_SIZE_PX / 2));
                int16_t target_x = (int16_t)(player_center_x - (boss_width / 2));
                int16_t min_x = BOSS_ARENA_MARGIN_X;
                int16_t max_x = (int16_t)(SCREEN_WIDTH - boss_width - BOSS_ARENA_MARGIN_X);

                if (target_x < min_x) {
                    target_x = min_x;
                } else if (target_x > max_x) {
                    target_x = max_x;
                }

                boss_x = target_x;
                boss_y = (int16_t)(SCREEN_HEIGHT + BOSS_WAVE_DIVE_UNDER_PLAYER_OFFSET_PX);
                boss_wave_dive_phase = BOSS_WAVE_DIVE_PHASE_RISE_FROM_BOTTOM;
            }
        } else {
            boss_y = (int16_t)(boss_y - BOSS_WAVE_DIVE_RISE_SPEED_PX);
            if (boss_y <= BOSS_ARENA_TOP_Y) {
                boss_y = BOSS_ARENA_TOP_Y;
                boss_wave_dive_active = false;
                boss_wave_dive_phase = BOSS_WAVE_DIVE_PHASE_EXIT_TOP;
                boss_pivot_index = 0;
            }
        }
    } else if (boss_wave_slide_active) {
        int16_t boss_width = (int16_t)(BOSS_GRID_COLS * ENEMY_SPRITE_SIZE_PX);

        if (boss_wave_slide_phase == BOSS_WAVE_SLIDE_PHASE_SINK_TO_BOTTOM) {
            boss_y = (int16_t)(boss_y + BOSS_WAVE_SLIDE_SINK_SPEED_PX);
            if (boss_y >= BOSS_WAVE_SLIDE_BOTTOM_Y) {
                boss_y = BOSS_WAVE_SLIDE_BOTTOM_Y;
                /* Lock on to the player's current x position */
                int16_t player_center_x = (int16_t)(player_x + (PLAYER_SPRITE_SIZE_PX / 2));
                int16_t target_x = (int16_t)(player_center_x - (boss_width / 2));
                int16_t min_x = BOSS_ARENA_MARGIN_X;
                int16_t max_x = (int16_t)(SCREEN_WIDTH - boss_width - BOSS_ARENA_MARGIN_X);

                if (target_x < min_x) {
                    target_x = min_x;
                } else if (target_x > max_x) {
                    target_x = max_x;
                }

                boss_wave_slide_target_x = target_x;
                boss_wave_slide_phase = BOSS_WAVE_SLIDE_PHASE_SLIDE_X;
            }
        } else if (boss_wave_slide_phase == BOSS_WAVE_SLIDE_PHASE_SLIDE_X) {
            if (boss_x < boss_wave_slide_target_x) {
                boss_x = (int16_t)(boss_x + BOSS_WAVE_SLIDE_SPEED_PX);
                if (boss_x > boss_wave_slide_target_x) {
                    boss_x = boss_wave_slide_target_x;
                }
            } else if (boss_x > boss_wave_slide_target_x) {
                boss_x = (int16_t)(boss_x - BOSS_WAVE_SLIDE_SPEED_PX);
                if (boss_x < boss_wave_slide_target_x) {
                    boss_x = boss_wave_slide_target_x;
                }
            }

            if (boss_x == boss_wave_slide_target_x) {
                boss_wave_slide_phase = BOSS_WAVE_SLIDE_PHASE_RISE_TO_ARENA;
            }
        } else {
            boss_y = (int16_t)(boss_y - BOSS_WAVE_SLIDE_RISE_SPEED_PX);
            if (boss_y <= BOSS_ARENA_TOP_Y) {
                boss_y = BOSS_ARENA_TOP_Y;
                boss_wave_slide_active = false;
                boss_wave_slide_phase = BOSS_WAVE_SLIDE_PHASE_SINK_TO_BOTTOM;
                boss_pivot_index = 0;
            }
        }
    } else {
        int16_t target_x = boss_pivot_x[boss_pivot_index];
        int16_t target_y = boss_pivot_y[boss_pivot_index];

        if (boss_x < target_x) {
            boss_x = (int16_t)(boss_x + BOSS_PIVOT_STEP_X);
            if (boss_x > target_x) {
                boss_x = target_x;
            }
        } else if (boss_x > target_x) {
            boss_x = (int16_t)(boss_x - BOSS_PIVOT_STEP_X);
            if (boss_x < target_x) {
                boss_x = target_x;
            }
        }

        if (boss_y < target_y) {
            boss_y = (int16_t)(boss_y + BOSS_PIVOT_STEP_Y);
            if (boss_y > target_y) {
                boss_y = target_y;
            }
        } else if (boss_y > target_y) {
            boss_y = (int16_t)(boss_y - BOSS_PIVOT_STEP_Y);
            if (boss_y < target_y) {
                boss_y = target_y;
            }
        }

        if (boss_x == target_x && boss_y == target_y) {
            boss_pivot_index++;
            if (boss_pivot_index >= BOSS_PIVOT_COUNT) {
                boss_pivot_index = 0;
            }
        }
    }

    if (!boss_entering && !boss_wave_dive_active && !boss_wave_slide_active) {
        boss_attack_cycle_timer = (uint16_t)(boss_attack_cycle_timer + 1);
        if (boss_attack_cycle_timer >= BOSS_ATTACK_CYCLE_FRAMES) {
            boss_attack_cycle_timer = 0;
        }

        if (boss_attack_cycle_timer == 0) {
            boss_attack_fire_timer = 0;
            boss_attack_volleys_fired = 0;
        }

        if (boss_attack_cycle_timer < BOSS_ATTACK_WINDOW_FRAMES) {
            boss_frame_set = boss_frame_set_attack_base;
            if (boss_attack_fire_timer > 0) {
                boss_attack_fire_timer--;
            }

            if (boss_attack_volleys_fired < BOSS_ATTACK_VOLLEYS_PER_WINDOW &&
                boss_attack_fire_timer == 0) {
                int16_t player_center_x = (int16_t)(player_x + (PLAYER_SPRITE_SIZE_PX / 2));
                int16_t boss_center_x = (int16_t)(boss_x + ((BOSS_GRID_COLS * ENEMY_SPRITE_SIZE_PX) / 2));

                // First shot in burst needs alignment; once started, complete the 3-volley burst.
                if (boss_attack_volleys_fired > 0 ||
                    (player_center_x >= (int16_t)(boss_center_x - BOSS_SHOT_ALIGN_TOLERANCE_PX) &&
                     player_center_x <= (int16_t)(boss_center_x + BOSS_SHOT_ALIGN_TOLERANCE_PX))) {
                    boss_fire_dual_projectiles();
                    boss_attack_volleys_fired++;
                    boss_attack_fire_timer = BOSS_ATTACK_FIRE_INTERVAL_FRAMES;
                }
            }
        } else {
            boss_attack_fire_timer = 0;
            boss_anim_tick = (uint8_t)(boss_anim_tick + 1);
            if (boss_anim_tick >= BOSS_ANIM_STEP_FRAMES) {
                boss_anim_tick = 0;
                if (boss_frame_set == boss_frame_set_a_base) {
                    boss_frame_set = boss_frame_set_b_base;
                } else {
                    boss_frame_set = boss_frame_set_a_base;
                }
            }
        }
    } else {
        boss_attack_fire_timer = 0;
        boss_attack_cycle_timer = 0;
        boss_frame_set = boss_frame_set_a_base;
    }

    weakspot_x = (int16_t)(boss_x + boss_collision_x_min);
    weakspot_y = (int16_t)(boss_y + BOSS_WEAKSPOT_Y_MIN);
    weakspot_w = (int16_t)(boss_collision_x_max - boss_collision_x_min + 1);
    weakspot_h = (int16_t)(BOSS_WEAKSPOT_Y_MAX - BOSS_WEAKSPOT_Y_MIN + 1);

    if (boss_variant == BOSS_VARIANT_FOUR) {
        boss_can_take_damage = (enemy_get_active_count() >= BOSS_VARIANT_FOUR_MIN_ENEMIES_FOR_DAMAGE);
    }
    sprite_mode5_set_boss_palette_active(boss_can_take_damage);

    if (boss_health > 0 && boss_can_take_damage &&
        projectile_hit_test_enemy(weakspot_x, weakspot_y, weakspot_w, weakspot_h)) {
        if (boss_health > BOSS_DAMAGE_PER_HIT) {
            boss_health = (uint8_t)(boss_health - BOSS_DAMAGE_PER_HIT);
        } else {
            boss_health = 0;
        }
        score_add_points(BOSS_HIT_SCORE_POINTS);
        boss_hit_flash_timer = BOSS_HIT_FLASH_FRAMES;
        tile_mode2_set_boss_health(boss_health);
    }

    if (boss_can_take_damage && boss_hit_flash_timer > 0) {
        boss_hit_flash_timer--;
        sprite_mode5_set_boss_weakspot_flash(true);
    } else {
        boss_hit_flash_timer = 0;
        sprite_mode5_set_boss_weakspot_flash(false);
    }

    if (boss_health == 0) {
        state->level_banner_visible = true;
        boss_defeat_sequence_active = true;
        boss_defeat_timer = BOSS_DEFEAT_SEQUENCE_FRAMES;
        boss_defeat_spawn_timer = 0;
        boss_defeat_rng = (uint16_t)(0xC35Fu ^ (uint16_t)boss_x ^ ((uint16_t)boss_y << 1));
        boss_retreating = false;
        boss_wave_active = false;
        boss_wave_spawned = 0;
        boss_wave_inter_spawn_timer = 0;
        boss_attack_cycle_timer = 0;
        boss_attack_fire_timer = 0;
        boss_attack_volleys_fired = 0;
        boss_wave_dive_active = false;
        boss_wave_dive_phase = BOSS_WAVE_DIVE_PHASE_EXIT_TOP;
        boss_wave_slide_active = false;
        boss_wave_slide_phase = BOSS_WAVE_SLIDE_PHASE_SINK_TO_BOTTOM;
        enemy_clear_all();
        projectile_init();
        sprite_mode5_set_boss_weakspot_flash(false);
        return;
    }

    if (boss_fight_timer >= BOSS_FIGHT_TIMEOUT_FRAMES) {
        gameplay_boss_reset();
        projectile_init();
        enemy_clear_all();
        player_controller_reset_damage_state();
        tile_mode2_set_level_complete_banner(false);
        state->player_script = PLAYER_SCRIPT_TO_BONUS;
        return;
    }

    if (!boss_entering && !boss_retreating) {
        if (!boss_wave_active) {
            if (boss_wave_timer > 0) {
                boss_wave_timer--;
            } else {
                boss_wave_type = (state->current_level > 0) ? (uint8_t)(state->current_level - 1u) : 0u;
                if (boss_wave_type >= ENEMY_TYPE_COUNT) {
                    boss_wave_type = (uint8_t)(ENEMY_TYPE_COUNT - 1u);
                }
                boss_wave_spawned = 0;
                boss_wave_inter_spawn_timer = 0;
                boss_wave_count++;
                if (boss_wave_count == BOSS_ASTEROID_WAVE_THIRD ||
                    boss_wave_count == BOSS_ASTEROID_WAVE_SIXTH) {
                    projectile_spawn_asteroid_wave(BOSS_WAVE_ASTEROID_COUNT);
                }
                if (boss_variant == BOSS_VARIANT_TWO) {
                    boss_wave_dive_active = true;
                    boss_wave_dive_phase = BOSS_WAVE_DIVE_PHASE_EXIT_TOP;
                } else if (boss_variant == BOSS_VARIANT_THREE) {
                    boss_wave_slide_active = true;
                    boss_wave_slide_phase = BOSS_WAVE_SLIDE_PHASE_SINK_TO_BOTTOM;
                }
                boss_wave_active = true;
            }
        } else {
            if (boss_wave_inter_spawn_timer > 0) {
                boss_wave_inter_spawn_timer--;
            } else {
                enemy_spawn_for_boss(boss_wave_type, boss_wave_spawned);
                boss_wave_spawned++;

                if (boss_wave_spawned >= BOSS_WAVE_SPAWN_COUNT) {
                    boss_wave_active = false;
                    boss_wave_timer = BOSS_WAVE_INTERVAL_FRAMES;
                } else {
                    boss_wave_inter_spawn_timer = BOSS_WAVE_INTER_SPAWN_FRAMES;
                }
            }
        }
    }

    sprite_mode5_show_boss(boss_x, boss_y, boss_frame_set);
}

void gameplay_boss_reset(void)
{
    boss_active = false;
    boss_x = BOSS_START_X;
    boss_y = BOSS_START_Y;
    boss_anim_tick = 0;
    boss_frame_set = BOSS_FRAME_SET_A_BASE;
    boss_attack_cycle_timer = 0;
    boss_attack_fire_timer = 0;
    boss_attack_volleys_fired = 0;
    boss_health = BOSS_MAX_HEALTH;
    boss_fight_timer = 0;
    boss_hit_flash_timer = 0;
    boss_pivot_index = 0;
    boss_entering = true;
    boss_retreating = false;
    boss_wave_timer = BOSS_WAVE_INITIAL_DELAY_FRAMES;
    boss_wave_spawned = 0;
    boss_wave_type = 0;
    boss_wave_inter_spawn_timer = 0;
    boss_wave_active = false;
    boss_wave_count = 0;
    boss_variant = BOSS_VARIANT_ONE;
    boss_frame_set_a_base = BOSS_FRAME_SET_A_BASE;
    boss_frame_set_b_base = BOSS_FRAME_SET_B_BASE;
    boss_frame_set_attack_base = BOSS_FRAME_SET_ATTACK_BASE;
    boss_collision_x_min = BOSS_WEAKSPOT_X_MIN;
    boss_collision_x_max = BOSS_WEAKSPOT_X_MAX;
    boss_wave_dive_active = false;
    boss_wave_dive_phase = BOSS_WAVE_DIVE_PHASE_EXIT_TOP;
    boss_wave_slide_active = false;
    boss_wave_slide_phase = BOSS_WAVE_SLIDE_PHASE_SINK_TO_BOTTOM;
    boss_wave_slide_target_x = BOSS_START_X;
    boss_defeat_sequence_active = false;
    boss_defeat_timer = 0;
    boss_defeat_spawn_timer = 0;
    boss_defeat_rng = 0xC35Fu;
    sprite_mode5_set_boss_palette_active(false);
    tile_mode2_set_boss_hud_visible(false);
    sprite_mode5_hide_boss();
}
