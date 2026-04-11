#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "input.h"
#include "player_controller.h"
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

static bool prev_speed_down = false;
static bool prev_speed_up = false;

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

void player_controller_init(void)
{
    player_x_q8 = ((int32_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2)) << Q8_SHIFT;
    player_y_q8 = ((int32_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3)) << Q8_SHIFT;

    sprite_mode5_set_position((int16_t)(player_x_q8 >> Q8_SHIFT), (int16_t)(player_y_q8 >> Q8_SHIFT));
}

void player_controller_update(void)
{
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

    int32_t speed_q8 = SPEED_TO_Q8(player_speed);

    if (is_action_pressed(0, ACTION_MOVE_UP))    player_y_q8 -= speed_q8;
    if (is_action_pressed(0, ACTION_MOVE_DOWN))  player_y_q8 += speed_q8;
    if (is_action_pressed(0, ACTION_MOVE_LEFT))  player_x_q8 -= speed_q8;
    if (is_action_pressed(0, ACTION_MOVE_RIGHT)) player_x_q8 += speed_q8;

    int32_t max_x_q8 = ((int32_t)(SCREEN_WIDTH  - PLAYER_SPRITE_SIZE_PX)) << Q8_SHIFT;
    int32_t max_y_q8 = ((int32_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)) << Q8_SHIFT;

    if (player_x_q8 < 0)         player_x_q8 = 0;
    if (player_x_q8 > max_x_q8) player_x_q8 = max_x_q8;
    if (player_y_q8 < 0)         player_y_q8 = 0;
    if (player_y_q8 > max_y_q8) player_y_q8 = max_y_q8;

    sprite_mode5_set_position((int16_t)(player_x_q8 >> Q8_SHIFT), (int16_t)(player_y_q8 >> Q8_SHIFT));
}
