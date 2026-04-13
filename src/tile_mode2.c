#include <rp6502.h>
#include <stdio.h>
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
#define SCORE_TILE_INDEX_BASE 19
#define HEALTH_FLASH_TOGGLE_FRAMES 3
#define BONUS_TABLE_X 10
#define BONUS_TABLE_Y 5
#define BONUS_TABLE_ROWS 19
#define BONUS_TABLE_WIDTH 26
#define BONUS_ROW_Y_START (BONUS_TABLE_Y + 2)
#define BONUS_ROW_Y_STEP 2
#define BONUS_TOTAL_Y (BONUS_TABLE_Y + 16)
#define BONUS_ICON_TILE_X BONUS_TABLE_X

#ifndef COLOR_FROM_RGB8
#define COLOR_FROM_RGB8(r,g,b) (((b>>3)<<11)|((g>>3)<<6)|(r>>3))
#endif

#ifndef COLOR_ALPHA_MASK
#define COLOR_ALPHA_MASK (1u<<5)
#endif

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
        puts("xreg_vga_mode failed");
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
        puts("xreg_vga_mode failed");
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
        puts("xreg_vga_mode failed");
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

    puts("Mode2 tiles ready");
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
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 5), row_y, 250);
        tile_mode2_write_two_digits((uint8_t)(BONUS_TABLE_X + 7), row_y, 0);
        tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 9), row_y, 251);
        tile_mode2_write_four_digits((uint8_t)(BONUS_TABLE_X + 11), row_y, 0);
    }

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
    tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 5), row_y, 250);
    tile_mode2_write_two_digits((uint8_t)(BONUS_TABLE_X + 7), row_y, points_each);
    tile_mode2_write_tile(STARFIELD_HUD_DATA, STARFIELD_HUD_WIDTH, (uint8_t)(BONUS_TABLE_X + 9), row_y, 251);
    tile_mode2_write_four_digits((uint8_t)(BONUS_TABLE_X + 11), row_y, subtotal);
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