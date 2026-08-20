#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/stat.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "loim/auth.h"
#include "loim/document.h"
#include "loim/export_name.h"
#include "loim/i18n.h"
#include "loim/import_queue.h"
#include "loim/layout.h"
#include "loim/machine_code.h"
#include "loim/presentation.h"
#include "loim/settings.h"
#include "loim/texture_plan.h"
#include "loim/update.h"

static int failures = 0;

#define CHECK(expression)                                                        \
    do {                                                                         \
        if (!(expression)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #expression);                            \
            failures += 1;                                                       \
        }                                                                        \
    } while (0)


static void test_drop_queue_accepts_implicit_begin(void)
{
    loim_import_queue queue;

    loim_import_queue_init(&queue);
    CHECK(loim_import_queue_add(&queue, "/tmp/first.png") == LOIM_OK);
    CHECK(queue.collecting);
    CHECK(loim_import_queue_add(&queue, "/tmp/second image.png") == LOIM_OK);
    CHECK(loim_import_queue_complete(&queue) == LOIM_OK);
    CHECK(!queue.collecting);
    CHECK(queue.count == 2U);
    CHECK(strcmp(loim_import_queue_path(&queue, 0U), "/tmp/first.png") == 0);
    CHECK(strcmp(loim_import_queue_path(&queue, 1U), "/tmp/second image.png") == 0);
    loim_import_queue_destroy(&queue);
}

static void test_system_language_selection(void)
{
    CHECK(loim_locale_from_name("zh") == LOIM_LOCALE_ZH_CN);
    CHECK(loim_locale_from_name("zh-Hans-CN") == LOIM_LOCALE_ZH_CN);
    CHECK(loim_locale_from_name("zh_TW") == LOIM_LOCALE_ZH_CN);
    CHECK(loim_locale_from_name("en-US") == LOIM_LOCALE_EN);
    CHECK(loim_locale_from_name("fr") == LOIM_LOCALE_EN);
    CHECK(loim_locale_from_name(NULL) == LOIM_LOCALE_EN);
}

static void test_both_locales_are_complete(void)
{
    int locale;
    int key;

    for (locale = 0; locale < (int)LOIM_LOCALE_COUNT; ++locale) {
        CHECK(loim_locale_complete((loim_locale)locale));
        for (key = 0; key < (int)LOIM_TEXT_KEY_COUNT; ++key) {
            const char *translation = loim_text(
                (loim_locale)locale, (loim_text_key)key);

            CHECK(translation != NULL);
            CHECK(translation != NULL && translation[0] != '\0');
        }
    }
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_ZH_CN, LOIM_TEXT_SIGN_IN),
              "登录") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_EN, LOIM_TEXT_SIGN_IN),
              "Sign in") == 0);
    CHECK(strcmp(
              loim_status_text(LOIM_LOCALE_ZH_CN, LOIM_ERROR_OUT_OF_MEMORY),
              "内存不足") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_ZH_CN, LOIM_TEXT_REGISTER_NOW),
              "没有账号？去注册") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_EN, LOIM_TEXT_REGISTER_NOW),
              "No account? Register now") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_ZH_CN, LOIM_TEXT_ABOUT_COPYRIGHT),
              "Copyright © 2024 Ctdy123.com") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_ZH_CN, LOIM_TEXT_ABOUT_ACCOUNT_SECTION),
              "账户与订阅") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_EN, LOIM_TEXT_ABOUT_ACCOUNT_SECTION),
              "Account & subscription") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_ZH_CN, LOIM_TEXT_EMPTY_TITLE),
              "将图片拖放到这里") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_EN, LOIM_TEXT_EMPTY_SUBTITLE),
              "Import multiple images at once, or click to choose files") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_ZH_CN, LOIM_TEXT_CLEAR_TOOLTIP),
              "清空当前图片并返回导入页面") == 0);
    CHECK(strcmp(
              loim_text(LOIM_LOCALE_EN, LOIM_TEXT_CLEARED),
              "Current content cleared") == 0);
}

static void test_drop_queue_resets_for_next_drag(void)
{
    loim_import_queue queue;

    loim_import_queue_init(&queue);
    CHECK(loim_import_queue_begin(&queue) == LOIM_OK);
    CHECK(loim_import_queue_add(&queue, "old.png") == LOIM_OK);
    CHECK(loim_import_queue_complete(&queue) == LOIM_OK);
    CHECK(loim_import_queue_begin(&queue) == LOIM_OK);
    CHECK(queue.count == 0U);
    CHECK(loim_import_queue_add(&queue, "new.png") == LOIM_OK);
    CHECK(strcmp(loim_import_queue_path(&queue, 0U), "new.png") == 0);
    loim_import_queue_destroy(&queue);
}

static void test_workspace_mode_and_clear_availability(void)
{
    CHECK(loim_workspace_mode_resolve(0U, false, false) ==
          LOIM_WORKSPACE_EMPTY);
    CHECK(loim_workspace_mode_resolve(0U, true, false) ==
          LOIM_WORKSPACE_IMPORTING_EMPTY);
    /* The first decoded image must not reveal a half-finished workspace. */
    CHECK(loim_workspace_mode_resolve(1U, true, false) ==
          LOIM_WORKSPACE_IMPORTING_EMPTY);
    /* Appending to an existing workspace keeps its stable preview visible. */
    CHECK(loim_workspace_mode_resolve(1U, true, true) ==
          LOIM_WORKSPACE_READY);
    CHECK(loim_workspace_mode_resolve(1U, false, true) ==
          LOIM_WORKSPACE_READY);
    CHECK(!loim_workspace_shows_divider(LOIM_WORKSPACE_EMPTY));
    CHECK(!loim_workspace_shows_divider(LOIM_WORKSPACE_IMPORTING_EMPTY));
    CHECK(loim_workspace_shows_divider(LOIM_WORKSPACE_READY));
    CHECK(!loim_workspace_shows_divider((loim_workspace_mode)99));
    CHECK(!loim_workspace_clear_enabled(0U, false, false));
    CHECK(loim_workspace_clear_enabled(1U, false, false));
    CHECK(loim_workspace_clear_enabled(0U, true, false));
    CHECK(loim_workspace_clear_enabled(0U, false, true));
    CHECK(loim_import_generation_next(1U) == 2U);
    CHECK(loim_import_generation_next(UINT64_MAX) == 1U);
    CHECK(loim_import_result_should_commit(7U, 7U, false));
    CHECK(!loim_import_result_should_commit(7U, 6U, false));
    CHECK(!loim_import_result_should_commit(7U, 7U, true));
    CHECK(!loim_import_result_should_commit(0U, 0U, false));
}

static void test_column_cycle_and_import_progress(void)
{
    char export_path[256];
    uint32_t span_start = 0U;
    uint32_t span_length = 0U;
    CHECK(loim_columns_normalize(0U) == 1U);
    CHECK(loim_columns_normalize(2U) == 2U);
    CHECK(loim_columns_normalize(99U) == 3U);
    CHECK(loim_columns_next(1U) == 2U);
    CHECK(loim_columns_next(2U) == 3U);
    CHECK(loim_columns_next(3U) == 1U);
    CHECK(loim_sheet_slot_count(2U) == 4U);
    CHECK(loim_sheet_slot_count(3U) == 9U);
    /* Magazine layout fills each column from top to bottom before moving right. */
    CHECK(loim_sheet_slot_column(2U, 0U) == 0U);
    CHECK(loim_sheet_slot_row(2U, 0U) == 0U);
    CHECK(loim_sheet_slot_column(2U, 1U) == 0U);
    CHECK(loim_sheet_slot_row(2U, 1U) == 1U);
    CHECK(loim_sheet_slot_column(2U, 2U) == 1U);
    CHECK(loim_sheet_slot_row(2U, 2U) == 0U);
    CHECK(loim_sheet_slot_column(2U, 3U) == 1U);
    CHECK(loim_sheet_slot_row(2U, 3U) == 1U);
    CHECK(loim_sheet_slot_column(3U, 2U) == 0U);
    CHECK(loim_sheet_slot_row(3U, 2U) == 2U);
    CHECK(loim_sheet_slot_column(3U, 3U) == 1U);
    CHECK(loim_sheet_slot_row(3U, 3U) == 0U);
    CHECK(loim_sheet_slot_column(3U, 4U) == 1U);
    CHECK(loim_sheet_slot_row(3U, 4U) == 1U);
    CHECK(loim_sheet_count(0U, 2U) == 0U);
    CHECK(loim_sheet_count(4U, 2U) == 1U);
    CHECK(loim_sheet_count(5U, 2U) == 2U);
    CHECK(loim_sheet_count(10U, 3U) == 2U);
    CHECK(loim_sheet_count(48U, 2U) == 12U);
    CHECK(loim_sheet_slice_index(0U, 2U, 3U) == 3U);
    CHECK(loim_sheet_slice_index(1U, 2U, 3U) == 7U);
    CHECK(!loim_export_page_requires_watermark(false, 1U, 5U));
    CHECK(loim_export_page_requires_watermark(false, 1U, 6U));
    CHECK(!loim_export_page_requires_watermark(false, 2U, 1U));
    CHECK(loim_export_page_requires_watermark(false, 2U, 2U));
    CHECK(loim_export_page_requires_watermark(false, 3U, 1U));
    CHECK(!loim_export_page_requires_watermark(true, 2U, 99U));
    CHECK(loim_export_default_path(
        "/Users/me/Documents/", "/tmp/input.png", export_path, sizeof(export_path)));
    CHECK(strcmp(export_path, "/Users/me/Documents/LoimReader.pdf") == 0);
    CHECK(loim_export_default_path(
        NULL, "/tmp/imports/input.png", export_path, sizeof(export_path)));
    CHECK(strcmp(export_path, "/tmp/imports/LoimReader.pdf") == 0);
    CHECK(loim_export_default_path(
        NULL, "C:\\Pictures\\input.png", export_path, sizeof(export_path)));
    CHECK(strcmp(export_path, "C:\\Pictures\\LoimReader.pdf") == 0);
    CHECK(loim_a4_width_px(300U) == 2480U);
    CHECK(loim_a4_height_px(300U) == 3508U);
    /* A4 = 210x297 mm; px = dpi * mm / 25.4, rounded. */
    CHECK(loim_a4_width_px(150U) == 1240U);
    CHECK(loim_a4_height_px(150U) == 1754U);
    CHECK(loim_a4_width_px(100U) == 827U);
    CHECK(loim_a4_height_px(100U) == 1169U);
    CHECK(loim_progress_percent(0U, 0U) == 0U);
    CHECK(loim_progress_percent(1U, 3U) == 33U);
    CHECK(loim_progress_percent(3U, 3U) == 100U);
    CHECK(loim_progress_percent(4U, 3U) == 100U);
    /* Effective DPI: source pixels mapped 1:1 on the layout rectangle. */
    CHECK(loim_slice_effective_dpi(2480U, 3508U, 2480.0F, 3508.0F, 300U) == 300U);
    CHECK(loim_slice_effective_dpi(1240U, 1754U, 2480.0F, 3508.0F, 300U) == 150U);
    CHECK(loim_slice_effective_dpi(3000U, 5000U, 2480.0F, 3508.0F, 300U) == 300U);
    CHECK(loim_slice_effective_dpi(2480U, 3508U, 2480.0F, 3508.0F, 200U) == 200U);
    CHECK(loim_slice_effective_dpi(10U, 10U, 2480.0F, 3508.0F, 300U) == 1U);
    CHECK(loim_slice_effective_dpi(100U, 100U, 2480.0F, 3508.0F, 300U) == 8U);
    CHECK(loim_slice_effective_dpi(2480U, 3508U, 0.0F, 3508.0F, 100U) == 100U);
    CHECK(loim_slice_effective_dpi(2480U, 3508U, 2480.0F, 0.0F, 100U) == 100U);
    CHECK(loim_slice_effective_dpi(2480U, 3508U, 2480.0F, 3508.0F, 0U) == 1U);
    /* Width constraint binds when the source is wider than tall. */
    CHECK(loim_slice_effective_dpi(1240U, 3000U, 2480.0F, 3508.0F, 300U) == 150U);
    /* Height constraint binds when the source is taller than wide. */
    CHECK(loim_slice_effective_dpi(2480U, 1754U, 2480.0F, 3508.0F, 300U) == 150U);


    /* A non-empty sub-pixel destination must remain visible after quantization. */
    CHECK(loim_pixel_span_quantize(0.64F, 0.672F, 8U, &span_start, &span_length));
    CHECK(span_start < 8U);
    CHECK(span_length >= 1U);
    CHECK(span_start + span_length <= 8U);
    CHECK(!loim_pixel_span_quantize(1.0F, 0.0F, 8U, &span_start, &span_length));
    CHECK(!loim_pixel_span_quantize(0.0F, 1.0F, 0U, &span_start, &span_length));
    CHECK(loim_pixel_span_quantize(
        (float)(UINT32_MAX - 255U), 256.0F, UINT32_MAX,
        &span_start, &span_length));
    CHECK(span_start == UINT32_MAX - 255U);
    CHECK(span_length == 255U);
}

static void test_page_number_cycle_and_origin(void)
{
    float x = -1.0F;
    float y = -1.0F;

    CHECK(loim_page_number_normalize(LOIM_PAGE_NUMBER_NONE) ==
          LOIM_PAGE_NUMBER_NONE);
    CHECK(loim_page_number_normalize(LOIM_PAGE_NUMBER_BOTTOM_RIGHT) ==
          LOIM_PAGE_NUMBER_BOTTOM_RIGHT);
    CHECK(loim_page_number_normalize(LOIM_PAGE_NUMBER_BOTTOM_CENTER) ==
          LOIM_PAGE_NUMBER_BOTTOM_CENTER);
    CHECK(loim_page_number_normalize((loim_page_number_mode)99) ==
          LOIM_PAGE_NUMBER_NONE);
    CHECK(loim_page_number_next(LOIM_PAGE_NUMBER_NONE) ==
          LOIM_PAGE_NUMBER_BOTTOM_RIGHT);
    CHECK(loim_page_number_next(LOIM_PAGE_NUMBER_BOTTOM_RIGHT) ==
          LOIM_PAGE_NUMBER_BOTTOM_CENTER);
    CHECK(loim_page_number_next(LOIM_PAGE_NUMBER_BOTTOM_CENTER) ==
          LOIM_PAGE_NUMBER_NONE);
    CHECK(loim_page_number_next((loim_page_number_mode)-1) ==
          LOIM_PAGE_NUMBER_BOTTOM_RIGHT);

    CHECK(loim_page_number_origin(
        LOIM_PAGE_NUMBER_BOTTOM_RIGHT,
        1000.0F, 1400.0F, 80.0F, 100.0F, 24.0F, 30.0F, &x, &y));
    CHECK(x == 896.0F);
    CHECK(y == 1270.0F);
    CHECK(loim_page_number_origin(
        LOIM_PAGE_NUMBER_BOTTOM_CENTER,
        1000.0F, 1400.0F, 80.0F, 100.0F, 24.0F, 30.0F, &x, &y));
    CHECK(x == 488.0F);
    CHECK(y == 1270.0F);
    CHECK(!loim_page_number_origin(
        LOIM_PAGE_NUMBER_NONE,
        1000.0F, 1400.0F, 80.0F, 100.0F, 24.0F, 30.0F, &x, &y));
    CHECK(x == 0.0F);
    CHECK(y == 0.0F);
    CHECK(!loim_page_number_origin(
        (loim_page_number_mode)77,
        1000.0F, 1400.0F, 80.0F, 100.0F, 24.0F, 30.0F, &x, &y));
    CHECK(!loim_page_number_origin(
        LOIM_PAGE_NUMBER_BOTTOM_RIGHT,
        100.0F, 100.0F, 80.0F, 80.0F, 24.0F, 30.0F, &x, &y));
}

static void test_preview_width_and_divider_scroll_anchor(void)
{
    float scale = LOIM_PREVIEW_SCALE_DEFAULT;
    float narrow_width;
    float wide_width;
    float enlarged_width;
    float old_scroll = 600.0F;
    float moved_scroll;
    float restored_scroll;
    float difference;
    float old_height;
    float new_height;
    float old_maximum;
    float new_maximum;
    float bottom_scroll;

    CHECK(loim_preview_scale_adjust(scale, 1) > scale);
    CHECK(loim_preview_scale_adjust(scale, -1) < scale);
    CHECK(loim_preview_scale_adjust(LOIM_PREVIEW_SCALE_MAX, 1) ==
          LOIM_PREVIEW_SCALE_MAX);
    CHECK(loim_preview_scale_adjust(LOIM_PREVIEW_SCALE_MIN, -1) ==
          LOIM_PREVIEW_SCALE_MIN);
    CHECK(loim_preview_scale_adjust(-1.0F, 0) ==
          LOIM_PREVIEW_SCALE_DEFAULT);

    narrow_width = loim_preview_paper_width(500.0F, scale);
    wide_width = loim_preview_paper_width(900.0F, scale);
    enlarged_width = loim_preview_paper_width(
        500.0F, loim_preview_scale_adjust(scale, 1));
    CHECK(narrow_width > 369.9F && narrow_width < 370.1F);
    CHECK(wide_width > narrow_width);
    CHECK(enlarged_width > narrow_width);
    CHECK(enlarged_width <= 468.0F);
    CHECK(loim_preview_paper_width(20.0F, scale) == 0.0F);

    moved_scroll = loim_preview_scroll_reanchor(
        old_scroll, 600.0F, wide_width, narrow_width, 4U);
    restored_scroll = loim_preview_scroll_reanchor(
        moved_scroll, 600.0F, narrow_width, wide_width, 4U);
    difference = restored_scroll - old_scroll;
    if (difference < 0.0F) {
        difference = -difference;
    }
    CHECK(difference < 0.1F);
    CHECK(loim_preview_scroll_reanchor(
              0.0F, 600.0F, narrow_width, wide_width, 4U) == 0.0F);

    old_height = narrow_width * 297.0F / 210.0F;
    new_height = wide_width * 297.0F / 210.0F;
    old_maximum = 24.0F + 4.0F * (old_height + 24.0F) - 600.0F + 18.0F;
    new_maximum = 24.0F + 4.0F * (new_height + 24.0F) - 600.0F + 18.0F;
    bottom_scroll = loim_preview_scroll_reanchor(
        old_maximum, 600.0F, narrow_width, wide_width, 4U);
    difference = bottom_scroll - new_maximum;
    if (difference < 0.0F) {
        difference = -difference;
    }
    CHECK(difference < 0.1F);
}

static size_t test_utf8_character_count(const char *text)
{
    size_t count = 0U;
    const unsigned char *cursor = (const unsigned char *)text;

    while (*cursor != '\0') {
        if ((*cursor & 0xC0U) != 0x80U) {
            count += 1U;
        }
        cursor += 1U;
    }
    return count;
}

static bool test_utf8_is_valid(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    while (*cursor != '\0') {
        unsigned char lead = cursor[0];
        size_t remaining;
        size_t index;

        if (lead < 0x80U) {
            cursor += 1U;
            continue;
        }
        if (lead >= 0xC2U && lead <= 0xDFU) {
            remaining = 1U;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            if (cursor[1] == '\0' ||
                (lead == 0xE0U && cursor[1] < 0xA0U) ||
                (lead == 0xEDU && cursor[1] >= 0xA0U)) {
                return false;
            }
            remaining = 2U;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            if (cursor[1] == '\0' ||
                (lead == 0xF0U && cursor[1] < 0x90U) ||
                (lead == 0xF4U && cursor[1] >= 0x90U)) {
                return false;
            }
            remaining = 3U;
        } else {
            return false;
        }
        for (index = 1U; index <= remaining; ++index) {
            if (cursor[index] == '\0' || (cursor[index] & 0xC0U) != 0x80U) {
                return false;
            }
        }
        cursor += remaining + 1U;
    }
    return true;
}

static void test_export_default_filename_rules(void)
{
    static const char *const single[] = {
        "/tmp/黑客松报道.png"
    };
    static const char *const batch_common[] = {
        "/tmp/黑客松报道-01.png",
        "/tmp/黑客松报道-02.png",
        "/tmp/黑客松报道-第3页.png"
    };
    static const char *const generic_in_folder[] = {
        "/Users/me/采访资料/IMG_001.png",
        "/Users/me/采访资料/IMG_002.png"
    };
    static const char *const unrelated[] = {
        "/Users/me/Downloads/alpha.png",
        "/tmp/beta.png"
    };
    static const char *const generic_single[] = {
        "/Users/me/采访资料/sample.png"
    };
    static const char *const generic_page_names[] = {
        "/Users/me/Downloads/page-01.png",
        "/Users/me/Downloads/page-02.png"
    };
    static const char *const generic_wechat_names[] = {
        "/Users/me/Downloads/微信图片_20260810_123001.png",
        "/Users/me/Downloads/微信图片_20260810_123002.png"
    };
    static const char *const illegal_characters[] = {
        "/tmp/研究<计划>:\"草案\"|?.png"
    };
    static const char *const long_name[] = {
        "/tmp/2026年北京中学生黑客松活动完整报道以及参赛团队采访整理最终版本"
        "2026年北京中学生黑客松活动完整报道以及参赛团队采访整理最终版本.png"
    };
    static const char *const collision[] = {
        "/tmp/Naming Test.png"
    };
    static const char *const chinese_collision[] = {
        "/tmp/命名测试.png"
    };
    static const char *const root_image[] = {
        "/root-document-name-fixture.png"
    };
    char name[512];
    char path[1024];
    FILE *existing;

    CHECK(loim_export_suggested_filename(
        single, 1U, 2U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "黑客松报道-双栏.pdf") == 0);

    CHECK(loim_export_suggested_filename(
        batch_common, 3U, 3U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "黑客松报道-三栏.pdf") == 0);

    CHECK(loim_export_suggested_filename(
        generic_in_folder, 2U, 1U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "采访资料-单栏.pdf") == 0);

    CHECK(loim_export_suggested_filename(
        generic_page_names, 2U, 1U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "合并图片-20260810-1430-单栏.pdf") == 0);
    CHECK(loim_export_suggested_filename(
        generic_wechat_names, 2U, 1U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "合并图片-20260810-1430-单栏.pdf") == 0);

    CHECK(loim_export_suggested_filename(
        unrelated, 2U, 2U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "合并图片-20260810-1430-双栏.pdf") == 0);
    CHECK(loim_export_suggested_filename(
        unrelated, 2U, 2U, LOIM_LOCALE_EN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "Merged Images-20260810-1430-2 Columns.pdf") == 0);

    CHECK(loim_export_suggested_filename(
        generic_single, 1U, 1U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "采访资料-单栏.pdf") == 0);

    CHECK(loim_export_suggested_filename(
        illegal_characters, 1U, 2U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strcmp(name, "研究-计划-草案-双栏.pdf") == 0);

    CHECK(loim_export_suggested_filename(
        long_name, 1U, 2U, LOIM_LOCALE_ZH_CN, "20260810-1430",
        name, sizeof(name)));
    CHECK(strlen(name) <= LOIM_EXPORT_FILENAME_MAX_BYTES);
    CHECK(test_utf8_character_count(name) <= LOIM_EXPORT_FILENAME_MAX_CHARACTERS);
    CHECK(test_utf8_is_valid(name));
    CHECK(strncmp(name, "2026年北京中学生黑客松", strlen("2026年北京中学生黑客松")) == 0);
    CHECK(strstr(name, "…") != NULL);
    CHECK(strstr(name, "采访整理最终版本-双栏.pdf") != NULL);

    CHECK(loim_export_suggested_path(
        "/Users/me/Documents/", single, 1U, 2U, LOIM_LOCALE_ZH_CN,
        "20260810-1430", path, sizeof(path)));
    CHECK(strcmp(path, "/Users/me/Documents/黑客松报道-双栏.pdf") == 0);
    CHECK(loim_export_suggested_path(
        NULL, single, 1U, 2U, LOIM_LOCALE_ZH_CN,
        "20260810-1430", path, sizeof(path)));
    CHECK(strcmp(path, "/tmp/黑客松报道-双栏.pdf") == 0);
    CHECK(loim_export_suggested_path(
        "C:\\Users\\me\\Documents", single, 1U, 2U, LOIM_LOCALE_ZH_CN,
        "20260810-1430", path, sizeof(path)));
    CHECK(strcmp(path, "C:\\Users\\me\\Documents\\黑客松报道-双栏.pdf") == 0);
    CHECK(loim_export_suggested_path(
        NULL, root_image, 1U, 1U, LOIM_LOCALE_ZH_CN,
        "20260810-1430", path, sizeof(path)));
    CHECK(strcmp(path, "/root-document-name-fixture-单栏.pdf") == 0);

    (void)remove("Naming Test-2 Columns.pdf");
    (void)remove("Naming Test-2 Columns (2).pdf");
    existing = fopen("Naming Test-2 Columns.pdf", "wb");
    CHECK(existing != NULL);
    if (existing != NULL) {
        CHECK(fclose(existing) == 0);
    }
    CHECK(loim_export_suggested_path(
        ".", collision, 1U, 2U, LOIM_LOCALE_EN,
        "20260810-1430", path, sizeof(path)));
    CHECK(strcmp(path, "./Naming Test-2 Columns (2).pdf") == 0);
    (void)remove("Naming Test-2 Columns.pdf");
    (void)remove("Naming Test-2 Columns (2).pdf");

#if defined(_WIN32)
    (void)DeleteFileW(L"\x547D\x540D\x6D4B\x8BD5-\x53CC\x680F.pdf");
    (void)DeleteFileW(L"\x547D\x540D\x6D4B\x8BD5-\x53CC\x680F (2).pdf");
#else
    (void)remove("命名测试-双栏.pdf");
    (void)remove("命名测试-双栏 (2).pdf");
#endif
#if defined(_WIN32)
    /* Narrow fopen creates the file under the ANSI code page, while the
       collision probe below checks for the true UTF-8 name. */
    existing = _wfopen(L"\x547D\x540D\x6D4B\x8BD5-\x53CC\x680F.pdf", L"wb");
#else
    existing = fopen("命名测试-双栏.pdf", "wb");
#endif
    CHECK(existing != NULL);
    if (existing != NULL) {
        CHECK(fclose(existing) == 0);
    }
    CHECK(loim_export_suggested_path(
        ".", chinese_collision, 1U, 2U, LOIM_LOCALE_ZH_CN,
        "20260810-1430", path, sizeof(path)));
    CHECK(strcmp(path, "./命名测试-双栏 (2).pdf") == 0);
#if defined(_WIN32)
    (void)DeleteFileW(L"\x547D\x540D\x6D4B\x8BD5-\x53CC\x680F.pdf");
    (void)DeleteFileW(L"\x547D\x540D\x6D4B\x8BD5-\x53CC\x680F (2).pdf");
#else
    (void)remove("命名测试-双栏.pdf");
    (void)remove("命名测试-双栏 (2).pdf");
#endif
}


static void test_long_image_texture_plan(void)
{
    loim_texture_tile tiles[8];
    size_t count = 0U;
    size_t index;
    uint32_t next_y = 0U;

    CHECK(loim_texture_plan(1200U, 80218U, 16384U, tiles, 8U, &count) == LOIM_OK);
    CHECK(count == 5U);
    for (index = 0U; index < count; ++index) {
        CHECK(tiles[index].source_y_px == next_y);
        CHECK(tiles[index].height_px > 0U);
        CHECK(tiles[index].height_px <= 16384U);
        next_y += tiles[index].height_px;
    }
    CHECK(next_y == 80218U);
    CHECK(tiles[4].height_px == 14682U);
}

static void test_texture_plan_rejects_oversized_width(void)
{
    loim_texture_tile tile;
    size_t count = 0U;

    CHECK(loim_texture_plan(16385U, 100U, 16384U, &tile, 1U, &count) ==
          LOIM_ERROR_OVERFLOW);
}

static void test_machine_code_matches_legacy_qt_format(void)
{
    static const uint8_t machine_id[] = {'a', 'b', 'c'};
    char machine_code[LOIM_MACHINE_CODE_CAPACITY];

    CHECK(loim_machine_code_from_bytes(
              machine_id,
              sizeof(machine_id),
              machine_code,
              sizeof(machine_code)) == LOIM_OK);
    CHECK(strcmp(machine_code, "9001-5098-3CD2-4FB0-D696") == 0);
}

typedef struct mock_http {
    int calls;
    bool saw_url;
    bool saw_email;
    bool saw_architecture;
} mock_http;

static loim_status mock_login_post(
    void *context,
    const char *url,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status)
{
    static const char success_json[] =
        "{\"success\":true,\"token\":\"jwt-token\","
        "\"user\":{\"email\":\"person@example.com\","
        "\"subscription\":{\"type\":\"monthly\",\"active\":true,"
        "\"expiresAt\":\"2027-08-12T00:00:00.000Z\"}},"
        "\"message\":\"登录成功\"}";
    mock_http *mock = context;

    mock->calls += 1;
    mock->saw_url = strcmp(url, LOIM_AUTH_LOGIN_URL) == 0;
    mock->saw_email = strstr(json_body, "\"email\":\"person@example.com\"") != NULL;
    mock->saw_architecture = strstr(json_body, "\"architecture\":\"arm64\"") != NULL;
    if (strlen(success_json) + 1U > response_capacity) {
        return LOIM_ERROR_OVERFLOW;
    }
    memcpy(response, success_json, strlen(success_json) + 1U);
    *out_http_status = 200;
    return LOIM_OK;
}

static loim_login_request test_login_request(void)
{
    loim_login_request request;

    request.email = "person@example.com";
    request.password = "quote\" slash\\ newline\n";
    request.machine_code = "MAC-1234567890";
    request.platform = "macos";
    request.architecture = "arm64";
    request.os_name = "macOS";
    request.app_version = "3.0.0-test";
    return request;
}

static void test_login_payload_and_transport(void)
{
    loim_login_request request = test_login_request();
    loim_login_result result;
    mock_http mock = {0};
    int http_status = 0;

    CHECK(loim_auth_login(
        &request, mock_login_post, &mock, &result, &http_status) == LOIM_OK);
    CHECK(mock.calls == 1);
    CHECK(mock.saw_url);
    CHECK(mock.saw_email);
    CHECK(mock.saw_architecture);
    CHECK(http_status == 200);
    CHECK(result.success);
    CHECK(strcmp(result.token, "jwt-token") == 0);
    CHECK(strcmp(result.email, "person@example.com") == 0);
    CHECK(strcmp(result.subscription_type, "monthly") == 0);
    CHECK(result.subscription_active_present);
    CHECK(result.subscription_active);
    CHECK(strcmp(result.subscription_expires_at, "2027-08-12T00:00:00.000Z") == 0);
    CHECK(loim_auth_result_is_licensed(&result));
    CHECK(!loim_export_page_requires_watermark(
        loim_auth_result_is_licensed(&result), 3U, 99U));
}

static loim_status mock_session_post(
    void *context,
    const char *url,
    const char *bearer_token,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status)
{
    static const char session_json[] =
        "{\"success\":true,\"user\":{\"email\":\"person@example.com\","
        "\"subscription\":{\"type\":\"yearly\",\"active\":true,"
        "\"expiresAt\":\"2027-08-12T00:00:00.000Z\"}}}";
    mock_http *mock = context;

    mock->calls += 1;
    mock->saw_url = strcmp(url, LOIM_AUTH_SESSION_URL) == 0;
    mock->saw_email = strcmp(bearer_token, "jwt-token") == 0;
    mock->saw_architecture = strstr(json_body, "\"architecture\":\"arm64\"") != NULL;
    if (sizeof(session_json) > response_capacity) {
        return LOIM_ERROR_OVERFLOW;
    }
    memcpy(response, session_json, sizeof(session_json));
    *out_http_status = 200;
    return LOIM_OK;
}

static void test_session_restore_uses_bearer_and_refreshes_subscription(void)
{
    loim_login_request request = test_login_request();
    loim_login_result result;
    mock_http mock = {0};
    int http_status = 0;

    CHECK(loim_auth_restore_session(
        &request,
        "jwt-token",
        mock_session_post,
        &mock,
        &result,
        &http_status) == LOIM_OK);
    CHECK(mock.calls == 1);
    CHECK(mock.saw_url);
    CHECK(mock.saw_email);
    CHECK(mock.saw_architecture);
    CHECK(http_status == 200);
    CHECK(result.success);
    CHECK(loim_auth_result_is_licensed(&result));
    CHECK(strcmp(result.subscription_type, "yearly") == 0);
    CHECK(strcmp(result.subscription_expires_at, "2027-08-12T00:00:00.000Z") == 0);
}

static loim_status mock_legacy_session_post(
    void *context,
    const char *url,
    const char *bearer_token,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status)
{
    static const char legacy_json[] =
        "{\"success\":true,\"message\":\"heartbeat ok\"}";

    (void)context;
    (void)url;
    (void)bearer_token;
    (void)json_body;
    if (sizeof(legacy_json) > response_capacity) {
        return LOIM_ERROR_OVERFLOW;
    }
    memcpy(response, legacy_json, sizeof(legacy_json));
    *out_http_status = 200;
    return LOIM_OK;
}

static void test_session_restore_rejects_missing_subscription_details(void)
{
    loim_login_request request = test_login_request();
    loim_login_result result;
    int http_status = 0;

    CHECK(loim_auth_restore_session(
        &request,
        "jwt-token",
        mock_legacy_session_post,
        NULL,
        &result,
        &http_status) == LOIM_ERROR_INVALID_DATA);
    CHECK(http_status == 200);
}

static void test_subscription_authorization_uses_explicit_active_state(void)
{
    static const char active_free_json[] =
        "{\"success\":true,\"token\":\"jwt-token\","
        "\"user\":{\"email\":\"subscriber@example.com\","
        "\"subscription\":{\"type\":\"free\",\"active\":true}}}";
    static const char inactive_paid_json[] =
        "{\"success\":true,\"token\":\"jwt-token\","
        "\"user\":{\"email\":\"expired@example.com\","
        "\"subscription\":{\"type\":\"monthly\",\"active\":false}}}";
    static const char legacy_paid_json[] =
        "{\"success\":true,\"token\":\"jwt-token\","
        "\"user\":{\"email\":\"legacy@example.com\","
        "\"subscription\":{\"type\":\"yearly\"}}}";
    static const char missing_subscription_json[] =
        "{\"success\":true,\"token\":\"jwt-token\","
        "\"user\":{\"email\":\"free@example.com\"}}";
    loim_login_result result;

    CHECK(loim_auth_parse_login_response(active_free_json, &result) == LOIM_OK);
    CHECK(result.subscription_active_present);
    CHECK(result.subscription_active);
    CHECK(loim_auth_result_is_licensed(&result));

    CHECK(loim_auth_parse_login_response(inactive_paid_json, &result) == LOIM_OK);
    CHECK(result.subscription_active_present);
    CHECK(!result.subscription_active);
    CHECK(!loim_auth_result_is_licensed(&result));
    CHECK(loim_export_page_requires_watermark(
        loim_auth_result_is_licensed(&result), 3U, 1U));

    CHECK(loim_auth_parse_login_response(legacy_paid_json, &result) == LOIM_OK);
    CHECK(!result.subscription_active_present);
    CHECK(loim_auth_result_is_licensed(&result));

    CHECK(loim_auth_parse_login_response(missing_subscription_json, &result) == LOIM_OK);
    CHECK(!result.subscription_active_present);
    CHECK(!loim_auth_result_is_licensed(&result));
}

static void test_login_json_escapes_credentials(void)
{
    loim_login_request request = test_login_request();
    char json[2048];
    size_t length = 0U;

    CHECK(loim_auth_build_login_json(&request, json, sizeof(json), &length) == LOIM_OK);
    CHECK(length == strlen(json));
    CHECK(strstr(json, "quote\\\" slash\\\\ newline\\n") != NULL);
    CHECK(strstr(json, "\"clientName\":\"LoimReader\"") != NULL);
    CHECK(strstr(json, "\"deviceInfo\"") != NULL);
}

static void test_login_error_response(void)
{
    static const char error_json[] =
        "{\"success\":false,\"error\":\"DEVICE_LIMIT_EXCEEDED\","
        "\"message\":\"设备数量超出限制\",\"deviceLimitExceeded\":true}";
    loim_login_result result;

    CHECK(loim_auth_parse_login_response(error_json, &result) == LOIM_OK);
    CHECK(!result.success);
    CHECK(result.device_limit_exceeded);
    CHECK(strcmp(result.error_code, "DEVICE_LIMIT_EXCEEDED") == 0);
    CHECK(strcmp(result.message, "设备数量超出限制") == 0);
}

static void test_login_rejects_malformed_response(void)
{
    loim_login_result result;

    CHECK(loim_auth_parse_login_response(
              "{\"success\":false", &result) != LOIM_OK);
    CHECK(loim_auth_parse_login_response(
              "{\"success\":true,\"token\":\"\"}", &result) != LOIM_OK);
}

static void test_login_payload_reports_small_buffer(void)
{
    loim_login_request request = test_login_request();
    char json[16] = "unchanged";
    size_t length = 99U;

    CHECK(loim_auth_build_login_json(
              &request, json, sizeof(json), &length) == LOIM_ERROR_OVERFLOW);
    CHECK(json[0] == '\0');
    CHECK(length == 0U);
}

typedef struct mock_update_http {
    const char *response_json;
    int response_status;
    int calls;
    bool saw_url;
    bool saw_version;
    bool saw_platform;
    bool saw_architecture;
} mock_update_http;

static loim_status mock_update_post(
    void *context,
    const char *url,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status)
{
    mock_update_http *mock = context;
    size_t length = strlen(mock->response_json);

    mock->calls += 1;
    mock->saw_url = strcmp(url, LOIM_UPDATE_CHECK_URL) == 0;
    mock->saw_version = strstr(json_body, "\"currentVersion\":\"3.0.0\"") != NULL;
    mock->saw_platform = strstr(json_body, "\"platform\":\"macos\"") != NULL;
    mock->saw_architecture = strstr(json_body, "\"architecture\":\"arm64\"") != NULL;
    if (length + 1U > response_capacity) {
        return LOIM_ERROR_OVERFLOW;
    }
    memcpy(response, mock->response_json, length + 1U);
    *out_http_status = mock->response_status;
    return LOIM_OK;
}

static loim_update_request test_update_request(void)
{
    loim_update_request request;

    request.current_version = "3.0.0";
    request.platform = "macos";
    request.architecture = "arm64";
    return request;
}

static void test_update_300_to_301_contract(void)
{
    static const char response_json[] =
        "{\"success\":true,\"hasUpdate\":true,"
        "\"currentVersion\":\"3.0.0\",\"latestVersion\":\"3.0.1\","
        "\"forceUpdate\":false,\"updateInfo\":{"
        "\"downloadUrl\":\"https://ctdy123.com/download/loimreader/macos/arm64/"
        "LoimReader_3.0.1_macos_arm64.dmg\","
        "\"releaseNotes\":\"修复导出并改进\\n自动升级\"}}";
    loim_update_request request = test_update_request();
    loim_update_result result;
    mock_update_http mock = {response_json, 200, 0, false, false, false, false};
    int http_status = 0;

    CHECK(loim_update_check(
              &request,
              mock_update_post,
              &mock,
              &result,
              &http_status) == LOIM_OK);
    CHECK(mock.calls == 1);
    CHECK(mock.saw_url);
    CHECK(mock.saw_version);
    CHECK(mock.saw_platform);
    CHECK(mock.saw_architecture);
    CHECK(http_status == 200);
    CHECK(result.has_update);
    CHECK(!result.force_update);
    CHECK(strcmp(result.latest_version, "3.0.1") == 0);
    CHECK(strcmp(
              result.download_url,
              "https://ctdy123.com/download/loimreader/macos/arm64/"
              "LoimReader_3.0.1_macos_arm64.dmg") == 0);
    CHECK(strstr(result.release_notes, "自动升级") != NULL);
}

static void test_update_version_and_channel_rules(void)
{
    CHECK(loim_update_should_offer("3.0.0", "3.0.1"));
    CHECK(!loim_update_should_offer("3.0.0", "3.0.0"));
    CHECK(!loim_update_should_offer("3.0.1", "3.0.0"));
    CHECK(!loim_update_should_offer("3.0.0", "3.0.1-alpha.1"));
    CHECK(loim_update_should_offer("3.0.0-alpha.7", "3.0.0"));
    CHECK(loim_update_should_offer("3.0.1-alpha.1", "3.0.1-alpha.2"));
    CHECK(loim_update_should_offer("3.0.1-alpha.9", "3.0.1-alpha.10"));
    CHECK(loim_update_should_offer("3.0.1-alpha", "3.0.1-beta"));
    CHECK(!loim_update_should_offer("3.0.1+build.1", "3.0.1+build.2"));
    CHECK(!loim_update_should_offer("dev", "3.0.1"));
    CHECK(!loim_update_should_offer("3.0", "3.0.1"));
}

static void test_update_rejects_untrusted_or_wrong_target_download(void)
{
    static const char untrusted_json[] =
        "{\"success\":true,\"hasUpdate\":true,\"latestVersion\":\"3.0.1\","
        "\"forceUpdate\":false,\"updateInfo\":{"
        "\"downloadUrl\":\"https://example.com/LoimReader.dmg\"}}";
    static const char wrong_arch_json[] =
        "{\"success\":true,\"hasUpdate\":true,\"latestVersion\":\"3.0.1\","
        "\"forceUpdate\":false,\"updateInfo\":{"
        "\"downloadUrl\":\"https://ctdy123.com/download/loimreader/macos/amd64/"
        "LoimReader_3.0.1_macos_amd64.dmg\"}}";
    static const char wrong_version_file_json[] =
        "{\"success\":true,\"hasUpdate\":true,\"latestVersion\":\"3.0.1\","
        "\"forceUpdate\":false,\"updateInfo\":{"
        "\"downloadUrl\":\"https://ctdy123.com/download/loimreader/macos/arm64/"
        "LoimReader_3.0.0_macos_arm64.dmg\"}}";
    loim_update_request request = test_update_request();
    loim_update_result result;

    CHECK(loim_update_parse_check_response(
              &request, untrusted_json, &result) == LOIM_ERROR_INVALID_DATA);
    CHECK(loim_update_parse_check_response(
              &request, wrong_arch_json, &result) == LOIM_ERROR_INVALID_DATA);
    CHECK(loim_update_parse_check_response(
              &request, wrong_version_file_json, &result) == LOIM_ERROR_INVALID_DATA);
}

static void test_update_suppresses_stale_server_answers(void)
{
    static const char same_version_json[] =
        "{\"success\":true,\"hasUpdate\":true,\"latestVersion\":\"3.0.0\","
        "\"forceUpdate\":false,\"updateInfo\":{"
        "\"downloadUrl\":\"https://ctdy123.com/download/loimreader/macos/arm64/"
        "LoimReader_3.0.0_macos_arm64.dmg\"}}";
    static const char prerelease_json[] =
        "{\"success\":true,\"hasUpdate\":true,"
        "\"latestVersion\":\"3.0.1-alpha.1\",\"forceUpdate\":false,"
        "\"updateInfo\":{\"downloadUrl\":"
        "\"https://ctdy123.com/download/loimreader/macos/arm64/"
        "LoimReader_3.0.1-alpha.1_macos_arm64.dmg\"}}";
    loim_update_request request = test_update_request();
    loim_update_result result;

    CHECK(loim_update_parse_check_response(
              &request, same_version_json, &result) == LOIM_OK);
    CHECK(!result.has_update);
    CHECK(loim_update_parse_check_response(
              &request, prerelease_json, &result) == LOIM_OK);
    CHECK(!result.has_update);
}

static void test_update_parser_ignores_forward_compatible_fields(void)
{
    static const char response_json[] =
        "{\"schemaVersion\":2,\"success\":true,\"hasUpdate\":true,"
        "\"latestVersion\":\"3.0.1\",\"forceUpdate\":false,"
        "\"updateInfo\":{\"sizeBytes\":123456,"
        "\"downloadUrl\":\"https://ctdy123.com/download/loimreader/macos/arm64/"
        "LoimReader_3.0.1_macos_arm64.dmg\",\"releaseNotes\":null}}";
    loim_update_request request = test_update_request();
    loim_update_result result;

    CHECK(loim_update_parse_check_response(
              &request, response_json, &result) == LOIM_OK);
    CHECK(result.has_update);
    CHECK(result.release_notes[0] == '\0');
}

static void test_windows_update_requires_zip_package(void)
{
    static const char zip_json[] =
        "{\"success\":true,\"hasUpdate\":true,\"latestVersion\":\"3.0.3\","
        "\"forceUpdate\":false,\"updateInfo\":{"
        "\"downloadUrl\":\"https://ctdy123.com/download/loimreader/windows/amd64/"
        "LoimReader_3.0.3_windows_amd64.zip\"}}";
    static const char legacy_msi_json[] =
        "{\"success\":true,\"hasUpdate\":true,\"latestVersion\":\"3.0.3\","
        "\"forceUpdate\":false,\"updateInfo\":{"
        "\"downloadUrl\":\"https://ctdy123.com/download/loimreader/windows/amd64/"
        "LoimReader_3.0.3_windows_amd64.msi\"}}";
    loim_update_request request = {
        "3.0.2",
        "windows",
        "amd64"
    };
    loim_update_result result;

    CHECK(loim_update_parse_check_response(
              &request, zip_json, &result) == LOIM_OK);
    CHECK(result.has_update);
    CHECK(strcmp(
              result.download_url,
              "https://ctdy123.com/download/loimreader/windows/amd64/"
              "LoimReader_3.0.3_windows_amd64.zip") == 0);
    CHECK(loim_update_parse_check_response(
              &request, legacy_msi_json, &result) == LOIM_ERROR_INVALID_DATA);
}

static void test_update_rejects_http_failure(void)
{
    static const char response_json[] = "{\"success\":false}";
    loim_update_request request = test_update_request();
    loim_update_result result;
    mock_update_http mock = {response_json, 503, 0, false, false, false, false};
    int http_status = 0;

    CHECK(loim_update_check(
              &request,
              mock_update_post,
              &mock,
              &result,
              &http_status) == LOIM_ERROR_IO);
    CHECK(http_status == 503);
    CHECK(!result.has_update);
}

static void write_le16(unsigned char *destination, uint16_t value)
{
    destination[0] = (unsigned char)(value & 0xFFU);
    destination[1] = (unsigned char)((value >> 8U) & 0xFFU);
}

static void write_le32(unsigned char *destination, uint32_t value)
{
    destination[0] = (unsigned char)(value & 0xFFU);
    destination[1] = (unsigned char)((value >> 8U) & 0xFFU);
    destination[2] = (unsigned char)((value >> 16U) & 0xFFU);
    destination[3] = (unsigned char)((value >> 24U) & 0xFFU);
}

static bool write_test_bmp(const char *path, uint32_t width, uint32_t height)
{
    unsigned char header[54] = {0};
    unsigned char *row;
    uint32_t row_bytes;
    uint32_t pixel_bytes;
    uint32_t y;
    uint32_t x;
    FILE *file;
    bool success = true;

    if (path == NULL || width == 0U || height == 0U ||
        width > (UINT32_MAX - 3U) / 3U) {
        return false;
    }
    row_bytes = (width * 3U + 3U) & ~3U;
    if (height > (UINT32_MAX - (uint32_t)sizeof(header)) / row_bytes) {
        return false;
    }
    pixel_bytes = row_bytes * height;
    row = calloc(row_bytes, 1U);
    if (row == NULL) {
        return false;
    }
    for (x = 0U; x < width; ++x) {
        row[x * 3U] = (unsigned char)(64U + x % 128U);
        row[x * 3U + 1U] = (unsigned char)(96U + x % 96U);
        row[x * 3U + 2U] = (unsigned char)(128U + x % 64U);
    }
    header[0] = 'B';
    header[1] = 'M';
    write_le32(header + 2U, (uint32_t)sizeof(header) + pixel_bytes);
    write_le32(header + 10U, (uint32_t)sizeof(header));
    write_le32(header + 14U, 40U);
    write_le32(header + 18U, width);
    write_le32(header + 22U, height);
    write_le16(header + 26U, 1U);
    write_le16(header + 28U, 24U);
    write_le32(header + 34U, pixel_bytes);
    file = fopen(path, "wb");
    if (file == NULL || fwrite(header, 1U, sizeof(header), file) != sizeof(header)) {
        success = false;
    }
    for (y = 0U; success && y < height; ++y) {
        if (fwrite(row, 1U, row_bytes, file) != row_bytes) {
            success = false;
        }
    }
    if (file != NULL && fclose(file) != 0) {
        success = false;
    }
    free(row);
    return success;
}

static void test_settings_round_trip_and_clamping(void)
{
    loim_settings settings;
    loim_settings restored;
    char text[LOIM_SETTINGS_TEXT_CAPACITY];

    loim_settings_defaults(&settings);
    CHECK(settings.columns == 1U);
    CHECK(settings.page_number_mode == LOIM_PAGE_NUMBER_NONE);
    CHECK(settings.margin_ratio == 0.08F);
    CHECK(settings.preview_scale == LOIM_PREVIEW_SCALE_DEFAULT);
    CHECK(settings.split_ratio == 0.5F);
    CHECK(settings.window_width == LOIM_WINDOW_DEFAULT_WIDTH &&
        settings.window_height == LOIM_WINDOW_DEFAULT_HEIGHT &&
        !settings.has_window_position);

    /* Defaults survive an encode/decode round trip. */
    CHECK(loim_settings_encode(&settings, text, sizeof(text)) == LOIM_OK);
    memset(&restored, 0, sizeof(restored));
    CHECK(loim_settings_decode(text, &restored) == LOIM_OK);
    CHECK(restored.columns == settings.columns &&
        restored.page_number_mode == settings.page_number_mode &&
        restored.margin_ratio == settings.margin_ratio &&
        restored.preview_scale == settings.preview_scale);

    /* Custom values round-trip exactly (floats stored as scaled ints). */
    settings.columns = 3U;
    settings.page_number_mode = LOIM_PAGE_NUMBER_BOTTOM_CENTER;
    settings.margin_ratio = 0.12F;
    settings.preview_scale = 1.2F;
    settings.split_ratio = 0.62F;
    settings.window_width = 1200;
    settings.window_height = 800;
    settings.window_x = 120;
    settings.window_y = 90;
    settings.has_window_position = true;
    CHECK(loim_settings_encode(&settings, text, sizeof(text)) == LOIM_OK);
    memset(&restored, 0, sizeof(restored));
    CHECK(loim_settings_decode(text, &restored) == LOIM_OK);
    CHECK(restored.columns == 3U &&
        restored.page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER &&
        restored.margin_ratio == 0.12F && restored.preview_scale == 1.2F &&
        restored.split_ratio == 0.62F &&
        restored.window_width == 1200 && restored.window_height == 800 &&
        restored.window_x == 120 && restored.window_y == 90 &&
        restored.has_window_position);

    /* A file without coordinates has no saved position. */
    settings.has_window_position = false;
    CHECK(loim_settings_encode(&settings, text, sizeof(text)) == LOIM_OK);
    memset(&restored, 0, sizeof(restored));
    CHECK(loim_settings_decode(text, &restored) == LOIM_OK);
    CHECK(!restored.has_window_position);

    /* Out-of-range values clamp into the valid UI ranges. */
    memset(&restored, 0, sizeof(restored));
    CHECK(loim_settings_decode(
            "columns=9\npage_numbers=7\nmargin=80\nzoom=99\n"
            "split=99\nwidth=10\nheight=99999\n",
            &restored) == LOIM_OK);
    CHECK(restored.columns == 3U &&
        restored.page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER &&
        restored.margin_ratio == LOIM_MARGIN_RATIO_MAX &&
        restored.preview_scale == LOIM_PREVIEW_SCALE_MAX &&
        restored.split_ratio == LOIM_SPLIT_RATIO_MAX &&
        restored.window_width == LOIM_WINDOW_MIN_WIDTH &&
        restored.window_height == LOIM_WINDOW_MAX_SIZE &&
        !restored.has_window_position);

    /* Unknown keys and malformed lines are ignored; seeded defaults survive. */
    loim_settings_defaults(&restored);
    CHECK(loim_settings_decode(
            "future_toggle=1\n%%%%\ncolumns\nmargin=garbage\nzoom=8\n",
            &restored) == LOIM_OK);
    CHECK(restored.columns == 1U &&
        restored.page_number_mode == LOIM_PAGE_NUMBER_NONE &&
        restored.margin_ratio == 0.08F &&
        restored.preview_scale == 0.8F);

    /* Argument validation. */
    CHECK(loim_settings_encode(NULL, text, sizeof(text)) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_settings_encode(&settings, NULL, sizeof(text)) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_settings_encode(&settings, text, 4U) == LOIM_ERROR_OVERFLOW);
    CHECK(loim_settings_decode(NULL, &restored) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_settings_decode(text, NULL) == LOIM_ERROR_INVALID_ARGUMENT);
}

static void test_split_hint_enumeration_and_removal(void)
{
    loim_document *document = NULL;
    loim_source_info source = {"/tmp/split-source.png", 400U, 3000U};
    loim_split_hint_info info;
    size_t source_index = 99U;

    CHECK(loim_document_split_hint_count(NULL, 0U) == 0U);
    CHECK(loim_document_create(&document) == LOIM_OK);
    /* No sources yet: every accessor rejects or reports empty. */
    CHECK(loim_document_split_hint_count(document, 0U) == 0U);
    CHECK(loim_document_split_hint_at(document, 0U, 0U, &info) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_document_remove_split_hint(document, 0U, 0U) ==
        LOIM_ERROR_INVALID_ARGUMENT);

    CHECK(loim_document_add_source(document, &source, &source_index) == LOIM_OK);
    CHECK(source_index == 0U);
    CHECK(loim_document_add_split_hint(
              document, 0U, 100U, 0.5F, LOIM_SPLIT_HINT_WHITESPACE) == LOIM_OK);
    CHECK(loim_document_add_split_hint(
              document, 0U, 200U, 0.75F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_document_add_split_hint(
              document, 0U, 300U, 0.9F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_document_split_hint_count(document, 0U) == 3U);

    CHECK(loim_document_split_hint_at(document, 0U, 1U, &info) == LOIM_OK);
    CHECK(info.row == 200U);
    CHECK(info.quality == 0.75F);
    CHECK(info.kind == LOIM_SPLIT_HINT_MANUAL);
    CHECK(loim_document_split_hint_at(document, 0U, 3U, &info) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_document_split_hint_at(document, 9U, 0U, &info) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_document_split_hint_at(document, 0U, 0U, NULL) ==
        LOIM_ERROR_INVALID_ARGUMENT);

    /* Removing the middle hint shifts later hints down. */
    CHECK(loim_document_remove_split_hint(document, 0U, 1U) == LOIM_OK);
    CHECK(loim_document_split_hint_count(document, 0U) == 2U);
    CHECK(loim_document_split_hint_at(document, 0U, 0U, &info) == LOIM_OK);
    CHECK(info.row == 100U && info.kind == LOIM_SPLIT_HINT_WHITESPACE);
    CHECK(loim_document_split_hint_at(document, 0U, 1U, &info) == LOIM_OK);
    CHECK(info.row == 300U && info.kind == LOIM_SPLIT_HINT_MANUAL);
    CHECK(loim_document_remove_split_hint(document, 0U, 2U) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_document_remove_split_hint(document, 5U, 0U) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    CHECK(loim_document_remove_split_hint(NULL, 0U, 0U) ==
        LOIM_ERROR_INVALID_ARGUMENT);
    loim_document_destroy(document);
}

static void test_manual_split_hint_can_move_repeatedly(void)
{
    /* One 1000x8000 source normalizes to height 8000 at content width 1000,
       and a hint at source row r lands exactly at virtual row r. */
    loim_document *document = NULL;
    loim_source_info source = {"/tmp/long-strip.png", 1000U, 8000U};
    loim_layout_options options;
    loim_layout layout;
    size_t source_index = 0U;

    loim_layout_options_a4(&options, 1000U);
    CHECK(options.target_page_height_px == 1414U);
    CHECK(options.minimum_page_height_px == 1131U);
    CHECK(options.maximum_page_height_px == 1696U);
    CHECK(options.search_radius_px == 212U);

    CHECK(loim_document_create(&document) == LOIM_OK);
    CHECK(loim_document_add_source(document, &source, &source_index) == LOIM_OK);

    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.page_count == 6U);
    CHECK(layout.pages[0].height_px == 1414U);
    loim_layout_destroy(&layout);

    /* First manual drag: the seam moves from 1414 down to 1500. */
    CHECK(loim_document_add_split_hint(
              document, 0U, 1500U, 1.0F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.page_count == 6U);
    CHECK(layout.pages[0].height_px == 1500U);
    loim_layout_destroy(&layout);

    /* Stacking a second hint farther from the ideal would pin the seam at
       the first hint (equal bonus, nearest-to-ideal wins); the desktop app
       therefore replaces the previous override instead of stacking. */
    CHECK(loim_document_add_split_hint(
              document, 0U, 1600U, 1.0F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.pages[0].height_px == 1500U);
    loim_layout_destroy(&layout);

    /* Replacement flow: drop the old hint, add the new one, seam follows. */
    CHECK(loim_document_split_hint_count(document, 0U) == 2U);
    CHECK(loim_document_remove_split_hint(document, 0U, 0U) == LOIM_OK);
    CHECK(loim_document_remove_split_hint(document, 0U, 0U) == LOIM_OK);
    CHECK(loim_document_split_hint_count(document, 0U) == 0U);
    CHECK(loim_document_add_split_hint(
              document, 0U, 1600U, 1.0F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.pages[0].height_px == 1600U);
    loim_layout_destroy(&layout);

    /* A manual hint reaches beyond the search radius (1626) up to the legal
       maximum (1696) so larger drags never snap back. */
    CHECK(loim_document_remove_split_hint(document, 0U, 0U) == LOIM_OK);
    CHECK(loim_document_add_split_hint(
              document, 0U, 1690U, 1.0F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.pages[0].height_px == 1690U);
    loim_layout_destroy(&layout);

    /* Past the legal maximum the hint is rejected and the break falls back
       to the ideal page height. */
    CHECK(loim_document_remove_split_hint(document, 0U, 0U) == LOIM_OK);
    CHECK(loim_document_add_split_hint(
              document, 0U, 1800U, 1.0F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.pages[0].height_px == 1414U);
    loim_layout_destroy(&layout);

    /* Automatic hints stay limited to the search radius even at the same
       position where a manual hint is honored. */
    CHECK(loim_document_remove_split_hint(document, 0U, 0U) == LOIM_OK);
    CHECK(loim_document_add_split_hint(
              document, 0U, 1650U, 1.0F, LOIM_SPLIT_HINT_WHITESPACE) == LOIM_OK);
    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.pages[0].height_px == 1414U);
    loim_layout_destroy(&layout);
    CHECK(loim_document_remove_split_hint(document, 0U, 0U) == LOIM_OK);
    CHECK(loim_document_add_split_hint(
              document, 0U, 1650U, 1.0F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.pages[0].height_px == 1650U);
    loim_layout_destroy(&layout);

    /* Hints on different seams of the same source are both honored. */
    CHECK(loim_document_add_split_hint(
              document, 0U, 3100U, 1.0F, LOIM_SPLIT_HINT_MANUAL) == LOIM_OK);
    CHECK(loim_layout_build(document, &options, &layout) == LOIM_OK);
    CHECK(layout.pages[0].height_px == 1650U);
    CHECK(layout.pages[1].height_px == 1450U);
    loim_layout_destroy(&layout);

    loim_document_destroy(document);
}

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "--generate-pdf-fixtures") == 0) {
        return write_test_bmp(argv[2], 100U, 100U) &&
            write_test_bmp(argv[3], 1200U, 1697U)
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }
    test_system_language_selection();
    test_both_locales_are_complete();
    test_drop_queue_accepts_implicit_begin();
    test_drop_queue_resets_for_next_drag();
    test_workspace_mode_and_clear_availability();
    test_column_cycle_and_import_progress();
    test_page_number_cycle_and_origin();
    test_preview_width_and_divider_scroll_anchor();
    test_export_default_filename_rules();
    test_long_image_texture_plan();
    test_texture_plan_rejects_oversized_width();
    test_machine_code_matches_legacy_qt_format();
    test_login_payload_and_transport();
    test_session_restore_uses_bearer_and_refreshes_subscription();
    test_session_restore_rejects_missing_subscription_details();
    test_subscription_authorization_uses_explicit_active_state();
    test_login_json_escapes_credentials();
    test_login_error_response();
    test_login_rejects_malformed_response();
    test_login_payload_reports_small_buffer();
    test_update_300_to_301_contract();
    test_update_version_and_channel_rules();
    test_update_rejects_untrusted_or_wrong_target_download();
    test_update_suppresses_stale_server_answers();
    test_update_parser_ignores_forward_compatible_fields();
    test_windows_update_requires_zip_package();
    test_update_rejects_http_failure();
    test_settings_round_trip_and_clamping();
    test_split_hint_enumeration_and_removal();
    test_manual_split_hint_can_move_repeatedly();

    if (failures != 0) {
        fprintf(stderr, "%d desktop model test check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("all desktop model tests passed");
    return EXIT_SUCCESS;
}
