#ifndef ENEMY_H
#define ENEMY_H

#include <stdbool.h>
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
void enemy_start_level(uint8_t level_index);
void enemy_update(void);
uint8_t enemy_get_level(void);
uint8_t enemy_get_subwave(void);
bool enemy_is_level_complete(void);
bool enemy_hit_test_player(int16_t x, int16_t y, int16_t width, int16_t height);
void enemy_clear_all(void);
void enemy_prepare_bonus_icons(void);
void enemy_start_bonus_icon_fly_in(uint8_t enemy_type);
bool enemy_update_bonus_icon_fly_in(void);
void enemy_show_bonus_icons(void);
void enemy_hide_bonus_icons(void);
void enemy_start_game_over_animation(void);
void enemy_stop_game_over_animation(void);
bool enemy_is_game_over_animation_complete(void);

#endif // ENEMY_H
