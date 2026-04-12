#ifndef ENEMY_H
#define ENEMY_H

#include <stdint.h>

// --- Tunable wave parameters ---
#define ENEMY_WAVE_SIZE           5    // enemies per wave (starting value)
#define ENEMY_SPAWN_DELAY_FRAMES  120  // frames after game start before first wave
#define ENEMY_INTER_SPAWN_FRAMES  30   // frames between each enemy spawned in a wave
#define ENEMY_INTER_WAVE_FRAMES   90   // frames between waves

// --- Movement ---
#define ENEMY_SPEED_Y             1    // pixels/frame downward movement
#define ENEMY_ZIG_SPEED           2    // pixels/frame horizontal zig-zag

void enemy_init(void);
void enemy_update(void);

#endif // ENEMY_H
