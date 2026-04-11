#include <rp6502.h>
#include <stdio.h>
#include <stdint.h>
#include "constants.h"
#include "sprite_mode5.h"

// Store the player config address for updates
unsigned PLAYER_CONFIG;

void sprite_mode5_init(void) {
    int rc;
    int16_t center_x = (int16_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2);
    int16_t center_y = (int16_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3); // Start slightly lower than center for better composition

    PLAYER_CONFIG = SPRITE_DATA_END; // Just after the end of sprite data

    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, center_x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, center_y);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr, PLAYER_DATA);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, palette_ptr, PLAYER_PALETTE_ADDR);


    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, PLAYER_CONFIG, 1, 1, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }


    RIA.addr0 = PLAYER_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_palette[i] & 0xFF;
        RIA.rw0 = player_palette[i] >> 8;
    }


    puts("Mode5 player sprite ready");
}

/**
 * Update sprite position on screen
 * Clamps position to screen bounds
 */
void sprite_mode5_set_position(int16_t x, int16_t y)
{
    // Clamp X to valid screen range (0 to SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX)
    if (x < 0) x = 0;
    if (x > (int16_t)(SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX)) {
        x = (int16_t)(SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX);
    }
    
    // Clamp Y to valid screen range (0 to SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)
    if (y < 0) y = 0;
    if (y > (int16_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)) {
        y = (int16_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX);
    }
    
    // Update sprite position in XRAM
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, y);
}