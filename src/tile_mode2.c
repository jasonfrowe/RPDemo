#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include "constants.h"
#include "tile_mode2.h"
#include "sprite_mode5.h"


unsigned TILE_BG_CONFIG;
unsigned TILE_FG_CONFIG;
unsigned TILE_HUD_CONFIG;

static int16_t bg_scroll_y_half = 0;
static int16_t fg_scroll_y_half = 0;
static uint8_t bg_scroll_speed_half = 2;
static uint8_t fg_scroll_speed_half = 8;
static uint8_t fg_scroll_target_half = 8;
static uint8_t fg_slowdown_tick = 0;
static bool gameplay_transition_active = false;
static bool transition_to_gameplay = false;
static bool warp_tiles_replaced = false;
static bool warp_tile_backup_valid = false;
static uint8_t warp_tile_backup[5][32];
static uint16_t current_health_palette_color = 0;
static uint8_t health_flash_tick = 0;

#define TILE_SCROLL_WRAP_PX 480
#define TILE_SCROLL_WRAP_HALF_PX (TILE_SCROLL_WRAP_PX * 2)
#define BG_SCROLL_SPEED_HALF_PX 2
#define BG_GAME_SCROLL_SPEED_HALF_PX 1
#define FG_SCROLL_SPEED_HALF_PX 8
#define FG_GAME_SCROLL_SPEED_HALF_PX 2
#define FG_SLOWDOWN_STEP_HALF_PX 2
#define FG_SLOWDOWN_STEP_FRAMES 24
#define TITLE_RAINBOW_STEP_FRAMES 6
#define SCORE_DIGITS 6
#define SCORE_TILE_X 17
#define SCORE_TILE_Y 1
#define HISCORE_TEXT_X 13
#define HISCORE_TEXT_Y 17
#define HISCORE_VALUE_X 21
#define HISCORE_VALUE_Y 17
#define SCORE_TILE_INDEX_BASE 19
#define MULTIPLIER_TILE_X 0
#define MULTIPLIER_TILE_Y 28
#define HUD_SYMBOL_X_TILE_INDEX 225
#define HUD_SYMBOL_EQUALS_TILE_INDEX 226
#define PAUSED_TEXT "PAUSED"
#define PAUSED_TEXT_X 17
#define PAUSED_TEXT_Y 14
#define HUD_TEXT_YELLOW 0x57FF
#define LEVEL_TEXT_X 16
#define LEVEL_TEXT_Y 14
#define LEVEL_TEXT_LEN 8
#define LEVEL_COMPLETE_TEXT_X 13
#define LEVEL_COMPLETE_TEXT_Y 14
#define LEVEL_COMPLETE_TEXT_LEN 14
#define BONUS_CONTINUE_TEXT_X 14
#define BONUS_CONTINUE_TEXT_Y 25
#define BONUS_CONTINUE_TEXT_LEN 11
#define HEALTH_FLASH_TOGGLE_FRAMES 3
#define LIVES_ICON_TILE_INDEX 253
#define LIVES_SLOT_Y 1
#define LIVES_SLOT_0_X 26
#define LIVES_SLOT_1_X 28
#define LIVES_SLOT_2_X 30
#define LIVES_MAX_DISPLAY 3
#define SPEED_PICKUP_TILE_INDEX 254
#define SPEED_PICKUP_X_START 4
#define SPEED_PICKUP_Y 1
#define SPEED_PICKUP_MAX_DISPLAY 4
#define POWER_PICKUP_TILE_INDEX 255
#define POWER_PICKUP_X_START 9
#define POWER_PICKUP_Y 1
#define POWER_PICKUP_MAX_DISPLAY 4
#define BONUS_TABLE_X 10
#define BONUS_TABLE_Y 5
#define BONUS_TABLE_ROWS 19
#define BONUS_TABLE_WIDTH 26
#define BONUS_ROW_Y_START (BONUS_TABLE_Y + 2)
#define BONUS_ROW_Y_STEP 2
#define BOSS_LABEL_TEXT_LEN 4
#define BONUS_BOSS_ROW_Y (BONUS_ROW_Y_START + (ENEMY_TYPE_COUNT * BONUS_ROW_Y_STEP))
#define BONUS_TOTAL_Y (BONUS_TABLE_Y + 18)
#define BONUS_ICON_TILE_X BONUS_TABLE_X

#ifndef COLOR_FROM_RGB8
#define COLOR_FROM_RGB8(r,g,b) (((b>>3)<<11)|((g>>3)<<6)|(r>>3))
#endif

#ifndef COLOR_ALPHA_MASK
#define COLOR_ALPHA_MASK (1u<<5)
#endif

#define BOSS_HEALTH_TILE_FULL_INDEX 211
#define BOSS_HEALTH_TILE_EMPTY_INDEX 219
#define BOSS_LOW_HEALTH_THRESHOLD (BOSS_MAX_HEALTH / 3)

static uint8_t title_palette_tick = 0;
static uint8_t title_palette_phase = 0;

static const uint16_t title_rainbow_palette[] = {
    COLOR_FROM_RGB8(255, 0, 0)   | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(255, 128, 0) | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(255, 255, 0) | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(0, 255, 0)   | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(0, 255, 255) | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(0, 0, 255)   | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(255, 0, 255) | COLOR_ALPHA_MASK,
};

static const uint16_t hud_health_default_color = tile_hud_palette[10];
static const uint16_t hud_health_low_color = COLOR_FROM_RGB8(255, 24, 24) | COLOR_ALPHA_MASK;
static const uint16_t hud_health_flash_color = COLOR_FROM_RGB8(255, 255, 255) | COLOR_ALPHA_MASK;
static const uint16_t boss_health_good_color = COLOR_FROM_RGB8(32, 255, 32) | COLOR_ALPHA_MASK;
static const uint16_t boss_health_low_color = COLOR_FROM_RGB8(255, 32, 32) | COLOR_ALPHA_MASK;

static void tile_mode2_write_hud_palette_entry(uint8_t index, uint16_t color)
{
    RIA.addr0 = TILE_HUD_PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

static void tile_mode2_write_tile(unsigned tilemap_addr, uint8_t width, uint8_t x, uint8_t y, uint8_t tile_index)
{
    RIA.addr0 = tilemap_addr + ((unsigned)y * width) + x;
    RIA.step0 = 1;
    RIA.rw0 = tile_index;
}

static void tile_mode2_clear_hud_text(uint8_t x, uint8_t y, uint8_t len)
{
    for (uint8_t i = 0; i < len; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(x + i),
            y,
            0
        );
    }
}

static void tile_mode2_write_two_digits(uint8_t x, uint8_t y, uint16_t value)
{
    uint8_t tens;
    uint8_t ones;

    if (value > 99) {
        value = 99;
    }

    tens = (uint8_t)(value / 10u);
    ones = (uint8_t)(value % 10u);

    tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, x, y, (uint8_t)(SCORE_TILE_INDEX_BASE + tens));
    tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(x + 1), y, (uint8_t)(SCORE_TILE_INDEX_BASE + ones));
}

    static void tile_mode2_write_three_digits(uint8_t x, uint8_t y, uint16_t value)
    {
        uint8_t hundreds;
        uint8_t tens;
        uint8_t ones;

        if (value > 999) {
            value = 999;
        }

        hundreds = (uint8_t)(value / 100u);
        tens = (uint8_t)((value % 100u) / 10u);
        ones = (uint8_t)(value % 10u);

        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, x, y, (uint8_t)(SCORE_TILE_INDEX_BASE + hundreds));
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(x + 1), y, (uint8_t)(SCORE_TILE_INDEX_BASE + tens));
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(x + 2), y, (uint8_t)(SCORE_TILE_INDEX_BASE + ones));
    }

static void tile_mode2_write_four_digits(uint8_t x, uint8_t y, uint16_t value)
{
    uint16_t divisor = 1000;

    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t digit = (uint8_t)(value / divisor);
        value = (uint16_t)(value % divisor);
        divisor = (uint16_t)(divisor / 10u);
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(x + i), y, (uint8_t)(SCORE_TILE_INDEX_BASE + digit));
    }
}

static void tile_mode2_write_five_digits(uint8_t x, uint8_t y, uint32_t value)
{
    uint32_t divisor = 10000u;

    if (value > 99999u) {
        value = 99999u;
    }

    for (uint8_t i = 0; i < 5; ++i) {
        uint8_t digit = (uint8_t)(value / divisor);
        value = value % divisor;
        divisor /= 10u;
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(x + i), y, (uint8_t)(SCORE_TILE_INDEX_BASE + digit));
    }
}

static uint8_t tile_mode2_bonus_row_y(uint8_t enemy_type)
{
    return (uint8_t)(BONUS_ROW_Y_START + (enemy_type * BONUS_ROW_Y_STEP));
}

static void tile_mode2_clear_bonus_table_area(void)
{
    for (uint8_t y = BONUS_TABLE_Y; y < (BONUS_TABLE_Y + BONUS_TABLE_ROWS); ++y) {
        for (uint8_t x = BONUS_TABLE_X; x < (BONUS_TABLE_X + BONUS_TABLE_WIDTH); ++x) {
            tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, x, y, 0);
        }
    }
}

static void tile_mode2_clear_title_banner(void)
{
    for (uint8_t y = 4; y <= 16; ++y) {
        for (uint8_t x = 8; x <= 32; ++x) {
            tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, x, y, 0);
        }
    }
}

static void tile_mode2_copy_tile_bitmap(uint8_t dst_index, uint8_t src_index)
{
    uint8_t tile_data[32];
    unsigned src_addr = STARFIELD_TILES_DATA + ((unsigned)src_index * 32u);
    unsigned dst_addr = STARFIELD_TILES_DATA + ((unsigned)dst_index * 32u);

    RIA.addr0 = src_addr;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < sizeof(tile_data); ++i) {
        tile_data[i] = RIA.rw0;
    }

    RIA.addr0 = dst_addr;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < sizeof(tile_data); ++i) {
        RIA.rw0 = tile_data[i];
    }
}

static void tile_mode2_backup_warp_tiles(void)
{
    for (uint8_t t = 0; t < 5; ++t) {
        unsigned src_addr = STARFIELD_TILES_DATA + ((unsigned)(6 + t) * 32u);

        RIA.addr0 = src_addr;
        RIA.step0 = 1;
        for (uint8_t i = 0; i < 32; ++i) {
            warp_tile_backup[t][i] = RIA.rw0;
        }
    }
    warp_tile_backup_valid = true;
}

static void tile_mode2_restore_warp_tiles(void)
{
    if (!warp_tile_backup_valid) {
        return;
    }

    for (uint8_t t = 0; t < 5; ++t) {
        unsigned dst_addr = STARFIELD_TILES_DATA + ((unsigned)(6 + t) * 32u);

        RIA.addr0 = dst_addr;
        RIA.step0 = 1;
        for (uint8_t i = 0; i < 32; ++i) {
            RIA.rw0 = warp_tile_backup[t][i];
        }
    }
}

static void tile_mode2_replace_warp_tiles(void)
{
    for (uint8_t i = 0; i < 5; ++i) {
        tile_mode2_copy_tile_bitmap((uint8_t)(6 + i), (uint8_t)(220 + i));
    }
}

void tile_mode2_init(void) {
    int rc;
    int16_t center_x = (int16_t)(0);
    int16_t center_y = (int16_t)(0);

    bg_scroll_y_half = 0;
    fg_scroll_y_half = 0;
    bg_scroll_speed_half = BG_SCROLL_SPEED_HALF_PX;
    fg_scroll_speed_half = FG_SCROLL_SPEED_HALF_PX;
    fg_scroll_target_half = FG_SCROLL_SPEED_HALF_PX;
    fg_slowdown_tick = 0;
    gameplay_transition_active = false;
    transition_to_gameplay = false;
    warp_tiles_replaced = false;
    warp_tile_backup_valid = false;
    current_health_palette_color = 0;
    health_flash_tick = 0;
    title_palette_tick = 0;
    title_palette_phase = 0;

    TILE_BG_CONFIG = PLAYER_CONFIG + sizeof(vga_mode5_sprite_t); // Add after sprite config

    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, y_wrap, true);
    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, width_tiles,  STARFIELD_BG_WIDTH);
    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, height_tiles, STARFIELD_BG_HEIGHT);
    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, xram_data_ptr,    STARFIELD_BG_DATA); // tile ID grid
    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, xram_palette_ptr, TILE_BG_PALETTE_ADDR);
    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, xram_tile_ptr,    STARFIELD_TILES_DATA);        // tile bitmaps


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b0010 = 2
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x02, TILE_BG_CONFIG, 0, 24, 0) < 0) {
        return;
    }

    TILE_FG_CONFIG = TILE_BG_CONFIG + sizeof(vga_mode2_config_t); // Add after sprite config

    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, y_wrap, true);
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, width_tiles,  STARFIELD_FG_WIDTH);
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, height_tiles, STARFIELD_FG_HEIGHT);
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, xram_data_ptr, STARFIELD_FG_DATA); // tile ID grid
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, xram_palette_ptr, TILE_FG_PALETTE_ADDR);
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, xram_tile_ptr,    STARFIELD_TILES_DATA);        // tile bitmaps


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b0010 = 2
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x02, TILE_FG_CONFIG, 1, 24, 0) < 0) {
        return;
    }

    TILE_HUD_CONFIG = TILE_FG_CONFIG + sizeof(vga_mode2_config_t); // Add after sprite config

    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, y_wrap, true);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, width_tiles,  STARFIELD_HUD_WIDTH);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, height_tiles, STARFIELD_HUD_HEIGHT);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_data_ptr, STARFIELD_HUD_DATA); // tile ID grid
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_palette_ptr, TILE_HUD_PALETTE_ADDR);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_tile_ptr,    STARFIELD_TILES_DATA);        // tile bitmaps


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b0010 = 2
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x02, TILE_HUD_CONFIG, 2, 0, 0) < 0) {
        return;
    }


    RIA.addr0 = TILE_BG_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = tile_bg_palette[i] & 0xFF;
        RIA.rw0 = tile_bg_palette[i] >> 8;
    }

    RIA.addr0 = TILE_FG_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = tile_fg_palette[i] & 0xFF;
        RIA.rw0 = tile_fg_palette[i] >> 8;
    }

    RIA.addr0 = TILE_HUD_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = tile_hud_palette[i] & 0xFF;
        RIA.rw0 = tile_hud_palette[i] >> 8;
    }

    tile_mode2_write_hud_palette_entry(2, title_rainbow_palette[0]);
    tile_mode2_backup_warp_tiles();
    tile_mode2_set_score(0);
    tile_mode2_set_health(PLAYER_MAX_HEALTH);
    tile_mode2_update_health_fx(false, false);
}

void tile_mode2_set_score(uint32_t score)
{
    int8_t i;

    if (score > 999999u) {
        score = 999999u;
    }

        for (i = (SCORE_DIGITS - 1); i >= 0; --i) {
            uint8_t digit = (uint8_t)(score % 10u);
        score /= 10u;
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(SCORE_TILE_X + i),
            SCORE_TILE_Y,
            (uint8_t)(SCORE_TILE_INDEX_BASE + digit)
        );
    }
}

void tile_mode2_set_hiscore(uint32_t score)
{
    static const uint8_t hiscore_tiles[8] = {
        234, // H
        235, // I
        245, // S
        229, // C
        241, // O
        244, // R
        231, // E
        HUD_SYMBOL_EQUALS_TILE_INDEX,
    };
    int8_t i;

    if (score > 999999u) {
        score = 999999u;
    }

    // tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
    for (uint8_t t = 0; t < 8; ++t) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(HISCORE_TEXT_X + t),
            HISCORE_TEXT_Y,
            hiscore_tiles[t]
        );
    }

    for (i = (SCORE_DIGITS - 1); i >= 0; --i) {
        uint8_t digit = (uint8_t)(score % 10u);
        score /= 10u;
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(HISCORE_VALUE_X + i),
            HISCORE_VALUE_Y,
            (uint8_t)(SCORE_TILE_INDEX_BASE + digit)
        );
    }
}

void tile_mode2_set_multiplier(uint8_t multiplier)
{
    if (multiplier < 1u) {
        multiplier = 1u;
    }
    if (multiplier > 9u) {
        multiplier = 9u;
    }

    tile_mode2_write_tile(
        STARFIELD_HUD_DATA,
        STARFIELD_HUD_WIDTH,
        MULTIPLIER_TILE_X,
        MULTIPLIER_TILE_Y,
        (uint8_t)(SCORE_TILE_INDEX_BASE + multiplier)
    );
    tile_mode2_write_tile(
        STARFIELD_HUD_DATA,
        STARFIELD_HUD_WIDTH,
        (uint8_t)(MULTIPLIER_TILE_X + 1u),
        MULTIPLIER_TILE_Y,
        HUD_SYMBOL_X_TILE_INDEX
    );
}

void tile_mode2_set_paused_banner(bool visible)
{
    static const uint8_t paused_tiles[] = {
        242, // P
        227, // A
        247, // U
        245, // S
        231, // E
        230, // D
    };
    static const uint8_t paused_len = (uint8_t)(sizeof(paused_tiles) / sizeof(paused_tiles[0]));

    if (visible) {
        tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
        for (uint8_t i = 0; i < paused_len; ++i) {
            tile_mode2_write_tile(
                STARFIELD_HUD_DATA,
                STARFIELD_HUD_WIDTH,
                (uint8_t)(PAUSED_TEXT_X + i),
                PAUSED_TEXT_Y,
                paused_tiles[i]
            );
        }
        return;
    }

    for (uint8_t i = 0; i < paused_len; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(PAUSED_TEXT_X + i),
            PAUSED_TEXT_Y,
            0
        );
    }
}

void tile_mode2_set_level_banner(uint8_t level, bool visible)
{
    uint8_t level_tiles[LEVEL_TEXT_LEN] = {
        238, // L
        231, // E
        248, // V
        231, // E
        238, // L
        0,   // space
        19,  // 0
        19,  // 0
    };

    if (!visible) {
        tile_mode2_clear_hud_text(LEVEL_TEXT_X, LEVEL_TEXT_Y, LEVEL_TEXT_LEN);
        return;
    }

    if (level > 99) {
        level = 99;
    }

    level_tiles[6] = (uint8_t)(SCORE_TILE_INDEX_BASE + (level / 10u));
    level_tiles[7] = (uint8_t)(SCORE_TILE_INDEX_BASE + (level % 10u));

    tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
    for (uint8_t i = 0; i < LEVEL_TEXT_LEN; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(LEVEL_TEXT_X + i),
            LEVEL_TEXT_Y,
            level_tiles[i]
        );
    }
}

void tile_mode2_set_end_banner(bool victory)
{
    static const uint8_t you_win_tiles[7] = {
        251, // Y
        241, // O
        247, // U
        0,   // space
        249, // W
        235, // I
        240, // N
    };

    uint8_t x = 16u;
    uint8_t y = 14u;
    uint8_t len = 7u;

    tile_mode2_clear_hud_text(15u, y, 10u);
    if (!victory) {
        return;
    }

    tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);

    for (uint8_t i = 0; i < len; ++i) {
        uint8_t tile_index = you_win_tiles[i];
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(x + i), y, tile_index);
    }
}

void tile_mode2_set_level_complete_banner(bool visible)
{
    static const uint8_t level_complete_tiles[LEVEL_COMPLETE_TEXT_LEN] = {
        238, // L
        231, // E
        248, // V
        231, // E
        238, // L
        0,   // space
        229, // C
        241, // O
        239, // M
        242, // P
        238, // L
        231, // E
        246, // T
        231, // E
    };

    if (!visible) {
        tile_mode2_clear_hud_text(LEVEL_COMPLETE_TEXT_X, LEVEL_COMPLETE_TEXT_Y, LEVEL_COMPLETE_TEXT_LEN);
        return;
    }

    tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
    for (uint8_t i = 0; i < LEVEL_COMPLETE_TEXT_LEN; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(LEVEL_COMPLETE_TEXT_X + i),
            LEVEL_COMPLETE_TEXT_Y,
            level_complete_tiles[i]
        );
    }
}

void tile_mode2_set_level_failed_banner(bool visible)
{
    static const uint8_t level_failed_tiles[12] = {
        238, // L
        231, // E
        248, // V
        231, // E
        238, // L
        0,   // space
        232, // F
        227, // A
        235, // I
        238, // L
        231, // E
        230, // D
    };
    const uint8_t x = 14u;
    const uint8_t y = 14u;

    if (!visible) {
        tile_mode2_clear_hud_text(x, y, 12u);
        return;
    }

    tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
    for (uint8_t i = 0; i < 12u; ++i) {
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(x + i), y, level_failed_tiles[i]);
    }
}

void tile_mode2_set_bonus_continue_prompt(bool visible)
{
    static const uint8_t press_start_tiles[BONUS_CONTINUE_TEXT_LEN] = {
        242, // P
        244, // R
        231, // E
        245, // S
        245, // S
        0,   // space
        245, // S
        246, // T
        227, // A
        244, // R
        246, // T
    };

    if (!visible) {
        tile_mode2_clear_hud_text(BONUS_CONTINUE_TEXT_X, BONUS_CONTINUE_TEXT_Y, BONUS_CONTINUE_TEXT_LEN);
        return;
    }

    tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
    for (uint8_t i = 0; i < BONUS_CONTINUE_TEXT_LEN; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(BONUS_CONTINUE_TEXT_X + i),
            BONUS_CONTINUE_TEXT_Y,
            press_start_tiles[i]
        );
    }
}

void tile_mode2_set_push_start_prompt(bool visible)
{
    static const uint8_t push_start_tiles[10] = {
        242, // P
        247, // U
        245, // S
        234, // H
        0,   // space
        245, // S
        246, // T
        227, // A
        244, // R
        246, // T
    };
    const uint8_t x = 15u;
    const uint8_t y = BONUS_CONTINUE_TEXT_Y;

    if (!visible) {
        tile_mode2_clear_hud_text(x, y, 10u);
        return;
    }

    tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
    for (uint8_t i = 0; i < 10u; ++i) {
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(x + i), y, push_start_tiles[i]);
    }
}

void tile_mode2_start_gameplay_transition(void)
{
    tile_mode2_clear_title_banner();
    bg_scroll_speed_half = BG_GAME_SCROLL_SPEED_HALF_PX;
    fg_scroll_target_half = FG_GAME_SCROLL_SPEED_HALF_PX;
    fg_slowdown_tick = 0;
    gameplay_transition_active = true;
    transition_to_gameplay = true;
}

void tile_mode2_start_game_over_transition(void)
{
    bg_scroll_speed_half = BG_SCROLL_SPEED_HALF_PX;
    fg_scroll_target_half = FG_SCROLL_SPEED_HALF_PX;
    fg_slowdown_tick = 0;
    gameplay_transition_active = true;
    transition_to_gameplay = false;
    if (warp_tiles_replaced) {
        tile_mode2_restore_warp_tiles();
        warp_tiles_replaced = false;
    }
}

void tile_mode2_start_level_bonus_transition(void)
{
    bg_scroll_speed_half = BG_SCROLL_SPEED_HALF_PX;
    fg_scroll_target_half = FG_SCROLL_SPEED_HALF_PX;
    fg_slowdown_tick = 0;
    gameplay_transition_active = true;
    transition_to_gameplay = false;
    if (warp_tiles_replaced) {
        tile_mode2_restore_warp_tiles();
        warp_tiles_replaced = false;
    }
}

bool tile_mode2_restore_hud_from_rom(void)
{
    int fd;
    int total;
    static uint8_t hud_data[STARFIELD_HUD_SIZE];

    fd = open("ROM:StarFields_HUD_map.bin", O_RDONLY);
    if (fd < 0) {
        return false;
    }

    total = read(fd, hud_data, STARFIELD_HUD_SIZE);
    close(fd);

    if (total != STARFIELD_HUD_SIZE) {
        return false;
    }

    RIA.addr0 = STARFIELD_HUD_DATA;
    RIA.step0 = 1;
    for (unsigned i = 0; i < STARFIELD_HUD_SIZE; ++i) {
        RIA.rw0 = hud_data[i];
    }

    tile_mode2_write_hud_palette_entry(2, title_rainbow_palette[title_palette_phase]);
    return true;
}

void tile_mode2_update_title_palette(void)
{
    title_palette_tick = (uint8_t)(title_palette_tick + 1);
    if (title_palette_tick < TITLE_RAINBOW_STEP_FRAMES) {
        return;
    }

    title_palette_tick = 0;
    title_palette_phase = (uint8_t)((title_palette_phase + 1) %
        (sizeof(title_rainbow_palette) / sizeof(title_rainbow_palette[0])));

    tile_mode2_write_hud_palette_entry(2, title_rainbow_palette[title_palette_phase]);
}

void tile_mode2_set_health(uint8_t health)
{
    for (uint8_t i = 0; i < HEALTH_BAR_TILE_COUNT; ++i) {
        int16_t segment_health = (int16_t)health - (int16_t)(i * HEALTH_PER_BAR_TILE);
        uint8_t fill;
        uint8_t tile_index;

        if (segment_health <= 0) {
            fill = 0;
        } else if (segment_health >= HEALTH_PER_BAR_TILE) {
            fill = HEALTH_PER_BAR_TILE;
        } else {
            fill = (uint8_t)segment_health;
        }

        tile_index = (uint8_t)(HEALTH_BAR_TILE_EMPTY_INDEX - fill);
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(HEALTH_BAR_TILE_X + i),
            HEALTH_BAR_TILE_Y,
            tile_index
        );
    }
}

void tile_mode2_set_lives(uint8_t extra_lives)
{
    uint8_t clamped_lives = extra_lives;

    if (clamped_lives > LIVES_MAX_DISPLAY) {
        clamped_lives = LIVES_MAX_DISPLAY;
    }

    tile_mode2_write_tile(
        STARFIELD_HUD_DATA,
        STARFIELD_HUD_WIDTH,
        LIVES_SLOT_0_X,
        LIVES_SLOT_Y,
        (clamped_lives >= 1u) ? LIVES_ICON_TILE_INDEX : 0
    );

    tile_mode2_write_tile(
        STARFIELD_HUD_DATA,
        STARFIELD_HUD_WIDTH,
        LIVES_SLOT_1_X,
        LIVES_SLOT_Y,
        (clamped_lives >= 2u) ? LIVES_ICON_TILE_INDEX : 0
    );

    tile_mode2_write_tile(
        STARFIELD_HUD_DATA,
        STARFIELD_HUD_WIDTH,
        LIVES_SLOT_2_X,
        LIVES_SLOT_Y,
        (clamped_lives >= 3u) ? LIVES_ICON_TILE_INDEX : 0
    );
}

void tile_mode2_set_speed_pickups(uint8_t count)
{
    uint8_t clamped = count;

    if (clamped > SPEED_PICKUP_MAX_DISPLAY) {
        clamped = SPEED_PICKUP_MAX_DISPLAY;
    }

    for (uint8_t i = 0; i < SPEED_PICKUP_MAX_DISPLAY; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(SPEED_PICKUP_X_START + i),
            SPEED_PICKUP_Y,
            (i < clamped) ? SPEED_PICKUP_TILE_INDEX : 0
        );
    }
}

void tile_mode2_set_power_pickups(uint8_t count)
{
    uint8_t clamped = count;

    if (clamped > POWER_PICKUP_MAX_DISPLAY) {
        clamped = POWER_PICKUP_MAX_DISPLAY;
    }

    for (uint8_t i = 0; i < POWER_PICKUP_MAX_DISPLAY; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(POWER_PICKUP_X_START + i),
            POWER_PICKUP_Y,
            (i < clamped) ? POWER_PICKUP_TILE_INDEX : 0
        );
    }
}

void tile_mode2_update_health_fx(bool damage_flash_active, bool low_health)
{
    uint16_t desired_color;

    if (damage_flash_active) {
        health_flash_tick = (uint8_t)((health_flash_tick + 1) % (HEALTH_FLASH_TOGGLE_FRAMES * 2));
        if (health_flash_tick < HEALTH_FLASH_TOGGLE_FRAMES) {
            desired_color = hud_health_flash_color;
        } else if (low_health) {
            desired_color = hud_health_low_color;
        } else {
            desired_color = hud_health_default_color;
        }
    } else {
        health_flash_tick = 0;
        desired_color = low_health ? hud_health_low_color : hud_health_default_color;
    }

    if (desired_color != current_health_palette_color) {
        current_health_palette_color = desired_color;
        tile_mode2_write_hud_palette_entry(10, desired_color);
    }
}

void tile_mode2_set_boss_hud_visible(bool visible)
{
    static const uint8_t boss_tiles[BOSS_LABEL_TEXT_LEN] = {
        228, // B
        241, // O
        245, // S
        245, // S
    };

    if (!visible) {
        tile_mode2_clear_hud_text(BOSS_HUD_LABEL_X, BOSS_HUD_LABEL_Y, BOSS_LABEL_TEXT_LEN);
        tile_mode2_clear_hud_text(BOSS_HUD_HEALTH_X, BOSS_HUD_HEALTH_Y, HEALTH_BAR_TILE_COUNT);
        tile_mode2_write_hud_palette_entry(12, tile_hud_palette[12]);
        return;
    }

    tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
    for (uint8_t i = 0; i < BOSS_LABEL_TEXT_LEN; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(BOSS_HUD_LABEL_X + i),
            BOSS_HUD_LABEL_Y,
            boss_tiles[i]
        );
    }
}

void tile_mode2_set_boss_health(uint8_t health)
{
    uint16_t boss_color = (health <= BOSS_LOW_HEALTH_THRESHOLD) ? boss_health_low_color : boss_health_good_color;

    tile_mode2_write_hud_palette_entry(12, boss_color);

    for (uint8_t i = 0; i < HEALTH_BAR_TILE_COUNT; ++i) {
        int16_t segment_health = (int16_t)health - (int16_t)(i * HEALTH_PER_BAR_TILE);
        uint8_t fill;
        uint8_t tile_index;

        if (segment_health <= 0) {
            fill = 0;
        } else if (segment_health >= HEALTH_PER_BAR_TILE) {
            fill = HEALTH_PER_BAR_TILE;
        } else {
            fill = (uint8_t)segment_health;
        }

        tile_index = (uint8_t)(BOSS_HEALTH_TILE_EMPTY_INDEX - fill);
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(BOSS_HUD_HEALTH_X + i),
            BOSS_HUD_HEALTH_Y,
            tile_index
        );
    }
}

void tile_mode2_clear_level_bonus(void)
{
    tile_mode2_clear_bonus_table_area();
}

void tile_mode2_begin_level_bonus(uint8_t level, uint8_t multiplier)
{
    tile_mode2_clear_level_bonus();

    tile_mode2_write_two_digits((uint8_t)(BONUS_TABLE_X + 2), BONUS_TABLE_Y, level);
    tile_mode2_write_two_digits((uint8_t)(BONUS_TABLE_X + 7), BONUS_TABLE_Y, multiplier);

    for (uint8_t type = 0; type < ENEMY_TYPE_COUNT; ++type) {
        uint8_t row_y = tile_mode2_bonus_row_y(type);
        tile_mode2_write_two_digits((uint8_t)(BONUS_TABLE_X + 3), row_y, 0);
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 5), row_y, HUD_SYMBOL_X_TILE_INDEX);
        tile_mode2_write_three_digits((uint8_t)(BONUS_TABLE_X + 6), row_y, 0);
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 9), row_y, HUD_SYMBOL_EQUALS_TILE_INDEX);
        tile_mode2_write_four_digits((uint8_t)(BONUS_TABLE_X + 11), row_y, 0);
    }

    tile_mode2_set_bonus_boss_row(0);

    tile_mode2_set_bonus_pending_total(0);
}

void tile_mode2_set_bonus_row(uint8_t enemy_type, uint16_t kills, uint16_t points_each, uint16_t subtotal)
{
    uint8_t row_y;

    if (enemy_type >= ENEMY_TYPE_COUNT) {
        return;
    }

    row_y = tile_mode2_bonus_row_y(enemy_type);

    tile_mode2_write_two_digits((uint8_t)(BONUS_TABLE_X + 3), row_y, kills);
    tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 5), row_y, HUD_SYMBOL_X_TILE_INDEX);
    tile_mode2_write_three_digits((uint8_t)(BONUS_TABLE_X + 6), row_y, points_each);
    tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 9), row_y, HUD_SYMBOL_EQUALS_TILE_INDEX);
    tile_mode2_write_four_digits((uint8_t)(BONUS_TABLE_X + 11), row_y, subtotal);
}

void tile_mode2_set_bonus_boss_row(uint16_t boss_points)
{
    static const uint8_t boss_tiles[4] = {
        228, // B
        241, // O
        245, // S
        245, // S
    };

    tile_mode2_write_hud_palette_entry(2, HUD_TEXT_YELLOW);
    for (uint8_t i = 0; i < 4; ++i) {
        tile_mode2_write_tile(
            STARFIELD_HUD_DATA,
            STARFIELD_HUD_WIDTH,
            (uint8_t)(BONUS_TABLE_X + i),
            BONUS_BOSS_ROW_Y,
            boss_tiles[i]
        );
    }

    tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 4), BONUS_BOSS_ROW_Y, 0);
    tile_mode2_write_five_digits((uint8_t)(BONUS_TABLE_X + 10), BONUS_BOSS_ROW_Y, boss_points);
}

void tile_mode2_set_bonus_pending_total(uint32_t pending_total)
{
    tile_mode2_write_five_digits((uint8_t)(BONUS_TABLE_X + 10), BONUS_TOTAL_Y, pending_total);
}

int16_t tile_mode2_get_bonus_icon_target_x(void)
{
    return (int16_t)(BONUS_ICON_TILE_X * 8);
}

int16_t tile_mode2_get_bonus_icon_target_y(uint8_t enemy_type)
{
    if (enemy_type >= ENEMY_TYPE_COUNT) {
        enemy_type = 0;
    }

    return (int16_t)(tile_mode2_bonus_row_y(enemy_type) * 8);
}

void tile_mode2_render_level_bonus(uint8_t level, const uint16_t *kills, uint8_t multiplier)
{
    uint32_t total_bonus = 0;

    tile_mode2_begin_level_bonus(level, multiplier);

    for (uint8_t type = 0; type < ENEMY_TYPE_COUNT; ++type) {
        uint16_t base_points = (uint16_t)(10u + ((uint16_t)type * 5u));
        uint16_t points_each;
        uint16_t subtotal;

        if (base_points > 40u) {
            base_points = 40u;
        }

        points_each = (uint16_t)(base_points * multiplier);
        subtotal = (uint16_t)(kills[type] * points_each);
        total_bonus += subtotal;

        tile_mode2_set_bonus_row(type, kills[type], points_each, subtotal);
    }

    tile_mode2_set_bonus_pending_total(total_bonus);
}

void tile_mode2_update_scroll(void) {
    if (gameplay_transition_active && fg_scroll_speed_half != fg_scroll_target_half) {
        fg_slowdown_tick = (uint8_t)(fg_slowdown_tick + 1);
        if (fg_slowdown_tick >= FG_SLOWDOWN_STEP_FRAMES) {
            fg_slowdown_tick = 0;
            if (fg_scroll_speed_half > fg_scroll_target_half) {
                if (fg_scroll_speed_half > (fg_scroll_target_half + FG_SLOWDOWN_STEP_HALF_PX)) {
                    fg_scroll_speed_half = (uint8_t)(fg_scroll_speed_half - FG_SLOWDOWN_STEP_HALF_PX);
                } else {
                    fg_scroll_speed_half = fg_scroll_target_half;
                }
            } else {
                uint8_t next_speed = (uint8_t)(fg_scroll_speed_half + FG_SLOWDOWN_STEP_HALF_PX);
                if (next_speed < fg_scroll_target_half) {
                    fg_scroll_speed_half = next_speed;
                } else {
                    fg_scroll_speed_half = fg_scroll_target_half;
                }
            }
        }
    }

    if (gameplay_transition_active && transition_to_gameplay && !warp_tiles_replaced && fg_scroll_speed_half == fg_scroll_target_half) {
        tile_mode2_replace_warp_tiles();
        warp_tiles_replaced = true;
        gameplay_transition_active = false;
    } else if (gameplay_transition_active && !transition_to_gameplay && fg_scroll_speed_half == fg_scroll_target_half) {
        gameplay_transition_active = false;
    }

    bg_scroll_y_half = (int16_t)(bg_scroll_y_half + bg_scroll_speed_half);
    fg_scroll_y_half = (int16_t)(fg_scroll_y_half + fg_scroll_speed_half);

    if (bg_scroll_y_half >= TILE_SCROLL_WRAP_HALF_PX) {
        bg_scroll_y_half = (int16_t)(bg_scroll_y_half - TILE_SCROLL_WRAP_HALF_PX);
    }
    if (fg_scroll_y_half >= TILE_SCROLL_WRAP_HALF_PX) {
        fg_scroll_y_half = (int16_t)(fg_scroll_y_half - TILE_SCROLL_WRAP_HALF_PX);
    }

    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, y_pos_px, (int16_t)(bg_scroll_y_half / 2));
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, y_pos_px, (int16_t)(fg_scroll_y_half / 2));
}