#include "loim/pdf.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pdf_write_text(FILE *file, const char *text)
{
    return fputs(text, file) >= 0;
}

static long pdf_offset(FILE *file)
{
    long offset = ftell(file);

    return offset < 0L || offset > 9999999999L ? -1L : offset;
}

static int pdf_write_object_header(FILE *file, size_t object_number)
{
    return fprintf(file, "%zu 0 obj\n", object_number) >= 0;
}

loim_status loim_pdf_write_rgb(
    const char *path,
    const loim_pdf_rgb_page *pages,
    size_t page_count)
{
    FILE *file = NULL;
    long *offsets = NULL;
    size_t object_count;
    size_t page_index;
    loim_status result = LOIM_ERROR_IO;

    if (path == NULL || path[0] == '\0' || pages == NULL || page_count == 0U) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    if (page_count > (SIZE_MAX - 2U) / 3U) {
        return LOIM_ERROR_OVERFLOW;
    }
    object_count = 2U + page_count * 3U;
    if (object_count > SIZE_MAX / sizeof(*offsets)) {
        return LOIM_ERROR_OVERFLOW;
    }
    offsets = calloc(object_count + 1U, sizeof(*offsets));
    if (offsets == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        goto cleanup;
    }
    if (!pdf_write_text(file, "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n")) {
        goto cleanup;
    }
    offsets[1] = pdf_offset(file);
    if (offsets[1] < 0L || !pdf_write_object_header(file, 1U) ||
        !pdf_write_text(file, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n")) {
        goto cleanup;
    }
    offsets[2] = pdf_offset(file);
    if (offsets[2] < 0L || !pdf_write_object_header(file, 2U) ||
        fprintf(file, "<< /Type /Pages /Kids [") < 0) {
        goto cleanup;
    }
    for (page_index = 0U; page_index < page_count; ++page_index) {
        if (fprintf(file, "%zu 0 R%s", 3U + page_index * 3U,
                    page_index + 1U == page_count ? "]" : " ") < 0) {
            goto cleanup;
        }
    }
    if (fprintf(file, " /Count %zu >>\nendobj\n", page_count) < 0) {
        goto cleanup;
    }

    for (page_index = 0U; page_index < page_count; ++page_index) {
        const loim_pdf_rgb_page *page = &pages[page_index];
        uint32_t media_width = page->media_width_pt == 0U ? 595U : page->media_width_pt;
        uint32_t media_height = page->media_height_pt == 0U ? 842U : page->media_height_pt;
        size_t page_object = 3U + page_index * 3U;
        size_t content_object = page_object + 1U;
        size_t image_object = page_object + 2U;
        size_t bytes;
        char content[128];
        int content_length;

        if (page->width_px == 0U || page->height_px == 0U || page->rgb == NULL ||
            (size_t)page->width_px > SIZE_MAX / (size_t)page->height_px ||
            (size_t)page->width_px * (size_t)page->height_px > SIZE_MAX / 3U) {
            result = LOIM_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        bytes = (size_t)page->width_px * (size_t)page->height_px * 3U;
        offsets[page_object] = pdf_offset(file);
        if (offsets[page_object] < 0L || !pdf_write_object_header(file, page_object) ||
            fprintf(file,
                    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %u %u] "
                    "/Resources << /XObject << /Im0 %zu 0 R >> >> "
                    "/Contents %zu 0 R >>\nendobj\n",
                    media_width, media_height, image_object,
                    content_object) < 0) {
            goto cleanup;
        }
        content_length = snprintf(
            content,
            sizeof(content),
            "q %u 0 0 %u 0 0 cm /Im0 Do Q\n",
            media_width,
            media_height);
        if (content_length < 0 || (size_t)content_length >= sizeof(content)) {
            result = LOIM_ERROR_OVERFLOW;
            goto cleanup;
        }
        offsets[content_object] = pdf_offset(file);
        if (offsets[content_object] < 0L || !pdf_write_object_header(file, content_object) ||
            fprintf(file, "<< /Length %d >>\nstream\n", content_length) < 0 ||
            fwrite(content, 1U, (size_t)content_length, file) != (size_t)content_length ||
            !pdf_write_text(file, "endstream\nendobj\n")) {
            goto cleanup;
        }
        offsets[image_object] = pdf_offset(file);
        if (offsets[image_object] < 0L || !pdf_write_object_header(file, image_object) ||
            fprintf(file,
                    "<< /Type /XObject /Subtype /Image /Width %u /Height %u "
                    "/ColorSpace /DeviceRGB /BitsPerComponent 8 /Length %zu >>\n"
                    "stream\n",
                    page->width_px, page->height_px, bytes) < 0 ||
            fwrite(page->rgb, 1U, bytes, file) != bytes ||
            !pdf_write_text(file, "\nendstream\nendobj\n")) {
            goto cleanup;
        }
    }
    {
        long xref = pdf_offset(file);

        if (xref < 0L || fprintf(file, "xref\n0 %zu\n0000000000 65535 f \n",
                                  object_count + 1U) < 0) {
            goto cleanup;
        }
        for (page_index = 1U; page_index <= object_count; ++page_index) {
            if (offsets[page_index] < 0L ||
                fprintf(file, "%010ld 00000 n \n", offsets[page_index]) < 0) {
                goto cleanup;
            }
        }
        if (fprintf(file,
                    "trailer\n<< /Size %zu /Root 1 0 R >>\nstartxref\n%ld\n%%EOF\n",
                    object_count + 1U, xref) < 0) {
            goto cleanup;
        }
    }
    if (fclose(file) != 0) {
        file = NULL;
        goto cleanup;
    }
    file = NULL;
    result = LOIM_OK;

cleanup:
    if (file != NULL) {
        (void)fclose(file);
    }
    free(offsets);
    return result;
}
