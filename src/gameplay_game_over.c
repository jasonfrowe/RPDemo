#include "constants.h"
#include "enemy.h"
#include "game_state.h"
#include "gameplay_boss.h"
#include "music.h"
#include "player_controller.h"
#include "projectile.h"
#include "rng.h"
#include "score.h"
#include "sfx.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "gameplay_internal.h"

// Victory-only celebration: random firework bursts using the same
// explosion sprite/animation projectile.c already uses for enemy deaths
// and asteroid hits -- no new art needed, just projectile_spawn_explosion()
// aimed at random points instead of an enemy's position. Slots
// FIRST_ENEMY_PROJECTILE_SLOT..MAX_PROJECTILES are free the whole time (no
// enemy fire, no player fire besides whatever the player chooses to loose
// while flying around during their own victory lap).
#define VICTORY_FIREWORK_INTERVAL_FRAMES 18
#define VICTORY_FIREWORK_MARGIN_PX 24
#define VICTORY_FIREWORK_TOP_PX (HUD_TOP_PX + 8)
#define VICTORY_FIREWORK_BOTTOM_PX (SCREEN_HEIGHT - 40)

static uint16_t victory_fx_rng = 0x9E17u;
static uint16_t victory_firework_timer = 0;

// Victory-only "boss parade": each level's boss coasts in from off-screen
// top to the same resting spot it holds during a real fight (BOSS_START_X/
// Y, centered just below the HUD), that level's matching enemy type
// circles it a couple of times, then the whole formation drifts off the
// bottom before the next level's pairing enters -- level 1's enemies with
// its boss, then level 2's, and so on through level 7, back to back. Reuses
// the same sprite slots gameplay_boss.c/enemy.c use during a real fight
// (enemy slots for the orbiting escort, the boss's own reserved slots via
// sprite_mode5_show_boss()) -- both pools are otherwise idle on this
// screen, so there's nothing to conflict with.
typedef enum {
    VICTORY_PARADE_DONE = 0,
    VICTORY_PARADE_ENTER,
    VICTORY_PARADE_ORBIT,
    VICTORY_PARADE_EXIT,
} victory_parade_phase_t;

#define VICTORY_PARADE_START_DELAY_FRAMES 60
#define VICTORY_PARADE_ROUNDS 7
#define VICTORY_PARADE_ORBIT_ENEMY_COUNT 4
#define VICTORY_PARADE_ORBIT_POINTS 16
// 5 loops * 16 points * 3 frames/point = 240 frames = 4 seconds at 60Hz.
#define VICTORY_PARADE_ORBIT_LOOPS 5
#define VICTORY_PARADE_ORBIT_STEP_FRAMES 3
#define VICTORY_PARADE_BOSS_ENTER_SPEED_PX 2
#define VICTORY_PARADE_BOSS_EXIT_SPEED_PX 3
#define VICTORY_PARADE_BOSS_W (BOSS_GRID_COLS * ENEMY_SPRITE_SIZE_PX)
#define VICTORY_PARADE_BOSS_H (BOSS_GRID_ROWS * ENEMY_SPRITE_SIZE_PX)
#define VICTORY_PARADE_ENEMY_HALF (ENEMY_SPRITE_SIZE_PX / 2)
// BOSS_START_X/Y (constants.h) is where a real fight rests the boss --
// right under the HUD, which on this screen is exactly where the "STAR
// HOPPER" logo and PRESS START/HISCORE text live (baked into the HUD ROM
// template restored at the top of this screen, not drawn by game code).
// The parade needs to clear all of that, so it gets its own, lower resting
// row instead of reusing BOSS_START_Y -- BOSS_START_X (horizontal
// centering) is still correct as-is.
#define VICTORY_PARADE_BOSS_TARGET_Y 152

// A 16-point oval around the boss (wider than tall, so it clears a
// 48x32px boss without the top/bottom points running into the HUD or
// each other) -- generated once (radius 26px horizontal, 16px vertical)
// rather than computed with trig at runtime, which the 6502 has none of.
static const int8_t victory_orbit_dx[VICTORY_PARADE_ORBIT_POINTS] = {
     26,  24,  18,  10,   0, -10, -18, -24, -26, -24, -18, -10,   0,  10,  18,  24,
};
static const int8_t victory_orbit_dy[VICTORY_PARADE_ORBIT_POINTS] = {
      0,   6,  11,  14,  16,  14,  11,   6,   0,  -6, -11, -14, -16, -14, -11,  -6,
};

static victory_parade_phase_t parade_phase = VICTORY_PARADE_DONE;
static uint16_t parade_start_delay = 0;
static uint8_t parade_round = 0;
static int16_t parade_boss_y = 0;
static uint8_t parade_orbit_index = 0;
static uint8_t parade_orbit_tick = 0;
static uint8_t parade_orbit_loops = 0;

static void victory_parade_hide_enemies(void)
{
    for (uint8_t i = 0; i < VICTORY_PARADE_ORBIT_ENEMY_COUNT; ++i) {
        sprite_mode5_set_enemy(i, -32, -32, 0);
    }
}

static void victory_parade_place_orbit_enemies(int16_t center_x, int16_t center_y, uint8_t enemy_frame)
{
    for (uint8_t i = 0; i < VICTORY_PARADE_ORBIT_ENEMY_COUNT; ++i) {
        uint8_t point = (uint8_t)((parade_orbit_index +
            (uint8_t)(i * (VICTORY_PARADE_ORBIT_POINTS / VICTORY_PARADE_ORBIT_ENEMY_COUNT))) %
            VICTORY_PARADE_ORBIT_POINTS);
        int16_t ex = (int16_t)(center_x + victory_orbit_dx[point] - VICTORY_PARADE_ENEMY_HALF);
        int16_t ey = (int16_t)(center_y + victory_orbit_dy[point] - VICTORY_PARADE_ENEMY_HALF);
        sprite_mode5_set_enemy(i, ex, ey, enemy_frame);
    }
}

static void victory_parade_advance_orbit(void)
{
    parade_orbit_tick++;
    if (parade_orbit_tick >= VICTORY_PARADE_ORBIT_STEP_FRAMES) {
        parade_orbit_tick = 0;
        parade_orbit_index++;
        if (parade_orbit_index >= VICTORY_PARADE_ORBIT_POINTS) {
            parade_orbit_index = 0;
            parade_orbit_loops++;
        }
    }
}

static void victory_parade_start(void)
{
    parade_round = 0;
    parade_start_delay = VICTORY_PARADE_START_DELAY_FRAMES;
    parade_boss_y = (int16_t)(-VICTORY_PARADE_BOSS_H);
    parade_phase = VICTORY_PARADE_ENTER;
    sprite_mode5_hide_boss();
    victory_parade_hide_enemies();
}

static void victory_parade_update(void)
{
    uint8_t level;
    uint8_t boss_frame_base;
    uint8_t enemy_type;
    uint8_t enemy_frame;
    int16_t center_x;
    int16_t center_y;

    if (parade_phase == VICTORY_PARADE_DONE) {
        return;
    }

    if (parade_start_delay > 0) {
        parade_start_delay--;
        return;
    }

    level = (uint8_t)(parade_round + 1);
    boss_frame_base = gameplay_boss_frame_set_a_base_for_level(level);
    enemy_type = (parade_round < ENEMY_TYPE_COUNT) ? parade_round : (uint8_t)(ENEMY_TYPE_COUNT - 1u);
    enemy_frame = enemy_type_base_frame(enemy_type);
    center_x = (int16_t)(BOSS_START_X + (VICTORY_PARADE_BOSS_W / 2));
    center_y = (int16_t)(parade_boss_y + (VICTORY_PARADE_BOSS_H / 2));

    switch (parade_phase) {
        case VICTORY_PARADE_ENTER:
            parade_boss_y = (int16_t)(parade_boss_y + VICTORY_PARADE_BOSS_ENTER_SPEED_PX);
            if (parade_boss_y >= VICTORY_PARADE_BOSS_TARGET_Y) {
                parade_boss_y = VICTORY_PARADE_BOSS_TARGET_Y;
                parade_orbit_index = 0;
                parade_orbit_tick = 0;
                parade_orbit_loops = 0;
                parade_phase = VICTORY_PARADE_ORBIT;
            }
            sprite_mode5_show_boss(BOSS_START_X, parade_boss_y, boss_frame_base);
            break;

        case VICTORY_PARADE_ORBIT:
            sprite_mode5_show_boss(BOSS_START_X, parade_boss_y, boss_frame_base);
            victory_parade_advance_orbit();
            victory_parade_place_orbit_enemies(center_x, center_y, enemy_frame);
            if (parade_orbit_loops >= VICTORY_PARADE_ORBIT_LOOPS) {
                parade_phase = VICTORY_PARADE_EXIT;
            }
            break;

        case VICTORY_PARADE_EXIT:
            parade_boss_y = (int16_t)(parade_boss_y + VICTORY_PARADE_BOSS_EXIT_SPEED_PX);
            sprite_mode5_show_boss(BOSS_START_X, parade_boss_y, boss_frame_base);
            victory_parade_advance_orbit();
            victory_parade_place_orbit_enemies(center_x, center_y, enemy_frame);

            if (parade_boss_y > SCREEN_HEIGHT) {
                sprite_mode5_hide_boss();
                victory_parade_hide_enemies();

                // Loops forever rather than stopping after level 7 -- the
                // win screen sits until the timeout regardless (see
                // gameplay_update_game_over_state()), so this just keeps
                // the sign-off going the whole time instead of leaving a
                // static screen for however long the player lingers.
                parade_round = (uint8_t)((parade_round + 1) % VICTORY_PARADE_ROUNDS);
                parade_boss_y = (int16_t)(-VICTORY_PARADE_BOSS_H);
                parade_phase = VICTORY_PARADE_ENTER;
            }
            break;

        default:
            break;
    }
}

// Deliberately its own path, not a couple of `if (is_victory)` branches
// bolted onto the defeat flow below -- the two used to share the "GAME
// OVER" flying-letter animation and Gameover.vgm even on a win, with a
// tiny "YOU WIN" caption tacked on afterward as the only difference. The
// player still gets full control of their ship for this victory lap; only
// the defeat path hides it.
static void gameplay_update_victory_state(gameplay_runtime_t *state)
{
    if (!state->game_over_letters_started) {
        tile_mode2_start_game_over_transition();
        tile_mode2_restore_hud_from_rom();
        tile_mode2_set_score(score_get());
        tile_mode2_set_multiplier(score_get_multiplier());
        tile_mode2_set_paused_banner(false);
        tile_mode2_set_level_banner(state->current_level, false);
        state->level_banner_visible = false;
        tile_mode2_set_level_complete_banner(false);
        tile_mode2_set_end_banner(true);
        tile_mode2_set_bonus_continue_prompt(false);
        tile_mode2_set_health(0);

        music_set_track("ROM:Victory.vgm");
        sfx_play_player(SFX_VICTORY_ADDR, SFX_PRIORITY_TOP);

        victory_fx_rng = 0x9E17u;
        victory_firework_timer = 0;
        victory_parade_start();

        state->game_over_letters_started = true;
        state->game_over_scroll_started = true;
    }

    victory_parade_update();
    projectile_update();
    // Reuses the title screen's own rainbow-cycle mechanic on HUD palette
    // slot 2 -- the same slot tile_mode2_set_end_banner() draws "YOU WIN"
    // with, so the banner cycles color for free.
    tile_mode2_update_title_palette();

    if (victory_firework_timer > 0) {
        victory_firework_timer--;
    } else {
        int16_t x = rng_range(&victory_fx_rng, VICTORY_FIREWORK_MARGIN_PX,
                               (int16_t)(SCREEN_WIDTH - VICTORY_FIREWORK_MARGIN_PX));
        int16_t y = rng_range(&victory_fx_rng, VICTORY_FIREWORK_TOP_PX, VICTORY_FIREWORK_BOTTOM_PX);
        projectile_spawn_explosion(x, y);
        victory_firework_timer = VICTORY_FIREWORK_INTERVAL_FRAMES;
    }
}

static void gameplay_update_defeat_state(gameplay_runtime_t *state)
{
    if (!state->game_over_letters_started && player_controller_is_death_animation_complete()) {
        enemy_start_game_over_animation();
        music_set_track("ROM:Gameover.vgm");
        state->game_over_letters_started = true;
    }

    if (state->game_over_letters_started) {
        enemy_update();

        if (enemy_is_game_over_animation_complete()) {
            sprite_mode5_hide_player();

            if (!state->game_over_scroll_started) {
                if (state->game_over_scroll_delay_timer < GAME_OVER_SCROLL_START_DELAY_FRAMES) {
                    state->game_over_scroll_delay_timer++;
                } else {
                    tile_mode2_start_game_over_transition();
                    tile_mode2_restore_hud_from_rom();
                    tile_mode2_set_score(score_get());
                    tile_mode2_set_multiplier(score_get_multiplier());
                    tile_mode2_set_paused_banner(false);
                    tile_mode2_set_level_banner(state->current_level, false);
                    state->level_banner_visible = false;
                    tile_mode2_set_level_complete_banner(false);
                    tile_mode2_set_bonus_continue_prompt(false);
                    tile_mode2_set_health(0);
                    state->game_over_scroll_started = true;
                }
            }
        }
    }
}

void gameplay_update_game_over_state(gameplay_runtime_t *state)
{
    player_controller_update();

    if (state->game_over_is_victory) {
        gameplay_update_victory_state(state);
    } else {
        gameplay_update_defeat_state(state);
    }

    tile_mode2_update_health_fx(false, true);

    if (state->game_over_timer > 0) {
        state->game_over_timer--;
    }

    if (state->game_over_timer == 0) {
        game_state_init();
        gameplay_reset_to_title_scene(state);
    }
}
