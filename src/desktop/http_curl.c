#include "http_client.h"

#include "loim/update.h"

#include <curl/curl.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct response_writer {
    char *data;
    size_t capacity;
    size_t length;
    bool overflow;
} response_writer;

static size_t write_response(
    char *data,
    size_t element_size,
    size_t element_count,
    void *userdata)
{
    response_writer *writer = userdata;
    size_t byte_count;

    if (element_size != 0U && element_count > SIZE_MAX / element_size) {
        writer->overflow = true;
        return 0U;
    }
    byte_count = element_size * element_count;
    if (writer->length >= writer->capacity ||
        byte_count > writer->capacity - writer->length - 1U) {
        writer->overflow = true;
        return 0U;
    }
    memcpy(writer->data + writer->length, data, byte_count);
    writer->length += byte_count;
    writer->data[writer->length] = '\0';
    return byte_count;
}

bool loim_http_initialize(void)
{
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

void loim_http_cleanup(void)
{
    curl_global_cleanup();
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
    CURL *curl;
    struct curl_slist *headers = NULL;
    struct curl_slist *updated_headers;
    response_writer writer;
    CURLcode code;
    long http_status = 0L;
    char authorization[4200];

    (void)context;
    if (url == NULL ||
        (strcmp(url, LOIM_AUTH_LOGIN_URL) != 0 &&
         strcmp(url, LOIM_AUTH_SESSION_URL) != 0 &&
         strcmp(url, LOIM_UPDATE_CHECK_URL) != 0) ||
        json_body == NULL || response == NULL || response_capacity == 0U ||
        out_http_status == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    if ((strcmp(url, LOIM_AUTH_SESSION_URL) == 0) != (bearer_token != NULL) ||
        (bearer_token != NULL &&
         (bearer_token[0] == '\0' || strpbrk(bearer_token, "\r\n") != NULL))) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    curl = curl_easy_init();
    if (curl == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    if (headers == NULL) {
        curl_easy_cleanup(curl);
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    if (bearer_token != NULL) {
        int length = snprintf(
            authorization,
            sizeof(authorization),
            "Authorization: Bearer %s",
            bearer_token);

        if (length < 0 || (size_t)length >= sizeof(authorization)) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return LOIM_ERROR_OVERFLOW;
        }
        updated_headers = curl_slist_append(headers, authorization);
        if (updated_headers == NULL) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return LOIM_ERROR_OUT_OF_MEMORY;
        }
        headers = updated_headers;
    }
    writer.data = response;
    writer.capacity = response_capacity;
    writer.length = 0U;
    writer.overflow = false;
    response[0] = '\0';
    (void)curl_easy_setopt(curl, CURLOPT_URL, url);
    (void)curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    (void)curl_easy_setopt(curl, CURLOPT_POST, 1L);
    (void)curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    (void)curl_easy_setopt(
        curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)strlen(json_body));
    (void)curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    (void)curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writer);
    (void)curl_easy_setopt(curl, CURLOPT_USERAGENT, "LoimReader/3");
    (void)curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    (void)curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    (void)curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
#if LIBCURL_VERSION_NUM >= 0x075500
    /* CURLOPT_PROTOCOLS_STR / CURLOPT_REDIR_PROTOCOLS_STR landed in curl
     * 7.85.0; the glibc 2.31 build container ships curl 7.68.0. */
    (void)curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    (void)curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    (void)curl_easy_setopt(
        curl, CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTPS | CURLPROTO_HTTP));
    (void)curl_easy_setopt(
        curl, CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTPS | CURLPROTO_HTTP));
#endif
    (void)curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    code = curl_easy_perform(curl);
    if (code == CURLE_OK) {
        (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (writer.overflow) {
        return LOIM_ERROR_OVERFLOW;
    }
    if (code != CURLE_OK || http_status < 100L || http_status > INT_MAX) {
        return LOIM_ERROR_IO;
    }
    *out_http_status = (int)http_status;
    return LOIM_OK;
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
