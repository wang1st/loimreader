#ifndef LOIM_INTERNAL_H
#define LOIM_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "loim/document.h"

typedef struct loim_split_hint_internal {
    uint32_t row;
    float quality;
    loim_split_hint_kind kind;
} loim_split_hint_internal;

typedef struct loim_source_internal {
    char *path;
    uint32_t width_px;
    uint32_t height_px;
    loim_split_hint_internal *hints;
    size_t hint_count;
    size_t hint_capacity;
} loim_source_internal;

struct loim_document {
    loim_source_internal *sources;
    size_t source_count;
    size_t source_capacity;
};

loim_status loim_grow_array(
    void **array,
    size_t element_size,
    size_t required_count,
    size_t *capacity);

#endif
