#include "game_state.h"

static game_state_t g_state = GAME_STATE_TITLE;
static bool g_start_armed = false;

void game_state_init(void)
{
    g_state = GAME_STATE_TITLE;
    // Require a release before first Start press can trigger game start.
    g_start_armed = false;
}

game_state_t game_state_get(void)
{
    return g_state;
}

game_transition_t game_state_handle_start_button(bool start_pressed)
{
    if (!start_pressed) {
        g_start_armed = true;
        return GAME_TRANSITION_NONE;
    }

    if (!g_start_armed) {
        return GAME_TRANSITION_NONE;
    }

    // Consume this press until Start is released again.
    g_start_armed = false;

    if (g_state == GAME_STATE_TITLE) {
        g_state = GAME_STATE_PLAYING;
        return GAME_TRANSITION_START_GAME;
    }

    if (g_state == GAME_STATE_PLAYING) {
        g_state = GAME_STATE_PAUSED;
        return GAME_TRANSITION_PAUSE_GAME;
    }

    if (g_state == GAME_STATE_PAUSED) {
        g_state = GAME_STATE_PLAYING;
        return GAME_TRANSITION_UNPAUSE_GAME;
    }

    if (g_state == GAME_STATE_LEVEL_BONUS) {
        g_state = GAME_STATE_PLAYING;
        return GAME_TRANSITION_START_NEXT_LEVEL;
    }

    if (g_state == GAME_STATE_GAME_OVER) {
        g_state = GAME_STATE_TITLE;
        return GAME_TRANSITION_RETURN_TO_TITLE;
    }

    return GAME_TRANSITION_NONE;
}

game_transition_t game_state_enter_level_bonus(void)
{
    if (g_state != GAME_STATE_PLAYING) {
        return GAME_TRANSITION_NONE;
    }

    g_state = GAME_STATE_LEVEL_BONUS;
    g_start_armed = false;
    return GAME_TRANSITION_ENTER_LEVEL_BONUS;
}

game_transition_t game_state_enter_game_over(void)
{
    if (g_state == GAME_STATE_GAME_OVER) {
        return GAME_TRANSITION_NONE;
    }

    g_state = GAME_STATE_GAME_OVER;
    // Require release before accepting Start as a return-to-title action.
    g_start_armed = false;
    return GAME_TRANSITION_ENTER_GAME_OVER;
}
