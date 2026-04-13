#ifndef GAMEPLAY_INTERNAL_H
#define GAMEPLAY_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#include "game_state.h"

typedef enum {
    PLAYER_SCRIPT_NONE = 0,
    PLAYER_SCRIPT_TO_BONUS,
    PLAYER_SCRIPT_FROM_BONUS,
} player_script_t;

typedef struct {
    uint16_t game_over_timer;
    uint8_t hud_health_last;
    bool game_over_letters_started;
    bool game_over_scroll_started;
    uint16_t game_over_scroll_delay_timer;
    uint8_t current_level;
    bool level_banner_visible;
    bool bonus_entry_pending;
    uint8_t bonus_entry_hold_timer;
    player_script_t player_script;
} gameplay_runtime_t;

void gameplay_clear_bonus_entry_state(gameplay_runtime_t *state);
void gameplay_update_player_script(gameplay_runtime_t *state);
void gameplay_reset_to_title_scene(gameplay_runtime_t *state);

void gameplay_update_playing_state(gameplay_runtime_t *state);
void gameplay_update_bonus_state(gameplay_runtime_t *state);
void gameplay_update_game_over_state(gameplay_runtime_t *state);

#endif
