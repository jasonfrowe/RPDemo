#include "music.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "opl.h"
#include "vgm.h"

static vgm_player_t g_player;
static char g_status_line[64];
static const char *k_music_path = "ROM:RESOURCE.001.vgm";

static bool music_start_current(void) {
    opl_init();
    if (!vgm_open(&g_player, k_music_path, g_status_line, sizeof(g_status_line))) {
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

    vgm_update(&g_player, 735u, &track_ended, g_status_line, sizeof(g_status_line));
    if (track_ended) {
        vgm_close(&g_player);
        music_start_current();
    }
}
