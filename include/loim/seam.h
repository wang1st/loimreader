#ifndef LOIM_SEAM_H
#define LOIM_SEAM_H

#include <stddef.h>
#include <stdint.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct loim_seam_result {
    uint32_t row;
    float quality;
} loim_seam_result;

/*
 * Finds a visually quiet horizontal seam in an RGBA8 image. Transparent
 * pixels are treated as background. The function never retains pixel memory.
 */
loim_status loim_seam_find_rgba8(
    const uint8_t *pixels,
    uint32_t width,
    uint32_t height,
    size_t stride_bytes,
    uint32_t target_row,
    uint32_t search_radius,
    loim_seam_result *out_result);

#ifdef __cplusplus
}
#endif

#endif
