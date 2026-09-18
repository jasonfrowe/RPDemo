#include "music.h"

#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "opl.h"
#include "vgm.h"

// The emulator's OPL register-write queue is drained by the host's audio
// callback thread, which can take a variable amount of real time to start
// pulling samples after the machine boots. opl_init()'s register-clear burst
// plus the VGM's own startup instrument dump can together overflow that
// queue before draining has begun, silently dropping writes (observed as a
// channel that randomly never gets its instrument set up). Waiting a few
// vsyncs here gives the audio thread time to start before the heavy bursts
// land.
static void wait_vsyncs(uint8_t count) {
    while (count--) {
        uint8_t v = RIA.vsync;
        while (RIA.vsync == v) {
        }
    }
}

static vgm_player_t g_player;
static const char *k_music_path = "ROM:RESOURCE.001.vgm";

static bool music_start_current(void) {
    opl_init();
    if (!vgm_open(&g_player, k_music_path)) {
        return false;
    }

    // vgm_open defaults to using loop tags when present.
    return true;
}

void music_init(void) {
    memset(&g_player, 0, sizeof(g_player));
    g_player.fd = -1;

    opl_config(1, OPL_XRAM_ADDR);
    music_start_current();

    // See wait_vsyncs() above: let the host audio thread get going before
    // gameplay starts feeding it VGM commands on top of opl_init()'s burst.
    wait_vsyncs(4);
}

bool music_set_track(const char *path) {
    if (path == NULL) {
        return false;
    }

    if (strcmp(k_music_path, path) == 0) {
        return true;
    }

    if (g_player.fd >= 0) {
        vgm_close(&g_player);
        g_player.fd = -1;
    }

    k_music_path = path;
    return music_start_current();
}

void music_stop(void)
{
    if (g_player.fd >= 0) {
        vgm_close(&g_player);
        g_player.fd = -1;
    }
}

void music_update(void) {
    bool track_ended = false;

    if (g_player.fd < 0) {
        return;
    }

    vgm_update(&g_player, 735u, &track_ended);
    if (track_ended) {
        if (!vgm_restart(&g_player)) {
            vgm_close(&g_player);
            music_start_current();
        }
    }
}
