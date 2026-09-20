#ifndef SFX_H
#define SFX_H

#include <stdbool.h>
#include <stdint.h>

#include "constants.h"   // SFX_DATA
#include "sfx_layout.h"  // SFX_*_OFFSET (auto-generated)

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
#define SFX_PRIORITY_PICKUP     2 // PickUp, Tally (ch7)
#define SFX_PRIORITY_DESTROYED  3 // EnmyDie (ch8)
#define SFX_PRIORITY_TOP        4 // PlyrHit, PlyrDie, XtraLife, LvlClear, Victory (ch7)

// Absolute XRAM addresses of each clip's command stream -- SFX_DATA
// (constants.h) plus its auto-generated offset (sfx_layout.h). What
// sfx_play_player()/sfx_play_enemy() take in place of a ROM: path.
#define SFX_PLYRFIRE_ADDR (SFX_DATA + SFX_PLYRFIRE_OFFSET)
#define SFX_ENMYFIRE_ADDR (SFX_DATA + SFX_ENMYFIRE_OFFSET)
#define SFX_ENMYDIE_ADDR  (SFX_DATA + SFX_ENMYDIE_OFFSET)
#define SFX_PLYRHIT_ADDR  (SFX_DATA + SFX_PLYRHIT_OFFSET)
#define SFX_PLYRDIE_ADDR  (SFX_DATA + SFX_PLYRDIE_OFFSET)
#define SFX_PICKUP_ADDR   (SFX_DATA + SFX_PICKUP_OFFSET)
#define SFX_TALLY_ADDR    (SFX_DATA + SFX_TALLY_OFFSET)
#define SFX_LVLCLEAR_ADDR (SFX_DATA + SFX_LVLCLEAR_OFFSET)
#define SFX_LOWENRGY_ADDR (SFX_DATA + SFX_LOWENRGY_OFFSET)
#define SFX_XTRALIFE_ADDR (SFX_DATA + SFX_XTRALIFE_OFFSET)
#define SFX_VICTORY_ADDR  (SFX_DATA + SFX_VICTORY_OFFSET)

void sfx_init(void);
void sfx_play_player(uint16_t sfx_addr, uint8_t priority); // channel 7
void sfx_play_enemy(uint16_t sfx_addr, uint8_t priority);  // channel 8
void sfx_update(void);
void sfx_stop(void);
void sfx_set_low_energy_muted(bool muted);

#endif
