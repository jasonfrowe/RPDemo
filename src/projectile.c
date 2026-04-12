#include <stdint.h>
#include <stdbool.h>

#include "constants.h"
#include "projectile.h"
#include "sprite_mode5.h"

typedef enum {
    PROJECTILE_OWNER_NONE,
    PROJECTILE_OWNER_PLAYER,
    PROJECTILE_OWNER_ENEMY,
} projectile_owner_t;

typedef struct {
    bool               active;
    projectile_owner_t owner;
    int32_t            x_q8;
    int32_t            y_q8;
    int16_t            vx_q8;
    int16_t            vy_q8;
    uint8_t            frame_index;
} Projectile;

static Projectile projectiles[MAX_PROJECTILES];

#define Q8_SHIFT 8
#define TO_Q8(px) ((int32_t)(px) * (1 << Q8_SHIFT))

static void projectile_deactivate(uint8_t slot)
{
    projectiles[slot].active = false;
    projectiles[slot].owner = PROJECTILE_OWNER_NONE;
    projectiles[slot].x_q8 = TO_Q8(-32);
    projectiles[slot].y_q8 = TO_Q8(-32);
    projectiles[slot].vx_q8 = 0;
    projectiles[slot].vy_q8 = 0;
    projectiles[slot].frame_index = PLAYER_PROJECTILE_FRAME;
    sprite_mode5_set_projectile_position(slot, -32, -32);
}

static bool projectile_is_offscreen_enemy(uint8_t slot)
{
    int16_t x = (int16_t)(projectiles[slot].x_q8 >> Q8_SHIFT);
    int16_t y = (int16_t)(projectiles[slot].y_q8 >> Q8_SHIFT);

    return (
        x < -PROJECTILE_SPRITE_SIZE_PX ||
        x > SCREEN_WIDTH ||
        y < HUD_TOP_PX ||
        y > SCREEN_HEIGHT
    );
}

static bool projectile_is_offscreen_player(uint8_t slot)
{
    int16_t y = (int16_t)(projectiles[slot].y_q8 >> Q8_SHIFT);
    return (y < HUD_TOP_PX);
}

void projectile_init(void)
{
    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
        projectile_deactivate(i);
    }
    // Hardware slots are already positioned off-screen by sprite_mode5_init_projectiles()
}

void projectile_fire_player(int16_t x, int16_t y)
{
    for (uint8_t i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].active = true;
            projectiles[i].owner = PROJECTILE_OWNER_PLAYER;
            projectiles[i].x_q8 = TO_Q8(x);
            projectiles[i].y_q8 = TO_Q8(y);
            projectiles[i].vx_q8 = 0;
            projectiles[i].vy_q8 = (int16_t)(-TO_Q8(PROJECTILE_SPEED_PX));
            projectiles[i].frame_index = PLAYER_PROJECTILE_FRAME;
            sprite_mode5_set_projectile_frame(i, PLAYER_PROJECTILE_FRAME);
            sprite_mode5_set_projectile_position(i, x, y);
            return;
        }
    }
    // All player slots full — no-op
}

bool projectile_fire_enemy(int16_t x, int16_t y, int16_t vx_q8, int16_t vy_q8, uint8_t frame_index)
{
    for (uint8_t i = FIRST_ENEMY_PROJECTILE_SLOT; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].active = true;
            projectiles[i].owner = PROJECTILE_OWNER_ENEMY;
            projectiles[i].x_q8 = TO_Q8(x);
            projectiles[i].y_q8 = TO_Q8(y);
            projectiles[i].vx_q8 = vx_q8;
            projectiles[i].vy_q8 = vy_q8;
            projectiles[i].frame_index = frame_index;
            sprite_mode5_set_projectile_frame(i, frame_index);
            sprite_mode5_set_projectile_position(i, x, y);
            return true;
        }
    }

    return false;
}

bool projectile_hit_test_enemy(int16_t x, int16_t y, int16_t width, int16_t height)
{
    int16_t enemy_right = (int16_t)(x + width);
    int16_t enemy_bottom = (int16_t)(y + height);

    for (uint8_t i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        int16_t bullet_left;
        int16_t bullet_top;
        int16_t bullet_right;
        int16_t bullet_bottom;

        if (!projectiles[i].active) continue;
        if (projectiles[i].owner != PROJECTILE_OWNER_PLAYER) continue;

        bullet_left = (int16_t)(projectiles[i].x_q8 >> Q8_SHIFT);
        bullet_top = (int16_t)(projectiles[i].y_q8 >> Q8_SHIFT);
        bullet_right = (int16_t)(bullet_left + PROJECTILE_SPRITE_SIZE_PX);
        bullet_bottom = (int16_t)(bullet_top + PROJECTILE_SPRITE_SIZE_PX);

        if (bullet_right <= x || bullet_left >= enemy_right ||
            bullet_bottom <= y || bullet_top >= enemy_bottom) {
            continue;
        }

        // Hit: consume this projectile.
        projectile_deactivate(i);
        return true;
    }

    return false;
}

bool projectile_hit_test_player(int16_t x, int16_t y, int16_t width, int16_t height)
{
    int16_t player_right = (int16_t)(x + width);
    int16_t player_bottom = (int16_t)(y + height);

    for (uint8_t i = FIRST_ENEMY_PROJECTILE_SLOT; i < MAX_PROJECTILES; i++) {
        int16_t bullet_left;
        int16_t bullet_top;
        int16_t bullet_right;
        int16_t bullet_bottom;

        if (!projectiles[i].active) continue;
        if (projectiles[i].owner != PROJECTILE_OWNER_ENEMY) continue;

        bullet_left = (int16_t)(projectiles[i].x_q8 >> Q8_SHIFT);
        bullet_top = (int16_t)(projectiles[i].y_q8 >> Q8_SHIFT);
        bullet_right = (int16_t)(bullet_left + PROJECTILE_SPRITE_SIZE_PX);
        bullet_bottom = (int16_t)(bullet_top + PROJECTILE_SPRITE_SIZE_PX);

        if (bullet_right <= x || bullet_left >= player_right ||
            bullet_bottom <= y || bullet_top >= player_bottom) {
            continue;
        }

        projectile_deactivate(i);
        return true;
    }

    return false;
}

void projectile_update(void)
{
    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
        int16_t x;
        int16_t y;

        if (!projectiles[i].active) continue;

        projectiles[i].x_q8 += projectiles[i].vx_q8;
        projectiles[i].y_q8 += projectiles[i].vy_q8;

        x = (int16_t)(projectiles[i].x_q8 >> Q8_SHIFT);
        y = (int16_t)(projectiles[i].y_q8 >> Q8_SHIFT);

        if (projectiles[i].owner == PROJECTILE_OWNER_PLAYER) {
            if (projectile_is_offscreen_player(i)) {
                projectile_deactivate(i);
            } else {
                sprite_mode5_set_projectile_position(i, x, y);
            }
        } else {
            if (projectile_is_offscreen_enemy(i)) {
                projectile_deactivate(i);
            } else {
                sprite_mode5_set_projectile_position(i, x, y);
            }
        }
    }
}
