#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

#include <stdbool.h>

typedef struct {
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} vga_mode5_sprite_t;

// Palette extracted from Sprites/Player.png
static const uint16_t player_palette[16] = {
    0x0000, // transparent
    0xA820,
    0x0560,
    0xAD60,
    0x0035,
    0xA835,
    0x02B5,
    0xAD75,
    0x52AA,
    0xFAAA,
    0x57EA,
    0xFFEA,
    0x52BF, // index 12 -- engine glow
    0xFABF,
    0x57FF,
    0xFFFF,
};

// Palette extracted from Sprites/Projectiles.png
static const uint16_t projectiles_palette[16] = {
    0x0000,
    0xA820,
    0x0560,
    0xAD60,
    0x0035,
    0xA835,
    0x02B5,
    0xAD75,
    0x52AA,
    0xFAAA,
    0x57EA,
    0xFFEA,
    0x52BF,
    0xFABF,
    0x57FF,
    0xFFFF,
};

// Placeholder palette for Enemies — replace with values extracted from Enemies_4bpp.png
static const uint16_t enemy_palette[16] = {
    0x0000,
    0x0021,
    0x20EC,
    0x2AE5,
    0x226B,
    0x69EA,
    0x636C,
    0x19F8,
    0x3CA8,
    0x2CF7,
    0x9AEA,
    0xC468,
    0xAD32,
    0x0020,
    0x0020,
    0x0020,
};

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

#endif // SPRITE_MODE5_H
