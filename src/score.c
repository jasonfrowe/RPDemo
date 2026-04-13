#include <stdint.h>
#include <string.h>

#include "score.h"
#include "constants.h"
#include "tile_mode2.h"

static uint32_t g_score = 0;
static uint16_t g_level_kills[ENEMY_TYPE_COUNT];

uint8_t score_points_for_enemy(uint8_t enemy_type)
{
    uint8_t points = (uint8_t)(10u + (enemy_type * 5u));
    if (points > 40u) {
        points = 40u;
    }
    return points;
}

void score_init(void)
{
    g_score = 0;
    score_reset_level_kills();
    tile_mode2_set_score(g_score);
}

void score_add_enemy_kill(uint8_t enemy_type)
{
    uint8_t points = score_points_for_enemy(enemy_type);

    if (enemy_type < ENEMY_TYPE_COUNT) {
        g_level_kills[enemy_type]++;
    }

    if (g_score < 999999u) {
        g_score += points;
        if (g_score > 999999u) {
            g_score = 999999u;
        }
    }

    tile_mode2_set_score(g_score);
}

void score_add_points(uint32_t points)
{
    if (points == 0 || g_score >= 999999u) {
        return;
    }

    g_score += points;
    if (g_score > 999999u) {
        g_score = 999999u;
    }

    tile_mode2_set_score(g_score);
}

void score_reset_level_kills(void)
{
    memset(g_level_kills, 0, sizeof(g_level_kills));
}

uint16_t score_get_level_kills(uint8_t enemy_type)
{
    if (enemy_type >= ENEMY_TYPE_COUNT) {
        return 0;
    }
    return g_level_kills[enemy_type];
}

uint16_t score_get_level_total_kills(void)
{
    uint16_t total = 0;

    for (uint8_t i = 0; i < ENEMY_TYPE_COUNT; ++i) {
        total = (uint16_t)(total + g_level_kills[i]);
    }

    return total;
}

uint32_t score_add_level_bonus(uint8_t level_multiplier)
{
    uint32_t added = 0;

    if (level_multiplier == 0) {
        level_multiplier = 1;
    }

    for (uint8_t i = 0; i < ENEMY_TYPE_COUNT; ++i) {
        uint32_t base = score_points_for_enemy(i);
        uint32_t subtotal = (uint32_t)g_level_kills[i] * base * level_multiplier;
        added += subtotal;
    }

    if (g_score < 999999u) {
        g_score += added;
        if (g_score > 999999u) {
            g_score = 999999u;
        }
    }

    tile_mode2_set_score(g_score);
    return added;
}

uint32_t score_get(void)
{
    return g_score;
}
