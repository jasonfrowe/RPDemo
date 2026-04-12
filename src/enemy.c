#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "enemy.h"
#include "projectile.h"
#include "sprite_mode5.h"

// Zig-zag horizontal bounds (keep sprites fully on-screen)
#define ZIG_MIN_X  16
#define ZIG_MAX_X  (SCREEN_WIDTH - ENEMY_SPRITE_SIZE_PX - 16)

typedef struct {
    bool    active;
    uint8_t type;       // enemy type 0..(ENEMY_TYPE_COUNT-1)
    int16_t x;
    int16_t y;
} Enemy;

static Enemy enemies[MAX_ENEMIES];

// Wave state machine
typedef enum {
    WAVE_STATE_DELAY,     // counting down before first/next wave
    WAVE_STATE_SPAWNING,  // spawning wave enemies one by one
    WAVE_STATE_CLEARING,  // all spawned, waiting for them to leave screen
} WaveState;

static WaveState wave_state;
static uint8_t   wave_type;     // current enemy type (0..ENEMY_TYPE_COUNT-1)
static uint8_t   wave_spawned;  // how many enemies spawned in current wave
static uint16_t  wave_timer;    // countdown for DELAY and inter-spawn gaps

static int16_t enemy_path_x_for_y(int16_t y)
{
    int16_t span = (int16_t)(ZIG_MAX_X - ZIG_MIN_X);
    int16_t phase = (int16_t)(y - HUD_TOP_PX);
    int16_t cycle;

    if (phase < 0) {
        phase = 0;
    }

    // Tie horizontal phase to vertical progress so all enemies follow one path.
    phase = (int16_t)(phase * ENEMY_ZIG_SPEED);
    cycle = (int16_t)(2 * span);
    if (cycle <= 0) {
        return ZIG_MIN_X;
    }

    phase = (int16_t)(phase % cycle);
    if (phase < span) {
        return (int16_t)(ZIG_MIN_X + phase);
    }
    return (int16_t)(ZIG_MAX_X - (phase - span));
}

static void spawn_enemy(uint8_t slot)
{
    enemies[slot].active  = true;
    enemies[slot].type    = wave_type;
    enemies[slot].y       = (int16_t)(-ENEMY_SPRITE_SIZE_PX);
    enemies[slot].x       = enemy_path_x_for_y(enemies[slot].y);
    sprite_mode5_set_enemy(slot, enemies[slot].x, enemies[slot].y, wave_type);
}

static bool wave_slots_clear(void)
{
    for (uint8_t i = 0; i < ENEMY_WAVE_SIZE; i++) {
        if (enemies[i].active) return false;
    }
    return true;
}

void enemy_init(void)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = false;
        // Hardware positions are already off-screen from sprite_mode5_init_enemies()
    }

    wave_state   = WAVE_STATE_DELAY;
    wave_type    = 0;
    wave_spawned = 0;
    wave_timer   = ENEMY_SPAWN_DELAY_FRAMES;
}

void enemy_update(void)
{
    // 1. Update all active enemies
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        // Move down
        enemies[i].y += ENEMY_SPEED_Y;
        enemies[i].x = enemy_path_x_for_y(enemies[i].y);

        // Check collision against player projectiles once enemy has entered play area.
        if (enemies[i].y >= HUD_TOP_PX &&
            projectile_hit_test_enemy(
                enemies[i].x,
                enemies[i].y,
                ENEMY_SPRITE_SIZE_PX,
                ENEMY_SPRITE_SIZE_PX
            )) {
            enemies[i].active = false;
            sprite_mode5_set_enemy(i, -32, -32, enemies[i].type);
            continue;
        }

        // Disable when off the bottom of the screen
        if (enemies[i].y >= (int16_t)SCREEN_HEIGHT) {
            enemies[i].active = false;
            sprite_mode5_set_enemy(i, -32, -32, enemies[i].type);
        } else {
            sprite_mode5_set_enemy(i, enemies[i].x, enemies[i].y, enemies[i].type);
        }
    }

    // 2. Wave state machine
    switch (wave_state) {
        case WAVE_STATE_DELAY:
            if (wave_timer > 0) {
                wave_timer--;
            } else {
                // Start spawning
                wave_state = WAVE_STATE_SPAWNING;
                wave_timer = 0;
            }
            break;

        case WAVE_STATE_SPAWNING:
            if (wave_timer > 0) {
                wave_timer--;
            } else {
                // Spawn next enemy in wave
                spawn_enemy(wave_spawned);
                wave_spawned++;

                if (wave_spawned >= ENEMY_WAVE_SIZE) {
                    // All spawned — wait for them to clear
                    wave_state = WAVE_STATE_CLEARING;
                } else {
                    wave_timer = ENEMY_INTER_SPAWN_FRAMES;
                }
            }
            break;

        case WAVE_STATE_CLEARING:
            if (wave_slots_clear()) {
                // Advance to next enemy type and start next wave
                wave_type    = (uint8_t)((wave_type + 1) % ENEMY_TYPE_COUNT);
                wave_spawned = 0;
                wave_timer   = ENEMY_INTER_WAVE_FRAMES;
                wave_state   = WAVE_STATE_DELAY;
            }
            break;
    }
}
