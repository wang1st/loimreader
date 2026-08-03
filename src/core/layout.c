#include "loim/layout.h"

#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

typedef struct loim_normalized_source {
    uint64_t start;
    uint32_t height;
} loim_normalized_source;

static uint64_t loim_absolute_distance(uint64_t left, uint64_t right)
{
    return left >= right ? left - right : right - left;
}

static loim_status loim_normalized_height(
    const loim_source_internal *source,
    uint32_t content_width,
    uint32_t *out_height)
{
    uint64_t scaled;

    if (source == NULL || out_height == NULL || source->width_px == 0U) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    scaled = (uint64_t)source->height_px * (uint64_t)content_width;
    scaled = (scaled + (uint64_t)source->width_px - 1U) / (uint64_t)source->width_px;
    if (scaled == 0U || scaled > UINT32_MAX) {
        return LOIM_ERROR_OVERFLOW;
    }
    *out_height = (uint32_t)scaled;
    return LOIM_OK;
}

static loim_status loim_build_normalized_sources(
    const loim_document *document,
    const loim_layout_options *options,
    loim_normalized_source **out_sources,
    uint64_t *out_total_height)
{
    loim_normalized_source *normalized;
    uint64_t cursor = 0U;
    size_t index;

    if (document->source_count > SIZE_MAX / sizeof(*normalized)) {
        return LOIM_ERROR_OVERFLOW;
    }
    normalized = calloc(document->source_count, sizeof(*normalized));
    if (normalized == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0U; index < document->source_count; ++index) {
        loim_status status;
        uint32_t height = 0U;

        status = loim_normalized_height(
            &document->sources[index], options->content_width_px, &height);
        if (status != LOIM_OK) {
            free(normalized);
            return status;
        }
        normalized[index].start = cursor;
        normalized[index].height = height;
        if (cursor > UINT64_MAX - (uint64_t)height) {
            free(normalized);
            return LOIM_ERROR_OVERFLOW;
        }
        cursor += (uint64_t)height;
        if (index + 1U < document->source_count) {
            if (cursor > UINT64_MAX - (uint64_t)options->inter_image_gap_px) {
                free(normalized);
                return LOIM_ERROR_OVERFLOW;
            }
            cursor += (uint64_t)options->inter_image_gap_px;
        }
    }

    *out_sources = normalized;
    *out_total_height = cursor;
    return LOIM_OK;
}

static double loim_hint_bonus(loim_split_hint_kind kind, float quality)
{
    double weight = 0.42;

    if (kind == LOIM_SPLIT_HINT_CONTENT_GAP) {
        weight = 0.52;
    } else if (kind == LOIM_SPLIT_HINT_MANUAL) {
        weight = 1.0;
    }
    return weight * (double)quality;
}

static void loim_consider_break(
    uint64_t candidate,
    double bonus,
    uint64_t ideal,
    uint64_t legal_minimum,
    uint64_t legal_maximum,
    uint32_t target_height,
    uint64_t *best_candidate,
    double *best_score)
{
    double distance;
    double score;

    if (candidate < legal_minimum || candidate > legal_maximum) {
        return;
    }
    distance = (double)loim_absolute_distance(candidate, ideal) / (double)target_height;
    score = distance - bonus;
    if (score < *best_score ||
        (score == *best_score &&
         loim_absolute_distance(candidate, ideal) <
             loim_absolute_distance(*best_candidate, ideal))) {
        *best_score = score;
        *best_candidate = candidate;
    }
}

static uint64_t loim_choose_page_end(
    const loim_document *document,
    const loim_normalized_source *normalized,
    const loim_layout_options *options,
    uint64_t cursor,
    uint64_t total_height)
{
    uint64_t ideal = cursor + (uint64_t)options->target_page_height_px;
    uint64_t legal_minimum = cursor + (uint64_t)options->minimum_page_height_px;
    uint64_t legal_maximum = cursor + (uint64_t)options->maximum_page_height_px;
    uint64_t search_minimum;
    uint64_t search_maximum;
    uint64_t best_candidate;
    double best_score = DBL_MAX;
    size_t source_index;

    if (legal_maximum > total_height) {
        legal_maximum = total_height;
    }
    if (legal_minimum > total_height) {
        legal_minimum = total_height;
    }
    if (ideal < legal_minimum) {
        ideal = legal_minimum;
    } else if (ideal > legal_maximum) {
        ideal = legal_maximum;
    }
    search_minimum = ideal > (uint64_t)options->search_radius_px
        ? ideal - (uint64_t)options->search_radius_px
        : cursor;
    search_maximum = ideal + (uint64_t)options->search_radius_px;
    if (search_maximum < ideal || search_maximum > legal_maximum) {
        search_maximum = legal_maximum;
    }
    if (search_minimum < legal_minimum) {
        search_minimum = legal_minimum;
    }

    best_candidate = ideal;
    loim_consider_break(
        ideal, 0.0, ideal, legal_minimum, legal_maximum,
        options->target_page_height_px, &best_candidate, &best_score);

    for (source_index = 0U; source_index < document->source_count; ++source_index) {
        const loim_source_internal *source = &document->sources[source_index];
        uint64_t source_end = normalized[source_index].start +
            (uint64_t)normalized[source_index].height;
        size_t hint_index;

        if (source_end >= search_minimum && source_end <= search_maximum &&
            source_index + 1U < document->source_count) {
            loim_consider_break(
                source_end, 0.60, ideal, legal_minimum, legal_maximum,
                options->target_page_height_px, &best_candidate, &best_score);
            loim_consider_break(
                source_end + (uint64_t)options->inter_image_gap_px,
                0.58, ideal, legal_minimum, legal_maximum,
                options->target_page_height_px, &best_candidate, &best_score);
        }

        for (hint_index = 0U; hint_index < source->hint_count; ++hint_index) {
            const loim_split_hint_internal *hint = &source->hints[hint_index];
            uint64_t offset =
                ((uint64_t)hint->row * (uint64_t)normalized[source_index].height +
                 ((uint64_t)source->height_px / 2U)) /
                (uint64_t)source->height_px;
            uint64_t candidate = normalized[source_index].start + offset;

            if (candidate >= search_minimum && candidate <= search_maximum) {
                loim_consider_break(
                    candidate,
                    loim_hint_bonus(hint->kind, hint->quality),
                    ideal,
                    legal_minimum,
                    legal_maximum,
                    options->target_page_height_px,
                    &best_candidate,
                    &best_score);
            }
        }
    }
    return best_candidate;
}

static loim_status loim_append_page(
    loim_layout *layout,
    size_t *page_capacity,
    uint64_t start,
    uint64_t end)
{
    loim_status status;
    loim_page *page;
    uint64_t height;

    if (end <= start) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    height = end - start;
    if (height > UINT32_MAX) {
        return LOIM_ERROR_OVERFLOW;
    }
    status = loim_grow_array(
        (void **)&layout->pages,
        sizeof(*layout->pages),
        layout->page_count + 1U,
        page_capacity);
    if (status != LOIM_OK) {
        return status;
    }
    page = &layout->pages[layout->page_count];
    memset(page, 0, sizeof(*page));
    page->virtual_y_px = start;
    page->height_px = (uint32_t)height;
    layout->page_count += 1U;
    return LOIM_OK;
}

static loim_status loim_append_slice(
    loim_layout *layout,
    size_t *slice_capacity,
    size_t source_index,
    const loim_source_internal *source,
    const loim_normalized_source *normalized,
    uint64_t page_start,
    uint64_t overlap_start,
    uint64_t overlap_end)
{
    loim_status status;
    loim_page_slice *slice;
    uint64_t normalized_offset_start = overlap_start - normalized->start;
    uint64_t normalized_offset_end = overlap_end - normalized->start;
    uint64_t source_start;
    uint64_t source_end;
    uint64_t destination_y = overlap_start - page_start;
    uint64_t destination_height = overlap_end - overlap_start;

    source_start = normalized_offset_start * (uint64_t)source->height_px /
        (uint64_t)normalized->height;
    source_end = normalized_offset_end * (uint64_t)source->height_px /
        (uint64_t)normalized->height;
    if (overlap_end == normalized->start + (uint64_t)normalized->height) {
        source_end = (uint64_t)source->height_px;
    }
    if (source_start > UINT32_MAX || source_end > UINT32_MAX ||
        destination_y > UINT32_MAX || destination_height > UINT32_MAX ||
        source_end <= source_start) {
        return LOIM_ERROR_OVERFLOW;
    }

    status = loim_grow_array(
        (void **)&layout->slices,
        sizeof(*layout->slices),
        layout->slice_count + 1U,
        slice_capacity);
    if (status != LOIM_OK) {
        return status;
    }
    slice = &layout->slices[layout->slice_count];
    slice->source_index = source_index;
    slice->source_y_px = (uint32_t)source_start;
    slice->source_height_px = (uint32_t)(source_end - source_start);
    slice->destination_y_px = (uint32_t)destination_y;
    slice->destination_height_px = (uint32_t)destination_height;
    layout->slice_count += 1U;
    return LOIM_OK;
}

static loim_status loim_build_slices(
    const loim_document *document,
    const loim_normalized_source *normalized,
    loim_layout *layout)
{
    size_t slice_capacity = 0U;
    size_t page_index;

    for (page_index = 0U; page_index < layout->page_count; ++page_index) {
        loim_page *page = &layout->pages[page_index];
        uint64_t page_start = page->virtual_y_px;
        uint64_t page_end = page_start + (uint64_t)page->height_px;
        size_t source_index;

        page->first_slice = layout->slice_count;
        for (source_index = 0U; source_index < document->source_count; ++source_index) {
            uint64_t source_start = normalized[source_index].start;
            uint64_t source_end = source_start + (uint64_t)normalized[source_index].height;
            uint64_t overlap_start = page_start > source_start ? page_start : source_start;
            uint64_t overlap_end = page_end < source_end ? page_end : source_end;

            if (overlap_start < overlap_end) {
                loim_status status = loim_append_slice(
                    layout,
                    &slice_capacity,
                    source_index,
                    &document->sources[source_index],
                    &normalized[source_index],
                    page_start,
                    overlap_start,
                    overlap_end);
                if (status != LOIM_OK) {
                    return status;
                }
            }
        }
        page->slice_count = layout->slice_count - page->first_slice;
    }
    return LOIM_OK;
}

void loim_layout_options_a4(loim_layout_options *out_options, uint32_t content_width_px)
{
    uint64_t page_height;

    if (out_options == NULL) {
        return;
    }
    memset(out_options, 0, sizeof(*out_options));
    out_options->content_width_px = content_width_px;
    page_height = ((uint64_t)content_width_px * 297U + 104U) / 210U;
    if (page_height > UINT32_MAX) {
        page_height = UINT32_MAX;
    }
    out_options->target_page_height_px = (uint32_t)page_height;
    out_options->minimum_page_height_px =
        (uint32_t)(page_height * 80U / 100U);
    out_options->maximum_page_height_px =
        (uint32_t)(page_height * 120U / 100U);
    out_options->search_radius_px =
        (uint32_t)(page_height * 15U / 100U);
    out_options->inter_image_gap_px = content_width_px / 60U;
}

loim_status loim_layout_build(
    const loim_document *document,
    const loim_layout_options *options,
    loim_layout *out_layout)
{
    loim_normalized_source *normalized = NULL;
    uint64_t total_height = 0U;
    uint64_t cursor = 0U;
    size_t page_capacity = 0U;
    loim_status status;

    if (document == NULL || options == NULL || out_layout == NULL ||
        document->source_count == 0U || options->content_width_px == 0U ||
        options->target_page_height_px == 0U ||
        options->minimum_page_height_px == 0U ||
        options->minimum_page_height_px > options->target_page_height_px ||
        options->target_page_height_px > options->maximum_page_height_px) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    memset(out_layout, 0, sizeof(*out_layout));

    status = loim_build_normalized_sources(
        document, options, &normalized, &total_height);
    if (status != LOIM_OK) {
        return status;
    }
    out_layout->virtual_height_px = total_height;

    while (cursor < total_height) {
        uint64_t remaining = total_height - cursor;
        uint64_t page_end;

        if (remaining <= (uint64_t)options->maximum_page_height_px) {
            page_end = total_height;
        } else {
            page_end = loim_choose_page_end(
                document, normalized, options, cursor, total_height);
        }
        if (page_end <= cursor) {
            status = LOIM_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        status = loim_append_page(out_layout, &page_capacity, cursor, page_end);
        if (status != LOIM_OK) {
            goto cleanup;
        }
        cursor = page_end;
    }

    status = loim_build_slices(document, normalized, out_layout);

cleanup:
    free(normalized);
    if (status != LOIM_OK) {
        loim_layout_destroy(out_layout);
    }
    return status;
}

void loim_layout_destroy(loim_layout *layout)
{
    if (layout == NULL) {
        return;
    }
    free(layout->pages);
    free(layout->slices);
    memset(layout, 0, sizeof(*layout));
}
