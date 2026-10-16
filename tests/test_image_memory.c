#include "image_memory.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "failed at line %d: %s\n", __LINE__, #condition); \
    return EXIT_FAILURE; } } while (0)

int main(void)
{
    const uint64_t gib = 1024ULL * 1024ULL * 1024ULL;
    CHECK(loim_image_pixel_budget(8U * gib, 6U * gib) == 2U * gib / 12U);
    CHECK(loim_image_pixel_budget(8U * gib, gib) == gib / 24U);
    CHECK(loim_image_pixel_budget(32U * gib, 24U * gib) ==
        (8U * gib > SIZE_MAX ? (uint64_t)SIZE_MAX : 8U * gib) / 12U);
    CHECK(loim_image_pixel_budget(8U * gib, 0U) == gib / 12U);
    CHECK(loim_image_pixel_budget(0U, 0U) == 0U);
    CHECK(loim_image_pixel_budget(0U, gib) == gib / 24U);
    CHECK(loim_image_pixel_budget(8U * gib, 1U) == 0U);
    /* The user's 1200 x 102654 screenshot fits with sufficient headroom. */
    CHECK(loim_image_pixel_budget(8U * gib, 6U * gib) >= 1200ULL * 102654ULL);
    CHECK(loim_image_pixel_budget(8U * gib, gib) < 1200ULL * 102654ULL);
    CHECK(loim_image_pixel_budget(UINT64_MAX, UINT64_MAX) <= SIZE_MAX / 12U);
    return EXIT_SUCCESS;
}
