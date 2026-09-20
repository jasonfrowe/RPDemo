#ifndef GAMEPLAY_BOSS_H
#define GAMEPLAY_BOSS_H

#include "gameplay_internal.h"

void gameplay_boss_begin(gameplay_runtime_t *state);
void gameplay_boss_update(gameplay_runtime_t *state);
void gameplay_boss_reset(void);
uint8_t gameplay_boss_frame_set_a_base_for_level(uint8_t level);

#endif
