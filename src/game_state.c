#include <stdint.h>
#include "game_state.h"

// Frames a screen waits, once it is ready, before START is accepted.
#define START_GUARD_FRAMES 60

// START's debounce state: idle (not yet armed), armed (released, waiting for
// a press), or pressed (press seen, waiting for release to fire).
typedef enum {
    START_IDLE,
    START_ARMED,
    START_PRESSED,
} start_state_t;

static game_state_t g_state = GAME_STATE_TITLE;
static start_state_t g_start_state = START_IDLE;
static bool g_pause_armed = false;
static bool g_start_held = false;
static uint8_t g_start_guard = 0;
static game_state_t g_paused_from_state = GAME_STATE_PLAYING;

// Both buttons need a release before their next press counts, so the press
// that caused a transition can't cause another one.
static void disarm_buttons(void)
{
    g_start_state = START_IDLE;
    g_pause_armed = false;
}

void game_state_init(void)
{
    g_state = GAME_STATE_TITLE;
    g_paused_from_state = GAME_STATE_PLAYING;
    disarm_buttons();
    game_state_guard_start();
}

game_state_t game_state_get(void)
{
    return g_state;
}

void game_state_hold_start(void)
{
    g_start_held = true;
    g_start_state = START_IDLE;
}

void game_state_guard_start(void)
{
    g_start_held = false;
    g_start_guard = START_GUARD_FRAMES;
    g_start_state = START_IDLE;
}

bool game_state_start_ready(void)
{
    switch (g_state) {
        case GAME_STATE_TITLE:
        case GAME_STATE_LEVEL_BONUS:
        case GAME_STATE_LEVEL_FAILED:
        case GAME_STATE_GAME_OVER:
            return !g_start_held && g_start_guard == 0;
        default:
            return false;
    }
}

game_transition_t game_state_handle_buttons(bool start_pressed, bool pause_pressed)
{
    // PRESS BUTTON is drawn at the end of the frame the guard runs out, so
    // START only arms on a frame that begins with the prompt already up.
    // Advancing takes no press, then a press, then no press -- all seen
    // after the prompt appears.
    if (g_start_held) {
        g_start_state = START_IDLE;
    } else if (g_start_guard > 0) {
        g_start_guard--;
        g_start_state = START_IDLE;
    } else if (!start_pressed && g_start_state != START_PRESSED) {
        // Don't re-arm on the frame START is released -- that's the frame
        // that still needs to read as START_PRESSED, to fire the transition
        // below.
        g_start_state = START_ARMED;
    }

    // Pause arms on its own buttons only, so holding fire never blocks it.
    if (!pause_pressed) {
        g_pause_armed = true;
    }

    // PRESS BUTTON advances when the button is let go, not when it goes
    // down, so the key or button that starts a game isn't still held -- and
    // firing -- when play begins. Only a press that began after an armed
    // (released) frame counts.
    if (game_state_start_ready()) {
        if (start_pressed) {
            if (g_start_state == START_ARMED) {
                g_start_state = START_PRESSED;
            }
            return GAME_TRANSITION_NONE;
        }
        if (g_start_state != START_PRESSED) {
            return GAME_TRANSITION_NONE;
        }
        disarm_buttons();

        if (g_state == GAME_STATE_TITLE) {
            g_state = GAME_STATE_PLAYING;
            return GAME_TRANSITION_START_GAME;
        }

        if (g_state == GAME_STATE_LEVEL_BONUS) {
            g_state = GAME_STATE_PLAYING;
            return GAME_TRANSITION_START_NEXT_LEVEL;
        }

        if (g_state == GAME_STATE_LEVEL_FAILED) {
            g_state = GAME_STATE_PLAYING;
            return GAME_TRANSITION_RETRY_LEVEL;
        }

        // Game over or victory returns to the title screen (matching what
        // the timeout already does), not straight into a new run.
        g_state = GAME_STATE_TITLE;
        return GAME_TRANSITION_RETURN_TO_TITLE;
    }

    if (!pause_pressed || !g_pause_armed) {
        return GAME_TRANSITION_NONE;
    }

    if (g_state == GAME_STATE_PLAYING || g_state == GAME_STATE_BOSS) {
        disarm_buttons();
        g_paused_from_state = g_state;
        g_state = GAME_STATE_PAUSED;
        return GAME_TRANSITION_PAUSE_GAME;
    }

    if (g_state == GAME_STATE_PAUSED) {
        disarm_buttons();
        g_state = g_paused_from_state;
        return GAME_TRANSITION_UNPAUSE_GAME;
    }

    return GAME_TRANSITION_NONE;
}

game_transition_t game_state_enter_boss(void)
{
    if (g_state != GAME_STATE_PLAYING) {
        return GAME_TRANSITION_NONE;
    }

    g_state = GAME_STATE_BOSS;
    g_pause_armed = false;
    return GAME_TRANSITION_ENTER_BOSS;
}

game_transition_t game_state_enter_level_bonus(void)
{
    if (g_state != GAME_STATE_PLAYING && g_state != GAME_STATE_BOSS) {
        return GAME_TRANSITION_NONE;
    }

    g_state = GAME_STATE_LEVEL_BONUS;
    // The tally runs first; level_bonus.c guards START once it's done.
    game_state_hold_start();
    return GAME_TRANSITION_ENTER_LEVEL_BONUS;
}

game_transition_t game_state_enter_level_failed(void)
{
    if (g_state != GAME_STATE_BOSS) {
        return GAME_TRANSITION_NONE;
    }

    g_state = GAME_STATE_LEVEL_FAILED;
    game_state_guard_start();
    return GAME_TRANSITION_ENTER_LEVEL_FAILED;
}

game_transition_t game_state_enter_game_over(void)
{
    if (g_state == GAME_STATE_GAME_OVER) {
        return GAME_TRANSITION_NONE;
    }

    g_state = GAME_STATE_GAME_OVER;
    // gameplay_game_over.c guards START once the screen is up: at once for
    // a victory, after the death animation and GAME OVER letters for a loss.
    game_state_hold_start();
    return GAME_TRANSITION_ENTER_GAME_OVER;
}
