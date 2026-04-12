#ifndef PROJECTILE_H
#define PROJECTILE_H

#include <stdbool.h>
#include <stdint.h>

#define PLAYER_PROJECTILE_FRAME 0
#define ENEMY_PROJECTILE_FRAME  1

#define FIRST_ENEMY_PROJECTILE_SLOT MAX_PLAYER_PROJECTILES

void projectile_init(void);
void projectile_fire_player(int16_t x, int16_t y);
bool projectile_fire_enemy(int16_t x, int16_t y, int16_t vx_q8, int16_t vy_q8, uint8_t frame_index);
bool projectile_hit_test_enemy(int16_t x, int16_t y, int16_t width, int16_t height);
bool projectile_hit_test_player(int16_t x, int16_t y, int16_t width, int16_t height);
void projectile_update(void);

#endif // PROJECTILE_H
