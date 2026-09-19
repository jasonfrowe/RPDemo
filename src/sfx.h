#ifndef SFX_H
#define SFX_H

#include <stdint.h>

// Priority tiers for sfx_play()'s one-shot stingers on channel 7 -- a
// same-or-higher priority call always cuts in ("newest wins" within a
// tier); a strictly lower one is dropped outright rather than queued, so
// a torrent of low-priority events (player fire, fired constantly) can't
// cut off something rarer and more important. See generate_sfx.py's
// module docstring for the actual sounds and src/sfx.c for the engine.
#define SFX_PRIORITY_FIRE      1 // PlyrFire, EnmyFire
#define SFX_PRIORITY_PICKUP    2 // PickUp
#define SFX_PRIORITY_DESTROYED 3 // EnmyDie
#define SFX_PRIORITY_TOP       4 // PlyrHit, PlyrDie, LvlClear

void sfx_init(void);
void sfx_play(const char *path, uint8_t priority);
void sfx_update(void);
void sfx_stop(void);

#endif
