#ifndef SCORE_H
#define SCORE_H

#include <stdint.h>

void score_init(void);
uint8_t score_points_for_enemy(uint8_t enemy_type);
void score_add_points(uint32_t points);
void score_add_enemy_kill(uint8_t enemy_type);
void score_reset_level_kills(void);
uint16_t score_get_level_kills(uint8_t enemy_type);
uint16_t score_get_level_total_kills(void);
uint32_t score_add_level_bonus(uint8_t level_multiplier);
uint32_t score_get(void);

#endif // SCORE_H
