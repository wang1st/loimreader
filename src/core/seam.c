#include "loim/seam.h"

#include <float.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t loim_luma(const uint8_t *pixel)
{
    return ((uint32_t)pixel[0] * 54U +
            (uint32_t)pixel[1] * 183U +
            (uint32_t)pixel[2] * 19U) >> 8U;
}

static double loim_row_quality(
    const uint8_t *pixels,
    uint32_t width,
    size_t stride,
    uint32_t row)
{
    const uint8_t *current = pixels + (size_t)row * stride;
    const uint8_t *previous = row == 0U ? current : current - stride;
    uint64_t ink_sum = 0U;
    uint64_t edge_sum = 0U;
    uint32_t column;

    for (column = 0U; column < width; ++column) {
        const uint8_t *pixel = current + (size_t)column * 4U;
        const uint8_t *above = previous + (size_t)column * 4U;
        uint32_t alpha = (uint32_t)pixel[3];
        uint32_t luma = loim_luma(pixel);
        uint32_t above_luma = loim_luma(above);
        uint32_t darkness = (255U - luma) * alpha / 255U;
        uint32_t edge = luma >= above_luma ? luma - above_luma : above_luma - luma;

        ink_sum += (uint64_t)darkness;
        edge_sum += (uint64_t)edge * (uint64_t)alpha / 255U;
    }

    return 0.82 * (1.0 - (double)ink_sum / ((double)width * 255.0)) +
           0.18 * (1.0 - (double)edge_sum / ((double)width * 255.0));
}

loim_status loim_seam_find_rgba8(
    const uint8_t *pixels,
    uint32_t width,
    uint32_t height,
    size_t stride_bytes,
    uint32_t target_row,
    uint32_t search_radius,
    loim_seam_result *out_result)
{
    uint32_t first_row;
    uint32_t last_row;
    uint32_t row;
    uint32_t best_row;
    double best_quality = -DBL_MAX;

    if (pixels == NULL || out_result == NULL || width == 0U || height < 2U ||
        target_row >= height ||
#if SIZE_MAX < UINT64_MAX
        width > SIZE_MAX / 4U ||
#endif
        stride_bytes < (size_t)width * 4U ||
        (size_t)(height - 1U) > SIZE_MAX / stride_bytes) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }

    first_row = target_row > search_radius ? target_row - search_radius : 1U;
    if (first_row == 0U) {
        first_row = 1U;
    }
    last_row = target_row + search_radius;
    if (last_row < target_row || last_row >= height) {
        last_row = height - 1U;
    }
    best_row = target_row == 0U ? 1U : target_row;

    for (row = first_row; row <= last_row; ++row) {
        uint32_t distance = row >= target_row ? row - target_row : target_row - row;
        double distance_penalty = search_radius == 0U
            ? 0.0
            : 0.16 * (double)distance / (double)search_radius;
        double quality = loim_row_quality(pixels, width, stride_bytes, row) -
            distance_penalty;

        if (quality > best_quality) {
            best_quality = quality;
            best_row = row;
        }
    }
    if (best_quality < 0.0) {
        best_quality = 0.0;
    } else if (best_quality > 1.0) {
        best_quality = 1.0;
    }
    out_result->row = best_row;
    out_result->quality = (float)best_quality;
    return LOIM_OK;
}
