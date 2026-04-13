#include "level_bonus.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "gameplay_internal.h"

void gameplay_update_bonus_state(gameplay_runtime_t *state)
{
    sprite_mode5_set_frame(0);
    sprite_mode5_update_engine(false);
    level_bonus_update(&state->hud_health_last);
    if (level_bonus_is_complete()) {
        tile_mode2_update_title_palette();
    }
}
