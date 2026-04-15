#include "constants.h"
#include "enemy.h"
#include "game_state.h"
#include "music.h"
#include "player_controller.h"
#include "score.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "gameplay_internal.h"

void gameplay_update_game_over_state(gameplay_runtime_t *state)
{
    player_controller_update();

    if (!state->game_over_letters_started &&
        (state->game_over_is_victory || player_controller_is_death_animation_complete())) {
        enemy_start_game_over_animation();
        music_set_track("ROM:RESOURCE.011.vgm");
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
                    if (state->game_over_is_victory) {
                        tile_mode2_set_end_banner(true);
                    }
                    tile_mode2_set_bonus_continue_prompt(false);
                    tile_mode2_set_health(0);
                    state->game_over_scroll_started = true;
                }
            }
        }
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
