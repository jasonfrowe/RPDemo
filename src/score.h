#ifndef SCORE_H
#define SCORE_H

#include <stdint.h>

void score_init(void);
void score_add_enemy_kill(uint8_t enemy_type);
uint32_t score_get(void);

#endif // SCORE_H
