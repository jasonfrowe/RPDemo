/* RP6502 XRAM Mapping */

#ifndef XRAM_H
#define XRAM_H

#include <rp6502.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constants.h"

/* Snippets from the docs, copied unchanged. */
/* https://picocomputer.github.io/sdk.html#xram-memory-map */

/* RIA Keyboard */

#define KEYBOARD_NO_KEY 0
#define KEYBOARD_NUM_LOCK 1
#define KEYBOARD_CAPS_LOCK 2
#define KEYBOARD_SCROLL_LOCK 3

#define KEYBOARD_PRESSED(keys, code) ((keys)[(code) >> 3] & (1 << ((code) & 7)))

#define xreg_ria_keyboard(...) xreg(0, 0, 0, __VA_ARGS__)

typedef struct
{
    uint8_t keys[32];
} keyboard_t;

/* RIA Gamepads */

#define GAMEPAD_PLAYERS 4

#define GAMEPAD_DPAD_UP 0x01
#define GAMEPAD_DPAD_DOWN 0x02
#define GAMEPAD_DPAD_LEFT 0x04
#define GAMEPAD_DPAD_RIGHT 0x08

#define GAMEPAD_FEAT_TYPE_MASK 0x30
#define GAMEPAD_TYPE_UNKNOWN 0x00
#define GAMEPAD_TYPE_WESTERN 0x10
#define GAMEPAD_TYPE_EASTERN 0x20
#define GAMEPAD_TYPE_PLAYSTATION 0x30
#define GAMEPAD_FEAT_STICKS 0x40
#define GAMEPAD_FEAT_CONNECTED 0x80

#define GAMEPAD_LSTICK_UP 0x01
#define GAMEPAD_LSTICK_DOWN 0x02
#define GAMEPAD_LSTICK_LEFT 0x04
#define GAMEPAD_LSTICK_RIGHT 0x08
#define GAMEPAD_RSTICK_UP 0x10
#define GAMEPAD_RSTICK_DOWN 0x20
#define GAMEPAD_RSTICK_LEFT 0x40
#define GAMEPAD_RSTICK_RIGHT 0x80

#define GAMEPAD_BTN0_A 0x01
#define GAMEPAD_BTN0_B 0x02
#define GAMEPAD_BTN0_C 0x04
#define GAMEPAD_BTN0_X 0x08
#define GAMEPAD_BTN0_Y 0x10
#define GAMEPAD_BTN0_Z 0x20
#define GAMEPAD_BTN0_L1 0x40
#define GAMEPAD_BTN0_R1 0x80

#define GAMEPAD_BTN1_L2 0x01
#define GAMEPAD_BTN1_R2 0x02
#define GAMEPAD_BTN1_SELECT 0x04
#define GAMEPAD_BTN1_START 0x08
#define GAMEPAD_BTN1_HOME 0x10
#define GAMEPAD_BTN1_L3 0x20
#define GAMEPAD_BTN1_R3 0x40

#define xreg_ria_gamepad(...) xreg(0, 0, 2, __VA_ARGS__)

typedef struct
{
    struct
    {
        uint8_t dpad;
        uint8_t sticks;
        uint8_t btn0;
        uint8_t btn1;
        int8_t lx;
        int8_t ly;
        int8_t rx;
        int8_t ry;
        uint8_t l2;
        uint8_t r2;
    } player[GAMEPAD_PLAYERS];
} gamepad_t;

/* RIA OPL2 */

#define xreg_ria_opl(...) xreg(0, 1, 1, __VA_ARGS__)

typedef struct
{
    uint8_t reg[256];
} opl_t;

/* VGA Canvas */

#define xreg_vga_canvas(...) xreg(1, 0, 0, __VA_ARGS__)

#define CANVAS_CONSOLE 0
#define CANVAS_320X240 1
#define CANVAS_320X180 2
#define CANVAS_640X480 3
#define CANVAS_640X360 4

/* VGA Color */

#define COLOR_FROM_RGB8(r, g, b) \
    ((((unsigned)(b) >> 3) << 11) | (((unsigned)(g) >> 3) << 6) | ((unsigned)(r) >> 3))
#define COLOR_FROM_RGB5(r, g, b) \
    (((unsigned)(b) << 11) | ((unsigned)(g) << 6) | (unsigned)(r))
#define COLOR_ALPHA_MASK (1u << 5)

/* VGA Mode 2: Tile */

#define xreg_vga_mode2(...) xreg(1, 0, 1, 2, __VA_ARGS__)

#define MODE2_1BPP 0x00
#define MODE2_2BPP 0x01
#define MODE2_4BPP 0x02
#define MODE2_8BPP 0x03

#define MODE2_8X8 0x00
#define MODE2_16X16 0x08

#define MODE2_X_TRIM(cols) ((cols) << 4)
#define MODE2_Y_TRIM(rows) ((rows) << 8)

#define MODE2_TILE(bpp, size)                 \
    struct                                    \
    {                                         \
        struct                                \
        {                                     \
            uint8_t cols[(size) * (bpp) / 8]; \
        } rows[size];                         \
    }

typedef struct
{
    bool x_wrap;
    bool y_wrap;
    int16_t x_pos_px;
    int16_t y_pos_px;
    int16_t width_tiles;
    int16_t height_tiles;
    uint16_t xram_data_ptr;
    uint16_t xram_palette_ptr;
    uint16_t xram_tile_ptr;
} mode2_config_t;

/* VGA Mode 5: Sprite */

#define xreg_vga_mode5(...) xreg(1, 0, 1, 5, __VA_ARGS__)

#define MODE5_1BPP 0x00
#define MODE5_2BPP 0x01
#define MODE5_4BPP 0x02
#define MODE5_8BPP 0x03

#define MODE5_8X8 0x00
#define MODE5_16X16 0x08
#define MODE5_32X32 0x10
#define MODE5_64X64 0x18
#define MODE5_128X128 0x20
#define MODE5_256X256 0x28
#define MODE5_512X512 0x30
#define MODE5_CUSTOM 0x38

#define MODE5_HFLIP 0x10
#define MODE5_VFLIP 0x20
#define MODE5_HDOUBLE 0x40
#define MODE5_VDOUBLE 0x80

#define MODE5_IMAGE(bpp, size)                \
    struct                                    \
    {                                         \
        struct                                \
        {                                     \
            uint8_t cols[(size) * (bpp) / 8]; \
        } rows[size];                         \
    }

#define MODE5_SIZE(width, height) \
    ((((height) / 4 - 1) << 4) | ((width) / 4 - 1))

#define MODE5_CUSTOM_IMAGE(bpp, width, height)       \
    struct                                           \
    {                                                \
        struct                                       \
        {                                            \
            uint8_t cols[((width) * (bpp) + 7) / 8]; \
        } rows[height];                              \
    }

typedef struct
{
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} mode5_sprite_t;

typedef struct
{
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
    uint8_t width_height;
    uint8_t options;
} mode5_csprite_t;

/* Star Hopper's XRAM. Every asset and config is 4-bit color, */
/* so the images are 16-color and each palette has 1 << 4 entries. */

typedef MODE5_IMAGE(4, 16) sprite_16x16_t;
typedef MODE5_IMAGE(4, 8) sprite_8x8_t;
typedef MODE2_TILE(4, 8) tile_8x8_t;

#define PLAYER_FRAME_SIZE sizeof(sprite_16x16_t)
#define PROJECTILE_FRAME_SIZE sizeof(sprite_8x8_t)
#define ENEMY_FRAME_SIZE sizeof(sprite_16x16_t)

typedef struct
{
    /* First, so the OPL2 registers start on a page boundary. */
    opl_t opl;

    mode5_sprite_t player_config;
    mode2_config_t tile_bg_config;
    mode2_config_t tile_fg_config;
    mode2_config_t tile_hud_config;
    mode5_sprite_t projectile_config[MAX_PROJECTILES];
    mode5_sprite_t enemy_config[MAX_ENEMIES];

    uint16_t player_palette[1 << 4];
    uint16_t tile_bg_palette[1 << 4];
    uint16_t tile_fg_palette[1 << 4];
    uint16_t tile_hud_palette[1 << 4];
    uint16_t projectile_palette[1 << 4];
    uint16_t enemy_palette[1 << 4];

    keyboard_t keyboard;
    gamepad_t gamepad;

    /* Loaded from the ROM by CMakeLists.txt. */
    sprite_16x16_t player_data[PLAYER_FRAME_COUNT];
    uint8_t starfield_bg_data[STARFIELD_BG_HEIGHT][STARFIELD_BG_WIDTH];
    uint8_t starfield_fg_data[STARFIELD_FG_HEIGHT][STARFIELD_FG_WIDTH];
    uint8_t starfield_hud_data[STARFIELD_HUD_HEIGHT][STARFIELD_HUD_WIDTH];
    tile_8x8_t starfield_tiles_data[STARFIELD_TILE_COUNT];
    sprite_8x8_t projectile_data[PROJECTILE_FRAME_COUNT];
    sprite_16x16_t enemy_data[ENEMY_FRAME_COUNT];

    /* Last, so regenerating the SFX never moves anything else. */
    uint8_t sfx_data[SFX_DATA_SIZE];
} xram_layout_t;

#define XRAM_OPL offsetof(xram_layout_t, opl)
_Static_assert((XRAM_OPL & 0xFF) == 0, "The OPL2 registers must start on a page boundary.");

#define XRAM_PLAYER_CONFIG offsetof(xram_layout_t, player_config)
#define XRAM_TILE_BG_CONFIG offsetof(xram_layout_t, tile_bg_config)
#define XRAM_TILE_FG_CONFIG offsetof(xram_layout_t, tile_fg_config)
#define XRAM_TILE_HUD_CONFIG offsetof(xram_layout_t, tile_hud_config)
#define XRAM_PROJECTILE_CONFIG offsetof(xram_layout_t, projectile_config)
#define XRAM_ENEMY_CONFIG offsetof(xram_layout_t, enemy_config)

#define XRAM_PLAYER_PALETTE offsetof(xram_layout_t, player_palette)
#define XRAM_TILE_BG_PALETTE offsetof(xram_layout_t, tile_bg_palette)
#define XRAM_TILE_FG_PALETTE offsetof(xram_layout_t, tile_fg_palette)
#define XRAM_TILE_HUD_PALETTE offsetof(xram_layout_t, tile_hud_palette)
#define XRAM_PROJECTILE_PALETTE offsetof(xram_layout_t, projectile_palette)
#define XRAM_ENEMY_PALETTE offsetof(xram_layout_t, enemy_palette)

#define XRAM_KEYBOARD offsetof(xram_layout_t, keyboard)
#define XRAM_GAMEPAD offsetof(xram_layout_t, gamepad)

#define XRAM_PLAYER_DATA offsetof(xram_layout_t, player_data)
#define XRAM_STARFIELD_BG_DATA offsetof(xram_layout_t, starfield_bg_data)
#define XRAM_STARFIELD_FG_DATA offsetof(xram_layout_t, starfield_fg_data)
#define XRAM_STARFIELD_HUD_DATA offsetof(xram_layout_t, starfield_hud_data)
#define XRAM_STARFIELD_TILES_DATA offsetof(xram_layout_t, starfield_tiles_data)
#define XRAM_PROJECTILE_DATA offsetof(xram_layout_t, projectile_data)
#define XRAM_ENEMY_DATA offsetof(xram_layout_t, enemy_data)
#define XRAM_SFX_DATA offsetof(xram_layout_t, sfx_data)

#endif
