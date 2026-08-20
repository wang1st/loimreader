#ifndef LOIM_TEXTURE_PLAN_H
#define LOIM_TEXTURE_PLAN_H

#include <stddef.h>
#include <stdint.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct loim_texture_tile {
    uint32_t source_y_px;
    uint32_t height_px;
} loim_texture_tile;

loim_status loim_texture_plan(
    uint32_t width_px,
    uint32_t height_px,
    uint32_t maximum_texture_size,
    loim_texture_tile *tiles,
    size_t tile_capacity,
    size_t *out_tile_count);

#ifdef __cplusplus
}
#endif

#endif
