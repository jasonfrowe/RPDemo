#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "player_controller.h"
#include "sprite_mode5.h"

// The setters change only these RAM copies. sprite_mode5_commit() writes them
// to XRAM right after VSYNC, so a sprite changes only between frames unless the
// pass before it overran.
static sprite_configs_t sprites;
static uint16_t player_colors[1 << 4];
static uint16_t enemy_colors[1 << 4];
static bool player_colors_dirty = false;
static bool enemy_colors_dirty = false;

static uint8_t engine_phase = 0;
static uint8_t engine_tick = 0;
static bool damage_flash_active = false;
static bool boss_palette_active = false;
static uint8_t boss_weakspot_flash_tick = 0;

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

static void sprite_mode5_write_palette_entry(uint8_t index, uint16_t color)
{
    if (player_colors[index] != color) {
        player_colors[index] = color;
        player_colors_dirty = true;
    }
}

static void sprite_mode5_write_enemy_palette_entry(uint8_t index, uint16_t color)
{
    if (enemy_colors[index] != color) {
        enemy_colors[index] = color;
        enemy_colors_dirty = true;
    }
}

void sprite_mode5_init(void) {
    static const mode5_sprite_t config = {
        .x_pos_px = (SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2,
        .y_pos_px = (SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3, // Start slightly lower than center for better composition
        .xram_sprite_ptr = XRAM_PLAYER_DATA,
        .palette_ptr = XRAM_PLAYER_PALETTE,
    };

    sprites.player = config;
    xram0_write(XRAM_PLAYER_CONFIG, &sprites.player, sizeof(sprites.player));
    xram0_read(player_colors, XRAM_PLAYER_PALETTE, sizeof(player_colors));
    player_engine_default_color = player_colors[PLAYER_ENGINE_PALETTE_INDEX];
    player_flash_default_color = player_colors[PLAYER_FLASH_PALETTE_INDEX];

    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_PLAYER_CONFIG, 1, 2, 0, 0);

    player_colors[PLAYER_ENGINE_PALETTE_INDEX] = 0x0000;
    xram0_write(XRAM_PLAYER_PALETTE, player_colors, sizeof(player_colors));
}

void sprite_mode5_init_projectiles(void) {
    static const mode5_sprite_t hidden = {
        .x_pos_px = SPRITE_OFFSCREEN_PX,
        .y_pos_px = SPRITE_OFFSCREEN_PX,
        .xram_sprite_ptr = XRAM_PROJECTILE_DATA,
        .palette_ptr = XRAM_PROJECTILE_PALETTE,
    };

    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
        sprites.projectile[i] = hidden;
    }
    xram0_write(XRAM_PROJECTILE_CONFIG, sprites.projectile, sizeof(sprites.projectile));

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
        sprites.enemy[i] = hidden;
    }
    xram0_write(XRAM_ENEMY_CONFIG, sprites.enemy, sizeof(sprites.enemy));

    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_ENEMY_CONFIG, MAX_ENEMIES, 1, HUD_TOP_PX, 0);

    xram0_read(enemy_colors, XRAM_ENEMY_PALETTE, sizeof(enemy_colors));
    boss_palette_active = false;
    boss_weakspot_flash_tick = 0;
    boss_weakspot_default_color = enemy_colors[BOSS_WEAKSPOT_PALETTE_INDEX];
}

void sprite_mode5_set_enemy(uint8_t slot, int16_t x, int16_t y, uint8_t type)
{
    mode5_sprite_t *sprite = &sprites.enemy[slot];

    sprite->x_pos_px = x;
    sprite->y_pos_px = y;
    sprite->xram_sprite_ptr = XRAM_ENEMY_DATA + type * ENEMY_FRAME_SIZE;
}

void sprite_mode5_set_projectile_position(uint8_t slot, int16_t x, int16_t y)
{
    sprites.projectile[slot].x_pos_px = x;
    sprites.projectile[slot].y_pos_px = y;
}

void sprite_mode5_set_projectile_frame(uint8_t slot, uint8_t frame_index)
{
    if (frame_index >= PROJECTILE_FRAME_COUNT) {
        frame_index = 0;
    }

    sprites.projectile[slot].xram_sprite_ptr = XRAM_PROJECTILE_DATA + frame_index * PROJECTILE_FRAME_SIZE;
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

    sprites.player.x_pos_px = x;
    sprites.player.y_pos_px = y;
}

void sprite_mode5_set_frame(uint8_t frame_index)
{
    if (frame_index >= PLAYER_FRAME_COUNT) {
        frame_index = 0;
    }

    sprites.player.xram_sprite_ptr = XRAM_PLAYER_DATA + frame_index * PLAYER_FRAME_SIZE;
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
    for (uint8_t i = BOSS_SPRITE_SLOT_FIRST; i < MAX_ENEMIES; ++i) {
        sprites.enemy[i].x_pos_px = SPRITE_OFFSCREEN_PX;
        sprites.enemy[i].y_pos_px = SPRITE_OFFSCREEN_PX;
    }
}

void sprite_mode5_hide_player(void)
{
    sprites.player.x_pos_px = SPRITE_OFFSCREEN_PX;
    sprites.player.y_pos_px = SPRITE_OFFSCREEN_PX;
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
    sprite_mode5_write_enemy_palette_entry(BOSS_WEAKSPOT_PALETTE_INDEX, target_color);
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

    sprite_mode5_write_enemy_palette_entry(BOSS_WEAKSPOT_PALETTE_INDEX, target_color);
}

void sprite_mode5_commit(void)
{
    if (player_colors_dirty) {
        player_colors_dirty = false;
        xram0_write(XRAM_PLAYER_PALETTE, player_colors, sizeof(player_colors));
    }
    if (enemy_colors_dirty) {
        enemy_colors_dirty = false;
        xram0_write(XRAM_ENEMY_PALETTE, enemy_colors, sizeof(enemy_colors));
    }
    xram0_write(XRAM_SPRITE_CONFIGS, &sprites, sizeof(sprites));
}
