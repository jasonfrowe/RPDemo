#ifndef PLAYER_CONTROLLER_H
#define PLAYER_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

void player_controller_init(void);
void player_controller_reset_for_new_run(void);
void player_controller_update(void);
void player_controller_get_position(int16_t *x, int16_t *y);
void player_controller_set_position(int16_t x, int16_t y);
void player_controller_reset_damage_state(void);
void player_controller_begin_respawn(void);
void player_controller_get_center_position(int16_t *x, int16_t *y);
void player_controller_apply_damage(uint8_t amount);
void player_controller_heal(uint8_t amount);
uint8_t player_controller_get_health(void);
bool player_controller_is_destroyed(void);
bool player_controller_is_death_animation_complete(void);
bool player_controller_can_take_damage(void);
bool player_controller_is_damage_flash_active(void);
bool player_controller_is_low_health(void);

// Speed level 1..10: level 1 = 0.25 px/frame, level 10 = 2.5 px/frame.
// Each step is 0.25 px/frame. Default is 5 (1.25 px/frame).
// In-game: tap LT to slow down, tap RT to restore speed up to the unlocked cap.
void player_controller_set_speed(int level);
int  player_controller_get_speed(void);
void player_controller_apply_speed_pickup(void);
void player_controller_apply_power_pickup(void);
uint8_t player_controller_get_fire_rate(void);

#endif // PLAYER_CONTROLLER_H
