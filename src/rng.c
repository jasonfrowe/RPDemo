#include "rng.h"

uint16_t rng_next(uint16_t *state)
{
    *state = (uint16_t)((*state * 25173u) + 13849u);
    return *state;
}

int16_t rng_range(uint16_t *state, int16_t min_value, int16_t max_value)
{
    uint16_t span = (uint16_t)(max_value - min_value + 1);
    return (int16_t)(min_value + (rng_next(state) % span));
}
