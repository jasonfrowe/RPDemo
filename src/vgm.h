#ifndef VGM_H
#define VGM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int fd;
    uint32_t data_offset;
    uint32_t loop_offset;
    uint32_t wait_samples;
    bool reached_end;
    bool has_loop;
    bool loop_enabled;

    uint8_t buffer[512];
    uint16_t buffer_pos;
    uint16_t buffer_size;
} vgm_player_t;

bool vgm_open(vgm_player_t *player, const char *path, char *status_line, uint16_t status_size);
void vgm_close(vgm_player_t *player);
bool vgm_restart(vgm_player_t *player);
void vgm_update(vgm_player_t *player,
                uint32_t sample_budget,
                bool *track_ended,
                char *status_line,
                uint16_t status_size);

#endif
