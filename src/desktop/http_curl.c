#include "http_client.h"

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

loim_status loim_http_post_json(
    void *context,
    const char *url,
    const char *json_body,
    char *response,
    size_t response_capacity,
    int *out_http_status)
{
    CURL *curl;
    struct curl_slist *headers = NULL;
    response_writer writer;
    CURLcode code;
    long http_status = 0L;

    (void)context;
    if (url == NULL || strncmp(url, "https://", 8U) != 0 ||
        json_body == NULL || response == NULL || response_capacity == 0U ||
        out_http_status == NULL) {
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
    (void)curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    (void)curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
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
