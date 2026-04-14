#include <stdint.h>
#include <stdbool.h>

#include "constants.h"
#include "projectile.h"
#include "player_controller.h"
#include "rng.h"
#include "sprite_mode5.h"

typedef enum {
    PROJECTILE_OWNER_NONE,
    PROJECTILE_OWNER_PLAYER,
    PROJECTILE_OWNER_ENEMY,
    PROJECTILE_OWNER_ASTEROID,
    PROJECTILE_OWNER_PICKUP,
} projectile_owner_t;

typedef struct {
    bool               active;
    projectile_owner_t owner;
    int32_t            x_q8;
    int32_t            y_q8;
    int16_t            vx_q8;
    int16_t            vy_q8;
    uint8_t            frame_index;
    uint8_t            anim_tick;
} Projectile;

static Projectile projectiles[MAX_PROJECTILES];

#define Q8_SHIFT 8
#define TO_Q8(px) ((int32_t)(px) * (1 << Q8_SHIFT))
#define PROJECTILE_HITBOX_OFFSET_X 3
#define PROJECTILE_HITBOX_OFFSET_Y 2
#define PROJECTILE_HITBOX_WIDTH 2
#define PROJECTILE_HITBOX_HEIGHT 4
#define ASTEROID_PICKUP_HITBOX_OFFSET 0
#define ASTEROID_PICKUP_HITBOX_SIZE 8
#define ASTEROID_ANIM_TICK_FRAMES 6
#define PICKUP_VX_Q8 TO_Q8(2)
#define PICKUP_VY_Q8 (TO_Q8(1) / 4)
#define NO_PICKUP_FRAME 0xFFu

static uint16_t pickup_rng = 0xA37Fu;

static void projectile_deactivate(uint8_t slot);

static uint8_t projectile_roll_pickup_frame(void)
{
    uint8_t roll = (uint8_t)(rng_next(&pickup_rng) % 100u);

    if (roll < 50u) {
        return NO_PICKUP_FRAME;
    }
    if (roll < 90u) {
        return PICKUP_ENERGY_FRAME;
    }
    if (roll < 95u) {
        return PICKUP_POWER_FRAME;
    }
    return PICKUP_SPEED_FRAME;
}

static bool projectile_try_spawn_pickup(int16_t x, int16_t y, uint8_t frame_index)
{
    for (uint8_t i = FIRST_ENEMY_PROJECTILE_SLOT; i < MAX_PROJECTILES; ++i) {
        if (projectiles[i].active) {
            continue;
        }

        projectiles[i].active = true;
        projectiles[i].owner = PROJECTILE_OWNER_PICKUP;
        projectiles[i].x_q8 = TO_Q8(x);
        projectiles[i].y_q8 = TO_Q8(y);
        projectiles[i].vx_q8 = (rng_next(&pickup_rng) & 1u) ? PICKUP_VX_Q8 : (int16_t)-PICKUP_VX_Q8;
        projectiles[i].vy_q8 = PICKUP_VY_Q8;
        projectiles[i].frame_index = frame_index;
        projectiles[i].anim_tick = 0;
        sprite_mode5_set_projectile_frame(i, projectiles[i].frame_index);
        sprite_mode5_set_projectile_position(i, x, y);
        return true;
    }

    return false;
}

static void projectile_try_hit_asteroids(void)
{
    for (uint8_t b = 0; b < MAX_PLAYER_PROJECTILES; ++b) {
        int16_t bullet_left;
        int16_t bullet_top;
        int16_t bullet_right;
        int16_t bullet_bottom;

        if (!projectiles[b].active || projectiles[b].owner != PROJECTILE_OWNER_PLAYER) {
            continue;
        }

        bullet_left = (int16_t)((projectiles[b].x_q8 >> Q8_SHIFT) + PROJECTILE_HITBOX_OFFSET_X);
        bullet_top = (int16_t)((projectiles[b].y_q8 >> Q8_SHIFT) + PROJECTILE_HITBOX_OFFSET_Y);
        bullet_right = (int16_t)(bullet_left + PROJECTILE_HITBOX_WIDTH);
        bullet_bottom = (int16_t)(bullet_top + PROJECTILE_HITBOX_HEIGHT);

        for (uint8_t a = FIRST_ENEMY_PROJECTILE_SLOT; a < MAX_PROJECTILES; ++a) {
            int16_t asteroid_left;
            int16_t asteroid_top;
            int16_t asteroid_right;
            int16_t asteroid_bottom;
            uint8_t pickup_frame;

            if (!projectiles[a].active || projectiles[a].owner != PROJECTILE_OWNER_ASTEROID) {
                continue;
            }

            asteroid_left = (int16_t)((projectiles[a].x_q8 >> Q8_SHIFT) + ASTEROID_PICKUP_HITBOX_OFFSET);
            asteroid_top = (int16_t)((projectiles[a].y_q8 >> Q8_SHIFT) + ASTEROID_PICKUP_HITBOX_OFFSET);
            asteroid_right = (int16_t)(asteroid_left + ASTEROID_PICKUP_HITBOX_SIZE);
            asteroid_bottom = (int16_t)(asteroid_top + ASTEROID_PICKUP_HITBOX_SIZE);

            if (bullet_right <= asteroid_left || bullet_left >= asteroid_right ||
                bullet_bottom <= asteroid_top || bullet_top >= asteroid_bottom) {
                continue;
            }

            projectile_deactivate(b);
            pickup_frame = projectile_roll_pickup_frame();
            if (pickup_frame != NO_PICKUP_FRAME) {
                projectile_try_spawn_pickup((int16_t)(asteroid_left - ASTEROID_PICKUP_HITBOX_OFFSET),
                                            (int16_t)(asteroid_top - ASTEROID_PICKUP_HITBOX_OFFSET),
                                            pickup_frame);
            }
            projectile_deactivate(a);
            break;
        }
    }
}

static void projectile_try_collect_pickups(void)
{
    int16_t player_x;
    int16_t player_y;
    int16_t hitbox_x;
    int16_t hitbox_y;
    int16_t hitbox_right;
    int16_t hitbox_bottom;

    player_controller_get_position(&player_x, &player_y);
    hitbox_x = (int16_t)(player_x + PLAYER_HITBOX_OFFSET);
    hitbox_y = (int16_t)(player_y + PLAYER_HITBOX_OFFSET);
    hitbox_right = (int16_t)(hitbox_x + PLAYER_HITBOX_SIZE);
    hitbox_bottom = (int16_t)(hitbox_y + PLAYER_HITBOX_SIZE);

    for (uint8_t i = FIRST_ENEMY_PROJECTILE_SLOT; i < MAX_PROJECTILES; ++i) {
        int16_t pickup_left;
        int16_t pickup_top;
        int16_t pickup_right;
        int16_t pickup_bottom;

        if (!projectiles[i].active || projectiles[i].owner != PROJECTILE_OWNER_PICKUP) {
            continue;
        }

        pickup_left = (int16_t)((projectiles[i].x_q8 >> Q8_SHIFT) + ASTEROID_PICKUP_HITBOX_OFFSET);
        pickup_top = (int16_t)((projectiles[i].y_q8 >> Q8_SHIFT) + ASTEROID_PICKUP_HITBOX_OFFSET);
        pickup_right = (int16_t)(pickup_left + ASTEROID_PICKUP_HITBOX_SIZE);
        pickup_bottom = (int16_t)(pickup_top + ASTEROID_PICKUP_HITBOX_SIZE);

        if (pickup_right <= hitbox_x || pickup_left >= hitbox_right ||
            pickup_bottom <= hitbox_y || pickup_top >= hitbox_bottom) {
            continue;
        }

        if (projectiles[i].frame_index == PICKUP_ENERGY_FRAME) {
            player_controller_heal(8);
        } else if (projectiles[i].frame_index == PICKUP_SPEED_FRAME) {
            player_controller_apply_speed_pickup();
        } else if (projectiles[i].frame_index == PICKUP_POWER_FRAME) {
            player_controller_apply_power_pickup();
        }

        projectile_deactivate(i);
    }
}

static void projectile_deactivate(uint8_t slot)
{
    projectiles[slot].active = false;
    projectiles[slot].owner = PROJECTILE_OWNER_NONE;
    projectiles[slot].x_q8 = TO_Q8(-32);
    projectiles[slot].y_q8 = TO_Q8(-32);
    projectiles[slot].vx_q8 = 0;
    projectiles[slot].vy_q8 = 0;
    projectiles[slot].frame_index = PLAYER_PROJECTILE_FRAME;
    projectiles[slot].anim_tick = 0;
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
    return (y < (HUD_TOP_PX - PROJECTILE_SPRITE_SIZE_PX));
}

static bool projectile_is_offscreen_noncombat(uint8_t slot)
{
    int16_t x = (int16_t)(projectiles[slot].x_q8 >> Q8_SHIFT);
    int16_t y = (int16_t)(projectiles[slot].y_q8 >> Q8_SHIFT);

    return (
        x < -PROJECTILE_SPRITE_SIZE_PX ||
        x > SCREEN_WIDTH ||
        y < -PROJECTILE_SPRITE_SIZE_PX ||
        y > SCREEN_HEIGHT
    );
}

void projectile_init(void)
{
    pickup_rng = 0xA37Fu;
    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
        projectile_deactivate(i);
    }
    // Hardware slots are already positioned off-screen by sprite_mode5_init_projectiles()
}

void projectile_spawn_asteroid_wave(uint8_t count)
{
    if (count == 0) {
        return;
    }
    if (count > 3) {
        count = 3;
    }

    for (uint8_t spawned = 0; spawned < count; ++spawned) {
        for (uint8_t i = FIRST_ENEMY_PROJECTILE_SLOT; i < MAX_PROJECTILES; ++i) {
            int16_t x;
            int16_t y;

            if (projectiles[i].active) {
                continue;
            }

            x = rng_range(&pickup_rng, 8, (int16_t)(SCREEN_WIDTH - PROJECTILE_SPRITE_SIZE_PX - 8));
            y = (int16_t)(HUD_TOP_PX - PROJECTILE_SPRITE_SIZE_PX - (spawned * 12));
            projectiles[i].active = true;
            projectiles[i].owner = PROJECTILE_OWNER_ASTEROID;
            projectiles[i].x_q8 = TO_Q8(x);
            projectiles[i].y_q8 = TO_Q8(y);
            projectiles[i].vx_q8 = 0;
            projectiles[i].vy_q8 = TO_Q8(1);
            projectiles[i].frame_index = ASTEROID_FRAME_0;
            projectiles[i].anim_tick = 0;
            sprite_mode5_set_projectile_frame(i, projectiles[i].frame_index);
            sprite_mode5_set_projectile_position(i, x, y);
            break;
        }
    }
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

        bullet_left = (int16_t)((projectiles[i].x_q8 >> Q8_SHIFT) + PROJECTILE_HITBOX_OFFSET_X);
        bullet_top = (int16_t)((projectiles[i].y_q8 >> Q8_SHIFT) + PROJECTILE_HITBOX_OFFSET_Y);
        bullet_right = (int16_t)(bullet_left + PROJECTILE_HITBOX_WIDTH);
        bullet_bottom = (int16_t)(bullet_top + PROJECTILE_HITBOX_HEIGHT);

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

        bullet_left = (int16_t)((projectiles[i].x_q8 >> Q8_SHIFT) + PROJECTILE_HITBOX_OFFSET_X);
        bullet_top = (int16_t)((projectiles[i].y_q8 >> Q8_SHIFT) + PROJECTILE_HITBOX_OFFSET_Y);
        bullet_right = (int16_t)(bullet_left + PROJECTILE_HITBOX_WIDTH);
        bullet_bottom = (int16_t)(bullet_top + PROJECTILE_HITBOX_HEIGHT);

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
    projectile_try_hit_asteroids();
    projectile_try_collect_pickups();

    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
        int16_t x;
        int16_t y;

        if (!projectiles[i].active) continue;

        projectiles[i].x_q8 += projectiles[i].vx_q8;
        projectiles[i].y_q8 += projectiles[i].vy_q8;

        x = (int16_t)(projectiles[i].x_q8 >> Q8_SHIFT);
        y = (int16_t)(projectiles[i].y_q8 >> Q8_SHIFT);

        if (projectiles[i].owner == PROJECTILE_OWNER_PICKUP) {
            if (x <= 0 || x >= (int16_t)(SCREEN_WIDTH - PROJECTILE_SPRITE_SIZE_PX)) {
                projectiles[i].vx_q8 = (int16_t)-projectiles[i].vx_q8;
            }
        }

        if (projectiles[i].owner == PROJECTILE_OWNER_ASTEROID) {
            projectiles[i].anim_tick++;
            if (projectiles[i].anim_tick >= ASTEROID_ANIM_TICK_FRAMES) {
                uint8_t seq = (uint8_t)(projectiles[i].frame_index - ASTEROID_FRAME_0);
                projectiles[i].anim_tick = 0;
                seq = (uint8_t)((seq + 1) % 3u);
                projectiles[i].frame_index = (uint8_t)(ASTEROID_FRAME_0 + seq);
                sprite_mode5_set_projectile_frame(i, projectiles[i].frame_index);
            }
        }

        if (projectiles[i].owner == PROJECTILE_OWNER_PLAYER) {
            if (projectile_is_offscreen_player(i)) {
                projectile_deactivate(i);
            } else {
                sprite_mode5_set_projectile_position(i, x, y);
            }
        } else if (projectiles[i].owner == PROJECTILE_OWNER_ENEMY) {
            if (projectile_is_offscreen_enemy(i)) {
                projectile_deactivate(i);
            } else {
                sprite_mode5_set_projectile_position(i, x, y);
            }
        } else if (projectiles[i].owner == PROJECTILE_OWNER_ASTEROID ||
                   projectiles[i].owner == PROJECTILE_OWNER_PICKUP) {
            if (projectile_is_offscreen_noncombat(i)) {
                projectile_deactivate(i);
            } else {
                sprite_mode5_set_projectile_position(i, x, y);
            }
        }
    }
}
