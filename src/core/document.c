#include "loim/document.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

loim_status loim_grow_array(
    void **array,
    size_t element_size,
    size_t required_count,
    size_t *capacity)
{
    size_t next_capacity;
    void *next;

    if (array == NULL || capacity == NULL || element_size == 0U) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    if (required_count <= *capacity) {
        return LOIM_OK;
    }

    next_capacity = *capacity == 0U ? 4U : *capacity;
    while (next_capacity < required_count) {
        if (next_capacity > SIZE_MAX / 2U) {
            next_capacity = required_count;
            break;
        }
        next_capacity *= 2U;
    }
    if (next_capacity > SIZE_MAX / element_size) {
        return LOIM_ERROR_OVERFLOW;
    }

    next = realloc(*array, next_capacity * element_size);
    if (next == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    *array = next;
    *capacity = next_capacity;
    return LOIM_OK;
}

static char *loim_copy_string(const char *value)
{
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }
    length = strlen(value);
    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = malloc(length + 1U);
    if (copy != NULL) {
        memcpy(copy, value, length + 1U);
    }
    return copy;
}

loim_status loim_document_create(loim_document **out_document)
{
    loim_document *document;

    if (out_document == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    *out_document = NULL;
    document = calloc(1U, sizeof(*document));
    if (document == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    *out_document = document;
    return LOIM_OK;
}

void loim_document_destroy(loim_document *document)
{
    size_t index;

    if (document == NULL) {
        return;
    }
    for (index = 0U; index < document->source_count; ++index) {
        free(document->sources[index].path);
        free(document->sources[index].hints);
    }
    free(document->sources);
    free(document);
}

loim_status loim_document_add_source(
    loim_document *document,
    const loim_source_info *source,
    size_t *out_source_index)
{
    loim_status status;
    loim_source_internal *destination;
    char *path;

    if (document == NULL || source == NULL || source->path == NULL ||
        source->path[0] == '\0' || source->width_px == 0U || source->height_px == 0U) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }

    path = loim_copy_string(source->path);
    if (path == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    status = loim_grow_array(
        (void **)&document->sources,
        sizeof(*document->sources),
        document->source_count + 1U,
        &document->source_capacity);
    if (status != LOIM_OK) {
        free(path);
        return status;
    }

    destination = &document->sources[document->source_count];
    memset(destination, 0, sizeof(*destination));
    destination->path = path;
    destination->width_px = source->width_px;
    destination->height_px = source->height_px;
    if (out_source_index != NULL) {
        *out_source_index = document->source_count;
    }
    document->source_count += 1U;
    return LOIM_OK;
}

loim_status loim_document_add_split_hint(
    loim_document *document,
    size_t source_index,
    uint32_t source_row,
    float quality,
    loim_split_hint_kind kind)
{
    loim_source_internal *source;
    loim_split_hint_internal *hint;
    loim_status status;

    if (document == NULL || source_index >= document->source_count ||
        quality < 0.0F || quality > 1.0F ||
        kind < LOIM_SPLIT_HINT_WHITESPACE || kind > LOIM_SPLIT_HINT_MANUAL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    source = &document->sources[source_index];
    if (source_row == 0U || source_row >= source->height_px) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }

    status = loim_grow_array(
        (void **)&source->hints,
        sizeof(*source->hints),
        source->hint_count + 1U,
        &source->hint_capacity);
    if (status != LOIM_OK) {
        return status;
    }
    hint = &source->hints[source->hint_count];
    hint->row = source_row;
    hint->quality = quality;
    hint->kind = kind;
    source->hint_count += 1U;
    return LOIM_OK;
}

size_t loim_document_split_hint_count(
    const loim_document *document,
    size_t source_index)
{
    if (document == NULL || source_index >= document->source_count) {
        return 0U;
    }
    return document->sources[source_index].hint_count;
}

loim_status loim_document_split_hint_at(
    const loim_document *document,
    size_t source_index,
    size_t hint_index,
    loim_split_hint_info *out_hint)
{
    const loim_source_internal *source;
    const loim_split_hint_internal *hint;

    if (document == NULL || out_hint == NULL ||
        source_index >= document->source_count) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    source = &document->sources[source_index];
    if (hint_index >= source->hint_count) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    hint = &source->hints[hint_index];
    out_hint->row = hint->row;
    out_hint->quality = hint->quality;
    out_hint->kind = hint->kind;
    return LOIM_OK;
}

loim_status loim_document_remove_split_hint(
    loim_document *document,
    size_t source_index,
    size_t hint_index)
{
    loim_source_internal *source;

    if (document == NULL || source_index >= document->source_count) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    source = &document->sources[source_index];
    if (hint_index >= source->hint_count) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    if (hint_index + 1U < source->hint_count) {
        memmove(
            &source->hints[hint_index],
            &source->hints[hint_index + 1U],
            (source->hint_count - hint_index - 1U) * sizeof(*source->hints));
    }
    source->hint_count -= 1U;
    return LOIM_OK;
}

size_t loim_document_source_count(const loim_document *document)
{
    return document == NULL ? 0U : document->source_count;
}

loim_status loim_document_source_at(
    const loim_document *document,
    size_t source_index,
    loim_source_info *out_source)
{
    const loim_source_internal *source;

    if (document == NULL || out_source == NULL || source_index >= document->source_count) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    source = &document->sources[source_index];
    out_source->path = source->path;
    out_source->width_px = source->width_px;
    out_source->height_px = source->height_px;
    return LOIM_OK;
}
