#ifndef LOIM_DOCUMENT_H
#define LOIM_DOCUMENT_H

#include <stddef.h>
#include <stdint.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct loim_document loim_document;

typedef enum loim_split_hint_kind {
    LOIM_SPLIT_HINT_WHITESPACE = 0,
    LOIM_SPLIT_HINT_CONTENT_GAP = 1,
    LOIM_SPLIT_HINT_MANUAL = 2
} loim_split_hint_kind;

typedef struct loim_source_info {
    const char *path;
    uint32_t width_px;
    uint32_t height_px;
} loim_source_info;

typedef struct loim_split_hint_info {
    uint32_t row;
    float quality;
    loim_split_hint_kind kind;
} loim_split_hint_info;

loim_status loim_document_create(loim_document **out_document);
void loim_document_destroy(loim_document *document);

loim_status loim_document_add_source(
    loim_document *document,
    const loim_source_info *source,
    size_t *out_source_index);

loim_status loim_document_add_split_hint(
    loim_document *document,
    size_t source_index,
    uint32_t source_row,
    float quality,
    loim_split_hint_kind kind);

size_t loim_document_split_hint_count(
    const loim_document *document,
    size_t source_index);

loim_status loim_document_split_hint_at(
    const loim_document *document,
    size_t source_index,
    size_t hint_index,
    loim_split_hint_info *out_hint);

loim_status loim_document_remove_split_hint(
    loim_document *document,
    size_t source_index,
    size_t hint_index);

size_t loim_document_source_count(const loim_document *document);
loim_status loim_document_source_at(
    const loim_document *document,
    size_t source_index,
    loim_source_info *out_source);

#ifdef __cplusplus
}
#endif

#endif
