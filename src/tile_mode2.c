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

static int16_t bg_scroll_y = 0;
static int16_t fg_scroll_y = 0;

#define TILE_SCROLL_WRAP_PX 480
#define BG_SCROLL_SPEED_PX 1
#define FG_SCROLL_SPEED_PX 4

void tile_mode2_init(void) {
    int rc;
    int16_t center_x = (int16_t)(0);
    int16_t center_y = (int16_t)(0);

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

    puts("Mode2 tiles ready");
}

void tile_mode2_update_scroll(void) {
    bg_scroll_y = (int16_t)(bg_scroll_y + BG_SCROLL_SPEED_PX);
    fg_scroll_y = (int16_t)(fg_scroll_y + FG_SCROLL_SPEED_PX);

    if (bg_scroll_y >= TILE_SCROLL_WRAP_PX) {
        bg_scroll_y = (int16_t)(bg_scroll_y - TILE_SCROLL_WRAP_PX);
    }
    if (fg_scroll_y >= TILE_SCROLL_WRAP_PX) {
        fg_scroll_y = (int16_t)(fg_scroll_y - TILE_SCROLL_WRAP_PX);
    }

    xram0_struct_set(TILE_BG_CONFIG, vga_mode2_config_t, y_pos_px, bg_scroll_y);
    xram0_struct_set(TILE_FG_CONFIG, vga_mode2_config_t, y_pos_px, fg_scroll_y);
}