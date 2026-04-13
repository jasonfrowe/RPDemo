#include <rp6502.h>
#include <stdio.h>
#include <stdint.h>
#include "constants.h"
#include "player_controller.h"
#include "sprite_mode5.h"

// Store the player config address for updates
unsigned PLAYER_CONFIG;
unsigned PROJECTILE_CONFIG;
unsigned ENEMY_CONFIG;

static uint8_t player_frame = 0;
static uint8_t engine_phase = 0;
static uint8_t engine_tick = 0;
static bool damage_flash_active = false;

#define PLAYER_ENGINE_PALETTE_INDEX 12
#define ENGINE_ANIM_TICK_FRAMES 4

static const uint16_t engine_colors[3] = {
    0x52BF,
    0x57FF,
    0xFFFF,
};

static void sprite_mode5_write_palette_entry(uint8_t index, uint16_t color)
{
    RIA.addr0 = (unsigned)(PLAYER_PALETTE_ADDR + ((unsigned)index * 2u));
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

void sprite_mode5_init(void) {
    int rc;
    int16_t center_x = (int16_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2);
    int16_t center_y = (int16_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3); // Start slightly lower than center for better composition

    PLAYER_CONFIG = SPRITE_DATA_END; // Just after the end of sprite data

    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, center_x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, center_y);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr, PLAYER_DATA);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, palette_ptr, PLAYER_PALETTE_ADDR);
    player_frame = 0;


    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, PLAYER_CONFIG, 1, 2, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }


    RIA.addr0 = PLAYER_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_palette[i] & 0xFF;
        RIA.rw0 = player_palette[i] >> 8;
    }

    sprite_mode5_write_palette_entry(PLAYER_ENGINE_PALETTE_INDEX, 0x0000);

    puts("Mode5 player sprite ready");
}

void sprite_mode5_init_projectiles(void) {
    PROJECTILE_CONFIG = TILE_HUD_CONFIG + sizeof(vga_mode2_config_t); // Just after tile HUD config

    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {

        unsigned ptr = PROJECTILE_CONFIG + (i * sizeof(vga_mode5_sprite_t));

        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32); // Start off-screen
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, PROJECTILE_DATA);
        xram0_struct_set(ptr, vga_mode5_sprite_t, palette_ptr, PROJECTILE_PALETTE_ADDR);
    }

    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x02, PROJECTILE_CONFIG, MAX_PROJECTILES, 1, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    RIA.addr0 = PROJECTILE_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = projectiles_palette[i] & 0xFF;
        RIA.rw0 = projectiles_palette[i] >> 8;
    }

    puts("Mode5 projectile sprites ready");
}

void sprite_mode5_init_enemies(void) {
    ENEMY_CONFIG = PROJECTILE_CONFIG + (MAX_PROJECTILES * sizeof(vga_mode5_sprite_t));

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        unsigned ptr = ENEMY_CONFIG + ((unsigned)i * sizeof(vga_mode5_sprite_t));
        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, ENEMY_DATA);
        xram0_struct_set(ptr, vga_mode5_sprite_t, palette_ptr, ENEMY_PALETTE_ADDR);
    }

    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    // OPTIONS 0x0A = 16x16 sprites, 4bpp
    if (xreg_vga_mode(5, 0x0A, ENEMY_CONFIG, MAX_ENEMIES, 0, 24, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    RIA.addr0 = ENEMY_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = enemy_palette[i] & 0xFF;
        RIA.rw0 = enemy_palette[i] >> 8;
    }

    puts("Mode5 enemy sprites ready");
}

void sprite_mode5_set_enemy(uint8_t slot, int16_t x, int16_t y, uint8_t type)
{
    unsigned ptr = ENEMY_CONFIG + ((unsigned)slot * sizeof(vga_mode5_sprite_t));
    xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, y);
    xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr,
        (ENEMY_DATA + ((unsigned)type * ENEMY_FRAME_SIZE)));
}

void sprite_mode5_set_projectile_position(uint8_t slot, int16_t x, int16_t y)
{
    unsigned ptr = PROJECTILE_CONFIG + ((unsigned)slot * sizeof(vga_mode5_sprite_t));
    xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, y);
}

void sprite_mode5_set_projectile_frame(uint8_t slot, uint8_t frame_index)
{
    unsigned ptr = PROJECTILE_CONFIG + ((unsigned)slot * sizeof(vga_mode5_sprite_t));

    if (frame_index >= PROJECTILE_FRAME_COUNT) {
        frame_index = 0;
    }

    xram0_struct_set(
        ptr,
        vga_mode5_sprite_t,
        xram_sprite_ptr,
        (PROJECTILE_DATA + ((unsigned)frame_index * PROJECTILE_FRAME_SIZE))
    );
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
    
    // Clamp Y to valid play area (HUD_TOP_PX to SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)
    if (y < HUD_TOP_PX) y = HUD_TOP_PX;
    if (y > (int16_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)) {
        y = (int16_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX);
    }
    
    // Update sprite position in XRAM
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, y);
}

void sprite_mode5_set_frame(uint8_t frame_index)
{
    if (frame_index >= PLAYER_FRAME_COUNT) {
        frame_index = 0;
    }
    if (frame_index == player_frame) {
        return;
    }

    player_frame = frame_index;
    xram0_struct_set(
        PLAYER_CONFIG,
        vga_mode5_sprite_t,
        xram_sprite_ptr,
        (PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE))
    );
}

void sprite_mode5_update_engine(bool moving_down)
{
    if (moving_down) {
        engine_tick = 0;
        sprite_mode5_write_palette_entry(PLAYER_ENGINE_PALETTE_INDEX, 0x0000);
        return;
    }

    if (engine_tick == 0) {
        sprite_mode5_write_palette_entry(PLAYER_ENGINE_PALETTE_INDEX, engine_colors[engine_phase]);
        engine_phase = (uint8_t)((engine_phase + 1) % 3);
    }

    engine_tick = (uint8_t)((engine_tick + 1) % ENGINE_ANIM_TICK_FRAMES);
}

void sprite_mode5_set_damage_flash(bool active)
{
    uint16_t color;

    if (damage_flash_active == active) {
        return;
    }

    damage_flash_active = active;
    color = active ? player_palette[12] : player_palette[15];
    sprite_mode5_write_palette_entry(15, color);
}

void sprite_mode5_hide_player(void)
{
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, -32);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, -32);
}

void sprite_mode5_show_player(void)
{
    int16_t x;
    int16_t y;

    player_controller_get_position(&x, &y);
    sprite_mode5_set_position(x, y);
}
