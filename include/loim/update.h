#ifndef LOIM_UPDATE_H
#define LOIM_UPDATE_H

#include <stdbool.h>
#include <stddef.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOIM_UPDATE_CHECK_URL "https://ctdy123.com/api/client/version/check"
#define LOIM_UPDATE_RESPONSE_CAPACITY 65536U
#define LOIM_UPDATE_VERSION_CAPACITY 64U
#define LOIM_UPDATE_URL_CAPACITY 4096U
#define LOIM_UPDATE_NOTES_CAPACITY 8192U

typedef struct loim_update_request {
    const char *current_version;
    const char *platform;
    const char *architecture;
} loim_update_request;

typedef struct loim_update_result {
    bool has_update;
    bool force_update;
    char latest_version[LOIM_UPDATE_VERSION_CAPACITY];
    char download_url[LOIM_UPDATE_URL_CAPACITY];
    char release_notes[LOIM_UPDATE_NOTES_CAPACITY];
} loim_update_result;

typedef loim_status (*loim_update_post_fn)(
    void *context,
    const char *url,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status);

bool loim_update_should_offer(
    const char *current_version,
    const char *latest_version);

loim_status loim_update_build_check_json(
    const loim_update_request *request,
    char *output,
    size_t output_capacity,
    size_t *out_length);

loim_status loim_update_parse_check_response(
    const loim_update_request *request,
    const char *json,
    loim_update_result *out_result);

loim_status loim_update_check(
    const loim_update_request *request,
    loim_update_post_fn post,
    void *post_context,
    loim_update_result *out_result,
    int *out_http_status);

#ifdef __cplusplus
}
#endif

#endif
