#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "projectile.h"
#include "sprite_mode5.h"

typedef struct {
    bool    active;
    int16_t x;
    int16_t y;
} Projectile;

static Projectile projectiles[MAX_PROJECTILES];

void projectile_init(void)
{
    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = false;
        projectiles[i].x = -32;
        projectiles[i].y = -32;
    }
    // Hardware slots are already positioned off-screen by sprite_mode5_init_projectiles()
}

void projectile_fire_player(int16_t x, int16_t y)
{
    for (uint8_t i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].active = true;
            projectiles[i].x = x;
            projectiles[i].y = y;
            sprite_mode5_set_projectile_position(i, x, y);
            return;
        }
    }
    // All player slots full — no-op
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

        bullet_left = projectiles[i].x;
        bullet_top = projectiles[i].y;
        bullet_right = (int16_t)(bullet_left + PROJECTILE_SPRITE_SIZE_PX);
        bullet_bottom = (int16_t)(bullet_top + PROJECTILE_SPRITE_SIZE_PX);

        if (bullet_right <= x || bullet_left >= enemy_right ||
            bullet_bottom <= y || bullet_top >= enemy_bottom) {
            continue;
        }

        // Hit: consume this projectile.
        projectiles[i].active = false;
        projectiles[i].x = -32;
        projectiles[i].y = -32;
        sprite_mode5_set_projectile_position(i, -32, -32);
        return true;
    }

    return false;
}

void projectile_update(void)
{
    for (uint8_t i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        projectiles[i].y -= PROJECTILE_SPEED_PX;

        if (projectiles[i].y < HUD_TOP_PX) {
            // Bullet has left the play area — deactivate
            projectiles[i].active = false;
            sprite_mode5_set_projectile_position(i, -32, -32);
        } else {
            sprite_mode5_set_projectile_position(i, projectiles[i].x, projectiles[i].y);
        }
    }
}
