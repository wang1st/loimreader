#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loim/auth.h"
#include "loim/i18n.h"
#include "loim/import_queue.h"
#include "loim/machine_code.h"
#include "loim/presentation.h"
#include "loim/pdf.h"
#include "loim/texture_plan.h"

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

static void test_column_cycle_and_import_progress(void)
{
    char export_path[256];
    CHECK(loim_columns_normalize(0U) == 1U);
    CHECK(loim_columns_normalize(2U) == 2U);
    CHECK(loim_columns_normalize(99U) == 3U);
    CHECK(loim_columns_next(1U) == 2U);
    CHECK(loim_columns_next(2U) == 3U);
    CHECK(loim_columns_next(3U) == 1U);
    CHECK(loim_sheet_slot_count(2U) == 4U);
    CHECK(loim_sheet_slot_count(3U) == 9U);
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
    CHECK(loim_progress_percent(0U, 0U) == 0U);
    CHECK(loim_progress_percent(1U, 3U) == 33U);
    CHECK(loim_progress_percent(3U, 3U) == 100U);
    CHECK(loim_progress_percent(4U, 3U) == 100U);
}

static void test_pdf_writer_creates_a_valid_document(void)
{
    static const unsigned char pixel[] = {255U, 0U, 0U};
    static const char path[] = "loim-test-export.pdf";
    loim_pdf_rgb_page page = {1U, 1U, 595U, 842U, pixel};
    FILE *file;
    char header[9] = {0};

    CHECK(loim_pdf_write_rgb(path, &page, 1U) == LOIM_OK);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fread(header, 1U, sizeof(header) - 1U, file) == sizeof(header) - 1U);
        CHECK(memcmp(header, "%PDF-1.4", sizeof(header) - 1U) == 0);
        fclose(file);
        (void)remove(path);
    }
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
        "\"subscription\":{\"type\":\"monthly\",\"active\":true}},"
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

int main(void)
{
    test_system_language_selection();
    test_both_locales_are_complete();
    test_drop_queue_accepts_implicit_begin();
    test_drop_queue_resets_for_next_drag();
    test_column_cycle_and_import_progress();
    test_pdf_writer_creates_a_valid_document();
    test_long_image_texture_plan();
    test_texture_plan_rejects_oversized_width();
    test_machine_code_matches_legacy_qt_format();
    test_login_payload_and_transport();
    test_login_json_escapes_credentials();
    test_login_error_response();
    test_login_rejects_malformed_response();
    test_login_payload_reports_small_buffer();

    if (failures != 0) {
        fprintf(stderr, "%d desktop model test check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("all desktop model tests passed");
    return EXIT_SUCCESS;
}
