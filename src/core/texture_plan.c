#include "loim/texture_plan.h"

loim_status loim_texture_plan(
    uint32_t width_px,
    uint32_t height_px,
    uint32_t maximum_texture_size,
    loim_texture_tile *tiles,
    size_t tile_capacity,
    size_t *out_tile_count)
{
    uint64_t count64;
    size_t count;
    uint32_t source_y = 0U;
    size_t index;

    if (width_px == 0U || height_px == 0U || maximum_texture_size == 0U ||
        out_tile_count == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    if (width_px > maximum_texture_size) {
        return LOIM_ERROR_OVERFLOW;
    }
    count64 = ((uint64_t)height_px + (uint64_t)maximum_texture_size - 1U) /
        (uint64_t)maximum_texture_size;
    if (count64 > SIZE_MAX) {
        return LOIM_ERROR_OVERFLOW;
    }
    count = (size_t)count64;
    *out_tile_count = count;
    if (tiles == NULL) {
        return tile_capacity == 0U ? LOIM_OK : LOIM_ERROR_INVALID_ARGUMENT;
    }
    if (tile_capacity < count) {
        return LOIM_ERROR_OVERFLOW;
    }
    for (index = 0U; index < count; ++index) {
        uint32_t remaining = height_px - source_y;
        uint32_t tile_height = remaining > maximum_texture_size
            ? maximum_texture_size
            : remaining;

        tiles[index].source_y_px = source_y;
        tiles[index].height_px = tile_height;
        source_y += tile_height;
    }
    return LOIM_OK;
}
