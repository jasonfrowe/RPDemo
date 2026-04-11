#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

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
    0x52BF,
    0xFABF,
    0x57FF,
    0xFFFF,
};

void sprite_mode5_init(void);
void sprite_mode5_set_position(int16_t x, int16_t y);

#endif // SPRITE_MODE5_H
