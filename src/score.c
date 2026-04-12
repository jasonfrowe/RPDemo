#include <stdint.h>

#include "score.h"
#include "tile_mode2.h"

static uint32_t g_score = 0;

static uint8_t score_points_for_enemy(uint8_t enemy_type)
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
    tile_mode2_set_score(g_score);
}

void score_add_enemy_kill(uint8_t enemy_type)
{
    uint8_t points = score_points_for_enemy(enemy_type);

    if (g_score < 999999u) {
        g_score += points;
        if (g_score > 999999u) {
            g_score = 999999u;
        }
    }

    tile_mode2_set_score(g_score);
}

uint32_t score_get(void)
{
    return g_score;
}
