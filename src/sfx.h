#ifndef SFX_H
#define SFX_H

#include <stdint.h>

// Two independent one-shot queues -- channel 7 for everything triggered by
// the player, channel 8 for everything triggered by an enemy -- so a burst
// of enemy fire can never interrupt the player's own fire/hit/pickup cues
// (or vice versa) the way sharing one channel used to. A priority tier
// only ever competes against other events on the SAME channel: a
// same-or-higher priority call always cuts in ("newest wins" within a
// tier); a strictly lower one is dropped outright rather than queued. See
// generate_sfx.py's module docstring for the actual sounds and src/sfx.c
// for the engine.
#define SFX_PRIORITY_FIRE       1 // PlyrFire (ch7) / EnmyFire (ch8)
#define SFX_PRIORITY_LOW_ENERGY 2 // LowEnrgy (ch7, periodic retrigger)
#define SFX_PRIORITY_PICKUP     2 // PickUp (ch7)
#define SFX_PRIORITY_DESTROYED  3 // EnmyDie (ch8)
#define SFX_PRIORITY_TOP        4 // PlyrHit, PlyrDie, XtraLife, LvlClear (ch7)

void sfx_init(void);
void sfx_play_player(const char *path, uint8_t priority); // channel 7
void sfx_play_enemy(const char *path, uint8_t priority);  // channel 8
void sfx_update(void);
void sfx_stop(void);

#endif
