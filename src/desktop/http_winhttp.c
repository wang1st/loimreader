#include "http_client.h"

#include "loim/update.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

bool loim_http_initialize(void)
{
    return true;
}

void loim_http_cleanup(void)
{
}

static bool make_authorization_header(
    const char *token,
    wchar_t *output,
    size_t output_capacity)
{
    static const wchar_t prefix[] = L"Authorization: Bearer ";
    size_t prefix_length = sizeof(prefix) / sizeof(prefix[0]) - 1U;
    size_t token_length;
    size_t index;

    if (token == NULL || token[0] == '\0') {
        return false;
    }
    token_length = strlen(token);
    if (prefix_length + token_length + 3U > output_capacity) {
        return false;
    }
    memcpy(output, prefix, prefix_length * sizeof(*output));
    for (index = 0U; index < token_length; ++index) {
        unsigned char value = (unsigned char)token[index];

        if (value < 0x21U || value > 0x7EU) {
            return false;
        }
        output[prefix_length + index] = (wchar_t)value;
    }
    output[prefix_length + token_length] = L'\r';
    output[prefix_length + token_length + 1U] = L'\n';
    output[prefix_length + token_length + 2U] = L'\0';
    return true;
}

static loim_status post_json_impl(
    void *context,
    const char *url,
    const char *bearer_token,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status)
{
    static const wchar_t content_type[] =
        L"Content-Type: application/json; charset=utf-8\r\n";
    HINTERNET session = NULL;
    HINTERNET connection = NULL;
    HINTERNET request = NULL;
    size_t body_size;
    size_t response_size = 0U;
    DWORD status_code = 0U;
    DWORD status_size = sizeof(status_code);
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    const wchar_t *request_path;
    wchar_t authorization[4200];
    loim_status result = LOIM_ERROR_IO;

    (void)context;
    if (url == NULL || json_body == NULL || response == NULL || response_capacity == 0U ||
        out_http_status == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    if (strcmp(url, LOIM_AUTH_LOGIN_URL) == 0) {
        request_path = L"/api/auth/client/login";
        if (bearer_token != NULL) {
            return LOIM_ERROR_INVALID_ARGUMENT;
        }
    } else if (strcmp(url, LOIM_AUTH_SESSION_URL) == 0) {
        request_path = L"/api/auth/client/heartbeat";
        if (!make_authorization_header(
                bearer_token,
                authorization,
                sizeof(authorization) / sizeof(authorization[0]))) {
            return LOIM_ERROR_INVALID_ARGUMENT;
        }
    } else if (strcmp(url, LOIM_UPDATE_CHECK_URL) == 0) {
        request_path = L"/api/client/version/check";
        if (bearer_token != NULL) {
            return LOIM_ERROR_INVALID_ARGUMENT;
        }
    } else {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    body_size = strlen(json_body);
    if (body_size > UINT32_MAX) {
        return LOIM_ERROR_OVERFLOW;
    }
    response[0] = '\0';
    session = WinHttpOpen(
        L"LoimReader/3",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0U);
    if (session == NULL) {
        goto cleanup;
    }
    (void)WinHttpSetTimeouts(session, 8000, 8000, 25000, 25000);
    connection = WinHttpConnect(
        session, L"ctdy123.com", INTERNET_DEFAULT_HTTPS_PORT, 0U);
    if (connection == NULL) {
        goto cleanup;
    }
    request = WinHttpOpenRequest(
        connection,
        L"POST",
        request_path,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (request == NULL) {
        goto cleanup;
    }
    (void)WinHttpSetOption(
        request,
        WINHTTP_OPTION_REDIRECT_POLICY,
        &redirect_policy,
        sizeof(redirect_policy));
    if (bearer_token != NULL && !WinHttpAddRequestHeaders(
            request,
            authorization,
            (DWORD)-1L,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
        goto cleanup;
    }
    if (!WinHttpSendRequest(
            request,
            content_type,
            (DWORD)(sizeof(content_type) / sizeof(content_type[0]) - 1U),
            (void *)json_body,
            (DWORD)body_size,
            (DWORD)body_size,
            0U) ||
        !WinHttpReceiveResponse(request, NULL) ||
        !WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code,
            &status_size,
            WINHTTP_NO_HEADER_INDEX)) {
        goto cleanup;
    }
    for (;;) {
        DWORD available = 0U;
        DWORD read_size = 0U;

        if (!WinHttpQueryDataAvailable(request, &available)) {
            goto cleanup;
        }
        if (available == 0U) {
            break;
        }
        if (response_size >= response_capacity ||
            (size_t)available > response_capacity - response_size - 1U) {
            result = LOIM_ERROR_OVERFLOW;
            goto cleanup;
        }
        if (!WinHttpReadData(
                request,
                response + response_size,
                available,
                &read_size) ||
            read_size == 0U) {
            goto cleanup;
        }
        response_size += (size_t)read_size;
        response[response_size] = '\0';
    }
    if (status_code > (DWORD)INT_MAX) {
        result = LOIM_ERROR_OVERFLOW;
        goto cleanup;
    }
    *out_http_status = (int)status_code;
    result = LOIM_OK;

cleanup:
    if (request != NULL) {
        (void)WinHttpCloseHandle(request);
    }
    if (connection != NULL) {
        (void)WinHttpCloseHandle(connection);
    }
    if (session != NULL) {
        (void)WinHttpCloseHandle(session);
    }
    return result;
}

loim_status loim_http_post_json(
    void *context,
    const char *url,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status)
{
    return post_json_impl(
        context,
        url,
        NULL,
        json_body,
        response,
        response_capacity,
        out_http_status);
}

loim_status loim_http_post_json_authorized(
    void *context,
    const char *url,
    const char *bearer_token,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status)
{
    return post_json_impl(
        context,
        url,
        bearer_token,
        json_body,
        response,
        response_capacity,
        out_http_status);
}
