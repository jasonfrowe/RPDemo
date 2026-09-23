#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "sfx_layout.h" // auto-generated SFX_DATA_SIZE -- see tools/generate_sfx.py

// Screen dimensions -- must match xreg_vga_canvas(CANVAS_320X240) in main.c
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite and tile data. The XRAM layout that holds them is src/xram.h.
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels
#define PLAYER_FRAME_COUNT      6                  // idle, left, right, explode frames (3, 4, 5)

#define STARFIELD_BG_WIDTH      40                 // Width of starfield background in tiles
#define STARFIELD_BG_HEIGHT     60                 // Height of starfield background in tiles

#define STARFIELD_FG_WIDTH      40                 // Width of starfield foreground in tiles
#define STARFIELD_FG_HEIGHT     60                 // Height of starfield foreground in tiles

#define STARFIELD_HUD_WIDTH     40                 // Width of starfield HUD in tiles
#define STARFIELD_HUD_HEIGHT    30                 // Height of starfield HUD in tiles
#define STARFIELD_HUD_SIZE      (STARFIELD_HUD_WIDTH * STARFIELD_HUD_HEIGHT) // 1200 bytes

#define STARFIELD_TILE_COUNT    256                // 8x8 4bpp tiles shared by all three tile planes

#define PROJECTILE_SPRITE_SIZE_PX   8                 // Projectile sprite is 8x8 pixels
#define PROJECTILE_FRAME_COUNT  13                  // 13 frames for projectile/pickups/asteroids/explosions
#define MAX_PROJECTILES         40                  // Max number of projectiles on screen at once
#define MAX_PLAYER_PROJECTILES  8                   // Slots 0..(MAX_PLAYER_PROJECTILES-1) are reserved for the player

// Projectile movement
#define PROJECTILE_SPEED_PX     4                   // Pixels per frame
#define PLAYER_FIRE_RATE        20                  // Frames between player shots (lower = faster)
#define PLAYER_FIRE_RATE_MIN    16                  // Cap for power pickups (lower = faster)
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

#define BOSS_STAGE_MUSIC_TRACK "ROM:Boss.vgm"
#define BOSS_FINAL_STAGE_MUSIC_TRACK "ROM:BossFinal.vgm" // Level 7's boss only -- the final boss

#define ENEMY_SPRITE_SIZE_PX   16
#define ENEMY_FRAME_COUNT      176                  // enemies, GAME OVER letters and bosses
#define ENEMY_TYPE_COUNT       7
#define MAX_ENEMIES            32

#endif // CONSTANTS_H