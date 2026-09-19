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

static vgm_player_t g_sfx_player;   // channel 7: one-shot event stingers
static uint8_t g_sfx_priority = 0;  // 0 while nothing is playing

static vgm_player_t g_alarm_player; // channel 8: low-energy warning beep
static uint16_t g_alarm_timer = 0;
static bool g_alarm_playing = false;

static void sfx_silence_channel7(void) {
    opl_write(0xB7, 0x00);
}

static void sfx_silence_channel8(void) {
    opl_write(0xB8, 0x00);
}

void sfx_init(void) {
    g_sfx_player.fd = -1;
    g_alarm_player.fd = -1;
    g_sfx_priority = 0;
    g_alarm_timer = 0;
    g_alarm_playing = false;
}

void sfx_play(const char *path, uint8_t priority) {
    if (g_sfx_priority > 0 && priority < g_sfx_priority) {
        // A lower-priority event while a higher-priority one-shot is
        // still playing -- drop it rather than cut off something more
        // important (e.g. player-fire spam interrupting player-destroyed).
        return;
    }

    if (g_sfx_player.fd >= 0) {
        vgm_close(&g_sfx_player);
    }
    // Every switch risks leaving a stuck note: the interrupted clip could
    // have ended mid-note with channel 7's key-on bit still set, and
    // nothing downstream clears it on its own -- vgm_open() just starts
    // reading a new file, it doesn't know the channel was mid-note.
    sfx_silence_channel7();

    if (vgm_open(&g_sfx_player, path)) {
        g_sfx_player.loop_enabled = false; // one-shots never loop
        g_sfx_priority = priority;
    } else {
        g_sfx_priority = 0;
    }
}

static void sfx_update_oneshot(void) {
    bool track_ended = false;

    if (g_sfx_player.fd < 0) {
        return;
    }

    vgm_update(&g_sfx_player, SFX_SAMPLE_BUDGET, &track_ended);
    if (track_ended) {
        vgm_close(&g_sfx_player);
        g_sfx_priority = 0;
    }
}

static void sfx_update_alarm(void) {
    if (!player_controller_is_low_health()) {
        if (g_alarm_player.fd >= 0) {
            vgm_close(&g_alarm_player);
            sfx_silence_channel8();
        }
        g_alarm_playing = false;
        g_alarm_timer = 0;
        return;
    }

    if (g_alarm_player.fd >= 0) {
        bool track_ended = false;
        vgm_update(&g_alarm_player, SFX_SAMPLE_BUDGET, &track_ended);
        if (track_ended) {
            vgm_close(&g_alarm_player);
            g_alarm_playing = false;
        }
        return;
    }

    if (g_alarm_timer > 0) {
        g_alarm_timer--;
        return;
    }

    if (vgm_open(&g_alarm_player, "ROM:LowEnrgy.vgm")) {
        g_alarm_player.loop_enabled = false;
        g_alarm_playing = true;
    }
    g_alarm_timer = SFX_LOW_ENERGY_PERIOD_FRAMES;
}

void sfx_update(void) {
    sfx_update_oneshot();
    sfx_update_alarm();
}

void sfx_stop(void) {
    if (g_sfx_player.fd >= 0) {
        vgm_close(&g_sfx_player);
    }
    if (g_alarm_player.fd >= 0) {
        vgm_close(&g_alarm_player);
    }
    sfx_silence_channel7();
    sfx_silence_channel8();
    g_sfx_priority = 0;
    g_alarm_playing = false;
    g_alarm_timer = 0;
}
