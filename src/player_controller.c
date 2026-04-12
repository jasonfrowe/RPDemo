#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constants.h"
#include "input.h"
#include "player_controller.h"
#include "projectile.h"
#include "sprite_mode5.h"

// Speed level 1..16 maps to 0.25..4.0 pixels per frame (each step = 0.25 px/frame).
// To tune: change PLAYER_DEFAULT_SPEED. Range is PLAYER_SPEED_MIN to PLAYER_SPEED_MAX.
#define PLAYER_DEFAULT_SPEED  5     // 1.25 px/frame
#define PLAYER_SPEED_MIN      1     // 0.25 px/frame
#define PLAYER_SPEED_MAX      16    // 4.00 px/frame

// Internal: convert speed level to Q8 fixed-point (1 level = 0.25 px = 64 Q8 units).
#define Q8_SHIFT 8
#define SPEED_TO_Q8(level) ((int32_t)(level) * 64)

static int     player_speed  = PLAYER_DEFAULT_SPEED;
static int32_t player_x_q8;
static int32_t player_y_q8;
static uint8_t player_health = PLAYER_MAX_HEALTH;
static uint8_t hit_cooldown = 0;
static uint8_t damage_flash_timer = 0;
static bool player_destroyed = false;
static uint8_t death_anim_frame = PLAYER_DEATH_FRAME_START;
static uint8_t death_anim_tick = 0;

static bool prev_speed_down = false;
static bool prev_speed_up = false;
static uint8_t fire_cooldown = 0;

void player_controller_reset_for_new_run(void)
{
    player_speed = PLAYER_DEFAULT_SPEED;
    player_x_q8 = ((int32_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2)) << Q8_SHIFT;
    player_y_q8 = ((int32_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3)) << Q8_SHIFT;
    player_health = PLAYER_MAX_HEALTH;
    hit_cooldown = 0;
    damage_flash_timer = 0;
    player_destroyed = false;
    death_anim_frame = PLAYER_DEATH_FRAME_START;
    death_anim_tick = 0;
    fire_cooldown = 0;
    prev_speed_down = false;
    prev_speed_up = false;
    sprite_mode5_set_frame(0);
    sprite_mode5_set_position((int16_t)(player_x_q8 >> Q8_SHIFT), (int16_t)(player_y_q8 >> Q8_SHIFT));
}

void player_controller_set_speed(int level)
{
    if (level < PLAYER_SPEED_MIN) level = PLAYER_SPEED_MIN;
    if (level > PLAYER_SPEED_MAX) level = PLAYER_SPEED_MAX;
    player_speed = level;
}

int player_controller_get_speed(void)
{
    return player_speed;
}

void player_controller_get_position(int16_t *x, int16_t *y)
{
    if (x != NULL) {
        *x = (int16_t)(player_x_q8 >> Q8_SHIFT);
    }
    if (y != NULL) {
        *y = (int16_t)(player_y_q8 >> Q8_SHIFT);
    }
}

void player_controller_get_center_position(int16_t *x, int16_t *y)
{
    int16_t top_left_x = (int16_t)(player_x_q8 >> Q8_SHIFT);
    int16_t top_left_y = (int16_t)(player_y_q8 >> Q8_SHIFT);

    if (x != NULL) {
        *x = (int16_t)(top_left_x + (PLAYER_SPRITE_SIZE_PX / 2));
    }
    if (y != NULL) {
        *y = (int16_t)(top_left_y + (PLAYER_SPRITE_SIZE_PX / 2));
    }
}

void player_controller_init(void)
{
    player_controller_reset_for_new_run();
}

void player_controller_apply_damage(uint8_t amount)
{
    if (!player_controller_can_take_damage()) {
        return;
    }

    if (amount >= player_health) {
        player_health = 0;
    } else {
        player_health = (uint8_t)(player_health - amount);
    }

    hit_cooldown = PLAYER_HIT_COOLDOWN_FRAMES;
    damage_flash_timer = PLAYER_HIT_FLASH_FRAMES;

    if (player_health == 0) {
        player_destroyed = true;
        fire_cooldown = 0;
        death_anim_frame = PLAYER_DEATH_FRAME_START;
        death_anim_tick = 0;
        sprite_mode5_set_frame(death_anim_frame);
    }
}

uint8_t player_controller_get_health(void)
{
    return player_health;
}

bool player_controller_is_destroyed(void)
{
    return player_destroyed;
}

bool player_controller_can_take_damage(void)
{
    return (!player_destroyed && hit_cooldown == 0);
}

bool player_controller_is_damage_flash_active(void)
{
    return (damage_flash_timer > 0);
}

bool player_controller_is_low_health(void)
{
    return (player_health > 0 && player_health <= PLAYER_LOW_HEALTH_THRESHOLD);
}

void player_controller_update(void)
{
    if (hit_cooldown > 0) {
        hit_cooldown--;
    }
    if (damage_flash_timer > 0) {
        damage_flash_timer--;
    }

    if (player_destroyed) {
        death_anim_tick++;
        if (death_anim_tick >= PLAYER_DEATH_FRAME_STEP_FRAMES) {
            death_anim_tick = 0;
            if (death_anim_frame < PLAYER_DEATH_FRAME_END) {
                death_anim_frame++;
                sprite_mode5_set_frame(death_anim_frame);
            }
        }
        return;
    }

    // Tap LT to decrease speed by 1, tap RT to increase speed by 1.
    bool speed_down_now = is_action_pressed(0, ACTION_BTN_LT);
    bool speed_up_now = is_action_pressed(0, ACTION_BTN_RT);

    if (speed_down_now && !prev_speed_down) {
        player_controller_set_speed(player_speed - 1);
    }
    if (speed_up_now && !prev_speed_up) {
        player_controller_set_speed(player_speed + 1);
    }
    prev_speed_down = speed_down_now;
    prev_speed_up = speed_up_now;

    bool moving_up = is_action_pressed(0, ACTION_MOVE_UP);
    bool moving_down = is_action_pressed(0, ACTION_MOVE_DOWN);
    bool moving_left = is_action_pressed(0, ACTION_MOVE_LEFT);
    bool moving_right = is_action_pressed(0, ACTION_MOVE_RIGHT);

    // Fire projectile on X — held down, rate-limited
    if (fire_cooldown > 0) fire_cooldown--;
    if (is_action_pressed(0, ACTION_BTN_X) && fire_cooldown == 0) {
        int16_t px = (int16_t)(player_x_q8 >> Q8_SHIFT);
        int16_t py = (int16_t)(player_y_q8 >> Q8_SHIFT);
        // Spawn at horizontal center of player, top edge
        projectile_fire_player((int16_t)(px + (PLAYER_SPRITE_SIZE_PX - PROJECTILE_SPRITE_SIZE_PX) / 2), py);
        fire_cooldown = PLAYER_FIRE_RATE;
    }

    int32_t speed_q8 = SPEED_TO_Q8(player_speed);

    if (moving_up)    player_y_q8 -= speed_q8;
    if (moving_down)  player_y_q8 += speed_q8;
    if (moving_left)  player_x_q8 -= speed_q8;
    if (moving_right) player_x_q8 += speed_q8;

    sprite_mode5_update_engine(moving_down);

    if (moving_left && !moving_right) {
        sprite_mode5_set_frame(2);
    } else if (moving_right && !moving_left) {
        sprite_mode5_set_frame(1);
    } else {
        sprite_mode5_set_frame(0);
    }

    int32_t max_x_q8 = ((int32_t)(SCREEN_WIDTH  - PLAYER_SPRITE_SIZE_PX)) << Q8_SHIFT;
    int32_t max_y_q8 = ((int32_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)) << Q8_SHIFT;

    if (player_x_q8 < 0)         player_x_q8 = 0;
    if (player_x_q8 > max_x_q8) player_x_q8 = max_x_q8;
    if (player_y_q8 < ((int32_t)HUD_TOP_PX << Q8_SHIFT)) {
        player_y_q8 = ((int32_t)HUD_TOP_PX << Q8_SHIFT);
    }
    if (player_y_q8 > max_y_q8) player_y_q8 = max_y_q8;

    sprite_mode5_set_position((int16_t)(player_x_q8 >> Q8_SHIFT), (int16_t)(player_y_q8 >> Q8_SHIFT));
}
