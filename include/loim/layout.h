#ifndef LOIM_LAYOUT_H
#define LOIM_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

#include "loim/document.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct loim_layout_options {
    uint32_t content_width_px;
    uint32_t target_page_height_px;
    uint32_t minimum_page_height_px;
    uint32_t maximum_page_height_px;
    uint32_t search_radius_px;
    uint32_t inter_image_gap_px;
} loim_layout_options;

typedef struct loim_page_slice {
    size_t source_index;
    uint32_t source_y_px;
    uint32_t source_height_px;
    uint32_t destination_y_px;
    uint32_t destination_height_px;
} loim_page_slice;

typedef struct loim_page {
    uint64_t virtual_y_px;
    uint32_t height_px;
    size_t first_slice;
    size_t slice_count;
} loim_page;

typedef struct loim_layout {
    loim_page *pages;
    size_t page_count;
    loim_page_slice *slices;
    size_t slice_count;
    uint64_t virtual_height_px;
} loim_layout;

void loim_layout_options_a4(loim_layout_options *out_options, uint32_t content_width_px);
loim_status loim_layout_build(
    const loim_document *document,
    const loim_layout_options *options,
    loim_layout *out_layout);
void loim_layout_destroy(loim_layout *layout);

#ifdef __cplusplus
}
#endif

#endif
