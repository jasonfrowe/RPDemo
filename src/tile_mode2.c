#include <rp6502.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
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
static bool warp_tiles_replaced = false;

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
    warp_tiles_replaced = false;
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
    tile_mode2_set_score(0);

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

void tile_mode2_update_scroll(void) {
    if (gameplay_transition_active && fg_scroll_speed_half > fg_scroll_target_half) {
        fg_slowdown_tick = (uint8_t)(fg_slowdown_tick + 1);
        if (fg_slowdown_tick >= FG_SLOWDOWN_STEP_FRAMES) {
            fg_slowdown_tick = 0;
            if (fg_scroll_speed_half > (fg_scroll_target_half + FG_SLOWDOWN_STEP_HALF_PX)) {
                fg_scroll_speed_half = (uint8_t)(fg_scroll_speed_half - FG_SLOWDOWN_STEP_HALF_PX);
            } else {
                fg_scroll_speed_half = fg_scroll_target_half;
            }
        }
    }

    if (gameplay_transition_active && !warp_tiles_replaced && fg_scroll_speed_half == fg_scroll_target_half) {
        tile_mode2_replace_warp_tiles();
        warp_tiles_replaced = true;
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