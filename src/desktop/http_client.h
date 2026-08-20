#ifndef LOIM_HTTP_CLIENT_H
#define LOIM_HTTP_CLIENT_H

#include <stdbool.h>

#include "loim/auth.h"

bool loim_http_initialize(void);
void loim_http_cleanup(void);
loim_status loim_http_post_json(
    void *context,
    const char *url,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status);
loim_status loim_http_post_json_authorized(
    void *context,
    const char *url,
    const char *bearer_token,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status);

#endif
