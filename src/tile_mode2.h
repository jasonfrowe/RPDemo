#ifndef TILE_MODE2_H
#define TILE_MODE2_H

#include <stdbool.h>
#include <stdint.h>

// Palette extracted from Sprites/Tiles.png
static const uint16_t tile_bg_palette[16] = {
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

static const uint16_t tile_fg_palette[16] = {
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

static const uint16_t tile_hud_palette[16] = {
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

void tile_mode2_init(void);
void tile_mode2_start_gameplay_transition(void);
void tile_mode2_start_game_over_transition(void);
bool tile_mode2_restore_hud_from_rom(void);
void tile_mode2_update_title_palette(void);
void tile_mode2_set_score(uint32_t score);
void tile_mode2_set_health(uint8_t health);
void tile_mode2_update_health_fx(bool damage_flash_active, bool low_health);
void tile_mode2_update_scroll(void);

#endif // TILE_MODE2_H
