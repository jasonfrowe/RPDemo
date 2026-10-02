#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

#include "xram.h"
#include <stdint.h>
#include <stdbool.h>

void sprite_mode5_init(void);
void sprite_mode5_init_projectiles(void);
void sprite_mode5_init_enemies(void);
void sprite_mode5_set_position(int16_t x, int16_t y);
void sprite_mode5_set_projectile_position(uint8_t slot, int16_t x, int16_t y);
void sprite_mode5_set_projectile_frame(uint8_t slot, uint8_t frame_index);
void sprite_mode5_set_enemy(uint8_t slot, int16_t x, int16_t y, uint8_t type);
void sprite_mode5_set_frame(uint8_t frame_index);
void sprite_mode5_update_engine(bool moving_down);
void sprite_mode5_set_damage_flash(bool active);
void sprite_mode5_hide_player(void);
void sprite_mode5_show_player(void);
void sprite_mode5_show_boss(int16_t x, int16_t y, uint8_t frame_set_base);
void sprite_mode5_hide_boss(void);
void sprite_mode5_set_boss_palette_active(bool active);
void sprite_mode5_set_boss_weakspot_flash(bool active);
void sprite_mode5_commit(void);

#endif // SPRITE_MODE5_H
