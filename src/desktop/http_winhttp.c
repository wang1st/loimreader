#include "http_client.h"

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

loim_status loim_http_post_json(
    void *context,
    const char *url,
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
    loim_status result = LOIM_ERROR_IO;

    (void)context;
    if (url == NULL || strcmp(url, LOIM_AUTH_LOGIN_URL) != 0 ||
        json_body == NULL || response == NULL || response_capacity == 0U ||
        out_http_status == NULL) {
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
        L"/api/auth/client/login",
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
