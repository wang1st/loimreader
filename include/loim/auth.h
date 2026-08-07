#ifndef LOIM_AUTH_H
#define LOIM_AUTH_H

#include <stdbool.h>
#include <stddef.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOIM_AUTH_LOGIN_URL "https://ctdy123.com/api/auth/client/login"
#define LOIM_AUTH_RESPONSE_CAPACITY 16384U

typedef struct loim_login_request {
    const char *email;
    const char *password;
    const char *machine_code;
    const char *platform;
    const char *architecture;
    const char *os_name;
    const char *app_version;
} loim_login_request;

typedef struct loim_login_result {
    bool success;
    bool device_limit_exceeded;
    char token[4096];
    char email[256];
    char subscription_type[64];
    char error_code[64];
    char message[512];
} loim_login_result;

typedef loim_status (*loim_auth_post_fn)(
    void *context,
    const char *url,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status);

loim_status loim_auth_build_login_json(
    const loim_login_request *request,
    char *output,
    size_t output_capacity,
    size_t *out_length);

loim_status loim_auth_parse_login_response(
    const char *json,
    loim_login_result *out_result);

loim_status loim_auth_login(
    const loim_login_request *request,
    loim_auth_post_fn post,
    void *post_context,
    loim_login_result *out_result,
    int *out_http_status);

#ifdef __cplusplus
}
#endif

#endif
