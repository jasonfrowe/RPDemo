#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "xram.h"
#include "tile_mode2.h"

static int16_t bg_scroll_y_half = 0;
static int16_t fg_scroll_y_half = 0;
static uint8_t bg_scroll_speed_half = 2;
static uint8_t fg_scroll_speed_half = 8;
static uint8_t fg_scroll_target_half = 8;
static uint8_t fg_slowdown_tick = 0;
static bool gameplay_transition_active = false;
static bool transition_to_gameplay = false;
static bool warp_tiles_replaced = false;
static uint16_t current_health_palette_color = 0;
static uint8_t health_flash_tick = 0;
static uint8_t lives_flash_slot = 0;
static bool lives_flash_steady_is_icon = false;
static uint16_t lives_flash_timer = 0;
static uint8_t lives_flash_tick = 0;

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
#define PAUSED_TEXT_X 17
#define PAUSED_TEXT_Y 14
#define HUD_TEXT_YELLOW (COLOR_FROM_RGB5(31, 31, 10) | COLOR_ALPHA_MASK)
#define HUD_TEXT_PALETTE_INDEX 2
#define HUD_HEALTH_PALETTE_INDEX 10
#define HUD_BOSS_HEALTH_PALETTE_INDEX 12
#define LEVEL_TEXT_X 16
#define LEVEL_TEXT_Y 14
#define LEVEL_TEXT_LEN 8
#define LEVEL_COMPLETE_TEXT_X 13
#define LEVEL_COMPLETE_TEXT_Y 14
#define LEVEL_COMPLETE_TEXT_LEN 14
#define PRESS_BUTTON_TEXT_X 14
#define PRESS_BUTTON_TEXT_LEN 12
#define PRESS_BUTTON_HIDDEN 0xFF
#define HEALTH_FLASH_TOGGLE_FRAMES 3
#define LIVES_ICON_TILE_INDEX 253
#define LIVES_SLOT_Y 1
#define LIVES_SLOT_0_X 26
#define LIVES_MAX_DISPLAY 3
#define LIVES_FLASH_DURATION_FRAMES 30
#define LIVES_FLASH_TOGGLE_FRAMES 4
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

#define BOSS_HEALTH_TILE_EMPTY_INDEX 219
#define BOSS_LOW_HEALTH_THRESHOLD (BOSS_MAX_HEALTH / 3)

#define WARP_TILE_INDEX 6
#define WARP_TILE_COUNT 5
#define WARP_TILE_REPLACEMENT_INDEX 220

static tile_8x8_t warp_tile_backup[WARP_TILE_COUNT];

static uint8_t title_palette_tick = 0;
static uint8_t title_palette_phase = 0;
static uint8_t press_button_prompt_y = PRESS_BUTTON_HIDDEN;

static const uint16_t title_rainbow_palette[] = {
    COLOR_FROM_RGB8(255, 0, 0)   | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(255, 128, 0) | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(255, 255, 0) | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(0, 255, 0)   | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(0, 255, 255) | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(0, 0, 255)   | COLOR_ALPHA_MASK,
    COLOR_FROM_RGB8(255, 0, 255) | COLOR_ALPHA_MASK,
};

static const uint8_t boss_tiles[BOSS_LABEL_TEXT_LEN] = {
    228, // B
    241, // O
    245, // S
    245, // S
};

static const uint16_t hud_health_low_color = COLOR_FROM_RGB8(255, 24, 24) | COLOR_ALPHA_MASK;
static const uint16_t hud_health_flash_color = COLOR_FROM_RGB8(255, 255, 255) | COLOR_ALPHA_MASK;
static const uint16_t boss_health_good_color = COLOR_FROM_RGB8(32, 255, 32) | COLOR_ALPHA_MASK;
static const uint16_t boss_health_low_color = COLOR_FROM_RGB8(255, 32, 32) | COLOR_ALPHA_MASK;

// Colors from the palette asset, for the HUD entries that change at run time.
static uint16_t hud_health_default_color;
static uint16_t hud_boss_health_default_color;

static uint16_t tile_mode2_read_hud_palette_entry(uint8_t index)
{
    return xram0_peek16(XRAM_TILE_HUD_PALETTE + (unsigned)index * sizeof(uint16_t));
}

static void tile_mode2_write_hud_palette_entry(uint8_t index, uint16_t color)
{
    xram0_poke16(XRAM_TILE_HUD_PALETTE + (unsigned)index * sizeof(uint16_t), color);
}

static unsigned tile_mode2_hud_addr(uint8_t x, uint8_t y)
{
    return XRAM_STARFIELD_HUD_DATA + (unsigned)y * STARFIELD_HUD_WIDTH + x;
}

static void tile_mode2_write_tile(uint8_t x, uint8_t y, uint8_t tile_index)
{
    xram0_poke8(tile_mode2_hud_addr(x, y), tile_index);
}

static void tile_mode2_write_tiles(uint8_t x, uint8_t y, const uint8_t *tiles, uint8_t len)
{
    xram0_write(tile_mode2_hud_addr(x, y), tiles, len);
}

static void tile_mode2_clear_hud_text(uint8_t x, uint8_t y, uint8_t len)
{
    xram0_set(tile_mode2_hud_addr(x, y), 0, len);
}

static void tile_mode2_format_number(uint8_t *tiles, uint32_t value, uint8_t digits)
{
    for (uint8_t i = digits; i-- > 0;) {
        tiles[i] = (uint8_t)(SCORE_TILE_INDEX_BASE + value % 10u);
        value /= 10u;
    }

    // Values wider than the field clamp to all nines.
    if (value != 0u) {
        memset(tiles, SCORE_TILE_INDEX_BASE + 9, digits);
    }
}

static void tile_mode2_write_number(uint8_t x, uint8_t y, uint32_t value, uint8_t digits)
{
    uint8_t tiles[SCORE_DIGITS];

    tile_mode2_format_number(tiles, value, digits);
    tile_mode2_write_tiles(x, y, tiles, digits);
}

static void tile_mode2_write_bar(uint8_t x, uint8_t y, uint8_t health, uint8_t empty_tile_index)
{
    uint8_t tiles[HEALTH_BAR_TILE_COUNT];

    for (uint8_t i = 0; i < HEALTH_BAR_TILE_COUNT; ++i) {
        int16_t segment_health = (int16_t)health - (int16_t)(i * HEALTH_PER_BAR_TILE);
        uint8_t fill;

        if (segment_health <= 0) {
            fill = 0;
        } else if (segment_health >= HEALTH_PER_BAR_TILE) {
            fill = HEALTH_PER_BAR_TILE;
        } else {
            fill = (uint8_t)segment_health;
        }

        tiles[i] = (uint8_t)(empty_tile_index - fill);
    }

    tile_mode2_write_tiles(x, y, tiles, sizeof(tiles));
}

static void tile_mode2_write_pickups(uint8_t x, uint8_t y, uint8_t tile_index, uint8_t count, uint8_t max_display)
{
    for (uint8_t i = 0; i < max_display; ++i) {
        tile_mode2_write_tile((uint8_t)(x + i), y, (i < count) ? tile_index : 0);
    }
}

static uint8_t tile_mode2_bonus_row_y(uint8_t enemy_type)
{
    return (uint8_t)(BONUS_ROW_Y_START + (enemy_type * BONUS_ROW_Y_STEP));
}

static void tile_mode2_clear_title_banner(void)
{
    for (uint8_t y = 4; y <= 16; ++y) {
        tile_mode2_clear_hud_text(8, y, 32 - 8 + 1);
    }
}

static unsigned tile_mode2_tile_addr(uint8_t tile_index)
{
    return XRAM_STARFIELD_TILES_DATA + (unsigned)tile_index * sizeof(tile_8x8_t);
}

static void tile_mode2_backup_warp_tiles(void)
{
    xram0_read(warp_tile_backup, tile_mode2_tile_addr(WARP_TILE_INDEX), sizeof(warp_tile_backup));
}

static void tile_mode2_restore_warp_tiles(void)
{
    xram0_write(tile_mode2_tile_addr(WARP_TILE_INDEX), warp_tile_backup, sizeof(warp_tile_backup));
}

static void tile_mode2_replace_warp_tiles(void)
{
    xram_move(tile_mode2_tile_addr(WARP_TILE_INDEX),
              tile_mode2_tile_addr(WARP_TILE_REPLACEMENT_INDEX),
              sizeof(warp_tile_backup));
}

void tile_mode2_init(void)
{
    static const mode2_config_t bg_config = {
        .x_wrap = true,
        .y_wrap = true,
        .x_pos_px = 0,
        .y_pos_px = 0,
        .width_tiles = STARFIELD_BG_WIDTH,
        .height_tiles = STARFIELD_BG_HEIGHT,
        .xram_data_ptr = XRAM_STARFIELD_BG_DATA,     // tile ID grid
        .xram_palette_ptr = XRAM_TILE_PALETTE,
        .xram_tile_ptr = XRAM_STARFIELD_TILES_DATA,  // tile bitmaps
    };
    static const mode2_config_t fg_config = {
        .x_wrap = true,
        .y_wrap = true,
        .x_pos_px = 0,
        .y_pos_px = 0,
        .width_tiles = STARFIELD_FG_WIDTH,
        .height_tiles = STARFIELD_FG_HEIGHT,
        .xram_data_ptr = XRAM_STARFIELD_FG_DATA,     // tile ID grid
        .xram_palette_ptr = XRAM_TILE_PALETTE,
        .xram_tile_ptr = XRAM_STARFIELD_TILES_DATA,  // tile bitmaps
    };
    static const mode2_config_t hud_config = {
        .x_wrap = true,
        .y_wrap = true,
        .x_pos_px = 0,
        .y_pos_px = 0,
        .width_tiles = STARFIELD_HUD_WIDTH,
        .height_tiles = STARFIELD_HUD_HEIGHT,
        .xram_data_ptr = XRAM_STARFIELD_HUD_DATA,    // tile ID grid
        .xram_palette_ptr = XRAM_TILE_HUD_PALETTE,
        .xram_tile_ptr = XRAM_STARFIELD_TILES_DATA,  // tile bitmaps
    };

    bg_scroll_y_half = 0;
    fg_scroll_y_half = 0;
    bg_scroll_speed_half = BG_SCROLL_SPEED_HALF_PX;
    fg_scroll_speed_half = FG_SCROLL_SPEED_HALF_PX;
    fg_scroll_target_half = FG_SCROLL_SPEED_HALF_PX;
    fg_slowdown_tick = 0;
    gameplay_transition_active = false;
    transition_to_gameplay = false;
    warp_tiles_replaced = false;
    current_health_palette_color = 0;
    health_flash_tick = 0;
    title_palette_tick = 0;
    title_palette_phase = 0;
    hud_health_default_color = tile_mode2_read_hud_palette_entry(HUD_HEALTH_PALETTE_INDEX);
    hud_boss_health_default_color = tile_mode2_read_hud_palette_entry(HUD_BOSS_HEALTH_PALETTE_INDEX);

    xram0_write(XRAM_TILE_BG_CONFIG, &bg_config, sizeof(bg_config));
    // Mode 2 args: OPTIONS, CONFIG, PLANE, BEGIN, END
    // Plane 0 = background fill layer, below the HUD rows
    if (xreg_vga_mode2(MODE2_4BPP | MODE2_8X8, XRAM_TILE_BG_CONFIG, 0, HUD_TOP_PX, 0) < 0) {
        return;
    }

    xram0_write(XRAM_TILE_FG_CONFIG, &fg_config, sizeof(fg_config));
    // Plane 1 = foreground fill layer, below the HUD rows
    if (xreg_vga_mode2(MODE2_4BPP | MODE2_8X8, XRAM_TILE_FG_CONFIG, 1, HUD_TOP_PX, 0) < 0) {
        return;
    }

    xram0_write(XRAM_TILE_HUD_CONFIG, &hud_config, sizeof(hud_config));
    // Plane 2 = HUD fill layer, full screen
    if (xreg_vga_mode2(MODE2_4BPP | MODE2_8X8, XRAM_TILE_HUD_CONFIG, 2, 0, 0) < 0) {
        return;
    }

    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, title_rainbow_palette[0]);
    tile_mode2_backup_warp_tiles();
    tile_mode2_set_score(0);
    tile_mode2_set_health(PLAYER_MAX_HEALTH);
    tile_mode2_update_health_fx(false, false);
}

void tile_mode2_set_score(uint32_t score)
{
    tile_mode2_write_number(SCORE_TILE_X, SCORE_TILE_Y, score, SCORE_DIGITS);
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

    tile_mode2_write_tiles(HISCORE_TEXT_X, HISCORE_TEXT_Y, hiscore_tiles, sizeof(hiscore_tiles));
    tile_mode2_write_number(HISCORE_VALUE_X, HISCORE_VALUE_Y, score, SCORE_DIGITS);
}

void tile_mode2_set_multiplier(uint8_t multiplier)
{
    if (multiplier < 1u) {
        multiplier = 1u;
    }
    if (multiplier > 9u) {
        multiplier = 9u;
    }

    tile_mode2_write_tile(MULTIPLIER_TILE_X, MULTIPLIER_TILE_Y, (uint8_t)(SCORE_TILE_INDEX_BASE + multiplier));
    tile_mode2_write_tile((uint8_t)(MULTIPLIER_TILE_X + 1u), MULTIPLIER_TILE_Y, HUD_SYMBOL_X_TILE_INDEX);
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

    if (!visible) {
        tile_mode2_clear_hud_text(PAUSED_TEXT_X, PAUSED_TEXT_Y, sizeof(paused_tiles));
        return;
    }

    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, HUD_TEXT_YELLOW);
    tile_mode2_write_tiles(PAUSED_TEXT_X, PAUSED_TEXT_Y, paused_tiles, sizeof(paused_tiles));
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

    tile_mode2_format_number(&level_tiles[6], level, 2);

    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, HUD_TEXT_YELLOW);
    tile_mode2_write_tiles(LEVEL_TEXT_X, LEVEL_TEXT_Y, level_tiles, LEVEL_TEXT_LEN);
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
    const uint8_t x = 16u;
    const uint8_t y = 14u;

    tile_mode2_clear_hud_text(15u, y, 10u);
    if (!victory) {
        return;
    }

    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, HUD_TEXT_YELLOW);
    tile_mode2_write_tiles(x, y, you_win_tiles, sizeof(you_win_tiles));
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

    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, HUD_TEXT_YELLOW);
    tile_mode2_write_tiles(LEVEL_COMPLETE_TEXT_X, LEVEL_COMPLETE_TEXT_Y, level_complete_tiles, LEVEL_COMPLETE_TEXT_LEN);
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
        tile_mode2_clear_hud_text(x, y, sizeof(level_failed_tiles));
        return;
    }

    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, HUD_TEXT_YELLOW);
    tile_mode2_write_tiles(x, y, level_failed_tiles, sizeof(level_failed_tiles));
}

void tile_mode2_show_press_button_prompt(uint8_t y)
{
    static const uint8_t press_button_tiles[PRESS_BUTTON_TEXT_LEN] = {
        242, // P
        244, // R
        231, // E
        245, // S
        245, // S
        0,   // space
        228, // B
        247, // U
        246, // T
        246, // T
        241, // O
        240, // N
    };

    if (press_button_prompt_y == y) {
        return;
    }
    tile_mode2_hide_press_button_prompt();

    tile_mode2_write_tiles(PRESS_BUTTON_TEXT_X, y, press_button_tiles, PRESS_BUTTON_TEXT_LEN);
    press_button_prompt_y = y;
}

void tile_mode2_hide_press_button_prompt(void)
{
    if (press_button_prompt_y == PRESS_BUTTON_HIDDEN) {
        return;
    }
    tile_mode2_clear_hud_text(PRESS_BUTTON_TEXT_X, press_button_prompt_y, PRESS_BUTTON_TEXT_LEN);
    press_button_prompt_y = PRESS_BUTTON_HIDDEN;
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

void tile_mode2_start_warp_transition(void)
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

void tile_mode2_restore_hud(void)
{
    xram_move(XRAM_STARFIELD_HUD_DATA, XRAM_STARFIELD_HUD_DEFAULT, STARFIELD_HUD_SIZE);
    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, title_rainbow_palette[title_palette_phase]);
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

    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, title_rainbow_palette[title_palette_phase]);
}

void tile_mode2_set_health(uint8_t health)
{
    tile_mode2_write_bar(HEALTH_BAR_TILE_X, HEALTH_BAR_TILE_Y, health, HEALTH_BAR_TILE_EMPTY_INDEX);
}

static uint8_t tile_mode2_lives_slot_x(uint8_t slot)
{
    return (uint8_t)(LIVES_SLOT_0_X + 2u * slot);
}

void tile_mode2_set_lives(uint8_t extra_lives)
{
    // Any HUD redraw of the authoritative lives state (scene transitions,
    // resets) supersedes an in-progress flash rather than leaving it to
    // blink over a screen it no longer describes -- genuine gain/loss
    // events re-arm it explicitly via tile_mode2_flash_life_change() right
    // after calling this.
    lives_flash_timer = 0;

    for (uint8_t slot = 0; slot < LIVES_MAX_DISPLAY; ++slot) {
        tile_mode2_write_tile(tile_mode2_lives_slot_x(slot), LIVES_SLOT_Y, (extra_lives > slot) ? LIVES_ICON_TILE_INDEX : 0);
    }
}

// Call right after tile_mode2_set_lives() at a genuine gain/loss event
// (not a HUD reset/restore) to blink the one slot icon that just changed.
// old_lives/new_lives are the values immediately either side of the
// ++/-- -- whichever is higher names the slot that flipped either way.
void tile_mode2_flash_life_change(uint8_t old_lives, uint8_t new_lives)
{
    uint8_t high = (old_lives > new_lives) ? old_lives : new_lives;

    if (high == 0u || high > LIVES_MAX_DISPLAY) {
        return;
    }

    lives_flash_slot = (uint8_t)(high - 1u);
    lives_flash_steady_is_icon = (new_lives > lives_flash_slot);
    lives_flash_timer = LIVES_FLASH_DURATION_FRAMES;
    lives_flash_tick = 0;
}

// Called once per frame regardless of game state (see gameplay_frame()) so
// an in-progress flash always finishes even if the state changes mid-blink.
void tile_mode2_update_lives_fx(void)
{
    uint8_t slot_x;
    uint8_t steady_tile;
    uint8_t tile_index;

    if (lives_flash_timer == 0u) {
        return;
    }

    lives_flash_timer--;
    lives_flash_tick = (uint8_t)((lives_flash_tick + 1u) % (LIVES_FLASH_TOGGLE_FRAMES * 2u));

    slot_x = tile_mode2_lives_slot_x(lives_flash_slot);
    steady_tile = lives_flash_steady_is_icon ? LIVES_ICON_TILE_INDEX : 0;
    tile_index = (lives_flash_tick < LIVES_FLASH_TOGGLE_FRAMES)
        ? (uint8_t)((steady_tile == 0u) ? LIVES_ICON_TILE_INDEX : 0u)
        : steady_tile;

    tile_mode2_write_tile(slot_x, LIVES_SLOT_Y, tile_index);

    if (lives_flash_timer == 0u) {
        // Land exactly on the steady state so we never get stuck mid-blink.
        tile_mode2_write_tile(slot_x, LIVES_SLOT_Y, steady_tile);
    }
}

void tile_mode2_set_speed_pickups(uint8_t count)
{
    tile_mode2_write_pickups(SPEED_PICKUP_X_START, SPEED_PICKUP_Y, SPEED_PICKUP_TILE_INDEX, count, SPEED_PICKUP_MAX_DISPLAY);
}

void tile_mode2_set_power_pickups(uint8_t count)
{
    tile_mode2_write_pickups(POWER_PICKUP_X_START, POWER_PICKUP_Y, POWER_PICKUP_TILE_INDEX, count, POWER_PICKUP_MAX_DISPLAY);
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
        tile_mode2_write_hud_palette_entry(HUD_HEALTH_PALETTE_INDEX, desired_color);
    }
}

void tile_mode2_set_boss_hud_visible(bool visible)
{
    if (!visible) {
        tile_mode2_clear_hud_text(BOSS_HUD_LABEL_X, BOSS_HUD_LABEL_Y, BOSS_LABEL_TEXT_LEN);
        tile_mode2_clear_hud_text(BOSS_HUD_HEALTH_X, BOSS_HUD_HEALTH_Y, HEALTH_BAR_TILE_COUNT);
        tile_mode2_write_hud_palette_entry(HUD_BOSS_HEALTH_PALETTE_INDEX, hud_boss_health_default_color);
        return;
    }

    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, HUD_TEXT_YELLOW);
    tile_mode2_write_tiles(BOSS_HUD_LABEL_X, BOSS_HUD_LABEL_Y, boss_tiles, BOSS_LABEL_TEXT_LEN);
}

void tile_mode2_set_boss_health(uint8_t health)
{
    uint16_t boss_color = (health <= BOSS_LOW_HEALTH_THRESHOLD) ? boss_health_low_color : boss_health_good_color;

    tile_mode2_write_hud_palette_entry(HUD_BOSS_HEALTH_PALETTE_INDEX, boss_color);
    tile_mode2_write_bar(BOSS_HUD_HEALTH_X, BOSS_HUD_HEALTH_Y, health, BOSS_HEALTH_TILE_EMPTY_INDEX);
}

void tile_mode2_clear_level_bonus(void)
{
    for (uint8_t y = BONUS_TABLE_Y; y < (BONUS_TABLE_Y + BONUS_TABLE_ROWS); ++y) {
        tile_mode2_clear_hud_text(BONUS_TABLE_X, y, BONUS_TABLE_WIDTH);
    }
}

void tile_mode2_begin_level_bonus(uint8_t level, uint8_t multiplier)
{
    tile_mode2_clear_level_bonus();

    tile_mode2_write_number((uint8_t)(BONUS_TABLE_X + 2), BONUS_TABLE_Y, level, 2);
    tile_mode2_write_number((uint8_t)(BONUS_TABLE_X + 7), BONUS_TABLE_Y, multiplier, 2);

    for (uint8_t type = 0; type < ENEMY_TYPE_COUNT; ++type) {
        tile_mode2_set_bonus_row(type, 0, 0, 0);
    }

    tile_mode2_set_bonus_boss_row(0);

    tile_mode2_set_bonus_pending_total(0);
}

void tile_mode2_set_bonus_row(uint8_t enemy_type, uint16_t kills, uint16_t points_each, uint16_t subtotal)
{
    uint8_t tiles[12];

    if (enemy_type >= ENEMY_TYPE_COUNT) {
        return;
    }

    tile_mode2_format_number(&tiles[0], kills, 2);
    tiles[2] = HUD_SYMBOL_X_TILE_INDEX;
    tile_mode2_format_number(&tiles[3], points_each, 3);
    tiles[6] = HUD_SYMBOL_EQUALS_TILE_INDEX;
    tile_mode2_format_number(&tiles[7], subtotal, 5);
    tile_mode2_write_tiles((uint8_t)(BONUS_TABLE_X + 3), tile_mode2_bonus_row_y(enemy_type), tiles, sizeof(tiles));
}

void tile_mode2_set_bonus_boss_row(uint16_t boss_points)
{
    tile_mode2_write_hud_palette_entry(HUD_TEXT_PALETTE_INDEX, HUD_TEXT_YELLOW);
    tile_mode2_write_tiles(BONUS_TABLE_X, BONUS_BOSS_ROW_Y, boss_tiles, BOSS_LABEL_TEXT_LEN);
    tile_mode2_write_tile((uint8_t)(BONUS_TABLE_X + 4), BONUS_BOSS_ROW_Y, 0);
    tile_mode2_write_number((uint8_t)(BONUS_TABLE_X + 10), BONUS_BOSS_ROW_Y, boss_points, 5);
}

void tile_mode2_set_bonus_pending_total(uint32_t pending_total)
{
    tile_mode2_write_number((uint8_t)(BONUS_TABLE_X + 10), BONUS_TOTAL_Y, pending_total, 5);
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

void tile_mode2_update_scroll(void)
{
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

    xram0_poke16(XRAM_TILE_BG_CONFIG + offsetof(mode2_config_t, y_pos_px), (uint16_t)(bg_scroll_y_half / 2));
    xram0_poke16(XRAM_TILE_FG_CONFIG + offsetof(mode2_config_t, y_pos_px), (uint16_t)(fg_scroll_y_half / 2));
}