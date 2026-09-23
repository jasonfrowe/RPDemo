#include "sfx.h"

#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>

#include "opl.h"
#include "player_controller.h"

// Every SFX clip lives preloaded in XRAM at XRAM_SFX_DATA (xram.h), baked
// into the ROM image at build time exactly like the sprite/tile bitmaps
// beside it (see CMakeLists.txt's XRAM Assets section) -- not opened as a
// ROM: file the way music streams are. That matters specifically for SFX:
// the RP6502 ROM: filesystem's open() scans the whole named-asset
// directory linearly and only reads on demand (rp6502/src/core/rom/
// asset.c's own comment: "no index and no asset bytes are held in RAM"),
// a fixed cost regardless of file size, paid on *every single trigger* --
// music amortizes that one-time cost over minutes of streaming a track,
// but a bullet fired 60 times a second cannot. Reading straight out of
// XRAM (this file's sfx_xram_player_t) skips that entirely: no open(),
// no directory scan, no read() syscall, just a couple of RIA register
// accesses per command byte, already at least as cheap as a single VGM
// buffer refill and now paid at most once per channel per frame (see
// retriggered_this_frame below).
//
// The command format is the same one generate_sfx.py's SfxBuilder emits
// for the standalone .vgm files (0x5A reg/val, 0x61 16-bit wait, 0x66
// end) minus the 0x40-byte VGM file header -- vgm.c's fuller opcode set
// (loop points, data blocks, skip-N) was never needed here since this
// tool never emits any of that, so this reader only understands the three
// opcodes that actually appear.

// One 60Hz frame's worth of "samples" (this project's VGM convention is
// always 44100 Hz regardless of actual playback rate -- see vgm.c),
// matching music_update()'s own budget. These clips are all well under a
// second.
#define SFX_SAMPLE_BUDGET 735u

// How many frames between retriggers of the low-energy warning beep while
// health stays low -- LowEnrgy is a short clip, not a looped one; this
// timer is what turns it into a periodic pulse instead of a one-shot that
// plays once and falls silent.
#define SFX_LOW_ENERGY_PERIOD_FRAMES 40u

typedef struct {
    uint16_t read_ptr;     // next XRAM address to read; only meaningful while active
    uint16_t wait_samples;
    bool active;
} sfx_xram_player_t;

static uint8_t sfx_xram_read_byte(sfx_xram_player_t *p) {
    RIA.addr0 = p->read_ptr++;
    return RIA.rw0;
}

// Returns false once the clip has ended (0x66) -- the caller stops calling
// this for the frame, same shape as vgm.c's parse_next_command.
static bool sfx_xram_parse_next(sfx_xram_player_t *p) {
    uint8_t cmd = sfx_xram_read_byte(p);

    if (cmd == 0x5A) {
        uint8_t reg = sfx_xram_read_byte(p);
        uint8_t val = sfx_xram_read_byte(p);
        opl_write(reg, val);
        return true;
    }
    if (cmd == 0x61) {
        uint8_t lo = sfx_xram_read_byte(p);
        uint8_t hi = sfx_xram_read_byte(p);
        p->wait_samples = (uint16_t)((uint16_t)lo | ((uint16_t)hi << 8));
        return true;
    }
    // 0x66 (end) or anything else unexpected: stop rather than run off
    // into whatever XRAM holds past this clip.
    p->active = false;
    return false;
}

static void sfx_xram_advance(sfx_xram_player_t *p, uint16_t sample_budget) {
    if (!p->active) {
        return;
    }

    while (sample_budget > 0) {
        if (p->wait_samples > 0) {
            uint16_t step = (p->wait_samples > sample_budget) ? sample_budget : p->wait_samples;
            p->wait_samples = (uint16_t)(p->wait_samples - step);
            sample_budget = (uint16_t)(sample_budget - step);
            continue;
        }
        if (!sfx_xram_parse_next(p)) {
            return;
        }
    }
}

// One of the two independent one-shot channels (7: player, 8: enemy --
// see sfx.h's module comment). Each tracks its own priority so a tier only
// ever competes against events on the SAME channel.
typedef struct {
    sfx_xram_player_t player;
    uint8_t priority; // 0 while nothing is playing
    uint8_t silence_reg;
    // At most one *new* trigger per channel per frame, unless it's a real
    // priority upgrade -- without this, something that fires several
    // same-priority one-shots within a single frame (e.g. an enemy that
    // explodes into a spray of bullets, each independently calling
    // sfx_play_enemy()) restarts the channel's player for every single
    // one, immediately clobbering the previous one before a single frame
    // of it has even rendered. Reset once per frame in sfx_update().
    bool retriggered_this_frame;
} sfx_channel_t;

static sfx_channel_t g_player_ch = {.silence_reg = 0xB7};
static sfx_channel_t g_enemy_ch = {.silence_reg = 0xB8};

static uint16_t g_low_energy_timer = 0;
static bool g_low_energy_muted = false;

static void sfx_channel_init(sfx_channel_t *ch) {
    ch->player.active = false;
    ch->priority = 0;
    ch->retriggered_this_frame = false;
}

static void sfx_channel_play(sfx_channel_t *ch, uint16_t sfx_addr, uint8_t priority) {
    if (ch->priority > 0 && priority < ch->priority) {
        // A lower-priority event while a higher-priority one-shot is
        // still playing on this channel -- drop it rather than cut off
        // something more important.
        return;
    }
    if (ch->retriggered_this_frame && priority <= ch->priority) {
        // Already accepted a new trigger this frame -- drop a same-or-
        // lower-priority repeat rather than restart again (see
        // retriggered_this_frame's comment). A genuine priority upgrade
        // still gets through.
        return;
    }
    ch->retriggered_this_frame = true;

    // Every switch risks leaving a stuck note: the interrupted clip could
    // have ended mid-note with the channel's key-on bit still set, and
    // nothing downstream clears it on its own.
    opl_write(ch->silence_reg, 0x00);

    ch->player.read_ptr = sfx_addr;
    ch->player.wait_samples = 0;
    ch->player.active = true;
    ch->priority = priority;
}

static void sfx_channel_update(sfx_channel_t *ch) {
    if (!ch->player.active) {
        return;
    }
    sfx_xram_advance(&ch->player, SFX_SAMPLE_BUDGET);
    if (!ch->player.active) {
        ch->priority = 0;
    }
}

static void sfx_channel_stop(sfx_channel_t *ch) {
    ch->player.active = false;
    opl_write(ch->silence_reg, 0x00);
    ch->priority = 0;
}

void sfx_init(void) {
    sfx_channel_init(&g_player_ch);
    sfx_channel_init(&g_enemy_ch);
    g_low_energy_timer = 0;
}

void sfx_play_player(uint16_t sfx_addr, uint8_t priority) {
    sfx_channel_play(&g_player_ch, sfx_addr, priority);
}

void sfx_play_enemy(uint16_t sfx_addr, uint8_t priority) {
    sfx_channel_play(&g_enemy_ch, sfx_addr, priority);
}

static void sfx_update_low_energy(void) {
    if (g_low_energy_muted || !player_controller_is_low_health()) {
        g_low_energy_timer = 0;
        return;
    }

    if (g_low_energy_timer > 0) {
        g_low_energy_timer--;
        return;
    }

    // Just another player-channel event, subject to the same priority
    // rules as everything else -- if a higher-priority cue (e.g.
    // PlyrDie) is mid-playback, this retry is dropped and simply tried
    // again next period rather than cutting anything off.
    sfx_play_player(SFX_LOWENRGY_ADDR, SFX_PRIORITY_LOW_ENERGY);
    g_low_energy_timer = SFX_LOW_ENERGY_PERIOD_FRAMES;
}

void sfx_update(void) {
    // Called once per frame, before this frame's gameplay logic runs (see
    // gameplay.c) -- so clearing the flag here means "no new trigger yet
    // this frame" for whatever sfx_play_player()/sfx_play_enemy() calls
    // are about to happen below it in the frame.
    g_player_ch.retriggered_this_frame = false;
    g_enemy_ch.retriggered_this_frame = false;

    sfx_channel_update(&g_player_ch);
    sfx_channel_update(&g_enemy_ch);
    sfx_update_low_energy();
}

void sfx_stop(void) {
    sfx_channel_stop(&g_player_ch);
    sfx_channel_stop(&g_enemy_ch);
    g_low_energy_timer = 0;
}

// The level-bonus screen tallies score over the same player channel this
// warning periodically retriggers on -- muting it there keeps the beep
// from competing with (or just distracting from) the payout SFX while the
// player's health bar is quietly refilling in the background.
void sfx_set_low_energy_muted(bool muted) {
    g_low_energy_muted = muted;
}
