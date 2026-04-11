#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U            // Starting address in XRAM for sprite data

#define PLAYER_DATA            (SPRITE_DATA_START) // Address for main tile bitmap data
#define PLAYER_DATA_SIZE        0x0080U            // 128 bytes (16x16 at 4bpp)
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels

#define STARFIELD_BG_DATA      (PLAYER_DATA + PLAYER_DATA_SIZE) // Address for starfield background tilemap
#define STARFIELD_BG_SIZE       0x0960U            // 2400 bytes (40x60 tilemap)
#define STARFIELD_BG_WIDTH      40                 // Width of starfield background in tiles
#define STARFIELD_BG_HEIGHT     60                 // Height of starfield background in tiles

#define STARFIELD_FG_DATA      (STARFIELD_BG_DATA + STARFIELD_BG_SIZE) // Address for starfield foreground tilemap
#define STARFIELD_FG_SIZE       0x0960U            // 2400 bytes (40x60 tilemap)
#define STARFIELD_FG_WIDTH      40                 // Width of starfield foreground in tiles
#define STARFIELD_FG_HEIGHT     60                 // Height of starfield foreground in tiles

#define STARFIELD_HUD_DATA     (STARFIELD_FG_DATA + STARFIELD_FG_SIZE) // Address for starfield HUD tilemap
#define STARFIELD_HUD_SIZE      0x04B0U            // 1200 bytes (40x30 tilemap)
#define STARFIELD_HUD_WIDTH     40                 // Width of starfield HUD in tiles
#define STARFIELD_HUD_HEIGHT    30                 // Height of starfield HUD in tiles

#define STARFIELD_TILES_DATA   (STARFIELD_HUD_DATA + STARFIELD_HUD_SIZE) // Address for starfield tile bitmaps
#define STARFIELD_TILES_SIZE    0x0600U            // 1536 bytes (48 tiles at 32 bytes each for 4bpp)


#define SPRITE_DATA_END        (STARFIELD_TILES_DATA + STARFIELD_TILES_SIZE) // End of sprite data


// Palette configurations
#define PLAYER_PALETTE_ADDR    0xFC00  // 16-color palette (32 bytes, 0xFC00-0xFC1F)
#define PLAYER_PALETTE_SIZE    0x0020
#define TILE_BG_PALETTE_ADDR   0xFC20  // 16-color palette for tile background (32 bytes, 0xFC20-0xFC3F)
#define TILE_BG_PALETTE_SIZE   0x0020
#define TILE_FG_PALETTE_ADDR   0xFC40  // 16-color palette for tile foreground (32 bytes, 0xFC40-0xFC5F)
#define TILE_FG_PALETTE_SIZE   0x0020
#define TILE_HUD_PALETTE_ADDR  0xFC60  // 16-color palette for tile HUD (32 bytes, 0xFC60-0xFC7F)
#define TILE_HUD_PALETTE_SIZE  0x0020

// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield

// Configs 
extern unsigned PLAYER_CONFIG; // Address in XRAM where player sprite config is stored, for updates
extern unsigned TILE_BG_CONFIG; // Address in XRAM where tile background config is stored, for updates
extern unsigned TILE_FG_CONFIG; // Address in XRAM where tile foreground config is stored, for updates
extern unsigned TILE_HUD_CONFIG; // Address in XRAM where tile HUD config is stored, for updates

#endif // CONSTANTS_H