#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "score.h"
#include "constants.h"
#include "tile_mode2.h"

static uint32_t g_score = 0;
static uint32_t g_hiscore = 0;
static uint16_t g_level_kills[ENEMY_TYPE_COUNT];
static uint8_t g_multiplier = 1;
static uint8_t g_kills_since_hit = 0;

#define SCORE_MULTIPLIER_MAX 5u
#define HISCORE_FILE "highscore_starhopper.dat"

static void score_try_update_hiscore(void)
{
    if (g_score > g_hiscore) {
        g_hiscore = g_score;
    }
}

static void score_load_hiscore(void)
{
    int fd = open(HISCORE_FILE, O_RDONLY);
    if (fd < 0) {
        g_hiscore = 0;
        return;
    }

    if (read(fd, &g_hiscore, sizeof(g_hiscore)) != (int)sizeof(g_hiscore)) {
        g_hiscore = 0;
    }
    close(fd);

    if (g_hiscore > 999999u) {
        g_hiscore = 999999u;
    }
}

uint8_t score_points_for_enemy(uint8_t enemy_type)
{
    if (enemy_type == 6u) {
        return 100u;
    }

    uint8_t points = (uint8_t)(10u + (enemy_type * 5u));
    if (points > 40u) {
        points = 40u;
    }
    return points;
}

void score_init(void)
{
    g_score = 0;
    g_multiplier = 1;
    g_kills_since_hit = 0;
    score_load_hiscore();
    score_reset_level_kills();
    tile_mode2_set_score(g_score);
    tile_mode2_set_multiplier(g_multiplier);
}

void score_add_enemy_kill(uint8_t enemy_type)
{
    uint32_t points = score_points_for_enemy(enemy_type);

    if (enemy_type < ENEMY_TYPE_COUNT) {
        g_level_kills[enemy_type]++;
    }

    g_kills_since_hit++;

    points *= g_multiplier;

    if (g_score < 999999u) {
        g_score += points;
        if (g_score > 999999u) {
            g_score = 999999u;
        }
    }

    score_try_update_hiscore();

    if ((g_kills_since_hit % 2u) == 0u && g_multiplier < SCORE_MULTIPLIER_MAX) {
        g_multiplier++;
    }

    tile_mode2_set_score(g_score);
    tile_mode2_set_multiplier(g_multiplier);
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

    score_try_update_hiscore();
    tile_mode2_set_score(g_score);
}

void score_reset_level_kills(void)
{
    memset(g_level_kills, 0, sizeof(g_level_kills));
}

void score_reset_multiplier(void)
{
    g_multiplier = 1;
    g_kills_since_hit = 0;
    tile_mode2_set_multiplier(g_multiplier);
}

uint8_t score_get_multiplier(void)
{
    return g_multiplier;
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

    score_try_update_hiscore();
    tile_mode2_set_score(g_score);
    return added;
}

uint32_t score_get(void)
{
    return g_score;
}

uint32_t score_get_hiscore(void)
{
    return g_hiscore;
}

void score_commit_hiscore(void)
{
    int fd = open(HISCORE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;
    }

    (void)write(fd, &g_hiscore, sizeof(g_hiscore));
    close(fd);
}
