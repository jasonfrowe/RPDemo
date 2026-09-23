#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "player_controller.h"
#include "sprite_mode5.h"

static uint8_t player_frame = 0;
static uint8_t engine_phase = 0;
static uint8_t engine_tick = 0;
static bool damage_flash_active = false;
static bool boss_palette_active = false;
static uint8_t boss_weakspot_flash_tick = 0;
static uint16_t boss_weakspot_current_color = 0;

#define PLAYER_ENGINE_PALETTE_INDEX 12
#define ENGINE_ANIM_TICK_FRAMES 4
#define BOSS_WEAKSPOT_PALETTE_INDEX 6
#define BOSS_WEAKSPOT_FIGHT_COLOR (COLOR_FROM_RGB5(31, 31, 10) | COLOR_ALPHA_MASK)
#define BOSS_WEAKSPOT_FLASH_RED (COLOR_FROM_RGB5(31, 0, 0) | COLOR_ALPHA_MASK)
#define BOSS_WEAKSPOT_FLASH_TOGGLE_FRAMES 3

static const uint16_t engine_colors[3] = {
    COLOR_FROM_RGB5(31, 10, 10) | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB5(31, 31, 10) | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB5(31, 31, 31) | COLOR_ALPHA_MASK,
};

static void sprite_mode5_write_palette_entry(uint8_t index, uint16_t color)
{
    RIA.addr0 = (unsigned)(XRAM_PLAYER_PALETTE + ((unsigned)index * sizeof(uint16_t)));
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

static void sprite_mode5_write_enemy_palette_entry(uint8_t index, uint16_t color)
{
    RIA.addr0 = (unsigned)(XRAM_ENEMY_PALETTE + ((unsigned)index * sizeof(uint16_t)));
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

void sprite_mode5_init(void) {
    int rc;
    int16_t center_x = (int16_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2);
    int16_t center_y = (int16_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3); // Start slightly lower than center for better composition

    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, center_x);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, y_pos_px, center_y);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, xram_sprite_ptr, XRAM_PLAYER_DATA);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, palette_ptr, XRAM_PLAYER_PALETTE);
    player_frame = 0;


    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_PLAYER_CONFIG, 1, 2, 0, 0) < 0) {
        return;
    }


    RIA.addr0 = XRAM_PLAYER_PALETTE;
    RIA.step0 = 1;
    for (int i = 0; i < (int)(sizeof(player_palette) / sizeof(player_palette[0])); i++) {
        RIA.rw0 = player_palette[i] & 0xFF;
        RIA.rw0 = player_palette[i] >> 8;
    }

    sprite_mode5_write_palette_entry(PLAYER_ENGINE_PALETTE_INDEX, 0x0000);
}

void sprite_mode5_init_projectiles(void) {
    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {

        unsigned ptr = XRAM_PROJECTILE_CONFIG + (i * sizeof(mode5_sprite_t));

        xram0_struct_set(ptr, mode5_sprite_t, x_pos_px, -32); // Start off-screen
        xram0_struct_set(ptr, mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, mode5_sprite_t, xram_sprite_ptr, XRAM_PROJECTILE_DATA);
        xram0_struct_set(ptr, mode5_sprite_t, palette_ptr, XRAM_PROJECTILE_PALETTE);
    }

    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode5(MODE5_4BPP | MODE5_8X8, XRAM_PROJECTILE_CONFIG, MAX_PROJECTILES, 0, HUD_TOP_PX, 0) < 0) {
        return;
    }

    RIA.addr0 = XRAM_PROJECTILE_PALETTE;
    RIA.step0 = 1;
    for (int i = 0; i < (int)(sizeof(projectiles_palette) / sizeof(projectiles_palette[0])); i++) {
        RIA.rw0 = projectiles_palette[i] & 0xFF;
        RIA.rw0 = projectiles_palette[i] >> 8;
    }
}

void sprite_mode5_init_enemies(void) {
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        unsigned ptr = XRAM_ENEMY_CONFIG + ((unsigned)i * sizeof(mode5_sprite_t));
        xram0_struct_set(ptr, mode5_sprite_t, x_pos_px, -32);
        xram0_struct_set(ptr, mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, mode5_sprite_t, xram_sprite_ptr, XRAM_ENEMY_DATA);
        xram0_struct_set(ptr, mode5_sprite_t, palette_ptr, XRAM_ENEMY_PALETTE);
    }

    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_ENEMY_CONFIG, MAX_ENEMIES, 1, HUD_TOP_PX, 0) < 0) {
        return;
    }

    RIA.addr0 = XRAM_ENEMY_PALETTE;
    RIA.step0 = 1;
    for (int i = 0; i < (int)(sizeof(enemy_palette) / sizeof(enemy_palette[0])); i++) {
        RIA.rw0 = enemy_palette[i] & 0xFF;
        RIA.rw0 = enemy_palette[i] >> 8;
    }

    boss_palette_active = false;
    boss_weakspot_flash_tick = 0;
    boss_weakspot_current_color = enemy_palette[BOSS_WEAKSPOT_PALETTE_INDEX];
}

void sprite_mode5_set_enemy(uint8_t slot, int16_t x, int16_t y, uint8_t type)
{
    unsigned ptr = XRAM_ENEMY_CONFIG + ((unsigned)slot * sizeof(mode5_sprite_t));
    xram0_struct_set(ptr, mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(ptr, mode5_sprite_t, y_pos_px, y);
    xram0_struct_set(ptr, mode5_sprite_t, xram_sprite_ptr,
        (XRAM_ENEMY_DATA + ((unsigned)type * ENEMY_FRAME_SIZE)));
}

void sprite_mode5_set_projectile_position(uint8_t slot, int16_t x, int16_t y)
{
    unsigned ptr = XRAM_PROJECTILE_CONFIG + ((unsigned)slot * sizeof(mode5_sprite_t));
    xram0_struct_set(ptr, mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(ptr, mode5_sprite_t, y_pos_px, y);
}

void sprite_mode5_set_projectile_frame(uint8_t slot, uint8_t frame_index)
{
    unsigned ptr = XRAM_PROJECTILE_CONFIG + ((unsigned)slot * sizeof(mode5_sprite_t));

    if (frame_index >= PROJECTILE_FRAME_COUNT) {
        frame_index = 0;
    }

    xram0_struct_set(
        ptr,
        mode5_sprite_t,
        xram_sprite_ptr,
        (XRAM_PROJECTILE_DATA + ((unsigned)frame_index * PROJECTILE_FRAME_SIZE))
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
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, y_pos_px, y);
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
        XRAM_PLAYER_CONFIG,
        mode5_sprite_t,
        xram_sprite_ptr,
        (XRAM_PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE))
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

void sprite_mode5_show_boss(int16_t x, int16_t y, uint8_t frame_set_base)
{
    for (uint8_t row = 0; row < BOSS_GRID_ROWS; ++row) {
        for (uint8_t col = 0; col < BOSS_GRID_COLS; ++col) {
            uint8_t tile = (uint8_t)(row * BOSS_GRID_COLS + col);
            sprite_mode5_set_enemy(
                (uint8_t)(BOSS_SPRITE_SLOT_FIRST + tile),
                (int16_t)(x + (int16_t)(col * ENEMY_SPRITE_SIZE_PX)),
                (int16_t)(y + (int16_t)(row * ENEMY_SPRITE_SIZE_PX)),
                (uint8_t)(frame_set_base + tile)
            );
        }
    }
}

void sprite_mode5_hide_boss(void)
{
    for (uint8_t i = 0; i < BOSS_SPRITE_COUNT; ++i) {
        unsigned ptr = XRAM_ENEMY_CONFIG + ((unsigned)(BOSS_SPRITE_SLOT_FIRST + i) * sizeof(mode5_sprite_t));
        xram0_struct_set(ptr, mode5_sprite_t, x_pos_px, -32);
        xram0_struct_set(ptr, mode5_sprite_t, y_pos_px, -32);
    }
}

void sprite_mode5_hide_player(void)
{
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, -32);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, y_pos_px, -32);
}

void sprite_mode5_show_player(void)
{
    int16_t x;
    int16_t y;

    player_controller_get_position(&x, &y);
    sprite_mode5_set_position(x, y);
}

void sprite_mode5_set_boss_palette_active(bool active)
{
    uint16_t target_color;

    if (boss_palette_active == active) {
        return;
    }

    boss_palette_active = active;
    boss_weakspot_flash_tick = 0;
    target_color = active ? BOSS_WEAKSPOT_FIGHT_COLOR : enemy_palette[BOSS_WEAKSPOT_PALETTE_INDEX];
    if (boss_weakspot_current_color != target_color) {
        boss_weakspot_current_color = target_color;
        sprite_mode5_write_enemy_palette_entry(BOSS_WEAKSPOT_PALETTE_INDEX, target_color);
    }
}

void sprite_mode5_set_boss_weakspot_flash(bool active)
{
    uint16_t target_color;

    if (!boss_palette_active) {
        return;
    }

    if (active) {
        boss_weakspot_flash_tick = (uint8_t)((boss_weakspot_flash_tick + 1) % (BOSS_WEAKSPOT_FLASH_TOGGLE_FRAMES * 2));
        if (boss_weakspot_flash_tick < BOSS_WEAKSPOT_FLASH_TOGGLE_FRAMES) {
            target_color = BOSS_WEAKSPOT_FLASH_RED;
        } else {
            target_color = BOSS_WEAKSPOT_FIGHT_COLOR;
        }
    } else {
        boss_weakspot_flash_tick = 0;
        target_color = BOSS_WEAKSPOT_FIGHT_COLOR;
    }

    if (boss_weakspot_current_color != target_color) {
        boss_weakspot_current_color = target_color;
        sprite_mode5_write_enemy_palette_entry(BOSS_WEAKSPOT_PALETTE_INDEX, target_color);
    }
}
