#include "sfx.h"

#include <stdbool.h>
#include <stdint.h>

#include "opl.h"
#include "player_controller.h"
#include "vgm.h"

// One 60Hz frame's worth of VGM samples, matching music_update()'s own
// budget (src/music.c) -- these clips are all well under a second, so the
// 256-command-per-call cap in vgm.c never comes close to mattering here.
#define SFX_SAMPLE_BUDGET 735u

// How many frames between retriggers of the low-energy warning beep while
// health stays low -- LowEnrgy.vgm is a short clip, not a looped one (see
// generate_sfx.py); this timer is what turns it into a periodic pulse
// instead of a one-shot that plays once and falls silent.
#define SFX_LOW_ENERGY_PERIOD_FRAMES 40u

// One of the two independent one-shot channels (7: player, 8: enemy --
// see sfx.h's module comment). Each tracks its own priority so a tier only
// ever competes against events on the SAME channel.
typedef struct {
    vgm_player_t player;
    uint8_t priority; // 0 while nothing is playing
    uint8_t silence_reg;
} sfx_channel_t;

static sfx_channel_t g_player_ch = {.silence_reg = 0xB7};
static sfx_channel_t g_enemy_ch = {.silence_reg = 0xB8};

static uint16_t g_low_energy_timer = 0;

static void sfx_channel_init(sfx_channel_t *ch) {
    ch->player.fd = -1;
    ch->priority = 0;
}

static void sfx_channel_play(sfx_channel_t *ch, const char *path, uint8_t priority) {
    if (ch->priority > 0 && priority < ch->priority) {
        // A lower-priority event while a higher-priority one-shot is
        // still playing on this channel -- drop it rather than cut off
        // something more important.
        return;
    }

    if (ch->player.fd >= 0) {
        vgm_close(&ch->player);
    }
    // Every switch risks leaving a stuck note: the interrupted clip could
    // have ended mid-note with the channel's key-on bit still set, and
    // nothing downstream clears it on its own -- vgm_open() just starts
    // reading a new file, it doesn't know the channel was mid-note.
    opl_write(ch->silence_reg, 0x00);

    if (vgm_open(&ch->player, path)) {
        ch->player.loop_enabled = false; // one-shots never loop
        ch->priority = priority;
    } else {
        ch->priority = 0;
    }
}

static void sfx_channel_update(sfx_channel_t *ch) {
    bool track_ended = false;

    if (ch->player.fd < 0) {
        return;
    }

    vgm_update(&ch->player, SFX_SAMPLE_BUDGET, &track_ended);
    if (track_ended) {
        vgm_close(&ch->player);
        ch->priority = 0;
    }
}

static void sfx_channel_stop(sfx_channel_t *ch) {
    if (ch->player.fd >= 0) {
        vgm_close(&ch->player);
    }
    opl_write(ch->silence_reg, 0x00);
    ch->priority = 0;
}

void sfx_init(void) {
    sfx_channel_init(&g_player_ch);
    sfx_channel_init(&g_enemy_ch);
    g_low_energy_timer = 0;
}

void sfx_play_player(const char *path, uint8_t priority) {
    sfx_channel_play(&g_player_ch, path, priority);
}

void sfx_play_enemy(const char *path, uint8_t priority) {
    sfx_channel_play(&g_enemy_ch, path, priority);
}

static void sfx_update_low_energy(void) {
    if (!player_controller_is_low_health()) {
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
    sfx_play_player("ROM:LowEnrgy.vgm", SFX_PRIORITY_LOW_ENERGY);
    g_low_energy_timer = SFX_LOW_ENERGY_PERIOD_FRAMES;
}

void sfx_update(void) {
    sfx_channel_update(&g_player_ch);
    sfx_channel_update(&g_enemy_ch);
    sfx_update_low_energy();
}

void sfx_stop(void) {
    sfx_channel_stop(&g_player_ch);
    sfx_channel_stop(&g_enemy_ch);
    g_low_energy_timer = 0;
}
