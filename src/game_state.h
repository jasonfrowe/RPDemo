#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdbool.h>

typedef enum {
    GAME_STATE_TITLE,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_BOSS,
    GAME_STATE_LEVEL_BONUS,
    GAME_STATE_LEVEL_FAILED,
    GAME_STATE_GAME_OVER,
} game_state_t;

typedef enum {
    GAME_TRANSITION_NONE,
    GAME_TRANSITION_START_GAME,
    GAME_TRANSITION_PAUSE_GAME,
    GAME_TRANSITION_UNPAUSE_GAME,
    GAME_TRANSITION_ENTER_BOSS,
    GAME_TRANSITION_ENTER_LEVEL_BONUS,
    GAME_TRANSITION_ENTER_LEVEL_FAILED,
    GAME_TRANSITION_START_NEXT_LEVEL,
    GAME_TRANSITION_RETRY_LEVEL,
    GAME_TRANSITION_ENTER_GAME_OVER,
    GAME_TRANSITION_RETURN_TO_TITLE,
} game_transition_t;

void game_state_init(void);
game_state_t game_state_get(void);
// START (any key or face button) and PAUSE (P, Pause, Start or Select),
// held this frame. PAUSE acts on a fresh press. START advances a PRESS
// BUTTON screen once it has seen no press, a press, then no press again,
// all after the prompt appeared.
game_transition_t game_state_handle_buttons(bool start_pressed, bool pause_pressed);
// Ignore START until game_state_guard_start(), for a screen that isn't
// ready for a press yet.
void game_state_hold_start(void);
// Accept START again after a short guard, so a press can't carry over
// from whatever the player was doing before this screen appeared.
void game_state_guard_start(void);
// True once this screen's PRESS BUTTON prompt should show: START is
// neither held nor guarded, and a press will be acted on.
bool game_state_start_ready(void);
game_transition_t game_state_enter_boss(void);
game_transition_t game_state_enter_level_bonus(void);
game_transition_t game_state_enter_level_failed(void);
game_transition_t game_state_enter_game_over(void);

#endif // GAME_STATE_H
