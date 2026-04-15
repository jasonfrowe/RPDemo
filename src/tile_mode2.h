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
void tile_mode2_start_level_bonus_transition(void);
bool tile_mode2_restore_hud_from_rom(void);
void tile_mode2_update_title_palette(void);
void tile_mode2_set_score(uint32_t score);
void tile_mode2_set_multiplier(uint8_t multiplier);
void tile_mode2_set_paused_banner(bool visible);
void tile_mode2_set_level_banner(uint8_t level, bool visible);
void tile_mode2_set_end_banner(bool victory);
void tile_mode2_set_level_complete_banner(bool visible);
void tile_mode2_set_level_failed_banner(bool visible);
void tile_mode2_set_bonus_continue_prompt(bool visible);
void tile_mode2_set_push_start_prompt(bool visible);
void tile_mode2_set_health(uint8_t health);
void tile_mode2_set_lives(uint8_t extra_lives);
void tile_mode2_update_health_fx(bool damage_flash_active, bool low_health);
void tile_mode2_set_boss_hud_visible(bool visible);
void tile_mode2_set_boss_health(uint8_t health);
void tile_mode2_clear_level_bonus(void);
void tile_mode2_begin_level_bonus(uint8_t level, uint8_t multiplier);
void tile_mode2_set_bonus_row(uint8_t enemy_type, uint16_t kills, uint16_t points_each, uint16_t subtotal);
void tile_mode2_set_bonus_boss_row(uint16_t boss_points);
void tile_mode2_set_bonus_pending_total(uint32_t pending_total);
int16_t tile_mode2_get_bonus_icon_target_x(void);
int16_t tile_mode2_get_bonus_icon_target_y(uint8_t enemy_type);
void tile_mode2_render_level_bonus(uint8_t level, const uint16_t *kills, uint8_t multiplier);
void tile_mode2_update_scroll(void);

#endif // TILE_MODE2_H
