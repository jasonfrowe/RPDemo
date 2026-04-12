#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "constants.h"
#include "enemy.h"
#include "player_controller.h"
#include "projectile.h"
#include "score.h"
#include "sprite_mode5.h"

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
#define TYPE1_FIRE_INTERVAL_FRAMES 3
#define TYPE0_INTER_SPAWN_FRAMES 10
#define TYPE3_MIN_TARGET_Y (HUD_TOP_PX + 40)
#define TYPE3_MAX_TARGET_Y 120
#define TYPE3_ATTACK_DURATION_FRAMES 480
#define TYPE3_WAVE_FIRE_INTERVAL 8
#define TYPE6_DETONATE_DIST_X 40
#define TYPE6_DETONATE_DIST_Y 40

#define TYPE5_FORMATION_SPACING_X 28
#define TYPE5_FORMATION_Y        (HUD_TOP_PX + 24)
#define TYPE5_STEP_DOWN_Q8       TO_Q8(12)
#define TYPE5_STEP_DOWN_SPEED_Q8 ENEMY_SLOW_SPEED_Q8

typedef struct {
    bool    active;
    uint8_t type;
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

typedef enum {
    WAVE_STATE_DELAY,
    WAVE_STATE_SPAWNING,
    WAVE_STATE_CLEARING,
} WaveState;

static WaveState wave_state;
static uint8_t wave_type;
static uint8_t wave_spawned;
static uint16_t wave_timer;

static uint16_t rng_state;
static int16_t wave_origin_x;
static uint8_t wave_variant;
static uint8_t wave_type0_shots_remaining;
static int32_t formation_anchor_x_q8;
static int32_t formation_anchor_y_q8;
static int16_t formation_vx_q8;
static int32_t formation_step_down_remaining_q8;

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

static uint16_t enemy_rand(void)
{
    rng_state = (uint16_t)(rng_state * 25173u + 13849u);
    return rng_state;
}

static int16_t enemy_rand_range(int16_t min_value, int16_t max_value)
{
    uint16_t span = (uint16_t)(max_value - min_value + 1);
    return (int16_t)(min_value + (enemy_rand() % span));
}

static int16_t enemy_abs16(int16_t value)
{
    return (value < 0) ? (int16_t)(-value) : value;
}

static void enemy_deactivate(uint8_t slot)
{
    enemies[slot].active = false;
    sprite_mode5_set_enemy(slot, -32, -32, enemies[slot].type);
}

static void enemy_reset_slot(uint8_t slot)
{
    enemies[slot].active = false;
    enemies[slot].type = wave_type;
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

static bool enemy_fire_aimed(uint8_t slot, int16_t speed_q8)
{
    int16_t bullet_x;
    int16_t bullet_y;
    int16_t player_x;
    int16_t player_y;
    int16_t vx_q8;
    int16_t vy_q8;

    enemy_get_bullet_origin(slot, &bullet_x, &bullet_y);
    player_controller_get_center_position(&player_x, &player_y);
    enemy_compute_aim_velocity(bullet_x, bullet_y, player_x, player_y, speed_q8, &vx_q8, &vy_q8);
    return projectile_fire_enemy(bullet_x, bullet_y, vx_q8, vy_q8, ENEMY_PROJECTILE_FRAME);
}

static bool enemy_fire_aimed_y_offset(uint8_t slot, int16_t speed_q8, int16_t target_y_offset)
{
    int16_t bullet_x;
    int16_t bullet_y;
    int16_t player_x;
    int16_t player_y;
    int16_t vx_q8;
    int16_t vy_q8;

    enemy_get_bullet_origin(slot, &bullet_x, &bullet_y);
    player_controller_get_center_position(&player_x, &player_y);
    enemy_compute_aim_velocity(
        bullet_x,
        bullet_y,
        player_x,
        (int16_t)(player_y + target_y_offset),
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
        enemies[slot].type
    );
}

static bool enemy_try_hit(uint8_t slot)
{
    int16_t x = FROM_Q8(enemies[slot].x_q8);
    int16_t y = FROM_Q8(enemies[slot].y_q8);

    if (y < HUD_TOP_PX) {
        return false;
    }

    if (projectile_hit_test_enemy(x, y, ENEMY_SPRITE_SIZE_PX, ENEMY_SPRITE_SIZE_PX)) {
        score_add_enemy_kill(enemies[slot].type);
        enemy_deactivate(slot);
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

static void enemy_prepare_wave(void)
{
    wave_variant = (uint8_t)(enemy_rand() & 1u);

    switch (wave_type) {
        case 0:
            wave_origin_x = enemy_rand_range(48, (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 48));
            wave_type0_shots_remaining = (uint8_t)(1u + (enemy_rand() & 1u));
            break;

        case 5: {
            int16_t formation_width = (int16_t)(ENEMY_SPRITE_SIZE_PX + ((ENEMY_WAVE_SIZE - 1) * TYPE5_FORMATION_SPACING_X));
            formation_anchor_y_q8 = TO_Q8(TYPE5_FORMATION_Y);
            formation_step_down_remaining_q8 = 0;
            if (wave_variant == 0) {
                formation_anchor_x_q8 = TO_Q8(-formation_width);
                formation_vx_q8 = ENEMY_SLOW_SPEED_Q8;
            } else {
                formation_anchor_x_q8 = TO_Q8(SCREEN_WIDTH);
                formation_vx_q8 = (int16_t)-ENEMY_SLOW_SPEED_Q8;
            }
            break;
        }

        default:
            break;
    }
}

static uint16_t enemy_get_inter_spawn_frames_for_wave(void)
{
    if (wave_type == 0) {
        return TYPE0_INTER_SPAWN_FRAMES;
    }
    return ENEMY_INTER_SPAWN_FRAMES;
}

static void spawn_enemy(uint8_t slot)
{
    int16_t start_x;
    int16_t start_y;

    enemy_reset_slot(slot);
    enemies[slot].active = true;

    switch (wave_type) {
        case 0:
            enemies[slot].phase = TYPE0_PHASE_ZIG;
            enemies[slot].x_q8 = TO_Q8(wave_origin_x);
            enemies[slot].y_q8 = TO_Q8(-ENEMY_SPRITE_SIZE_PX);
            break;

        case 1:
            enemies[slot].phase = TYPE1_PHASE_RISE;
            switch (slot) {
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
            enemies[slot].x_q8 = TO_Q8(enemies[slot].home_x);
            enemies[slot].y_q8 = TO_Q8((int16_t)(SCREEN_HEIGHT + ENEMY_SPRITE_SIZE_PX));
            enemies[slot].shots_remaining = TYPE1_SHOTS_PER_ENEMY;
            enemies[slot].fire_timer = (uint16_t)(4 + slot);
            break;

        case 2:
            enemies[slot].path_variant = wave_variant;
            enemy_type2_waypoint(wave_variant, 0, &start_x, &start_y);
            enemies[slot].x_q8 = TO_Q8(start_x);
            enemies[slot].y_q8 = TO_Q8(start_y);
            enemies[slot].waypoint_index = 1;
            enemies[slot].fire_timer = (uint16_t)(20 + (slot * 8));
            break;

        case 3:
            enemies[slot].phase = TYPE3_PHASE_MOVE;
            enemies[slot].x_q8 = TO_Q8(enemy_rand_range(24, (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 24)));
            enemies[slot].y_q8 = TO_Q8((int16_t)(-ENEMY_SPRITE_SIZE_PX));
            if (wave_variant == 0) {
                static const int16_t target_xs[ENEMY_WAVE_SIZE] = {44, 104, 152, 208, 264};
                static const int16_t target_ys[ENEMY_WAVE_SIZE] = {64, 84, 72, 88, 68};
                enemies[slot].target_x = target_xs[slot];
                enemies[slot].target_y = target_ys[slot];
            } else {
                static const int16_t target_xs[ENEMY_WAVE_SIZE] = {60, 116, 168, 220, 276};
                static const int16_t target_ys[ENEMY_WAVE_SIZE] = {88, 70, 92, 74, 86};
                enemies[slot].target_x = target_xs[slot];
                enemies[slot].target_y = target_ys[slot];
            }
            enemies[slot].timer = TYPE3_ATTACK_DURATION_FRAMES;
            enemies[slot].fire_timer = (uint16_t)(6 + (slot * 2));
            enemies[slot].spiral_step = (uint8_t)(slot * 2);
            break;

        case 4:
            enemies[slot].phase = TYPE4_PHASE_DESCEND;
            enemies[slot].x_q8 = TO_Q8(enemy_rand_range(24, (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 24)));
            enemies[slot].y_q8 = TO_Q8((int16_t)(-ENEMY_SPRITE_SIZE_PX));
            enemies[slot].timer = (uint16_t)(36 + (slot * 12));
            break;

        case 5:
            enemies[slot].home_x = (int16_t)(slot * TYPE5_FORMATION_SPACING_X);
            enemies[slot].home_y = 0;
            enemies[slot].x_q8 = formation_anchor_x_q8 + TO_Q8(enemies[slot].home_x);
            enemies[slot].y_q8 = formation_anchor_y_q8;
            enemies[slot].fire_timer = (uint16_t)(30 + (slot * 10));
            break;

        case 6:
            enemies[slot].phase = TYPE6_PHASE_CHASE;
            enemies[slot].x_q8 = TO_Q8(enemy_rand_range(24, (int16_t)(SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 24)));
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
    for (uint8_t i = 0; i < ENEMY_WAVE_SIZE; i++) {
        if (enemies[i].active) {
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
            (enemy_rand() & 0x3Fu) == 0u) {
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
            enemies[slot].timer = 54;
        }
    } else if (enemies[slot].phase == TYPE1_PHASE_ATTACK) {
        if (enemies[slot].timer > 0) {
            enemies[slot].timer--;
        }
        if (enemies[slot].fire_timer > 0) {
            enemies[slot].fire_timer--;
        }
        if (enemies[slot].shots_remaining > 0 && enemies[slot].fire_timer == 0) {
            static const int16_t aim_pattern_y_offsets[TYPE1_SHOTS_PER_ENEMY] = {0, -12, 0, 12, 0, -6};
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
            if (enemy_fire_player_fan_step(slot, BULLET_MEDIUM_SPEED_Q8)) {
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

static void update_pattern5_anchor(void)
{
    int16_t formation_width = (int16_t)(ENEMY_SPRITE_SIZE_PX + ((ENEMY_WAVE_SIZE - 1) * TYPE5_FORMATION_SPACING_X));
    int16_t left;
    int16_t right;

    formation_anchor_x_q8 += formation_vx_q8;
    left = FROM_Q8(formation_anchor_x_q8);
    right = (int16_t)(left + formation_width);

    if (left <= 8) {
        formation_anchor_x_q8 = TO_Q8(8);
        formation_vx_q8 = ENEMY_SLOW_SPEED_Q8;
        formation_step_down_remaining_q8 += TYPE5_STEP_DOWN_Q8;
    } else if (right >= (SCREEN_WIDTH - 8)) {
        formation_anchor_x_q8 = TO_Q8((SCREEN_WIDTH - 8) - formation_width);
        formation_vx_q8 = (int16_t)-ENEMY_SLOW_SPEED_Q8;
        formation_step_down_remaining_q8 += TYPE5_STEP_DOWN_Q8;
    }

    if (formation_step_down_remaining_q8 > 0) {
        int16_t step_q8 = TYPE5_STEP_DOWN_SPEED_Q8;
        if (formation_step_down_remaining_q8 < step_q8) {
            step_q8 = (int16_t)formation_step_down_remaining_q8;
        }
        formation_anchor_y_q8 += step_q8;
        formation_step_down_remaining_q8 -= step_q8;
    }
}

static void update_pattern5(uint8_t slot)
{
    enemies[slot].x_q8 = formation_anchor_x_q8 + TO_Q8(enemies[slot].home_x);
    enemies[slot].y_q8 = formation_anchor_y_q8 + TO_Q8(enemies[slot].home_y);

    if (FROM_Q8(enemies[slot].y_q8) >= SCREEN_HEIGHT) {
        enemy_deactivate(slot);
        return;
    }

    if (enemies[slot].fire_timer > 0) {
        enemies[slot].fire_timer--;
    }
    if (FROM_Q8(enemies[slot].y_q8) >= HUD_TOP_PX && enemies[slot].fire_timer == 0) {
        enemy_fire_aimed(slot, BULLET_MEDIUM_SPEED_Q8);
        enemies[slot].fire_timer = (uint16_t)(52 + (slot * 6));
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

    enemy_compute_aim_velocity(enemy_x, enemy_y, player_x, player_y, ENEMY_MEDIUM_SPEED_Q8, &vx_q8, &vy_q8);
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
    wave_spawned = 0;
    wave_timer = ENEMY_SPAWN_DELAY_FRAMES;
    wave_origin_x = (SCREEN_WIDTH / 2);
    wave_variant = 0;
    wave_type0_shots_remaining = 0;
    formation_anchor_x_q8 = 0;
    formation_anchor_y_q8 = 0;
    formation_vx_q8 = 0;

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemy_reset_slot(i);
        sprite_mode5_set_enemy(i, -32, -32, 0);
    }
}

void enemy_update(void)
{
    if (wave_type == 5 && !wave_slots_clear()) {
        update_pattern5_anchor();
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            continue;
        }

        update_enemy_pattern(i);

        if (!enemies[i].active) {
            continue;
        }

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
                spawn_enemy(wave_spawned);
                wave_spawned++;

                if (wave_spawned >= ENEMY_WAVE_SIZE) {
                    wave_state = WAVE_STATE_CLEARING;
                } else {
                    wave_timer = enemy_get_inter_spawn_frames_for_wave();
                }
            }
            break;

        case WAVE_STATE_CLEARING:
            if (wave_slots_clear()) {
                wave_type = (uint8_t)((wave_type + 1) % ENEMY_TYPE_COUNT);
                wave_spawned = 0;
                wave_timer = ENEMY_INTER_WAVE_FRAMES;
                wave_state = WAVE_STATE_DELAY;
            }
            break;
    }
}
