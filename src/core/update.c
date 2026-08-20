#include "loim/update.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct semver {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    const char *prerelease;
    size_t prerelease_length;
} semver;

typedef struct json_writer {
    char *output;
    size_t capacity;
    size_t length;
    bool failed;
} json_writer;

static bool supported_platform(const char *platform)
{
    return platform != NULL &&
        (strcmp(platform, "windows") == 0 || strcmp(platform, "linux") == 0 ||
         strcmp(platform, "macos") == 0);
}

static bool supported_architecture(const char *architecture)
{
    return architecture != NULL &&
        (strcmp(architecture, "amd64") == 0 || strcmp(architecture, "arm64") == 0);
}

static bool ascii_alphanumeric(unsigned char value)
{
    return (value >= (unsigned char)'0' && value <= (unsigned char)'9') ||
        (value >= (unsigned char)'A' && value <= (unsigned char)'Z') ||
        (value >= (unsigned char)'a' && value <= (unsigned char)'z');
}

static bool update_request_valid(const loim_update_request *request)
{
    return request != NULL && request->current_version != NULL &&
        request->current_version[0] != '\0' && supported_platform(request->platform) &&
        supported_architecture(request->architecture);
}

static bool parse_core_number(const char **cursor, uint32_t *output)
{
    const char *start = *cursor;
    uint32_t value = 0U;

    if (!isdigit((unsigned char)*start) ||
        (*start == '0' && isdigit((unsigned char)start[1]))) {
        return false;
    }
    while (isdigit((unsigned char)**cursor)) {
        unsigned int digit = (unsigned int)(**cursor - '0');

        if (value > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
        *cursor += 1;
    }
    *output = value;
    return true;
}

static bool identifier_sequence_valid(const char *start, size_t length, bool prerelease)
{
    const char *cursor = start;
    const char *end = start + length;

    if (length == 0U) {
        return false;
    }
    while (cursor < end) {
        const char *identifier = cursor;
        bool numeric = true;

        while (cursor < end && *cursor != '.') {
            if (!ascii_alphanumeric((unsigned char)*cursor) && *cursor != '-') {
                return false;
            }
            if (!isdigit((unsigned char)*cursor)) {
                numeric = false;
            }
            cursor += 1;
        }
        if (cursor == identifier ||
            (prerelease && numeric && cursor - identifier > 1 && *identifier == '0')) {
            return false;
        }
        if (cursor < end) {
            cursor += 1;
            if (cursor == end) {
                return false;
            }
        }
    }
    return true;
}

static bool parse_semver(const char *text, semver *output)
{
    const char *cursor = text;
    const char *prerelease = NULL;
    const char *build = NULL;

    if (text == NULL || output == NULL ||
        !parse_core_number(&cursor, &output->major) || *cursor++ != '.' ||
        !parse_core_number(&cursor, &output->minor) || *cursor++ != '.' ||
        !parse_core_number(&cursor, &output->patch)) {
        return false;
    }
    output->prerelease = NULL;
    output->prerelease_length = 0U;
    if (*cursor == '-') {
        prerelease = cursor + 1;
        cursor = prerelease;
        while (*cursor != '\0' && *cursor != '+') {
            cursor += 1;
        }
        if (!identifier_sequence_valid(
                prerelease, (size_t)(cursor - prerelease), true)) {
            return false;
        }
        output->prerelease = prerelease;
        output->prerelease_length = (size_t)(cursor - prerelease);
    }
    if (*cursor == '+') {
        build = cursor + 1;
        cursor = build + strlen(build);
        if (!identifier_sequence_valid(build, (size_t)(cursor - build), false)) {
            return false;
        }
    }
    return *cursor == '\0';
}

static bool identifier_is_numeric(const char *text, size_t length)
{
    size_t index;

    for (index = 0U; index < length; ++index) {
        if (!isdigit((unsigned char)text[index])) {
            return false;
        }
    }
    return true;
}

static int compare_identifier(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length)
{
    bool left_numeric = identifier_is_numeric(left, left_length);
    bool right_numeric = identifier_is_numeric(right, right_length);
    int comparison;

    if (left_numeric != right_numeric) {
        return left_numeric ? -1 : 1;
    }
    if (left_numeric && left_length != right_length) {
        return left_length < right_length ? -1 : 1;
    }
    comparison = memcmp(left, right, left_length < right_length ? left_length : right_length);
    if (comparison != 0) {
        return comparison < 0 ? -1 : 1;
    }
    if (left_length == right_length) {
        return 0;
    }
    return left_length < right_length ? -1 : 1;
}

static int compare_prerelease(const semver *left, const semver *right)
{
    size_t left_offset = 0U;
    size_t right_offset = 0U;

    if (left->prerelease == NULL || right->prerelease == NULL) {
        if (left->prerelease == right->prerelease) {
            return 0;
        }
        return left->prerelease == NULL ? 1 : -1;
    }
    while (left_offset < left->prerelease_length &&
           right_offset < right->prerelease_length) {
        size_t left_end = left_offset;
        size_t right_end = right_offset;
        int comparison;

        while (left_end < left->prerelease_length &&
               left->prerelease[left_end] != '.') {
            left_end += 1U;
        }
        while (right_end < right->prerelease_length &&
               right->prerelease[right_end] != '.') {
            right_end += 1U;
        }
        comparison = compare_identifier(
            left->prerelease + left_offset,
            left_end - left_offset,
            right->prerelease + right_offset,
            right_end - right_offset);
        if (comparison != 0) {
            return comparison;
        }
        left_offset = left_end < left->prerelease_length ? left_end + 1U : left_end;
        right_offset = right_end < right->prerelease_length ? right_end + 1U : right_end;
    }
    if (left_offset == left->prerelease_length &&
        right_offset == right->prerelease_length) {
        return 0;
    }
    return left_offset == left->prerelease_length ? -1 : 1;
}

static int compare_semver(const semver *left, const semver *right)
{
    if (left->major != right->major) return left->major < right->major ? -1 : 1;
    if (left->minor != right->minor) return left->minor < right->minor ? -1 : 1;
    if (left->patch != right->patch) return left->patch < right->patch ? -1 : 1;
    return compare_prerelease(left, right);
}

bool loim_update_should_offer(
    const char *current_version,
    const char *latest_version)
{
    semver current;
    semver latest;

    if (!parse_semver(current_version, &current) ||
        !parse_semver(latest_version, &latest)) {
        return false;
    }
    if (current.prerelease == NULL && latest.prerelease != NULL) {
        return false;
    }
    return compare_semver(&latest, &current) > 0;
}

static void writer_append_bytes(json_writer *writer, const char *bytes, size_t length)
{
    if (writer->failed) return;
    if (writer->length >= writer->capacity || length >= writer->capacity ||
        writer->length > writer->capacity - 1U - length) {
        writer->failed = true;
        return;
    }
    memcpy(writer->output + writer->length, bytes, length);
    writer->length += length;
    writer->output[writer->length] = '\0';
}

static void writer_append(json_writer *writer, const char *text)
{
    writer_append_bytes(writer, text, strlen(text));
}

static void writer_append_json_string(json_writer *writer, const char *text)
{
    static const char digits[] = "0123456789abcdef";
    const unsigned char *cursor = (const unsigned char *)text;

    writer_append(writer, "\"");
    while (*cursor != 0U && !writer->failed) {
        char escape[6] = {'\\', 'u', '0', '0', '0', '0'};

        if (*cursor == '"' || *cursor == '\\') {
            char escaped[2] = {'\\', (char)*cursor};
            writer_append_bytes(writer, escaped, sizeof(escaped));
        } else if (*cursor < 0x20U) {
            escape[4] = digits[*cursor >> 4U];
            escape[5] = digits[*cursor & 0x0FU];
            writer_append_bytes(writer, escape, sizeof(escape));
        } else {
            writer_append_bytes(writer, (const char *)cursor, 1U);
        }
        cursor += 1;
    }
    writer_append(writer, "\"");
}

static void writer_property(
    json_writer *writer,
    const char *name,
    const char *value,
    bool comma)
{
    if (comma) writer_append(writer, ",");
    writer_append_json_string(writer, name);
    writer_append(writer, ":");
    writer_append_json_string(writer, value);
}

loim_status loim_update_build_check_json(
    const loim_update_request *request,
    char *output,
    size_t output_capacity,
    size_t *out_length)
{
    json_writer writer;

    if (!update_request_valid(request) || output == NULL || output_capacity == 0U ||
        out_length == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    writer.output = output;
    writer.capacity = output_capacity;
    writer.length = 0U;
    writer.failed = false;
    output[0] = '\0';
    writer_append(&writer, "{");
    writer_property(&writer, "clientName", "LoimReader", false);
    writer_property(&writer, "currentVersion", request->current_version, true);
    writer_property(&writer, "platform", request->platform, true);
    writer_property(&writer, "architecture", request->architecture, true);
    writer_append(&writer, "}");
    if (writer.failed) {
        output[0] = '\0';
        *out_length = 0U;
        return LOIM_ERROR_OVERFLOW;
    }
    *out_length = writer.length;
    return LOIM_OK;
}

static const char *skip_space(const char *cursor, const char *end)
{
    while (cursor < end && isspace((unsigned char)*cursor)) cursor += 1;
    return cursor;
}

static const char *skip_json_string(const char *cursor, const char *end)
{
    if (cursor >= end || *cursor != '"') return NULL;
    cursor += 1;
    while (cursor < end) {
        unsigned char value = (unsigned char)*cursor;

        if (value < 0x20U) return NULL;
        if (*cursor == '"') return cursor + 1;
        if (*cursor == '\\') {
            cursor += 1;
            if (cursor >= end || strchr("\"\\/bfnrtu", *cursor) == NULL) return NULL;
            if (*cursor == 'u') {
                size_t index;

                for (index = 0U; index < 4U; ++index) {
                    cursor += 1;
                    if (cursor >= end || !isxdigit((unsigned char)*cursor)) return NULL;
                }
            }
        }
        cursor += 1;
    }
    return NULL;
}

static const char *skip_json_value(const char *cursor, const char *end, unsigned int depth);

static const char *skip_json_number(const char *cursor, const char *end)
{
    if (cursor < end && *cursor == '-') cursor += 1;
    if (cursor >= end) return NULL;
    if (*cursor == '0') {
        cursor += 1;
        if (cursor < end && isdigit((unsigned char)*cursor)) return NULL;
    } else {
        if (*cursor < '1' || *cursor > '9') return NULL;
        while (cursor < end && isdigit((unsigned char)*cursor)) cursor += 1;
    }
    if (cursor < end && *cursor == '.') {
        cursor += 1;
        if (cursor >= end || !isdigit((unsigned char)*cursor)) return NULL;
        while (cursor < end && isdigit((unsigned char)*cursor)) cursor += 1;
    }
    if (cursor < end && (*cursor == 'e' || *cursor == 'E')) {
        cursor += 1;
        if (cursor < end && (*cursor == '+' || *cursor == '-')) cursor += 1;
        if (cursor >= end || !isdigit((unsigned char)*cursor)) return NULL;
        while (cursor < end && isdigit((unsigned char)*cursor)) cursor += 1;
    }
    return cursor;
}

static const char *skip_json_compound(
    const char *cursor,
    const char *end,
    unsigned int depth,
    char close,
    bool object)
{
    cursor = skip_space(cursor + 1, end);
    if (cursor < end && *cursor == close) return cursor + 1;
    while (cursor < end) {
        if (object) {
            cursor = skip_json_string(cursor, end);
            if (cursor == NULL) return NULL;
            cursor = skip_space(cursor, end);
            if (cursor >= end || *cursor != ':') return NULL;
            cursor = skip_space(cursor + 1, end);
        }
        cursor = skip_json_value(cursor, end, depth + 1U);
        if (cursor == NULL) return NULL;
        cursor = skip_space(cursor, end);
        if (cursor < end && *cursor == close) return cursor + 1;
        if (cursor >= end || *cursor != ',') return NULL;
        cursor = skip_space(cursor + 1, end);
    }
    return NULL;
}

static const char *skip_json_value(const char *cursor, const char *end, unsigned int depth)
{
    const char *start = cursor;

    if (depth > 64U || cursor >= end) return NULL;
    if (*cursor == '"') return skip_json_string(cursor, end);
    if (*cursor == '{') return skip_json_compound(cursor, end, depth, '}', true);
    if (*cursor == '[') return skip_json_compound(cursor, end, depth, ']', false);
    if (*cursor == '-' || isdigit((unsigned char)*cursor)) {
        return skip_json_number(cursor, end);
    }
    while (cursor < end && *cursor != ',' && *cursor != '}' && *cursor != ']' &&
           !isspace((unsigned char)*cursor)) {
        cursor += 1;
    }
    if (cursor == start) return NULL;
    if ((size_t)(cursor - start) == 4U && memcmp(start, "true", 4U) == 0) return cursor;
    if ((size_t)(cursor - start) == 5U && memcmp(start, "false", 5U) == 0) return cursor;
    if ((size_t)(cursor - start) == 4U && memcmp(start, "null", 4U) == 0) return cursor;
    return NULL;
}

static bool json_document_object(
    const char *json,
    const char **out_begin,
    const char **out_end)
{
    const char *input_end = json + strlen(json);
    const char *begin = skip_space(json, input_end);
    const char *end;

    if (begin >= input_end || *begin != '{') return false;
    end = skip_json_value(begin, input_end, 0U);
    if (end == NULL || skip_space(end, input_end) != input_end) return false;
    *out_begin = begin;
    *out_end = end;
    return true;
}

static bool json_find_member(
    const char *object,
    const char *object_end,
    const char *key,
    const char **out_value,
    const char **out_value_end)
{
    const char *cursor = skip_space(object + 1, object_end);
    size_t key_length = strlen(key);

    while (cursor < object_end && *cursor != '}') {
        const char *key_end = skip_json_string(cursor, object_end);
        const char *value;
        const char *value_end;

        if (key_end == NULL) return false;
        value = skip_space(key_end, object_end);
        if (value >= object_end || *value != ':') return false;
        value = skip_space(value + 1, object_end);
        value_end = skip_json_value(value, object_end, 0U);
        if (value_end == NULL) return false;
        if ((size_t)(key_end - cursor) == key_length + 2U &&
            memcmp(cursor + 1, key, key_length) == 0) {
            *out_value = value;
            *out_value_end = value_end;
            return true;
        }
        cursor = skip_space(value_end, object_end);
        if (cursor < object_end && *cursor == ',') {
            cursor = skip_space(cursor + 1, object_end);
        } else if (cursor >= object_end || *cursor != '}') {
            return false;
        }
    }
    return false;
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool append_utf8(char *output, size_t capacity, size_t *length, uint32_t codepoint)
{
    unsigned char encoded[4];
    size_t count;

    if (codepoint <= 0x7FU) {
        encoded[0] = (unsigned char)codepoint;
        count = 1U;
    } else if (codepoint <= 0x7FFU) {
        encoded[0] = (unsigned char)(0xC0U | (codepoint >> 6U));
        encoded[1] = (unsigned char)(0x80U | (codepoint & 0x3FU));
        count = 2U;
    } else if (codepoint <= 0xFFFFU) {
        encoded[0] = (unsigned char)(0xE0U | (codepoint >> 12U));
        encoded[1] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        encoded[2] = (unsigned char)(0x80U | (codepoint & 0x3FU));
        count = 3U;
    } else if (codepoint <= 0x10FFFFU) {
        encoded[0] = (unsigned char)(0xF0U | (codepoint >> 18U));
        encoded[1] = (unsigned char)(0x80U | ((codepoint >> 12U) & 0x3FU));
        encoded[2] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        encoded[3] = (unsigned char)(0x80U | (codepoint & 0x3FU));
        count = 4U;
    } else {
        return false;
    }
    if (*length >= capacity || count > capacity - *length - 1U) return false;
    memcpy(output + *length, encoded, count);
    *length += count;
    return true;
}

static bool parse_hex4(const char *text, uint32_t *output)
{
    int a = hex_value(text[0]);
    int b = hex_value(text[1]);
    int c = hex_value(text[2]);
    int d = hex_value(text[3]);

    if (a < 0 || b < 0 || c < 0 || d < 0) return false;
    *output = ((uint32_t)a << 12U) | ((uint32_t)b << 8U) |
        ((uint32_t)c << 4U) | (uint32_t)d;
    return true;
}

static bool json_extract_string(
    const char *value,
    const char *value_end,
    char *output,
    size_t capacity)
{
    const char *cursor = value + 1;
    size_t length = 0U;

    if (capacity == 0U || value >= value_end || *value != '"' ||
        value_end[-1] != '"') return false;
    while (cursor < value_end - 1) {
        uint32_t codepoint;

        if (*cursor != '\\') {
            if ((unsigned char)*cursor < 0x20U || length >= capacity - 1U) return false;
            output[length++] = *cursor++;
            continue;
        }
        cursor += 1;
        if (cursor >= value_end - 1) return false;
        switch (*cursor) {
        case '"': codepoint = '"'; break;
        case '\\': codepoint = '\\'; break;
        case '/': codepoint = '/'; break;
        case 'b': codepoint = '\b'; break;
        case 'f': codepoint = '\f'; break;
        case 'n': codepoint = '\n'; break;
        case 'r': codepoint = '\r'; break;
        case 't': codepoint = '\t'; break;
        case 'u':
            if (cursor + 4 >= value_end || !parse_hex4(cursor + 1, &codepoint)) return false;
            cursor += 4;
            if (codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
                uint32_t low;

                if (cursor + 6 >= value_end || cursor[1] != '\\' || cursor[2] != 'u' ||
                    !parse_hex4(cursor + 3, &low) || low < 0xDC00U || low > 0xDFFFU) {
                    return false;
                }
                codepoint = 0x10000U + ((codepoint - 0xD800U) << 10U) +
                    (low - 0xDC00U);
                cursor += 6;
            } else if (codepoint >= 0xDC00U && codepoint <= 0xDFFFU) {
                return false;
            }
            break;
        default:
            return false;
        }
        if (!append_utf8(output, capacity, &length, codepoint)) return false;
        cursor += 1;
    }
    output[length] = '\0';
    return true;
}

static bool json_extract_bool(
    const char *value,
    const char *value_end,
    bool *output)
{
    size_t length = (size_t)(value_end - value);

    if (length == 4U && memcmp(value, "true", 4U) == 0) {
        *output = true;
        return true;
    }
    if (length == 5U && memcmp(value, "false", 5U) == 0) {
        *output = false;
        return true;
    }
    return false;
}

static bool trusted_download_url(
    const loim_update_request *request,
    const char *latest_version,
    const char *url)
{
    static const char base[] = "https://ctdy123.com/download/loimreader/";
    const char *extension;
    char expected[LOIM_UPDATE_URL_CAPACITY];
    int expected_length;

    if (strcmp(request->platform, "windows") == 0) extension = ".zip";
    else if (strcmp(request->platform, "linux") == 0) extension = ".deb";
    else extension = ".dmg";
    expected_length = snprintf(
        expected,
        sizeof(expected),
        "%s%s/%s/LoimReader_%s_%s_%s%s",
        base,
        request->platform,
        request->architecture,
        latest_version,
        request->platform,
        request->architecture,
        extension);
    return expected_length >= 0 && (size_t)expected_length < sizeof(expected) &&
        strcmp(url, expected) == 0;
}

loim_status loim_update_parse_check_response(
    const loim_update_request *request,
    const char *json,
    loim_update_result *out_result)
{
    const char *object;
    const char *object_end;
    const char *value;
    const char *value_end;
    const char *info;
    const char *info_end;
    bool success;
    bool server_has_update;

    if (!update_request_valid(request) || json == NULL || out_result == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (!json_document_object(json, &object, &object_end) ||
        !json_find_member(object, object_end, "success", &value, &value_end) ||
        !json_extract_bool(value, value_end, &success) || !success ||
        !json_find_member(object, object_end, "hasUpdate", &value, &value_end) ||
        !json_extract_bool(value, value_end, &server_has_update)) {
        return LOIM_ERROR_INVALID_DATA;
    }
    if (!server_has_update) return LOIM_OK;
    if (!json_find_member(object, object_end, "latestVersion", &value, &value_end) ||
        !json_extract_string(
            value, value_end, out_result->latest_version,
            sizeof(out_result->latest_version))) {
        return LOIM_ERROR_INVALID_DATA;
    }
    if (!loim_update_should_offer(
            request->current_version, out_result->latest_version)) {
        return LOIM_OK;
    }
    if (json_find_member(object, object_end, "forceUpdate", &value, &value_end) &&
        !json_extract_bool(value, value_end, &out_result->force_update)) {
        return LOIM_ERROR_INVALID_DATA;
    }
    if (!json_find_member(object, object_end, "updateInfo", &info, &info_end) ||
        info >= info_end || *info != '{' ||
        !json_find_member(info, info_end, "downloadUrl", &value, &value_end) ||
        !json_extract_string(
            value, value_end, out_result->download_url,
            sizeof(out_result->download_url)) ||
        !trusted_download_url(
            request, out_result->latest_version, out_result->download_url)) {
        return LOIM_ERROR_INVALID_DATA;
    }
    if (json_find_member(info, info_end, "releaseNotes", &value, &value_end)) {
        if ((size_t)(value_end - value) != 4U || memcmp(value, "null", 4U) != 0) {
            if (!json_extract_string(
                    value, value_end, out_result->release_notes,
                    sizeof(out_result->release_notes))) {
                return LOIM_ERROR_INVALID_DATA;
            }
        }
    }
    out_result->has_update = true;
    return LOIM_OK;
}

loim_status loim_update_check(
    const loim_update_request *request,
    loim_update_post_fn post,
    void *post_context,
    loim_update_result *out_result,
    int *out_http_status)
{
    char request_json[1024];
    char response_json[LOIM_UPDATE_RESPONSE_CAPACITY];
    size_t request_length = 0U;
    loim_status status;

    if (!update_request_valid(request) || post == NULL || out_result == NULL ||
        out_http_status == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    *out_http_status = 0;
    status = loim_update_build_check_json(
        request, request_json, sizeof(request_json), &request_length);
    if (status != LOIM_OK) return status;
    (void)request_length;
    response_json[0] = '\0';
    status = post(
        post_context,
        LOIM_UPDATE_CHECK_URL,
        request_json,
        response_json,
        sizeof(response_json),
        out_http_status);
    if (status != LOIM_OK) return status;
    if (*out_http_status < 200 || *out_http_status >= 300) return LOIM_ERROR_IO;
    return loim_update_parse_check_response(request, response_json, out_result);
}
