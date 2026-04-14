#ifndef RNG_H
#define RNG_H

#include <stdint.h>

uint16_t rng_next(uint16_t *state);
int16_t rng_range(uint16_t *state, int16_t min_value, int16_t max_value);

#endif // RNG_H
