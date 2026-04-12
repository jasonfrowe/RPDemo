#ifndef PLAYER_CONTROLLER_H
#define PLAYER_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

void player_controller_init(void);
void player_controller_reset_for_new_run(void);
void player_controller_update(void);
void player_controller_get_position(int16_t *x, int16_t *y);
void player_controller_get_center_position(int16_t *x, int16_t *y);
void player_controller_apply_damage(uint8_t amount);
uint8_t player_controller_get_health(void);
bool player_controller_is_destroyed(void);
bool player_controller_can_take_damage(void);
bool player_controller_is_damage_flash_active(void);
bool player_controller_is_low_health(void);

// Speed level 1..16: level 1 = 0.25 px/frame, level 16 = 4.0 px/frame.
// Each step is 0.25 px/frame. Default is 3 (0.75 px/frame).
// In-game: tap LT to slow down, tap RT to speed up.
void player_controller_set_speed(int level);
int  player_controller_get_speed(void);

#endif // PLAYER_CONTROLLER_H
