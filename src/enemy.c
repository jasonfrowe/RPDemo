#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "constants.h"
#include "enemy.h"
#include "player_controller.h"
#include "projectile.h"
#include "rng.h"
#include "score.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"

#define Q8_SHIFT 8
#define TO_Q8(px) ((int32_t)(px) * (1 << Q8_SHIFT))
#define FROM_Q8(value) ((int16_t)((value) >> Q8_SHIFT))

#define ENEMY_MEDIUM_SPEED_Q8 ((int16_t)TO_Q8(2))
#define ENEMY_SLOW_SPEED_Q8   ((int16_t)TO_Q8(1))
#define ENEMY_FAST_SPEED_Q8   ((int16_t)TO_Q8(3))
#define ENEMY_DIVE_SPEED_Q8   ((int16_t)TO_Q8(4))

#define BULLET_SLOW_SPEED_Q8   ((int16_t)TO_Q8(1))
#define BULLET_MEDIUM_SPEED_Q8 ((int16_t)TO_Q8(2))
#define BULLET_FAST_SPEED_Q8   ((int16_t)TO_Q8(3))

#define TYPE0_PHASE_ZIG   0
#define TYPE0_PHASE_DIVE  1

#define TYPE1_PHASE_RISE    0
#define TYPE1_PHASE_ATTACK  1
#define TYPE1_PHASE_EXIT    2

#define TYPE3_PHASE_MOVE   0
#define TYPE3_PHASE_ATTACK 1
#define TYPE3_PHASE_EXIT   2

#define TYPE4_PHASE_DESCEND 0
#define TYPE4_PHASE_DIVE    1

#define TYPE6_PHASE_CHASE 0

#define TYPE1_MIDPOINT_Y   (SCREEN_HEIGHT / 2)
#define TYPE1_SHOTS_PER_ENEMY 6
#define TYPE1_FIRE_INTERVAL_FRAMES 10
#define TYPE0_INTER_SPAWN_FRAMES 10
#define TYPE3_MIN_TARGET_Y (HUD_TOP_PX + 40)
#define TYPE3_MAX_TARGET_Y 120
#define TYPE3_ATTACK_DURATION_FRAMES 480
#define TYPE3_WAVE_FIRE_INTERVAL 8
#define TYPE6_DETONATE_DIST_X 40
#define TYPE6_DETONATE_DIST_Y 40
#define AIM_LEAD_MIN_FRAMES 3
#define AIM_LEAD_MAX_FRAMES 10
#define AIM_MAX_LEAD_PER_AXIS 6

#define TYPE5_FORMATION_SPACING_X 28
#define TYPE5_FORMATION_SPACING_Y 18
#define TYPE5_MAX_COLUMNS 5
#define TYPE5_FORMATION_Y        (HUD_TOP_PX + 24)
#define TYPE5_STEP_DOWN_Q8       TO_Q8(12)
#define TYPE5_STEP_DOWN_SPEED_Q8 ENEMY_SLOW_SPEED_Q8
#define TYPE5_PHASE_FORM_IN      0
#define TYPE5_PHASE_FORMED       1

#define GAME_OVER_LETTER_COUNT 8
#define GAME_OVER_FIRST_FRAME 42
#define GAME_OVER_LETTER_SPEED_Q8 ENEMY_SLOW_SPEED_Q8
#define GAME_OVER_LETTER_START_DELAY_FRAMES 8
#define GAME_OVER_VERTICAL_OFFSET_PX 24

#define ENEMY_FRAMES_PER_TYPE 6
#define ENEMY_ACTIVE_FRAMES 3
#define ENEMY_DEATH_FRAME_START 3
#define ENEMY_ACTIVE_ANIM_STEP_FRAMES 8

#define LEVEL_SUBWAVE_COUNT 13
#define WAVE_CLEAR_TIMEOUT_FRAMES 240
#define BONUS_ICON_FLY_IN_START_X (-20)
#define BONUS_ICON_FLY_IN_SPEED_Q8 TO_Q8(4)

typedef struct {
    bool    active;
    bool    dying;
    uint8_t type;
    uint8_t wave_id;
    uint8_t anim_frame;
    uint8_t anim_tick;
    uint8_t death_frame;
    uint8_t death_tick;
    uint8_t phase;
    uint8_t slot_index;
    uint8_t path_variant;
    uint8_t waypoint_index;
    uint8_t shots_remaining;
    uint8_t spiral_step;
    uint16_t timer;
    uint16_t fire_timer;
    int32_t x_q8;
    int32_t y_q8;
    int16_t vx_q8;
    int16_t vy_q8;
    int16_t target_x;
    int16_t target_y;
    int16_t home_x;
    int16_t home_y;
} Enemy;

static Enemy enemies[MAX_ENEMIES];

typedef struct {
    bool active;
    uint8_t wave_id;
    uint8_t enemy_type;
    uint8_t type5_spawn_count;
    uint8_t type5_spawned_count;
    uint8_t formation_columns;
    uint8_t type_group_rank;
    bool formation_started;
    int32_t formation_anchor_x_q8;
    int32_t formation_anchor_y_q8;
    int16_t formation_vx_q8;
    int32_t formation_step_down_remaining_q8;
} WaveGroup;

static WaveGroup wave_groups[MAX_ENEMIES];

typedef enum {
    WAVE_STATE_DELAY,
    WAVE_STATE_SPAWNING,
    WAVE_STATE_CLEARING,
} WaveState;

static WaveState wave_state;
static uint8_t wave_type;
static uint8_t wave_spawned;
static uint16_t wave_timer;
static uint16_t wave_clear_timeout_timer;

static uint16_t rng_state;
static int16_t wave_origin_x;
static uint8_t wave_variant;
static uint8_t wave_type0_shots_remaining;
static uint8_t current_level;
static uint8_t current_subwave;
static bool level_complete;
static uint8_t wave_primary_type;
static uint8_t wave_spawn_count;
static uint8_t wave_spawn_types[MAX_ENEMIES];
static uint8_t active_wave_id;
static int32_t formation_anchor_x_q8;
static int32_t formation_anchor_y_q8;
static int16_t formation_vx_q8;
static int32_t formation_step_down_remaining_q8;
static uint8_t formation_columns;
static uint8_t type5_spawn_count;
static uint8_t type5_spawned_count;
static bool formation_started;
static int16_t tracked_player_x;
static int16_t tracked_player_y;
static int16_t tracked_player_vx;
static int16_t tracked_player_vy;
static bool player_tracking_initialized;
static bool game_over_mode;
static bool game_over_animation_complete;
static uint8_t game_over_letter_start_delay;
static bool bonus_icon_anim_active;
static bool bonus_icon_anim_complete;
static uint8_t bonus_icon_anim_type;
static int32_t bonus_icon_anim_x_q8;
static int32_t bonus_icon_anim_y_q8;
static int32_t bonus_icon_anim_target_x_q8;
static int32_t bonus_icon_anim_target_y_q8;

typedef struct {
    bool active;
    int32_t x_q8;
    int32_t y_q8;
    int32_t target_x_q8;
    int32_t target_y_q8;
    uint8_t frame;
} GameOverLetter;

static GameOverLetter game_over_letters[GAME_OVER_LETTER_COUNT];

static WaveGroup *enemy_wave_group_get(uint8_t wave_id)
{
    return &wave_groups[(uint8_t)(wave_id % MAX_ENEMIES)];
}

static void enemy_deactivate(uint8_t slot);

static const int8_t radial_dirs[8][2] = {
    { 8,  0},
    { 6,  6},
    { 0,  8},
    {-6,  6},
    {-8,  0},
    {-6, -6},
    { 0, -8},
    { 6, -6},
};

static const int8_t spiral_dirs[16][2] = {
    { 8,  0},
    { 7,  3},
    { 6,  6},
    { 3,  7},
    { 0,  8},
    {-3,  7},
    {-6,  6},
    {-7,  3},
    {-8,  0},
    {-7, -3},
    {-6, -6},
    {-3, -7},
    { 0, -8},
    { 3, -7},
    { 6, -6},
    { 7, -3},
};

static int16_t enemy_abs16(int16_t value)
{
    return (value < 0) ? (int16_t)(-value) : value;
}

static uint8_t enemy_base_frame_for_type(uint8_t enemy_type)
{
    return (uint8_t)(enemy_type * ENEMY_FRAMES_PER_TYPE);
}

static void enemy_update_active_animation(uint8_t slot)
{
    uint8_t base_frame;
    uint8_t frame_offset;

    if (enemies[slot].dying) {
        return;
    }

    enemies[slot].anim_tick++;
    if (enemies[slot].anim_tick < ENEMY_ACTIVE_ANIM_STEP_FRAMES) {
        return;
    }

    enemies[slot].anim_tick = 0;
    base_frame = enemy_base_frame_for_type(enemies[slot].type);
    frame_offset = (uint8_t)(enemies[slot].anim_frame - base_frame);
    frame_offset = (uint8_t)((frame_offset + 1) % ENEMY_ACTIVE_FRAMES);
    enemies[slot].anim_frame = (uint8_t)(base_frame + frame_offset);
}

static void enemy_begin_death(uint8_t slot)
{
    uint8_t base_frame = enemy_base_frame_for_type(enemies[slot].type);

    enemies[slot].dying = true;
    enemies[slot].death_frame = ENEMY_DEATH_FRAME_START;
    enemies[slot].death_tick = 0;
    enemies[slot].anim_frame = (uint8_t)(base_frame + ENEMY_DEATH_FRAME_START);
}

static void enemy_update_death(uint8_t slot)
{
    uint8_t base_frame;

    if (!enemies[slot].dying) {
        return;
    }

    enemies[slot].death_tick++;
    if (enemies[slot].death_tick < PLAYER_DEATH_FRAME_STEP_FRAMES) {
        return;
    }

    enemies[slot].death_tick = 0;
    if (enemies[slot].death_frame < 5u) {
        enemies[slot].death_frame++;
        base_frame = enemy_base_frame_for_type(enemies[slot].type);
        enemies[slot].anim_frame = (uint8_t)(base_frame + enemies[slot].death_frame);
    } else {
        enemy_deactivate(slot);
    }
}

static void enemy_deactivate(uint8_t slot)
{
    enemies[slot].active = false;
    enemies[slot].dying = false;
    sprite_mode5_set_enemy(slot, -32, -32, enemies[slot].anim_frame);
}

static void enemy_reset_slot(uint8_t slot)
{
    enemies[slot].active = false;
    enemies[slot].dying = false;
    enemies[slot].type = 0;
    enemies[slot].wave_id = 0;
    enemies[slot].anim_frame = 0;
    enemies[slot].anim_tick = 0;
    enemies[slot].death_frame = ENEMY_DEATH_FRAME_START;
    enemies[slot].death_tick = 0;
    enemies[slot].phase = 0;
    enemies[slot].slot_index = slot;
    enemies[slot].path_variant = wave_variant;
    enemies[slot].waypoint_index = 0;
    enemies[slot].shots_remaining = 0;
    enemies[slot].spiral_step = 0;
    enemies[slot].timer = 0;
    enemies[slot].fire_timer = 0;
    enemies[slot].x_q8 = TO_Q8(-32);
    enemies[slot].y_q8 = TO_Q8(-32);
    enemies[slot].vx_q8 = 0;
    enemies[slot].vy_q8 = 0;
    enemies[slot].target_x = 0;
    enemies[slot].target_y = 0;
    enemies[slot].home_x = 0;
    enemies[slot].home_y = 0;
}

static bool enemy_is_offscreen(uint8_t slot)
{
    int16_t x = FROM_Q8(enemies[slot].x_q8);
    int16_t y = FROM_Q8(enemies[slot].y_q8);

    return (
        x < -64 ||
        x > (SCREEN_WIDTH + 64) ||
        y < -64 ||
        y > (SCREEN_HEIGHT + 64)
    );
}

static bool enemy_try_move_towards(uint8_t slot, int16_t target_x, int16_t target_y, int16_t speed_q8)
{
    int32_t dx = TO_Q8(target_x) - enemies[slot].x_q8;
    int32_t dy = TO_Q8(target_y) - enemies[slot].y_q8;
    int32_t adx = (dx < 0) ? -dx : dx;
    int32_t ady = (dy < 0) ? -dy : dy;
    int32_t scale = (adx > ady) ? adx : ady;

    if (adx <= speed_q8 && ady <= speed_q8) {
        enemies[slot].x_q8 = TO_Q8(target_x);
        enemies[slot].y_q8 = TO_Q8(target_y);
        return true;
    }

    if (scale == 0) {
        return true;
    }

    enemies[slot].x_q8 += (int32_t)((dx * speed_q8) / scale);
    enemies[slot].y_q8 += (int32_t)((dy * speed_q8) / scale);
    return false;
}

static void enemy_compute_aim_velocity(
    int16_t origin_x,
    int16_t origin_y,
    int16_t target_x,
    int16_t target_y,
    int16_t speed_q8,
    int16_t *vx_q8,
    int16_t *vy_q8
)
{
    int32_t dx = (int32_t)target_x - origin_x;
    int32_t dy = (int32_t)target_y - origin_y;
    int32_t adx = (dx < 0) ? -dx : dx;
    int32_t ady = (dy < 0) ? -dy : dy;
    int32_t scale = (adx > ady) ? adx : ady;

    if (scale == 0) {
        *vx_q8 = 0;
        *vy_q8 = speed_q8;
        return;
    }

    *vx_q8 = (int16_t)((dx * speed_q8) / scale);
    *vy_q8 = (int16_t)((dy * speed_q8) / scale);
}

static void enemy_get_bullet_origin(uint8_t slot, int16_t *x, int16_t *y)
{
    int16_t enemy_x = FROM_Q8(enemies[slot].x_q8);
    int16_t enemy_y = FROM_Q8(enemies[slot].y_q8);

    *x = (int16_t)(enemy_x + ((ENEMY_SPRITE_SIZE_PX - PROJECTILE_SPRITE_SIZE_PX) / 2));
    *y = (int16_t)(enemy_y + ((ENEMY_SPRITE_SIZE_PX - PROJECTILE_SPRITE_SIZE_PX) / 2));
}

static int16_t enemy_clamp16(int16_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static bool enemy_q8_move_towards(int32_t *x_q8, int32_t *y_q8, int32_t target_x_q8, int32_t target_y_q8, int16_t speed_q8)
{
    int32_t dx = target_x_q8 - *x_q8;
    int32_t dy = target_y_q8 - *y_q8;
    int32_t adx = (dx < 0) ? -dx : dx;
    int32_t ady = (dy < 0) ? -dy : dy;
    int32_t scale = (adx > ady) ? adx : ady;

    if (adx <= speed_q8 && ady <= speed_q8) {
        *x_q8 = target_x_q8;
        *y_q8 = target_y_q8;
        return true;
    }

    if (scale == 0) {
        return true;
    }

    *x_q8 += (dx * speed_q8) / scale;
    *y_q8 += (dy * speed_q8) / scale;
    return false;
}

static void enemy_setup_game_over_letter(uint8_t index, int16_t start_x, int16_t start_y, int16_t target_x, int16_t target_y)
{
    game_over_letters[index].active = true;
    game_over_letters[index].x_q8 = TO_Q8(start_x);
    game_over_letters[index].y_q8 = TO_Q8(start_y);
    game_over_letters[index].target_x_q8 = TO_Q8(target_x);
    game_over_letters[index].target_y_q8 = TO_Q8(target_y);
    game_over_letters[index].frame = (uint8_t)(GAME_OVER_FIRST_FRAME + index);

    sprite_mode5_set_enemy(index, start_x, start_y, game_over_letters[index].frame);
}

static void enemy_update_game_over_letters(void)
{
    bool all_done = true;

    if (game_over_letter_start_delay > 0) {
        game_over_letter_start_delay--;
        game_over_animation_complete = false;
        return;
    }

    for (uint8_t i = 0; i < GAME_OVER_LETTER_COUNT; ++i) {
        int16_t draw_x;
        int16_t draw_y;

        if (!game_over_letters[i].active) {
            continue;
        }

        if (!enemy_q8_move_towards(
            &game_over_letters[i].x_q8,
            &game_over_letters[i].y_q8,
            game_over_letters[i].target_x_q8,
            game_over_letters[i].target_y_q8,
            GAME_OVER_LETTER_SPEED_Q8
        )) {
            all_done = false;
        }

        draw_x = FROM_Q8(game_over_letters[i].x_q8);
        draw_y = FROM_Q8(game_over_letters[i].y_q8);
        sprite_mode5_set_enemy(i, draw_x, draw_y, game_over_letters[i].frame);
    }

    game_over_animation_complete = all_done;
}

static void enemy_update_player_tracking(void)
{
    int16_t current_x;
    int16_t current_y;
    int16_t measured_vx;
    int16_t measured_vy;

    player_controller_get_center_position(&current_x, &current_y);

    if (!player_tracking_initialized) {
        tracked_player_vx = 0;
        tracked_player_vy = 0;
        tracked_player_x = current_x;
        tracked_player_y = current_y;
        player_tracking_initialized = true;
        return;
    }

    measured_vx = (int16_t)(current_x - tracked_player_x);
    measured_vy = (int16_t)(current_y - tracked_player_y);

    // Smooth per-frame velocity to reduce jittery over/under-leading.
    tracked_player_vx = (int16_t)(((int32_t)tracked_player_vx * 3 + measured_vx) / 4);
    tracked_player_vy = (int16_t)(((int32_t)tracked_player_vy * 3 + measured_vy) / 4);
    tracked_player_x = current_x;
    tracked_player_y = current_y;
}

static int16_t enemy_get_aim_lead_frames(uint8_t slot, int16_t speed_q8)
{
    int16_t bullet_x;
    int16_t bullet_y;
    int16_t dx;
    int16_t dy;
    int16_t distance;
    int16_t speed_px;
    int16_t lead_frames;

    enemy_get_bullet_origin(slot, &bullet_x, &bullet_y);

    dx = (int16_t)(tracked_player_x - bullet_x);
    dy = (int16_t)(tracked_player_y - bullet_y);
    distance = enemy_abs16(dx);
    if (enemy_abs16(dy) > distance) {
        distance = enemy_abs16(dy);
    }

    speed_px = (int16_t)(speed_q8 >> Q8_SHIFT);
    if (speed_px <= 0) {
        speed_px = 1;
    }

    lead_frames = (int16_t)((distance / speed_px) / 2);
    return enemy_clamp16(lead_frames, AIM_LEAD_MIN_FRAMES, AIM_LEAD_MAX_FRAMES);
}

static void enemy_get_predicted_player_center(int16_t *x, int16_t *y, int16_t lead_frames)
{
    int16_t lead_vx = tracked_player_vx;
    int16_t lead_vy = tracked_player_vy;
    int16_t lead_x;
    int16_t lead_y;

    lead_vx = enemy_clamp16(lead_vx, (int16_t)-AIM_MAX_LEAD_PER_AXIS, AIM_MAX_LEAD_PER_AXIS);
    lead_vy = enemy_clamp16(lead_vy, (int16_t)-AIM_MAX_LEAD_PER_AXIS, AIM_MAX_LEAD_PER_AXIS);

    lead_x = (int16_t)(tracked_player_x + (lead_vx * lead_frames));
    lead_y = (int16_t)(tracked_player_y + (lead_vy * lead_frames));

    lead_x = enemy_clamp16(lead_x, 0, (int16_t)(SCREEN_WIDTH - 1));
    lead_y = enemy_clamp16(lead_y, HUD_TOP_PX, (int16_t)(SCREEN_HEIGHT - 1));

    *x = lead_x;
    *y = lead_y;
}

static bool enemy_fire_aimed(uint8_t slot, int16_t speed_q8)
{
        int16_t lead_frames;
    int16_t bullet_x;
    int16_t bullet_y;
    int16_t target_x;
    int16_t target_y;
    int16_t vx_q8;
    int16_t vy_q8;

    enemy_get_bullet_origin(slot, &bullet_x, &bullet_y);
    lead_frames = enemy_get_aim_lead_frames(slot, speed_q8);
    enemy_get_predicted_player_center(&target_x, &target_y, lead_frames);
    enemy_compute_aim_velocity(bullet_x, bullet_y, target_x, target_y, speed_q8, &vx_q8, &vy_q8);
    return projectile_fire_enemy(bullet_x, bullet_y, vx_q8, vy_q8, ENEMY_PROJECTILE_FRAME);
}

static bool enemy_fire_aimed_y_offset(uint8_t slot, int16_t speed_q8, int16_t target_y_offset)
{
        int16_t lead_frames;
    int16_t bullet_x;
    int16_t bullet_y;
    int16_t target_x;
    int16_t target_y;
    int16_t vx_q8;
    int16_t vy_q8;

    enemy_get_bullet_origin(slot, &bullet_x, &bullet_y);
    lead_frames = enemy_get_aim_lead_frames(slot, speed_q8);
    enemy_get_predicted_player_center(&target_x, &target_y, lead_frames);
    enemy_compute_aim_velocity(
        bullet_x,
        bullet_y,
        target_x,
        (int16_t)(target_y + target_y_offset),
        speed_q8,
        &vx_q8,
        &vy_q8
    );

    return projectile_fire_enemy(bullet_x, bullet_y, vx_q8, vy_q8, ENEMY_PROJECTILE_FRAME);
}

static bool enemy_fire_directional(uint8_t slot, int8_t dir_x, int8_t dir_y, int16_t speed_q8)
{
    int16_t bullet_x;
    int16_t bullet_y;
    int16_t vx_q8 = (int16_t)((dir_x * speed_q8) / 8);
    int16_t vy_q8 = (int16_t)((dir_y * speed_q8) / 8);

    enemy_get_bullet_origin(slot, &bullet_x, &bullet_y);
    return projectile_fire_enemy(bullet_x, bullet_y, vx_q8, vy_q8, ENEMY_PROJECTILE_FRAME);
}

static void enemy_fire_barrage(uint8_t slot)
{
    for (uint8_t i = 0; i < 8; ++i) {
        enemy_fire_directional(slot, radial_dirs[i][0], radial_dirs[i][1], BULLET_MEDIUM_SPEED_Q8);
    }
}

static void enemy_fire_big_barrage(uint8_t slot)
{
    for (uint8_t i = 0; i < 16; ++i) {
        enemy_fire_directional(slot, spiral_dirs[i][0], spiral_dirs[i][1], BULLET_MEDIUM_SPEED_Q8);
    }
    for (uint8_t i = 0; i < 8; ++i) {
        enemy_fire_directional(slot, radial_dirs[i][0], radial_dirs[i][1], BULLET_FAST_SPEED_Q8);
    }
}

static bool enemy_fire_player_fan_step(uint8_t slot, int16_t speed_q8)
{
    static const int16_t y_offsets[] = {-72, -48, -24, 0, 24, 48, 72};
    const uint8_t fan_count = (uint8_t)(sizeof(y_offsets) / sizeof(y_offsets[0]));
    uint8_t step = (uint8_t)(enemies[slot].spiral_step % fan_count);
    bool fired = enemy_fire_aimed_y_offset(slot, speed_q8, y_offsets[step]);

    enemies[slot].spiral_step = (uint8_t)((step + 1) % fan_count);
    return fired;
}

static void enemy_sync_sprite(uint8_t slot)
{
    sprite_mode5_set_enemy(
        slot,
        FROM_Q8(enemies[slot].x_q8),
        FROM_Q8(enemies[slot].y_q8),
        enemies[slot].anim_frame
    );
}

static bool enemy_try_hit(uint8_t slot)
{
    int16_t x = FROM_Q8(enemies[slot].x_q8);
    int16_t y = FROM_Q8(enemies[slot].y_q8);

    if (y < HUD_TOP_PX) {
        return false;
    }

    if (enemies[slot].dying) {
        return false;
    }

    if (projectile_hit_test_enemy(x, y, ENEMY_SPRITE_SIZE_PX, ENEMY_SPRITE_SIZE_PX)) {
        score_add_enemy_kill(enemies[slot].type);
        enemy_begin_death(slot);
        return true;
    }

    return false;
}

static int16_t enemy_type0_x_for_y(int16_t y)
{
    int16_t amplitude = 56;
    int16_t min_x = 16;
    int16_t max_x = (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 16);
    int16_t origin = wave_origin_x;
    int16_t phase;
    int16_t period;
    int16_t offset;
    int16_t x;

    if (origin < (int16_t)(min_x + amplitude)) {
        origin = (int16_t)(min_x + amplitude);
    }
    if (origin > (int16_t)(max_x - amplitude)) {
        origin = (int16_t)(max_x - amplitude);
    }

    phase = (int16_t)((y + ENEMY_SPRITE_SIZE_PX) * ENEMY_ZIG_SPEED);
    if (phase < 0) {
        phase = 0;
    }

    period = (int16_t)(amplitude * 4);
    phase = (int16_t)(phase % period);
    if (phase < amplitude) {
        offset = phase;
    } else if (phase < (int16_t)(amplitude * 3)) {
        offset = (int16_t)((amplitude * 2) - phase);
    } else {
        offset = (int16_t)(phase - (amplitude * 4));
    }

    x = (int16_t)(origin + offset);
    if (x < min_x) {
        x = min_x;
    }
    if (x > max_x) {
        x = max_x;
    }
    return x;
}

static void enemy_type2_waypoint(uint8_t variant, uint8_t index, int16_t *x, int16_t *y)
{
    const int16_t safe_left = 8;
    const int16_t safe_right = (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 8);

    static const int16_t left_path[][2] = {
        {-ENEMY_SPRITE_SIZE_PX, -ENEMY_SPRITE_SIZE_PX},
        {8, HUD_TOP_PX},
        {SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 8, HUD_TOP_PX},
        {SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 8, SCREEN_HEIGHT - ENEMY_SPRITE_SIZE_PX},
        {8, SCREEN_HEIGHT - ENEMY_SPRITE_SIZE_PX},
        {-ENEMY_SPRITE_SIZE_PX, -ENEMY_SPRITE_SIZE_PX},
    };
    static const int16_t right_path[][2] = {
        {SCREEN_WIDTH, -ENEMY_SPRITE_SIZE_PX},
        {SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 8, HUD_TOP_PX},
        {8, HUD_TOP_PX},
        {8, SCREEN_HEIGHT - ENEMY_SPRITE_SIZE_PX},
        {SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 8, SCREEN_HEIGHT - ENEMY_SPRITE_SIZE_PX},
        {SCREEN_WIDTH, -ENEMY_SPRITE_SIZE_PX},
    };

    if (variant == 0) {
        *x = left_path[index][0];
        *y = left_path[index][1];
    } else {
        *x = right_path[index][0];
        *y = right_path[index][1];
    }

    if (*x < safe_left && *x >= 0) {
        *x = safe_left;
    }
    if (*x > safe_right && *x <= (SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX)) {
        *x = safe_right;
    }
}

static void enemy_enqueue_type(uint8_t type, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i) {
        if (wave_spawn_count >= MAX_ENEMIES) {
            return;
        }
        wave_spawn_types[wave_spawn_count++] = type;
    }
}

static uint8_t enemy_find_free_slot(void)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) {
            return i;
        }
    }
    return MAX_ENEMIES;
}

static uint8_t enemy_count_active_type(uint8_t enemy_type)
{
    uint8_t count = 0;

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active && enemies[i].type == enemy_type) {
            count++;
        }
    }

    return count;
}

static bool enemy_wave_group_has_active_type5(uint8_t wave_id)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active && enemies[i].wave_id == wave_id && enemies[i].type == 5) {
            return true;
        }
    }

    return false;
}

static bool enemy_type5_group_ready(uint8_t wave_id)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active || enemies[i].wave_id != wave_id || enemies[i].type != 5) {
            continue;
        }

        if (enemies[i].phase != TYPE5_PHASE_FORMED) {
            return false;
        }
    }

    return true;
}

static void update_pattern5_anchor(WaveGroup *group)
{
    int16_t formation_width = (int16_t)(ENEMY_SPRITE_SIZE_PX + ((group->formation_columns - 1) * TYPE5_FORMATION_SPACING_X));
    int16_t left;
    int16_t right;

    group->formation_anchor_x_q8 += group->formation_vx_q8;
    left = FROM_Q8(group->formation_anchor_x_q8);
    right = (int16_t)(left + formation_width);

    if (left <= 8) {
        group->formation_anchor_x_q8 = TO_Q8(8);
        group->formation_vx_q8 = ENEMY_SLOW_SPEED_Q8;
        group->formation_step_down_remaining_q8 += TYPE5_STEP_DOWN_Q8;
    } else if (right >= (SCREEN_WIDTH - 8)) {
        group->formation_anchor_x_q8 = TO_Q8((SCREEN_WIDTH - 8) - formation_width);
        group->formation_vx_q8 = (int16_t)-ENEMY_SLOW_SPEED_Q8;
        group->formation_step_down_remaining_q8 += TYPE5_STEP_DOWN_Q8;
    }

    if (group->formation_step_down_remaining_q8 > 0) {
        int16_t step_q8 = TYPE5_STEP_DOWN_SPEED_Q8;
        if (group->formation_step_down_remaining_q8 < step_q8) {
            step_q8 = (int16_t)group->formation_step_down_remaining_q8;
        }
        group->formation_anchor_y_q8 += step_q8;
        group->formation_step_down_remaining_q8 -= step_q8;
    }
}

static void enemy_update_type5_groups(void)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        WaveGroup *group = &wave_groups[i];

        if (!group->active || group->enemy_type != 5) {
            continue;
        }

        if (group->type5_spawned_count >= group->type5_spawn_count &&
            !enemy_wave_group_has_active_type5(group->wave_id)) {
            group->active = false;
            continue;
        }

        if (!group->formation_started && enemy_type5_group_ready(group->wave_id)) {
            group->formation_started = true;
        }

        if (group->formation_started) {
            update_pattern5_anchor(group);
        }
    }
}

static uint8_t enemy_get_primary_type_for_subwave(uint8_t subwave)
{
    // 13-step mirrored order: 0,1,2,3,4,5,6,5,4,3,2,1,0
    if (subwave <= 6) {
        return subwave;
    }
    return (uint8_t)(12 - subwave);
}

static void enemy_build_subwave_composition(void)
{
    static const uint8_t level2_extra[LEVEL_SUBWAVE_COUNT] = {1, 2, 3, 4, 5, 6, 4};
    static const uint8_t level3_extra[LEVEL_SUBWAVE_COUNT] = {1, 2, 3, 4, 5, 6, 4};
    static const uint8_t level4_extra[LEVEL_SUBWAVE_COUNT] = {1, 2, 3, 4, 5, 6, 4};
    static const uint8_t level5_extra_a[LEVEL_SUBWAVE_COUNT] = {1, 2, 3, 4, 5, 6, 4};
    static const uint8_t level5_extra_b[LEVEL_SUBWAVE_COUNT] = {3, 4, 5, 6, 0, 1, 2};

    wave_spawn_count = 0;
    wave_primary_type = enemy_get_primary_type_for_subwave(current_subwave);
    enemy_enqueue_type(wave_primary_type, ENEMY_WAVE_SIZE);

    if (current_level == 1) {
        // Level 1 is the baseline 5-of-type waves only.
    } else if (current_level == 2) {
        enemy_enqueue_type(level2_extra[current_subwave], 1);
    } else if (current_level == 3) {
        if (current_subwave == 5) {
            enemy_enqueue_type(6, 2);
            enemy_enqueue_type(5, 1);
        } else {
            enemy_enqueue_type(level3_extra[current_subwave], 3);
        }
    } else if (current_level == 4) {
        enemy_enqueue_type(level4_extra[current_subwave], 5);
    } else {
        enemy_enqueue_type(level5_extra_a[current_subwave], 5);
        enemy_enqueue_type(level5_extra_b[current_subwave], 5);
    }

    if (wave_spawn_count == 0) {
        wave_spawn_count = 1;
        wave_spawn_types[0] = wave_primary_type;
    }

    wave_type = wave_primary_type;
    wave_spawned = 0;
}

static void enemy_prepare_wave(void)
{
    enemy_build_subwave_composition();
    active_wave_id = (uint8_t)(active_wave_id + 1u);
    if (active_wave_id == 0) {
        active_wave_id = 1;
    }
    wave_variant = (uint8_t)(rng_next(&rng_state) & 1u);
    int16_t formation_width;
    WaveGroup *group = enemy_wave_group_get(active_wave_id);

    group->active = true;
    group->wave_id = active_wave_id;
    group->enemy_type = wave_type;
    group->type5_spawn_count = 0;
    group->type5_spawned_count = 0;
    group->formation_columns = 0;
    group->type_group_rank = 0;
    group->formation_started = false;
    group->formation_anchor_x_q8 = 0;
    group->formation_anchor_y_q8 = 0;
    group->formation_vx_q8 = 0;
    group->formation_step_down_remaining_q8 = 0;

    type5_spawn_count = 0;
    type5_spawned_count = 0;
    formation_started = false;

    for (uint8_t i = 0; i < wave_spawn_count; ++i) {
        if (wave_spawn_types[i] == 5) {
            type5_spawn_count++;
        }
    }

    if (type5_spawn_count > 0) {
        group->enemy_type = 5;
        group->type5_spawn_count = type5_spawn_count;
        formation_columns = type5_spawn_count;
        if (formation_columns > TYPE5_MAX_COLUMNS) {
            formation_columns = TYPE5_MAX_COLUMNS;
        }
        if (formation_columns == 0) {
            formation_columns = 1;
        }

        group->formation_columns = formation_columns;
        group->type_group_rank = (uint8_t)(enemy_count_active_type(5) / ENEMY_WAVE_SIZE);

        formation_width = (int16_t)(ENEMY_SPRITE_SIZE_PX + ((formation_columns - 1) * TYPE5_FORMATION_SPACING_X));
        group->formation_anchor_x_q8 = TO_Q8((int16_t)((SCREEN_WIDTH - formation_width) / 2));
        group->formation_anchor_y_q8 = TO_Q8((int16_t)(TYPE5_FORMATION_Y + (group->type_group_rank * (TYPE5_FORMATION_SPACING_Y * 3))));
        group->formation_step_down_remaining_q8 = 0;
        if (wave_variant == 0) {
            group->formation_vx_q8 = ENEMY_SLOW_SPEED_Q8;
        } else {
            group->formation_vx_q8 = (int16_t)-ENEMY_SLOW_SPEED_Q8;
        }
    }

    switch (wave_type) {
        case 0:
            wave_origin_x = rng_range(&rng_state, 48, (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 48));
            wave_type0_shots_remaining = (uint8_t)(1u + (rng_next(&rng_state) & 1u));
            break;

        case 5:
            break;

        default:
            break;
    }
}

static uint16_t enemy_get_inter_spawn_frames_for_type(uint8_t enemy_type)
{
    if (enemy_type == 0) {
        return TYPE0_INTER_SPAWN_FRAMES;
    }
    if (enemy_type == 5) {
        return 0;
    }
    return ENEMY_INTER_SPAWN_FRAMES;
}

static void spawn_enemy(uint8_t slot, uint8_t enemy_type, uint8_t wave_slot)
{
    int16_t start_x;
    int16_t start_y;
    uint8_t lane;
    uint8_t rank;

    enemy_reset_slot(slot);
    enemies[slot].active = true;
    enemies[slot].dying = false;
    enemies[slot].type = enemy_type;
    enemies[slot].wave_id = active_wave_id;
    enemies[slot].slot_index = wave_slot;
    enemies[slot].anim_frame = enemy_base_frame_for_type(enemy_type);
    enemies[slot].anim_tick = 0;
    enemies[slot].death_frame = ENEMY_DEATH_FRAME_START;
    enemies[slot].death_tick = 0;

    switch (enemy_type) {
        case 0:
            enemies[slot].phase = TYPE0_PHASE_ZIG;
            enemies[slot].x_q8 = TO_Q8(wave_origin_x);
            enemies[slot].y_q8 = TO_Q8(-ENEMY_SPRITE_SIZE_PX);
            break;

        case 1:
            enemies[slot].phase = TYPE1_PHASE_RISE;
            lane = (uint8_t)(wave_slot % ENEMY_WAVE_SIZE);
            rank = (uint8_t)(wave_slot / ENEMY_WAVE_SIZE);

            switch (lane) {
                case 0:
                    enemies[slot].home_x = 56;
                    enemies[slot].target_y = (int16_t)(TYPE1_MIDPOINT_Y - 24);
                    break;
                case 1:
                    enemies[slot].home_x = (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 56);
                    enemies[slot].target_y = (int16_t)(TYPE1_MIDPOINT_Y - 24);
                    break;
                case 2:
                    enemies[slot].home_x = (int16_t)((SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX) / 2);
                    enemies[slot].target_y = TYPE1_MIDPOINT_Y;
                    break;
                case 3:
                    enemies[slot].home_x = 96;
                    enemies[slot].target_y = (int16_t)(TYPE1_MIDPOINT_Y + 24);
                    break;
                default:
                    enemies[slot].home_x = (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 96);
                    enemies[slot].target_y = (int16_t)(TYPE1_MIDPOINT_Y + 24);
                    break;
            }

            // Additional ranks (for >5 enemies) are offset so they do not stack on top of lane 0..4.
            enemies[slot].home_x = (int16_t)(enemies[slot].home_x + ((int16_t)rank * 8) - 4);
            enemies[slot].target_y = (int16_t)(enemies[slot].target_y + ((int16_t)rank * 12));
            enemies[slot].x_q8 = TO_Q8(enemies[slot].home_x);
            enemies[slot].y_q8 = TO_Q8((int16_t)(SCREEN_HEIGHT + ENEMY_SPRITE_SIZE_PX));
            enemies[slot].shots_remaining = TYPE1_SHOTS_PER_ENEMY;
            enemies[slot].fire_timer = (uint16_t)(4 + (lane * 2) + rank);
            break;

        case 2:
            enemies[slot].path_variant = wave_variant;
            enemy_type2_waypoint(wave_variant, 0, &start_x, &start_y);
            enemies[slot].x_q8 = TO_Q8(start_x);
            enemies[slot].y_q8 = TO_Q8(start_y);
            enemies[slot].waypoint_index = 1;
            enemies[slot].fire_timer = (uint16_t)(20 + (wave_slot * 8));
            break;

        case 3:{
            uint8_t lane = (uint8_t)(wave_slot % ENEMY_WAVE_SIZE);
            uint8_t rank = (uint8_t)(wave_slot / ENEMY_WAVE_SIZE);
            uint8_t group_rank = (uint8_t)(enemy_count_active_type(3) / ENEMY_WAVE_SIZE);
            enemies[slot].phase = TYPE3_PHASE_MOVE;
            enemies[slot].x_q8 = TO_Q8(rng_range(&rng_state, 24, (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 24)));
            enemies[slot].y_q8 = TO_Q8((int16_t)(-ENEMY_SPRITE_SIZE_PX));
            if (wave_variant == 0) {
                static const int16_t target_xs[ENEMY_WAVE_SIZE] = {44, 104, 152, 208, 264};
                static const int16_t target_ys[ENEMY_WAVE_SIZE] = {64, 84, 72, 88, 68};
                enemies[slot].target_x = (int16_t)(target_xs[lane] + rank * 8 + ((group_rank & 1u) ? 10 : 0));
                enemies[slot].target_y = (int16_t)(target_ys[lane] + rank * 12 + (group_rank * 18));
            } else {
                static const int16_t target_xs[ENEMY_WAVE_SIZE] = {60, 116, 168, 220, 276};
                static const int16_t target_ys[ENEMY_WAVE_SIZE] = {88, 70, 92, 74, 86};
                enemies[slot].target_x = (int16_t)(target_xs[lane] + rank * 8 - ((group_rank & 1u) ? 10 : 0));
                enemies[slot].target_y = (int16_t)(target_ys[lane] + rank * 12 + (group_rank * 18));
            }
            enemies[slot].timer = TYPE3_ATTACK_DURATION_FRAMES;
            enemies[slot].fire_timer = (uint16_t)(6 + (wave_slot * 2));
            enemies[slot].spiral_step = (uint8_t)(wave_slot * 2);
            break;
        }
            break;

        case 4:
            enemies[slot].phase = TYPE4_PHASE_DESCEND;
            enemies[slot].x_q8 = TO_Q8(rng_range(&rng_state, 24, (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 24)));
            enemies[slot].y_q8 = TO_Q8((int16_t)(-ENEMY_SPRITE_SIZE_PX));
            enemies[slot].timer = (uint16_t)(36 + (wave_slot * 12));
            break;

        case 5:
        {
            WaveGroup *group = enemy_wave_group_get(active_wave_id);
            lane = (uint8_t)(type5_spawned_count % formation_columns);
            rank = (uint8_t)(type5_spawned_count / formation_columns);
            type5_spawned_count++;
            lane = (uint8_t)(group->type5_spawned_count % group->formation_columns);
            rank = (uint8_t)(group->type5_spawned_count / group->formation_columns);
            group->type5_spawned_count++;

            enemies[slot].phase = TYPE5_PHASE_FORM_IN;
            enemies[slot].home_x = (int16_t)(lane * TYPE5_FORMATION_SPACING_X);
            enemies[slot].home_y = (int16_t)(rank * TYPE5_FORMATION_SPACING_Y);
            enemies[slot].target_x = (int16_t)(FROM_Q8(group->formation_anchor_x_q8) + enemies[slot].home_x);
            enemies[slot].target_y = (int16_t)(FROM_Q8(group->formation_anchor_y_q8) + enemies[slot].home_y);
            enemies[slot].x_q8 = TO_Q8(enemies[slot].target_x);
            enemies[slot].y_q8 = TO_Q8((int16_t)(-ENEMY_SPRITE_SIZE_PX - (rank * 12)));
            enemies[slot].fire_timer = (uint16_t)(30 + (wave_slot * 10));
            break;
        }

        case 6:
            enemies[slot].phase = TYPE6_PHASE_CHASE;
            enemies[slot].x_q8 = TO_Q8(rng_range(&rng_state, 24, (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 24)));
            enemies[slot].y_q8 = TO_Q8((int16_t)(-ENEMY_SPRITE_SIZE_PX));
            break;

        default:
                enemy_deactivate(slot);
            return;
    }

    enemy_sync_sprite(slot);
}

static bool wave_slots_clear(void)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active && enemies[i].wave_id == active_wave_id) {
            return false;
        }
    }
    return true;
}

static void update_pattern0(uint8_t slot)
{
    int16_t player_x;
    int16_t player_y;
    int16_t center_x;

    if (enemies[slot].phase == TYPE0_PHASE_ZIG) {
        enemies[slot].y_q8 += ENEMY_MEDIUM_SPEED_Q8;
        enemies[slot].x_q8 = TO_Q8(enemy_type0_x_for_y(FROM_Q8(enemies[slot].y_q8)));

        center_x = (int16_t)(FROM_Q8(enemies[slot].x_q8) + (ENEMY_SPRITE_SIZE_PX / 2));
        player_controller_get_center_position(&player_x, &player_y);

        if (wave_type0_shots_remaining > 0 &&
            FROM_Q8(enemies[slot].y_q8) >= (HUD_TOP_PX + 20) &&
            FROM_Q8(enemies[slot].y_q8) < (SCREEN_HEIGHT - 48) &&
            (rng_next(&rng_state) & 0x3Fu) == 0u) {
            if (enemy_fire_aimed(slot, BULLET_MEDIUM_SPEED_Q8)) {
                wave_type0_shots_remaining--;
            }
        }

        if (enemy_abs16((int16_t)(center_x - player_x)) <= 8 &&
            FROM_Q8(enemies[slot].y_q8) >= (HUD_TOP_PX + 24)) {
            enemies[slot].phase = TYPE0_PHASE_DIVE;
            enemies[slot].vx_q8 = 0;
            enemies[slot].vy_q8 = ENEMY_DIVE_SPEED_Q8;
        }
    } else {
        enemies[slot].y_q8 += enemies[slot].vy_q8;
    }
}

static void update_pattern1(uint8_t slot)
{
    if (enemies[slot].phase == TYPE1_PHASE_RISE) {
        enemies[slot].y_q8 -= ENEMY_SLOW_SPEED_Q8;
        if (FROM_Q8(enemies[slot].y_q8) <= enemies[slot].target_y) {
            enemies[slot].y_q8 = TO_Q8(enemies[slot].target_y);
            enemies[slot].phase = TYPE1_PHASE_ATTACK;
            enemies[slot].timer = 84;
        }
    } else if (enemies[slot].phase == TYPE1_PHASE_ATTACK) {
        if (enemies[slot].timer > 0) {
            enemies[slot].timer--;
        }
        if (enemies[slot].fire_timer > 0) {
            enemies[slot].fire_timer--;
        }
        if (enemies[slot].shots_remaining > 0 && enemies[slot].fire_timer == 0) {
            static const int16_t aim_pattern_y_offsets[TYPE1_SHOTS_PER_ENEMY] = {0, -8, 0, 8, 0, -4};
            uint8_t shot_index = (uint8_t)(TYPE1_SHOTS_PER_ENEMY - enemies[slot].shots_remaining);
            if (enemy_fire_aimed_y_offset(slot, BULLET_MEDIUM_SPEED_Q8, aim_pattern_y_offsets[shot_index])) {
                enemies[slot].shots_remaining--;
            }
            enemies[slot].fire_timer = TYPE1_FIRE_INTERVAL_FRAMES;
        }
        if (enemies[slot].timer == 0) {
            enemies[slot].phase = TYPE1_PHASE_EXIT;
        }
    } else {
        enemies[slot].y_q8 -= ENEMY_SLOW_SPEED_Q8;
    }
}

static void update_pattern2(uint8_t slot)
{
    int16_t target_x;
    int16_t target_y;

    if (enemies[slot].fire_timer > 0) {
        enemies[slot].fire_timer--;
    }
    if (FROM_Q8(enemies[slot].y_q8) >= HUD_TOP_PX && enemies[slot].fire_timer == 0) {
        enemy_fire_aimed(slot, BULLET_SLOW_SPEED_Q8);
        enemies[slot].fire_timer = 42;
    }

    if (enemies[slot].waypoint_index >= 6) {
        enemy_deactivate(slot);
        return;
    }

    enemy_type2_waypoint(enemies[slot].path_variant, enemies[slot].waypoint_index, &target_x, &target_y);
    if (enemy_try_move_towards(slot, target_x, target_y, ENEMY_MEDIUM_SPEED_Q8)) {
        enemies[slot].waypoint_index++;
        if (enemies[slot].waypoint_index >= 6) {
            enemy_deactivate(slot);
        }
    }
}

static void update_pattern3(uint8_t slot)
{
    if (enemies[slot].phase == TYPE3_PHASE_MOVE) {
        if (enemy_try_move_towards(slot, enemies[slot].target_x, enemies[slot].target_y, ENEMY_MEDIUM_SPEED_Q8)) {
            enemies[slot].phase = TYPE3_PHASE_ATTACK;
        }
    } else if (enemies[slot].phase == TYPE3_PHASE_ATTACK) {
        if (enemies[slot].timer > 0) {
            enemies[slot].timer--;
        }
        if (enemies[slot].fire_timer > 0) {
            enemies[slot].fire_timer--;
        }
        if (enemies[slot].fire_timer == 0) {
            if (enemy_fire_player_fan_step(slot, BULLET_SLOW_SPEED_Q8)) {
                enemies[slot].fire_timer = TYPE3_WAVE_FIRE_INTERVAL;
            } else {
                enemies[slot].fire_timer = 2;
            }
        }
        if (enemies[slot].timer == 0) {
            enemies[slot].phase = TYPE3_PHASE_EXIT;
        }
    } else {
        enemies[slot].y_q8 += ENEMY_SLOW_SPEED_Q8;
    }
}

static void update_pattern4(uint8_t slot)
{
    if (enemies[slot].phase == TYPE4_PHASE_DESCEND) {
        enemies[slot].y_q8 += ENEMY_SLOW_SPEED_Q8;
        if (enemies[slot].timer > 0) {
            enemies[slot].timer--;
        }
        if (enemies[slot].timer == 0) {
            int16_t player_x;
            int16_t player_y;
            int16_t vx_q8;
            int16_t vy_q8;

            // Aim at player top-left for accurate sprite-corner targeting.
            player_controller_get_position(&player_x, &player_y);
            enemy_compute_aim_velocity(
                FROM_Q8(enemies[slot].x_q8),
                FROM_Q8(enemies[slot].y_q8),
                player_x,
                player_y,
                ENEMY_DIVE_SPEED_Q8,
                &vx_q8,
                &vy_q8
            );
            enemies[slot].vx_q8 = vx_q8;
            enemies[slot].vy_q8 = vy_q8;
            enemies[slot].phase = TYPE4_PHASE_DIVE;
        }
    } else {
        enemies[slot].x_q8 += enemies[slot].vx_q8;
        enemies[slot].y_q8 += enemies[slot].vy_q8;
    }
}

static void update_pattern5(uint8_t slot)
{
    WaveGroup *group = enemy_wave_group_get(enemies[slot].wave_id);

    if (enemies[slot].phase == TYPE5_PHASE_FORM_IN) {
        if (enemy_try_move_towards(slot, enemies[slot].target_x, enemies[slot].target_y, ENEMY_MEDIUM_SPEED_Q8)) {
            enemies[slot].phase = TYPE5_PHASE_FORMED;
        }
    } else if (group->active && group->formation_started) {
        enemies[slot].x_q8 = group->formation_anchor_x_q8 + TO_Q8(enemies[slot].home_x);
        enemies[slot].y_q8 = group->formation_anchor_y_q8 + TO_Q8(enemies[slot].home_y);
    }

    if (FROM_Q8(enemies[slot].y_q8) >= SCREEN_HEIGHT) {
        enemy_deactivate(slot);
        return;
    }

    if (enemies[slot].fire_timer > 0) {
        enemies[slot].fire_timer--;
    }
    if (group->active && group->formation_started && FROM_Q8(enemies[slot].y_q8) >= HUD_TOP_PX && enemies[slot].fire_timer == 0) {
        enemy_fire_aimed(slot, BULLET_MEDIUM_SPEED_Q8);
        enemies[slot].fire_timer = (uint16_t)(52 + (enemies[slot].slot_index * 6));
    }
}

static void update_pattern6(uint8_t slot)
{
    int16_t player_x;
    int16_t player_y;
    int16_t player_top_y;
    int16_t vx_q8;
    int16_t vy_q8;
    int16_t enemy_x;
    int16_t enemy_y;

    player_controller_get_center_position(&player_x, &player_y);
    player_controller_get_position(NULL, &player_top_y);
    enemy_x = FROM_Q8(enemies[slot].x_q8);
    enemy_y = FROM_Q8(enemies[slot].y_q8);

    enemy_compute_aim_velocity(enemy_x, enemy_y, player_x, player_y, ENEMY_FAST_SPEED_Q8, &vx_q8, &vy_q8);
    enemies[slot].x_q8 += vx_q8;
    enemies[slot].y_q8 += vy_q8;

    enemy_x = FROM_Q8(enemies[slot].x_q8);
    enemy_y = FROM_Q8(enemies[slot].y_q8);

    if ((enemy_abs16((int16_t)(enemy_x - player_x)) <= TYPE6_DETONATE_DIST_X &&
         enemy_abs16((int16_t)(enemy_y - player_y)) <= TYPE6_DETONATE_DIST_Y) ||
        enemy_y >= player_top_y) {
        enemy_fire_big_barrage(slot);
        enemy_deactivate(slot);
    }
}

static void update_enemy_pattern(uint8_t slot)
{
    if (enemies[slot].dying) {
        enemy_update_death(slot);
        return;
    }

    switch (enemies[slot].type) {
        case 0:
            update_pattern0(slot);
            break;
        case 1:
            update_pattern1(slot);
            break;
        case 2:
            update_pattern2(slot);
            break;
        case 3:
            update_pattern3(slot);
            break;
        case 4:
            update_pattern4(slot);
            break;
        case 5:
            update_pattern5(slot);
            break;
        case 6:
            update_pattern6(slot);
            break;
        default:
            enemy_deactivate(slot);
            break;
    }
}

void enemy_init(void)
{
    rng_state = 0x4A3Bu;
    wave_state = WAVE_STATE_DELAY;
    wave_type = 0;
    wave_primary_type = 0;
    active_wave_id = 0;
    wave_spawned = 0;
    wave_spawn_count = ENEMY_WAVE_SIZE;
    wave_timer = ENEMY_SPAWN_DELAY_FRAMES;
    wave_clear_timeout_timer = 0;
    wave_origin_x = (SCREEN_WIDTH / 2);
    wave_variant = 0;
    wave_type0_shots_remaining = 0;
    current_level = 1;
    current_subwave = 0;
    level_complete = false;
    formation_anchor_x_q8 = 0;
    formation_anchor_y_q8 = 0;
    formation_vx_q8 = 0;
    formation_columns = TYPE5_MAX_COLUMNS;
    type5_spawn_count = 0;
    type5_spawned_count = 0;
    formation_started = false;
    tracked_player_x = 0;
    tracked_player_y = 0;
    tracked_player_vx = 0;
    tracked_player_vy = 0;
    player_tracking_initialized = false;
    game_over_mode = false;
    game_over_animation_complete = false;
    game_over_letter_start_delay = 0;
    bonus_icon_anim_active = false;
    bonus_icon_anim_complete = false;
    bonus_icon_anim_type = 0;
    bonus_icon_anim_x_q8 = TO_Q8(BONUS_ICON_FLY_IN_START_X);
    bonus_icon_anim_y_q8 = TO_Q8(-32);
    bonus_icon_anim_target_x_q8 = TO_Q8(0);
    bonus_icon_anim_target_y_q8 = TO_Q8(0);

    for (uint8_t i = 0; i < GAME_OVER_LETTER_COUNT; ++i) {
        game_over_letters[i].active = false;
        game_over_letters[i].x_q8 = TO_Q8(-32);
        game_over_letters[i].y_q8 = TO_Q8(-32);
        game_over_letters[i].target_x_q8 = TO_Q8(-32);
        game_over_letters[i].target_y_q8 = TO_Q8(-32);
        game_over_letters[i].frame = (uint8_t)(GAME_OVER_FIRST_FRAME + i);
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemy_reset_slot(i);
        sprite_mode5_set_enemy(i, -32, -32, 0);
        wave_groups[i].active = false;
        wave_groups[i].wave_id = 0;
        wave_groups[i].enemy_type = 0;
        wave_groups[i].type5_spawn_count = 0;
        wave_groups[i].type5_spawned_count = 0;
        wave_groups[i].formation_columns = 0;
        wave_groups[i].type_group_rank = 0;
        wave_groups[i].formation_started = false;
        wave_groups[i].formation_anchor_x_q8 = 0;
        wave_groups[i].formation_anchor_y_q8 = 0;
        wave_groups[i].formation_vx_q8 = 0;
        wave_groups[i].formation_step_down_remaining_q8 = 0;
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        wave_spawn_types[i] = 0;
    }
}

void enemy_start_level(uint8_t level_index)
{
    if (level_index == 0) {
        level_index = 1;
    }

    current_level = level_index;
    current_subwave = 0;
    level_complete = false;
    wave_state = WAVE_STATE_DELAY;
    wave_spawned = 0;
    wave_spawn_count = 0;
    active_wave_id = 0;
    wave_timer = ENEMY_SPAWN_DELAY_FRAMES;
    wave_clear_timeout_timer = 0;
    type5_spawn_count = 0;
    type5_spawned_count = 0;
    formation_started = false;

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        enemy_deactivate(i);
        wave_groups[i].active = false;
    }

    bonus_icon_anim_active = false;
    bonus_icon_anim_complete = false;
}

uint8_t enemy_get_level(void)
{
    return current_level;
}

uint8_t enemy_get_subwave(void)
{
    return current_subwave;
}

bool enemy_is_level_complete(void)
{
    return level_complete;
}

bool enemy_has_active(void)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) {
            return true;
        }
    }

    return false;
}

void enemy_update(void)
{
    if (game_over_mode) {
        enemy_update_game_over_letters();
        return;
    }

    enemy_update_player_tracking();
    enemy_update_type5_groups();

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            continue;
        }

        update_enemy_pattern(i);

        if (!enemies[i].active) {
            continue;
        }

        if (enemies[i].dying) {
            enemy_sync_sprite(i);
            continue;
        }

        enemy_update_active_animation(i);

        if (enemy_try_hit(i)) {
            continue;
        }

        if (!enemies[i].active) {
            continue;
        }

        if (enemy_is_offscreen(i)) {
            enemy_deactivate(i);
            continue;
        }

        enemy_sync_sprite(i);
    }

    switch (wave_state) {
        case WAVE_STATE_DELAY:
            if (level_complete) {
                break;
            }
            if (wave_timer > 0) {
                wave_timer--;
            } else {
                enemy_prepare_wave();
                wave_state = WAVE_STATE_SPAWNING;
                wave_timer = 0;
            }
            break;

        case WAVE_STATE_SPAWNING:
            if (wave_timer > 0) {
                wave_timer--;
            } else {
                uint8_t free_slot = enemy_find_free_slot();

                if (wave_spawned < wave_spawn_count && wave_spawn_types[wave_spawned] == 5) {
                    bool spawned_any = false;

                    while (wave_spawned < wave_spawn_count && wave_spawn_types[wave_spawned] == 5) {
                        free_slot = enemy_find_free_slot();
                        if (free_slot >= MAX_ENEMIES) {
                            break;
                        }
                        spawn_enemy(free_slot, wave_spawn_types[wave_spawned], wave_spawned);
                        wave_spawned++;
                        spawned_any = true;
                    }

                    if (wave_spawned >= wave_spawn_count) {
                        wave_clear_timeout_timer = WAVE_CLEAR_TIMEOUT_FRAMES;
                        wave_state = WAVE_STATE_CLEARING;
                    } else {
                        wave_timer = spawned_any
                            ? enemy_get_inter_spawn_frames_for_type(wave_spawn_types[wave_spawned])
                            : 1;
                    }
                } else {
                    if (free_slot < MAX_ENEMIES) {
                        spawn_enemy(free_slot, wave_spawn_types[wave_spawned], wave_spawned);
                        wave_spawned++;
                    }

                    if (wave_spawned >= wave_spawn_count) {
                        wave_clear_timeout_timer = WAVE_CLEAR_TIMEOUT_FRAMES;
                        wave_state = WAVE_STATE_CLEARING;
                    } else {
                        wave_timer = (free_slot < MAX_ENEMIES)
                            ? enemy_get_inter_spawn_frames_for_type(wave_spawn_types[wave_spawned])
                            : 1;
                    }
                }
            }
            break;

        case WAVE_STATE_CLEARING:
            if (wave_clear_timeout_timer > 0) {
                wave_clear_timeout_timer--;
            }

            if (wave_slots_clear() || wave_clear_timeout_timer == 0) {
                current_subwave++;

                if (current_subwave >= LEVEL_SUBWAVE_COUNT) {
                    level_complete = true;
                    wave_state = WAVE_STATE_DELAY;
                    wave_timer = 0;
                } else {
                    wave_spawned = 0;
                    if (current_subwave > 6 && current_subwave < 11) {
                        projectile_spawn_asteroid_wave((uint8_t)(1u + (rng_next(&rng_state) % 3u)));
                    }
                    wave_timer = 0;
                    wave_state = WAVE_STATE_DELAY;
                }
            }
            break;
    }
}

bool enemy_hit_test_player(int16_t x, int16_t y, int16_t width, int16_t height)
{
    int16_t player_right = (int16_t)(x + width);
    int16_t player_bottom = (int16_t)(y + height);

    if (game_over_mode) {
        return false;
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        int16_t enemy_left;
        int16_t enemy_top;
        int16_t enemy_right;
        int16_t enemy_bottom;

        if (!enemies[i].active) {
            continue;
        }

        enemy_left = FROM_Q8(enemies[i].x_q8);
        enemy_top = FROM_Q8(enemies[i].y_q8);
        enemy_right = (int16_t)(enemy_left + ENEMY_SPRITE_SIZE_PX);
        enemy_bottom = (int16_t)(enemy_top + ENEMY_SPRITE_SIZE_PX);

        if (enemy_right <= x || enemy_left >= player_right ||
            enemy_bottom <= y || enemy_top >= player_bottom) {
            continue;
        }

        if (enemies[i].dying) {
            continue;
        }

        enemy_begin_death(i);
        return true;
    }

    return false;
}

void enemy_show_bonus_icons(void)
{
    for (uint8_t i = 0; i < ENEMY_TYPE_COUNT; ++i) {
        sprite_mode5_set_enemy(i, tile_mode2_get_bonus_icon_target_x(), tile_mode2_get_bonus_icon_target_y(i), enemy_base_frame_for_type(i));
    }
}

void enemy_clear_all(void)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        enemy_deactivate(i);
    }

    bonus_icon_anim_active = false;
    bonus_icon_anim_complete = false;
}

void enemy_prepare_bonus_icons(void)
{
    enemy_hide_bonus_icons();
    bonus_icon_anim_active = false;
    bonus_icon_anim_complete = false;
}

void enemy_start_bonus_icon_fly_in(uint8_t enemy_type)
{
    int16_t target_x;
    int16_t target_y;

    if (enemy_type >= ENEMY_TYPE_COUNT) {
        return;
    }

    target_x = tile_mode2_get_bonus_icon_target_x();
    target_y = tile_mode2_get_bonus_icon_target_y(enemy_type);

    bonus_icon_anim_active = true;
    bonus_icon_anim_complete = false;
    bonus_icon_anim_type = enemy_type;
    bonus_icon_anim_x_q8 = TO_Q8(BONUS_ICON_FLY_IN_START_X);
    bonus_icon_anim_y_q8 = TO_Q8(target_y);
    bonus_icon_anim_target_x_q8 = TO_Q8(target_x);
    bonus_icon_anim_target_y_q8 = TO_Q8(target_y);

    sprite_mode5_set_enemy(enemy_type, BONUS_ICON_FLY_IN_START_X, target_y, enemy_base_frame_for_type(enemy_type));
}

bool enemy_update_bonus_icon_fly_in(void)
{
    int16_t draw_x;
    int16_t draw_y;

    if (!bonus_icon_anim_active) {
        return bonus_icon_anim_complete;
    }

    if (enemy_q8_move_towards(
        &bonus_icon_anim_x_q8,
        &bonus_icon_anim_y_q8,
        bonus_icon_anim_target_x_q8,
        bonus_icon_anim_target_y_q8,
        BONUS_ICON_FLY_IN_SPEED_Q8
    )) {
        bonus_icon_anim_active = false;
        bonus_icon_anim_complete = true;
    }

    draw_x = FROM_Q8(bonus_icon_anim_x_q8);
    draw_y = FROM_Q8(bonus_icon_anim_y_q8);
    sprite_mode5_set_enemy(bonus_icon_anim_type, draw_x, draw_y, enemy_base_frame_for_type(bonus_icon_anim_type));

    return bonus_icon_anim_complete;
}

void enemy_hide_bonus_icons(void)
{
    for (uint8_t i = 0; i < ENEMY_TYPE_COUNT; ++i) {
        sprite_mode5_set_enemy(i, -32, -32, enemy_base_frame_for_type(i));
    }
}

void enemy_start_game_over_animation(void)
{
    const int16_t target_start_x = (int16_t)((SCREEN_WIDTH - (GAME_OVER_LETTER_COUNT * ENEMY_SPRITE_SIZE_PX)) / 2);
    const int16_t target_y = (int16_t)(((SCREEN_HEIGHT / 2) - (ENEMY_SPRITE_SIZE_PX / 2)) + GAME_OVER_VERTICAL_OFFSET_PX);

    game_over_mode = true;
    game_over_animation_complete = false;
    game_over_letter_start_delay = GAME_OVER_LETTER_START_DELAY_FRAMES;

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        enemy_deactivate(i);
    }

    for (uint8_t i = 0; i < GAME_OVER_LETTER_COUNT; ++i) {
        int16_t start_x;
        int16_t start_y;
        int16_t target_x = (int16_t)(target_start_x + (i * ENEMY_SPRITE_SIZE_PX));

        switch (i & 0x03u) {
            case 0:
                start_x = (int16_t)(-ENEMY_SPRITE_SIZE_PX - (i * 6));
                start_y = (int16_t)(40 + (i * 18));
                break;
            case 1:
                start_x = (int16_t)(SCREEN_WIDTH + ENEMY_SPRITE_SIZE_PX + (i * 6));
                start_y = (int16_t)(30 + (i * 16));
                break;
            case 2:
                start_x = (int16_t)(32 + (i * 28));
                start_y = (int16_t)(-ENEMY_SPRITE_SIZE_PX - (i * 5));
                break;
            default:
                start_x = (int16_t)(SCREEN_WIDTH - 40 - (i * 22));
                start_y = (int16_t)(SCREEN_HEIGHT + ENEMY_SPRITE_SIZE_PX + (i * 5));
                break;
        }

        enemy_setup_game_over_letter(i, start_x, start_y, target_x, target_y);
    }
}

void enemy_stop_game_over_animation(void)
{
    game_over_mode = false;
    game_over_animation_complete = false;
    game_over_letter_start_delay = 0;

    for (uint8_t i = 0; i < GAME_OVER_LETTER_COUNT; ++i) {
        game_over_letters[i].active = false;
        sprite_mode5_set_enemy(i, -32, -32, (uint8_t)(GAME_OVER_FIRST_FRAME + i));
    }
}

bool enemy_is_game_over_animation_complete(void)
{
    return game_over_animation_complete;
}

void enemy_spawn_for_boss(uint8_t enemy_type, uint8_t wave_slot)
{
    if (enemy_type == 5u) {
        WaveGroup *group;
        uint8_t cols = ENEMY_WAVE_SIZE;
        int16_t formation_width;

        if (cols > TYPE5_MAX_COLUMNS) {
            cols = TYPE5_MAX_COLUMNS;
        }
        if (cols == 0u) {
            cols = 1u;
        }

        if (wave_slot == 0u) {
            active_wave_id = (uint8_t)(active_wave_id + 1u);
            if (active_wave_id == 0u) {
                active_wave_id = 1u;
            }

            wave_variant = (uint8_t)(rng_next(&rng_state) & 1u);
            group = enemy_wave_group_get(active_wave_id);

            group->active = true;
            group->wave_id = active_wave_id;
            group->enemy_type = 5u;
            group->type5_spawn_count = ENEMY_WAVE_SIZE;
            group->type5_spawned_count = 0;
            group->formation_columns = cols;
            group->type_group_rank = (uint8_t)(enemy_count_active_type(5u) / ENEMY_WAVE_SIZE);
            group->formation_started = false;
            formation_width = (int16_t)(ENEMY_SPRITE_SIZE_PX + ((cols - 1u) * TYPE5_FORMATION_SPACING_X));
            group->formation_anchor_x_q8 = TO_Q8((int16_t)((SCREEN_WIDTH - formation_width) / 2));
            group->formation_anchor_y_q8 = TO_Q8((int16_t)(TYPE5_FORMATION_Y + (group->type_group_rank * (TYPE5_FORMATION_SPACING_Y * 3))));
            group->formation_step_down_remaining_q8 = 0;
            if (wave_variant == 0u) {
                group->formation_vx_q8 = ENEMY_SLOW_SPEED_Q8;
            } else {
                group->formation_vx_q8 = (int16_t)-ENEMY_SLOW_SPEED_Q8;
            }

            formation_columns = cols;
            type5_spawned_count = 0;
            formation_started = false;
        } else {
            group = enemy_wave_group_get(active_wave_id);
            if (!group->active || group->enemy_type != 5u || group->formation_columns == 0u) {
                return;
            }
            formation_columns = group->formation_columns;
        }
    }

    uint8_t free_slot = enemy_find_free_slot();
    if (free_slot < MAX_ENEMIES) {
        spawn_enemy(free_slot, enemy_type, wave_slot);
    }
}
