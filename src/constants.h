#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U            // Starting address in XRAM for sprite data

#define PLAYER_DATA            (SPRITE_DATA_START) // Address for main tile bitmap data
#define PLAYER_DATA_SIZE        0x0300U            // 768 bytes (6 frames 16x16 at 4bpp)
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels
#define PLAYER_FRAME_SIZE       0x0080U            // 128 bytes per 16x16 4bpp frame
#define PLAYER_FRAME_COUNT      6                  // idle, left, right, explode frames (3, 4, 5)

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
#define STARFIELD_TILES_SIZE    0x2000U             // 8192 bytes (256 tiles at 32 bytes each for 4bpp)

#define PROJECTILE_DATA        (STARFIELD_TILES_DATA + STARFIELD_TILES_SIZE) // Address for projectile sprite data
#define PROJECTILE_DATA_SIZE    0x0140U            // 320 bytes (10 frames 8x8 at 4bpp)
#define PROJECTILE_SPRITE_SIZE_PX   8                 // Projectile sprite is 8x8 pixels
#define PROJECTILE_FRAME_SIZE   0x0020U            // 32 bytes per 8x8 4bpp frame
#define PROJECTILE_FRAME_COUNT  10                  // 10 frames for projectile/pickups/asteroids
#define MAX_PROJECTILES         40                  // Max number of projectiles on screen at once
#define MAX_PLAYER_PROJECTILES  8                   // Slots 0..(MAX_PLAYER_PROJECTILES-1) are reserved for the player

// Projectile movement
#define PROJECTILE_SPEED_PX     4                   // Pixels per frame
#define PLAYER_FIRE_RATE        20                  // Frames between player shots (lower = faster)
#define PLAYER_FIRE_RATE_MIN    12                  // Cap for power pickups (lower = faster)
#define HUD_TOP_PX              24                  // Rows 0-23 are HUD; bullets expire when y < HUD_TOP_PX

// Player health and damage tuning
#define PLAYER_MAX_HEALTH           48
#define PLAYER_LOW_HEALTH_THRESHOLD 12
#define PLAYER_BULLET_DAMAGE         4
#define PLAYER_CONTACT_DAMAGE        4
#define PLAYER_HIT_COOLDOWN_FRAMES 108
#define PLAYER_HIT_FLASH_FRAMES     96
#define PLAYER_HITBOX_SIZE          13
#define PLAYER_HITBOX_OFFSET        ((PLAYER_SPRITE_SIZE_PX - PLAYER_HITBOX_SIZE) / 2)

// Player destruction animation uses frames 3, 4, 5
#define PLAYER_DEATH_FRAME_START    3
#define PLAYER_DEATH_FRAME_END      5
#define PLAYER_DEATH_FRAME_STEP_FRAMES 10
#define PLAYER_DEATH_FINAL_HOLD_FRAMES 12

// HUD health bar mapping: tiles (17,2)..(22,2), tile index 39 (full) .. 47 (empty)
#define HEALTH_BAR_TILE_X          17
#define HEALTH_BAR_TILE_Y           2
#define HEALTH_BAR_TILE_COUNT       6
#define HEALTH_BAR_TILE_FULL_INDEX 39
#define HEALTH_BAR_TILE_EMPTY_INDEX 47
#define HEALTH_PER_BAR_TILE         8

// Game-over timing
// 108.8 seconds at 60 FPS.
#define GAME_OVER_TIMEOUT_FRAMES 6528
#define GAME_OVER_SCROLL_START_DELAY_FRAMES 120

// Boss stage constants (data only; behavior wired separately)
#define BOSS_MAX_HEALTH 48
#define BOSS_WAVE_KILL_TARGET 15
#define BOSS_VULNERABLE_DAMAGE_CAP 24
#define BOSS_VULNERABLE_TIMEOUT_FRAMES (30 * 60)
#define BOSS_FIGHT_TIMEOUT_FRAMES (4 * 60 * 60)
#define BOSS_VICTORY_HOLD_FRAMES (3 * 60)

#define BOSS_GRID_COLS 3
#define BOSS_GRID_ROWS 2
#define BOSS_SPRITE_COUNT      (BOSS_GRID_COLS * BOSS_GRID_ROWS)
#define BOSS_SPRITE_SLOT_FIRST (MAX_ENEMIES - BOSS_SPRITE_COUNT)
#define BOSS_START_X ((SCREEN_WIDTH - (BOSS_GRID_COLS * ENEMY_SPRITE_SIZE_PX)) / 2)
#define BOSS_START_Y (HUD_TOP_PX + ENEMY_SPRITE_SIZE_PX)

#define BOSS_FRAME_SET_A_BASE 50
#define BOSS_FRAME_SET_B_BASE 56
#define BOSS_FRAME_SET_ATTACK_BASE 62

#define BOSS_PROJECTILE_LEFT_FRAME 8
#define BOSS_PROJECTILE_RIGHT_FRAME 9
#define BOSS_PROJECTILE_SEPARATION_PX 32

#define BOSS_WEAKSPOT_X_MIN 20
#define BOSS_WEAKSPOT_X_MAX 28
#define BOSS_WEAKSPOT_Y_MIN 27
#define BOSS_WEAKSPOT_Y_MAX 30

#define BOSS_HUD_LABEL_X 0
#define BOSS_HUD_LABEL_Y 24
#define BOSS_HUD_HEALTH_X 0
#define BOSS_HUD_HEALTH_Y 25

#define BOSS_STAGE_MUSIC_TRACK "music/RESOURCE.009.vgm"

#define ENEMY_DATA             (PROJECTILE_DATA + PROJECTILE_DATA_SIZE) // Address for enemy sprite data
#define ENEMY_DATA_SIZE        0x2200U              // 8704 bytes (68 frames 16x16 at 4bpp)
#define ENEMY_SPRITE_SIZE_PX   16
#define ENEMY_FRAME_SIZE       0x0080U              // 128 bytes per 16x16 4bpp frame
#define ENEMY_TYPE_COUNT       7
#define MAX_ENEMIES            32


#define SPRITE_DATA_END        (ENEMY_DATA + ENEMY_DATA_SIZE) // End of sprite data


// Palette configurations
#define PLAYER_PALETTE_ADDR    0xFC00  // 16-color palette (32 bytes, 0xFC00-0xFC1F)
#define PLAYER_PALETTE_SIZE    0x0020
#define TILE_BG_PALETTE_ADDR   0xFC20  // 16-color palette for tile background (32 bytes, 0xFC20-0xFC3F)
#define TILE_BG_PALETTE_SIZE   0x0020
#define TILE_FG_PALETTE_ADDR   0xFC40  // 16-color palette for tile foreground (32 bytes, 0xFC40-0xFC5F)
#define TILE_FG_PALETTE_SIZE   0x0020
#define TILE_HUD_PALETTE_ADDR  0xFC60  // 16-color palette for tile HUD (32 bytes, 0xFC60-0xFC7F)
#define TILE_HUD_PALETTE_SIZE  0x0020
#define PROJECTILE_PALETTE_ADDR 0xFC80 // 16-color palette for projectiles (32 bytes, 0xFC80-0xFC9F)
#define PROJECTILE_PALETTE_SIZE 0x0020
#define ENEMY_PALETTE_ADDR     0xFCA0  // 16-color palette for enemies (32 bytes, 0xFCA0-0xFCBF)
#define ENEMY_PALETTE_SIZE     0x0020

// OPL2 sound chip configuration
#define OPL_XRAM_ADDR   0xFE00  // Native RIA OPL2 register page
#define OPL_SIZE        0x0100

// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield

// Configs 
extern unsigned PLAYER_CONFIG; // Address in XRAM where player sprite config is stored, for updates
extern unsigned PROJECTILE_CONFIG; // Address in XRAM where projectile sprite config is stored, for updates
extern unsigned TILE_BG_CONFIG; // Address in XRAM where tile background config is stored, for updates
extern unsigned TILE_FG_CONFIG; // Address in XRAM where tile foreground config is stored, for updates
extern unsigned TILE_HUD_CONFIG; // Address in XRAM where tile HUD config is stored, for updates
extern unsigned ENEMY_CONFIG;    // Address in XRAM where enemy sprite configs start

#endif // CONSTANTS_H