#include "loim/presentation.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOIM_PREVIEW_BASE_WIDTH_RATIO 0.74F
#define LOIM_PREVIEW_HORIZONTAL_PADDING 32.0F
#define LOIM_PREVIEW_MINIMUM_WIDTH 90.0F
#define LOIM_PREVIEW_PAGE_GAP 24.0F
#define LOIM_PREVIEW_TOP_INSET 16.0F
#define LOIM_PREVIEW_SCROLL_OVERSCAN 18.0F
#define LOIM_A4_HEIGHT_TO_WIDTH (297.0F / 210.0F)

size_t loim_columns_normalize(size_t columns)
{
    return columns < 1U ? 1U : columns > 3U ? 3U : columns;
}

size_t loim_columns_next(size_t columns)
{
    columns = loim_columns_normalize(columns);
    return columns == 3U ? 1U : columns + 1U;
}

size_t loim_sheet_slot_count(size_t columns)
{
    columns = loim_columns_normalize(columns);
    return columns * columns;
}

size_t loim_sheet_slot_column(size_t columns, size_t slot)
{
    columns = loim_columns_normalize(columns);
    return slot / columns;
}

size_t loim_sheet_slot_row(size_t columns, size_t slot)
{
    columns = loim_columns_normalize(columns);
    return slot % columns;
}

size_t loim_sheet_count(size_t slice_count, size_t columns)
{
    size_t slots = loim_sheet_slot_count(columns);

    return slice_count == 0U ? 0U : 1U + (slice_count - 1U) / slots;
}

size_t loim_sheet_slice_index(size_t sheet_index, size_t columns, size_t slot)
{
    size_t slots = loim_sheet_slot_count(columns);

    if (sheet_index > (SIZE_MAX - slot) / slots) {
        return SIZE_MAX;
    }
    return sheet_index * slots + slot;
}

loim_page_number_mode loim_page_number_normalize(loim_page_number_mode mode)
{
    return mode == LOIM_PAGE_NUMBER_NONE ||
           mode == LOIM_PAGE_NUMBER_BOTTOM_RIGHT ||
           mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER
        ? mode
        : LOIM_PAGE_NUMBER_NONE;
}

loim_page_number_mode loim_page_number_next(loim_page_number_mode mode)
{
    mode = loim_page_number_normalize(mode);
    if (mode == LOIM_PAGE_NUMBER_NONE) {
        return LOIM_PAGE_NUMBER_BOTTOM_RIGHT;
    }
    return mode == LOIM_PAGE_NUMBER_BOTTOM_RIGHT
        ? LOIM_PAGE_NUMBER_BOTTOM_CENTER
        : LOIM_PAGE_NUMBER_NONE;
}

loim_workspace_mode loim_workspace_mode_resolve(
    size_t image_count,
    bool import_active,
    bool workspace_ready)
{
    if (workspace_ready && image_count > 0U) {
        return LOIM_WORKSPACE_READY;
    }
    return import_active
        ? LOIM_WORKSPACE_IMPORTING_EMPTY
        : LOIM_WORKSPACE_EMPTY;
}

bool loim_workspace_shows_divider(loim_workspace_mode mode)
{
    return mode == LOIM_WORKSPACE_READY;
}

bool loim_workspace_clear_enabled(
    size_t image_count,
    bool import_active,
    bool drop_collecting)
{
    return image_count > 0U || import_active || drop_collecting;
}

uint64_t loim_import_generation_next(uint64_t generation)
{
    return generation == UINT64_MAX ? 1U : generation + 1U;
}

bool loim_import_result_should_commit(
    uint64_t active_generation,
    uint64_t result_generation,
    bool cancel_requested)
{
    return !cancel_requested && active_generation != 0U &&
        active_generation == result_generation;
}

bool loim_page_number_origin(
    loim_page_number_mode mode,
    float page_width,
    float page_height,
    float margin_x,
    float margin_y,
    float text_width,
    float text_height,
    float *out_x,
    float *out_y)
{
    if (out_x == NULL || out_y == NULL) {
        return false;
    }
    *out_x = 0.0F;
    *out_y = 0.0F;
    mode = loim_page_number_normalize(mode);
    if (mode == LOIM_PAGE_NUMBER_NONE || !(page_width > 0.0F) ||
        !(page_height > 0.0F) || margin_x < 0.0F || margin_y < 0.0F ||
        !(text_width > 0.0F) || !(text_height > 0.0F) ||
        text_width > page_width || text_height + margin_y > page_height) {
        return false;
    }
    if (mode == LOIM_PAGE_NUMBER_BOTTOM_RIGHT) {
        if (text_width + margin_x > page_width) {
            return false;
        }
        *out_x = page_width - margin_x - text_width;
    } else {
        *out_x = (page_width - text_width) / 2.0F;
    }
    *out_y = page_height - margin_y - text_height;
    return true;
}

float loim_preview_scale_adjust(float scale, int direction)
{
    if (!(scale > 0.0F)) {
        scale = LOIM_PREVIEW_SCALE_DEFAULT;
    } else if (scale < LOIM_PREVIEW_SCALE_MIN) {
        scale = LOIM_PREVIEW_SCALE_MIN;
    } else if (scale > LOIM_PREVIEW_SCALE_MAX) {
        scale = LOIM_PREVIEW_SCALE_MAX;
    }
    if (direction > 0) {
        scale += LOIM_PREVIEW_SCALE_STEP;
    } else if (direction < 0) {
        scale -= LOIM_PREVIEW_SCALE_STEP;
    }
    if (scale < LOIM_PREVIEW_SCALE_MIN) {
        return LOIM_PREVIEW_SCALE_MIN;
    }
    return scale > LOIM_PREVIEW_SCALE_MAX ? LOIM_PREVIEW_SCALE_MAX : scale;
}

float loim_preview_paper_width(float viewport_width, float scale)
{
    float available_width;
    float minimum_width;
    float paper_width;

    if (!(viewport_width > LOIM_PREVIEW_HORIZONTAL_PADDING)) {
        return 0.0F;
    }
    scale = loim_preview_scale_adjust(scale, 0);
    available_width = viewport_width - LOIM_PREVIEW_HORIZONTAL_PADDING;
    minimum_width = available_width < LOIM_PREVIEW_MINIMUM_WIDTH
        ? available_width
        : LOIM_PREVIEW_MINIMUM_WIDTH;
    paper_width = viewport_width * LOIM_PREVIEW_BASE_WIDTH_RATIO * scale;
    if (paper_width < minimum_width) {
        return minimum_width;
    }
    return paper_width > available_width ? available_width : paper_width;
}

static float loim_preview_scroll_maximum(
    float paper_width,
    float viewport_height,
    size_t sheet_count)
{
    float paper_height;
    float content_height;

    if (!(paper_width > 0.0F) || !(viewport_height > 0.0F) ||
        sheet_count == 0U) {
        return 0.0F;
    }
    paper_height = paper_width * LOIM_A4_HEIGHT_TO_WIDTH;
    content_height = LOIM_PREVIEW_PAGE_GAP + (float)sheet_count *
        (paper_height + LOIM_PREVIEW_PAGE_GAP);
    return content_height > viewport_height
        ? content_height - viewport_height + LOIM_PREVIEW_SCROLL_OVERSCAN
        : 0.0F;
}

float loim_preview_scroll_reanchor(
    float scroll_y,
    float viewport_height,
    float old_paper_width,
    float new_paper_width,
    size_t sheet_count)
{
    float old_maximum = loim_preview_scroll_maximum(
        old_paper_width, viewport_height, sheet_count);
    float new_maximum = loim_preview_scroll_maximum(
        new_paper_width, viewport_height, sheet_count);
    float old_span;
    float new_span;
    float anchor;
    float result;

    if (!(scroll_y > 0.5F) || old_maximum == 0.0F) {
        return 0.0F;
    }
    if (scroll_y >= old_maximum - 0.5F) {
        return new_maximum;
    }
    old_span = old_paper_width * LOIM_A4_HEIGHT_TO_WIDTH +
        LOIM_PREVIEW_PAGE_GAP;
    new_span = new_paper_width * LOIM_A4_HEIGHT_TO_WIDTH +
        LOIM_PREVIEW_PAGE_GAP;
    if (!(old_span > 0.0F) || !(new_span > 0.0F) ||
        !(viewport_height > 0.0F) || sheet_count == 0U) {
        return 0.0F;
    }
    anchor = (scroll_y + viewport_height / 2.0F - LOIM_PREVIEW_TOP_INSET) /
        old_span;
    if (anchor < 0.0F) {
        anchor = 0.0F;
    } else if (anchor > (float)sheet_count) {
        anchor = (float)sheet_count;
    }
    result = anchor * new_span - viewport_height / 2.0F +
        LOIM_PREVIEW_TOP_INSET;
    if (result < 0.0F) {
        return 0.0F;
    }
    return result > new_maximum ? new_maximum : result;
}

bool loim_export_page_requires_watermark(
    bool licensed,
    size_t columns,
    size_t page_index)
{
    columns = loim_columns_normalize(columns);
    if (licensed) {
        return false;
    }
    if (columns == 1U) {
        return page_index > 5U;
    }
    if (columns == 2U) {
        return page_index > 1U;
    }
    return page_index > 0U;
}

bool loim_export_default_path(
    const char *documents_path,
    const char *image_path,
    char *output,
    size_t output_capacity)
{
    static const char filename[] = "LoimReader.pdf";
    const char *base = documents_path;
    size_t base_length;
    char separator = '/';
    int written;

    if (output == NULL || output_capacity == 0U) {
        return false;
    }
    output[0] = '\0';
    if (base == NULL || base[0] == '\0') {
        const char *slash;
        const char *backslash;
        const char *end;

        if (image_path == NULL || image_path[0] == '\0') {
            base = ".";
        } else {
            slash = strrchr(image_path, '/');
            backslash = strrchr(image_path, '\\');
            end = slash == NULL ? backslash
                : backslash == NULL || slash > backslash ? slash : backslash;
            if (end == NULL) {
                base = ".";
            } else {
                base_length = (size_t)(end - image_path);
                if (base_length + 1U > output_capacity) {
                    return false;
                }
                memcpy(output, image_path, base_length);
                output[base_length] = '\0';
                base = output;
            }
        }
    }
    base_length = strlen(base);
    if (strchr(base, '\\') != NULL && strchr(base, '/') == NULL) {
        separator = '\\';
    }
    if (base == output) {
        bool has_separator = base_length > 0U &&
            (base[base_length - 1U] == '/' || base[base_length - 1U] == '\\');
        size_t required = base_length + (has_separator ? 0U : 1U) + sizeof(filename);

        if (required > output_capacity) {
            return false;
        }
        if (!has_separator) {
            output[base_length++] = separator;
        }
        memcpy(output + base_length, filename, sizeof(filename));
        return true;
    }
    written = snprintf(
        output,
        output_capacity,
        "%s%s%s",
        base,
        base_length > 0U && (base[base_length - 1U] == '/' ||
            base[base_length - 1U] == '\\') ? "" : separator == '/' ? "/" : "\\",
        filename);
    return written >= 0 && (size_t)written < output_capacity;
}

uint32_t loim_a4_width_px(unsigned dpi)
{
    /* A4 width is 210 mm = 2100/254 in; dpi * in = px, rounded. */
    return dpi == 300U ? 2480U : (uint32_t)(((uint64_t)dpi * 2100U + 127U) / 254U);
}

uint32_t loim_a4_height_px(unsigned dpi)
{
    /* A4 height is 297 mm = 2970/254 in; dpi * in = px, rounded. */
    return dpi == 300U ? 3508U : (uint32_t)(((uint64_t)dpi * 2970U + 127U) / 254U);
}

unsigned loim_slice_effective_dpi(
    uint32_t source_width_px,
    uint32_t source_height_px,
    float layout_width_px,
    float layout_height_px,
    unsigned maximum_dpi)
{
    float dpi_by_width;
    float dpi_by_height;
    float slice_dpi;
    unsigned candidate;

    if (maximum_dpi < 1U) {
        maximum_dpi = 1U;
    }
    if (!(layout_width_px > 0.0F) || !(layout_height_px > 0.0F)) {
        return maximum_dpi;
    }
    dpi_by_width = (float)source_width_px * (float)LOIM_PDF_REFERENCE_DPI /
        layout_width_px;
    dpi_by_height = (float)source_height_px * (float)LOIM_PDF_REFERENCE_DPI /
        layout_height_px;
    slice_dpi = dpi_by_width < dpi_by_height ? dpi_by_width : dpi_by_height;
    if (slice_dpi >= (float)maximum_dpi) {
        return maximum_dpi;
    }
    candidate = (unsigned)slice_dpi;
    if (candidate < 1U) {
        return 1U;
    }
    return candidate;
}

unsigned loim_pdf_page_dpi(
    loim_pdf_dpi_policy policy,
    const unsigned *slice_dpis,
    size_t slice_count)
{
    unsigned useful_dpi = 0U;
    size_t index;

    if (policy == LOIM_PDF_DPI_FIXED_300) {
        return LOIM_PDF_REFERENCE_DPI;
    }
    for (index = 0U; index < slice_count; ++index) {
        if (slice_dpis != NULL && slice_dpis[index] > useful_dpi) {
            useful_dpi = slice_dpis[index];
        }
    }
    if (useful_dpi < LOIM_PDF_ADAPTIVE_MIN_DPI) {
        return LOIM_PDF_ADAPTIVE_MIN_DPI;
    }
    return useful_dpi > LOIM_PDF_REFERENCE_DPI
        ? LOIM_PDF_REFERENCE_DPI
        : useful_dpi;
}

bool loim_pixel_span_quantize(
    float start,
    float length,
    uint32_t limit,
    uint32_t *out_start,
    uint32_t *out_length)
{
    double precise_start;
    double precise_end;
    double precise_limit;
    uint32_t pixel_start;
    uint32_t pixel_end;

    if (out_start == NULL || out_length == NULL) {
        return false;
    }
    *out_start = 0U;
    *out_length = 0U;
    precise_start = (double)start;
    precise_limit = (double)limit;
    if (!(precise_start >= 0.0) || !(length > 0.0F) || limit == 0U ||
        !(precise_start < precise_limit)) {
        return false;
    }
    precise_end = precise_start + (double)length;
    if (!(precise_end > precise_start)) {
        return false;
    }
    if (precise_end > precise_limit) {
        precise_end = precise_limit;
    }
    pixel_start = (uint32_t)precise_start;
    pixel_end = (uint32_t)precise_end;
    if ((double)pixel_end < precise_end && pixel_end < limit) {
        pixel_end += 1U;
    }
    if (pixel_end <= pixel_start) {
        if (pixel_start >= limit) {
            return false;
        }
        pixel_end = pixel_start + 1U;
    }
    *out_start = pixel_start;
    *out_length = pixel_end - pixel_start;
    return true;
}

unsigned loim_progress_percent(size_t completed, size_t total)
{
    uintmax_t numerator;

    if (total == 0U) {
        return 0U;
    }
    if (completed >= total) {
        return 100U;
    }
    numerator = (uintmax_t)completed * 100U;
    return (unsigned)(numerator / (uintmax_t)total);
}
