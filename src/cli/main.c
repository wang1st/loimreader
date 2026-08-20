#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loim/document.h"
#include "loim/image_probe.h"
#include "loim/layout.h"
#include "loim/status.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#endif

static void loim_print_usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s [--width PIXELS] IMAGE [IMAGE ...]\n"
        "\n"
        "Probes multiple PNG/JPEG/GIF/BMP files and prints the A4 virtual-document\n"
        "page plan as JSON. Pixel decoding and rendering are intentionally separate.\n",
        program);
}

static int loim_parse_width(const char *value, uint32_t *out_width)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0UL ||
        parsed > UINT32_MAX) {
        return 0;
    }
    *out_width = (uint32_t)parsed;
    return 1;
}

static int loim_cli_run(int argc, char **argv)
{
    loim_document *document = NULL;
    loim_layout_options options;
    loim_layout layout;
    loim_status status;
    uint32_t content_width = 1440U;
    int first_image = 1;
    int argument;
    size_t page_index;

    memset(&layout, 0, sizeof(layout));
    for (argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--help") == 0 || strcmp(argv[argument], "-h") == 0) {
            loim_print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[argument], "--width") == 0) {
            if (argument + 1 >= argc ||
                !loim_parse_width(argv[argument + 1], &content_width)) {
                fprintf(stderr, "Invalid --width value\n");
                return EXIT_FAILURE;
            }
            argument += 1;
            first_image = argument + 1;
        } else {
            break;
        }
    }
    if (first_image >= argc) {
        loim_print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    status = loim_document_create(&document);
    if (status != LOIM_OK) {
        fprintf(stderr, "Cannot create document: %s\n", loim_status_string(status));
        return EXIT_FAILURE;
    }

    for (argument = first_image; argument < argc; ++argument) {
        loim_image_info image;
        loim_source_info source;

        status = loim_image_probe_file(argv[argument], &image);
        if (status != LOIM_OK) {
            fprintf(stderr, "Cannot import %s: %s\n", argv[argument], loim_status_string(status));
            loim_document_destroy(document);
            return EXIT_FAILURE;
        }
        source.path = argv[argument];
        source.width_px = image.width_px;
        source.height_px = image.height_px;
        status = loim_document_add_source(document, &source, NULL);
        if (status != LOIM_OK) {
            fprintf(stderr, "Cannot add %s: %s\n", argv[argument], loim_status_string(status));
            loim_document_destroy(document);
            return EXIT_FAILURE;
        }
    }

    loim_layout_options_a4(&options, content_width);
    status = loim_layout_build(document, &options, &layout);
    if (status != LOIM_OK) {
        fprintf(stderr, "Cannot paginate document: %s\n", loim_status_string(status));
        loim_document_destroy(document);
        return EXIT_FAILURE;
    }

    printf("{\n  \"sourceCount\": %zu,\n", loim_document_source_count(document));
    printf("  \"virtualHeight\": %" PRIu64 ",\n", layout.virtual_height_px);
    printf("  \"pageCount\": %zu,\n  \"pages\": [\n", layout.page_count);
    for (page_index = 0U; page_index < layout.page_count; ++page_index) {
        const loim_page *page = &layout.pages[page_index];
        size_t slice_offset;

        printf("    {\"index\": %zu, \"y\": %" PRIu64
               ", \"height\": %u, \"slices\": [",
               page_index, page->virtual_y_px, page->height_px);
        for (slice_offset = 0U; slice_offset < page->slice_count; ++slice_offset) {
            const loim_page_slice *slice =
                &layout.slices[page->first_slice + slice_offset];
            printf("%s{\"source\": %zu, \"sourceY\": %u, \"sourceHeight\": %u, "
                   "\"destinationY\": %u, \"destinationHeight\": %u}",
                   slice_offset == 0U ? "" : ", ",
                   slice->source_index,
                   slice->source_y_px,
                   slice->source_height_px,
                   slice->destination_y_px,
                   slice->destination_height_px);
        }
        printf("]}%s\n", page_index + 1U == layout.page_count ? "" : ",");
    }
    printf("  ]\n}\n");

    loim_layout_destroy(&layout);
    loim_document_destroy(document);
    return EXIT_SUCCESS;
}

#if defined(_WIN32)
static char *loim_wide_to_utf8(const wchar_t *value)
{
    int byte_count;
    char *utf8;

    byte_count = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        -1,
        NULL,
        0,
        NULL,
        NULL);
    if (byte_count <= 0) {
        return NULL;
    }
    utf8 = malloc((size_t)byte_count);
    if (utf8 == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value,
            -1,
            utf8,
            byte_count,
            NULL,
            NULL) == 0) {
        free(utf8);
        return NULL;
    }
    return utf8;
}

int wmain(int argc, wchar_t **wide_argv)
{
    char **utf8_argv;
    int argument;
    int result = EXIT_FAILURE;

    (void)SetConsoleOutputCP(CP_UTF8);
    utf8_argv = calloc((size_t)argc, sizeof(*utf8_argv));
    if (utf8_argv == NULL) {
        fputs("Cannot allocate UTF-8 argument list\n", stderr);
        return EXIT_FAILURE;
    }
    for (argument = 0; argument < argc; ++argument) {
        utf8_argv[argument] = loim_wide_to_utf8(wide_argv[argument]);
        if (utf8_argv[argument] == NULL) {
            fputs("Cannot convert command-line argument to UTF-8\n", stderr);
            goto cleanup;
        }
    }
    result = loim_cli_run(argc, utf8_argv);

cleanup:
    for (argument = 0; argument < argc; ++argument) {
        free(utf8_argv[argument]);
    }
    free(utf8_argv);
    return result;
}
#else
int main(int argc, char **argv)
{
    return loim_cli_run(argc, argv);
}
#endif
