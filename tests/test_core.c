#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loim/document.h"
#include "loim/image_probe.h"
#include "loim/layout.h"
#include "loim/seam.h"

static int failures = 0;

#define CHECK(expression)                                                        \
    do {                                                                         \
        if (!(expression)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n",                      \
                    __FILE__, __LINE__, #expression);                            \
            failures += 1;                                                       \
        }                                                                        \
    } while (0)

static loim_document *test_document(void)
{
    loim_document *document = NULL;
    CHECK(loim_document_create(&document) == LOIM_OK);
    return document;
}

static void add_source(
    loim_document *document,
    const char *path,
    uint32_t width,
    uint32_t height,
    size_t *out_index)
{
    loim_source_info source;
    source.path = path;
    source.width_px = width;
    source.height_px = height;
    CHECK(loim_document_add_source(document, &source, out_index) == LOIM_OK);
}

static void test_batch_prefers_image_boundary(void)
{
    loim_document *document = test_document();
    loim_layout_options options;
    loim_layout layout;

    memset(&layout, 0, sizeof(layout));
    add_source(document, "first.png", 1000U, 1000U, NULL);
    add_source(document, "second.png", 1000U, 1000U, NULL);
    options.content_width_px = 1000U;
    options.target_page_height_px = 1200U;
    options.minimum_page_height_px = 800U;
    options.maximum_page_height_px = 1500U;
    options.search_radius_px = 400U;
    options.inter_image_gap_px = 20U;

    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.virtual_height_px == 2020U);
    CHECK(layout.page_count == 2U);
    if (layout.page_count == 2U) {
        CHECK(layout.pages[0].height_px == 1000U);
        CHECK(layout.pages[0].slice_count == 1U);
        CHECK(layout.pages[1].virtual_y_px == 1000U);
        CHECK(layout.pages[1].height_px == 1020U);
        CHECK(layout.pages[1].slice_count == 1U);
        if (layout.pages[1].slice_count == 1U) {
            const loim_page_slice *slice =
                &layout.slices[layout.pages[1].first_slice];
            CHECK(slice->source_index == 1U);
            CHECK(slice->destination_y_px == 20U);
        }
    }
    loim_layout_destroy(&layout);
    loim_document_destroy(document);
}

static void test_content_gap_hint_beats_geometric_cut(void)
{
    loim_document *document = test_document();
    loim_layout_options options;
    loim_layout layout;
    size_t source_index = 0U;

    memset(&layout, 0, sizeof(layout));
    add_source(document, "long.png", 1000U, 3000U, &source_index);
    CHECK(loim_document_add_split_hint(
        document,
        source_index,
        1300U,
        1.0F,
        LOIM_SPLIT_HINT_CONTENT_GAP) == LOIM_OK);
    options.content_width_px = 1000U;
    options.target_page_height_px = 1400U;
    options.minimum_page_height_px = 1100U;
    options.maximum_page_height_px = 1700U;
    options.search_radius_px = 250U;
    options.inter_image_gap_px = 0U;

    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.page_count == 2U);
    if (layout.page_count == 2U) {
        CHECK(layout.pages[0].height_px == 1300U);
        CHECK(layout.pages[1].height_px == 1700U);
    }
    loim_layout_destroy(&layout);
    loim_document_destroy(document);
}

static void test_source_scaling_is_virtual(void)
{
    loim_document *document = test_document();
    loim_layout_options options;
    loim_layout layout;

    memset(&layout, 0, sizeof(layout));
    add_source(document, "wide.png", 2000U, 1000U, NULL);
    options.content_width_px = 1000U;
    options.target_page_height_px = 1000U;
    options.minimum_page_height_px = 500U;
    options.maximum_page_height_px = 1200U;
    options.search_radius_px = 200U;
    options.inter_image_gap_px = 0U;

    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.virtual_height_px == 500U);
    CHECK(layout.page_count == 1U);
    CHECK(layout.slice_count == 1U);
    if (layout.slice_count == 1U) {
        CHECK(layout.slices[0].source_y_px == 0U);
        CHECK(layout.slices[0].source_height_px == 1000U);
        CHECK(layout.slices[0].destination_height_px == 500U);
    }
    loim_layout_destroy(&layout);
    loim_document_destroy(document);
}

static void test_adjacent_slices_do_not_duplicate_source_rows(void)
{
    loim_document *document = test_document();
    loim_layout_options options;
    loim_layout layout;

    memset(&layout, 0, sizeof(layout));
    add_source(document, "scaled.png", 1000U, 200U, NULL);
    options.content_width_px = 1000U;
    options.target_page_height_px = 120U;
    options.minimum_page_height_px = 80U;
    options.maximum_page_height_px = 130U;
    options.search_radius_px = 20U;
    options.inter_image_gap_px = 0U;

    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.page_count == 2U);
    CHECK(layout.slice_count == 2U);
    if (layout.slice_count == 2U) {
        const loim_page_slice *first = &layout.slices[0];
        const loim_page_slice *second = &layout.slices[1];
        CHECK(first->source_y_px + first->source_height_px == second->source_y_px);
        CHECK(second->source_y_px + second->source_height_px == 200U);
    }
    loim_layout_destroy(&layout);
    loim_document_destroy(document);
}

static void test_seam_finds_white_band(void)
{
    enum { WIDTH = 16, HEIGHT = 40 };
    uint8_t pixels[WIDTH * HEIGHT * 4];
    loim_seam_result result;
    uint32_t row;
    uint32_t column;

    for (row = 0U; row < HEIGHT; ++row) {
        for (column = 0U; column < WIDTH; ++column) {
            size_t offset = ((size_t)row * WIDTH + column) * 4U;
            uint8_t value = row >= 18U && row <= 22U ? 255U : 0U;
            pixels[offset] = value;
            pixels[offset + 1U] = value;
            pixels[offset + 2U] = value;
            pixels[offset + 3U] = 255U;
        }
    }

    CHECK(loim_seam_find_rgba8(
        pixels,
        WIDTH,
        HEIGHT,
        (size_t)WIDTH * 4U,
        20U,
        8U,
        &result) == LOIM_OK);
    CHECK(result.row >= 18U && result.row <= 22U);
    CHECK(result.quality > 0.70F);
}

static void test_image_probe(void)
{
    char png_path[1024];
    char jpeg_path[1024];
    loim_image_info info;

    (void)snprintf(
        png_path, sizeof(png_path), "%s/images/sitelogo.png", LOIM_TEST_SOURCE_DIR);
    (void)snprintf(
        jpeg_path, sizeof(jpeg_path), "%s/images/myshop.jpeg", LOIM_TEST_SOURCE_DIR);
    CHECK(loim_image_probe_file(png_path, &info) == LOIM_OK);
    CHECK(info.format == LOIM_IMAGE_FORMAT_PNG);
    CHECK(info.width_px > 0U && info.height_px > 0U);
    CHECK(loim_image_probe_file(jpeg_path, &info) == LOIM_OK);
    CHECK(info.format == LOIM_IMAGE_FORMAT_JPEG);
    CHECK(info.width_px > 0U && info.height_px > 0U);
}

static void test_validation(void)
{
    loim_document *document = test_document();
    loim_layout_options options;
    loim_layout layout;
    loim_source_info invalid = { "bad.png", 0U, 100U };

    loim_layout_options_a4(&options, 210U);
    CHECK(options.target_page_height_px == 297U);
    CHECK(loim_document_add_source(document, &invalid, NULL) ==
          LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_layout_build(document, &options, &layout) ==
          LOIM_ERROR_INVALID_ARGUMENT);
    loim_document_destroy(document);
}

int main(void)
{
    test_batch_prefers_image_boundary();
    test_content_gap_hint_beats_geometric_cut();
    test_source_scaling_is_virtual();
    test_adjacent_slices_do_not_duplicate_source_rows();
    test_seam_finds_white_band();
    test_image_probe();
    test_validation();

    if (failures != 0) {
        fprintf(stderr, "%d test check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("all core tests passed");
    return EXIT_SUCCESS;
}
