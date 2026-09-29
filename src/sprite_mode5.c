#include <rp6502.h>
#include <stddef.h>
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

// Colors from the palette assets, for the entries that change at run time.
static uint16_t player_engine_default_color;
static uint16_t player_flash_default_color;
static uint16_t boss_weakspot_default_color;

#define PLAYER_ENGINE_PALETTE_INDEX 12
#define PLAYER_FLASH_PALETTE_INDEX 15
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

static unsigned sprite_mode5_projectile_addr(uint8_t slot)
{
    return XRAM_PROJECTILE_CONFIG + slot * sizeof(mode5_sprite_t);
}

static unsigned sprite_mode5_enemy_addr(uint8_t slot)
{
    return XRAM_ENEMY_CONFIG + slot * sizeof(mode5_sprite_t);
}

static uint16_t sprite_mode5_read_palette_entry(uint8_t index)
{
    return xram0_peek16(XRAM_PLAYER_PALETTE + index * sizeof(uint16_t));
}

static void sprite_mode5_write_palette_entry(uint8_t index, uint16_t color)
{
    xram0_poke16(XRAM_PLAYER_PALETTE + index * sizeof(uint16_t), color);
}

static uint16_t sprite_mode5_read_enemy_palette_entry(uint8_t index)
{
    return xram0_peek16(XRAM_ENEMY_PALETTE + index * sizeof(uint16_t));
}

static void sprite_mode5_write_enemy_palette_entry(uint8_t index, uint16_t color)
{
    xram0_poke16(XRAM_ENEMY_PALETTE + index * sizeof(uint16_t), color);
}

void sprite_mode5_init(void) {
    static const mode5_sprite_t config = {
        .x_pos_px = (SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2,
        .y_pos_px = (SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3, // Start slightly lower than center for better composition
        .xram_sprite_ptr = XRAM_PLAYER_DATA,
        .palette_ptr = XRAM_PLAYER_PALETTE,
    };

    xram0_write(XRAM_PLAYER_CONFIG, &config, sizeof(config));
    player_frame = 0;
    player_engine_default_color = sprite_mode5_read_palette_entry(PLAYER_ENGINE_PALETTE_INDEX);
    player_flash_default_color = sprite_mode5_read_palette_entry(PLAYER_FLASH_PALETTE_INDEX);

    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_PLAYER_CONFIG, 1, 2, 0, 0);

    sprite_mode5_write_palette_entry(PLAYER_ENGINE_PALETTE_INDEX, 0x0000);
}

void sprite_mode5_init_projectiles(void) {
    static const mode5_sprite_t hidden = {
        .x_pos_px = SPRITE_OFFSCREEN_PX,
        .y_pos_px = SPRITE_OFFSCREEN_PX,
        .xram_sprite_ptr = XRAM_PROJECTILE_DATA,
        .palette_ptr = XRAM_PROJECTILE_PALETTE,
    };

    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
        xram0_write(sprite_mode5_projectile_addr(i), &hidden, sizeof(hidden));
    }

    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    xreg_vga_mode5(MODE5_4BPP | MODE5_8X8, XRAM_PROJECTILE_CONFIG, MAX_PROJECTILES, 0, HUD_TOP_PX, 0);
}

void sprite_mode5_init_enemies(void) {
    static const mode5_sprite_t hidden = {
        .x_pos_px = SPRITE_OFFSCREEN_PX,
        .y_pos_px = SPRITE_OFFSCREEN_PX,
        .xram_sprite_ptr = XRAM_ENEMY_DATA,
        .palette_ptr = XRAM_ENEMY_PALETTE,
    };

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        xram0_write(sprite_mode5_enemy_addr(i), &hidden, sizeof(hidden));
    }

    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_ENEMY_CONFIG, MAX_ENEMIES, 1, HUD_TOP_PX, 0);

    boss_palette_active = false;
    boss_weakspot_flash_tick = 0;
    boss_weakspot_default_color = sprite_mode5_read_enemy_palette_entry(BOSS_WEAKSPOT_PALETTE_INDEX);
    boss_weakspot_current_color = boss_weakspot_default_color;
}

void sprite_mode5_set_enemy(uint8_t slot, int16_t x, int16_t y, uint8_t type)
{
    unsigned addr = sprite_mode5_enemy_addr(slot);
    xram0_poke16(addr + offsetof(mode5_sprite_t, x_pos_px), x);
    xram0_poke16(addr + offsetof(mode5_sprite_t, y_pos_px), y);
    xram0_poke16(addr + offsetof(mode5_sprite_t, xram_sprite_ptr),
        XRAM_ENEMY_DATA + type * ENEMY_FRAME_SIZE);
}

void sprite_mode5_set_projectile_position(uint8_t slot, int16_t x, int16_t y)
{
    unsigned addr = sprite_mode5_projectile_addr(slot);
    xram0_poke16(addr + offsetof(mode5_sprite_t, x_pos_px), x);
    xram0_poke16(addr + offsetof(mode5_sprite_t, y_pos_px), y);
}

void sprite_mode5_set_projectile_frame(uint8_t slot, uint8_t frame_index)
{
    if (frame_index >= PROJECTILE_FRAME_COUNT) {
        frame_index = 0;
    }

    xram0_poke16(sprite_mode5_projectile_addr(slot) + offsetof(mode5_sprite_t, xram_sprite_ptr),
        XRAM_PROJECTILE_DATA + frame_index * PROJECTILE_FRAME_SIZE);
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

    xram0_poke16(XRAM_PLAYER_CONFIG + offsetof(mode5_sprite_t, x_pos_px), x);
    xram0_poke16(XRAM_PLAYER_CONFIG + offsetof(mode5_sprite_t, y_pos_px), y);
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
    xram0_poke16(XRAM_PLAYER_CONFIG + offsetof(mode5_sprite_t, xram_sprite_ptr),
        XRAM_PLAYER_DATA + frame_index * PLAYER_FRAME_SIZE);
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
    color = active ? player_engine_default_color : player_flash_default_color;
    sprite_mode5_write_palette_entry(PLAYER_FLASH_PALETTE_INDEX, color);
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
        unsigned addr = sprite_mode5_enemy_addr((uint8_t)(BOSS_SPRITE_SLOT_FIRST + i));
        xram0_poke16(addr + offsetof(mode5_sprite_t, x_pos_px), SPRITE_OFFSCREEN_PX);
        xram0_poke16(addr + offsetof(mode5_sprite_t, y_pos_px), SPRITE_OFFSCREEN_PX);
    }
}

void sprite_mode5_hide_player(void)
{
    xram0_poke16(XRAM_PLAYER_CONFIG + offsetof(mode5_sprite_t, x_pos_px), SPRITE_OFFSCREEN_PX);
    xram0_poke16(XRAM_PLAYER_CONFIG + offsetof(mode5_sprite_t, y_pos_px), SPRITE_OFFSCREEN_PX);
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
    target_color = active ? BOSS_WEAKSPOT_FIGHT_COLOR : boss_weakspot_default_color;
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
