#ifndef PLAYER_CONTROLLER_H
#define PLAYER_CONTROLLER_H

#include <stdint.h>

void player_controller_init(void);
void player_controller_update(void);

// Speed level 1..16: level 1 = 0.25 px/frame, level 16 = 4.0 px/frame.
// Each step is 0.25 px/frame. Default is 3 (0.75 px/frame).
// In-game: tap LT to slow down, tap RT to speed up.
void player_controller_set_speed(int level);
int  player_controller_get_speed(void);

#endif // PLAYER_CONTROLLER_H
