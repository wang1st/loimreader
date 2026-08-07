#include "loim/auth.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct json_writer {
    char *output;
    size_t capacity;
    size_t length;
    bool failed;
} json_writer;

static void writer_append_bytes(json_writer *writer, const char *bytes, size_t length)
{
    if (writer->failed) {
        return;
    }
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

static void writer_append_hex_escape(json_writer *writer, unsigned char value)
{
    static const char digits[] = "0123456789abcdef";
    char escape[6] = {'\\', 'u', '0', '0', '0', '0'};

    escape[4] = digits[value >> 4U];
    escape[5] = digits[value & 0x0FU];
    writer_append_bytes(writer, escape, sizeof(escape));
}

static void writer_append_json_string(json_writer *writer, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    writer_append(writer, "\"");
    while (*cursor != 0U && !writer->failed) {
        switch (*cursor) {
        case '"':
            writer_append(writer, "\\\"");
            break;
        case '\\':
            writer_append(writer, "\\\\");
            break;
        case '\b':
            writer_append(writer, "\\b");
            break;
        case '\f':
            writer_append(writer, "\\f");
            break;
        case '\n':
            writer_append(writer, "\\n");
            break;
        case '\r':
            writer_append(writer, "\\r");
            break;
        case '\t':
            writer_append(writer, "\\t");
            break;
        default:
            if (*cursor < 0x20U) {
                writer_append_hex_escape(writer, *cursor);
            } else {
                writer_append_bytes(writer, (const char *)cursor, 1U);
            }
            break;
        }
        cursor += 1;
    }
    writer_append(writer, "\"");
}

static bool login_request_valid(const loim_login_request *request)
{
    return request != NULL && request->email != NULL && request->email[0] != '\0' &&
        request->password != NULL && request->password[0] != '\0' &&
        request->machine_code != NULL && strlen(request->machine_code) >= 8U &&
        request->platform != NULL && request->platform[0] != '\0' &&
        request->architecture != NULL && request->architecture[0] != '\0' &&
        request->os_name != NULL && request->os_name[0] != '\0' &&
        request->app_version != NULL && request->app_version[0] != '\0';
}

static void writer_append_property(
    json_writer *writer,
    const char *name,
    const char *value,
    bool comma)
{
    if (comma) {
        writer_append(writer, ",");
    }
    writer_append_json_string(writer, name);
    writer_append(writer, ":");
    writer_append_json_string(writer, value);
}

loim_status loim_auth_build_login_json(
    const loim_login_request *request,
    char *output,
    size_t output_capacity,
    size_t *out_length)
{
    json_writer writer;
    char user_agent[256];
    int user_agent_length;

    if (!login_request_valid(request) || output == NULL || output_capacity == 0U ||
        out_length == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    user_agent_length = snprintf(
        user_agent,
        sizeof(user_agent),
        "LoimReader/%s (%s)",
        request->app_version,
        request->os_name);
    if (user_agent_length < 0 || (size_t)user_agent_length >= sizeof(user_agent)) {
        return LOIM_ERROR_OVERFLOW;
    }
    writer.output = output;
    writer.capacity = output_capacity;
    writer.length = 0U;
    writer.failed = false;
    output[0] = '\0';
    writer_append(&writer, "{");
    writer_append_property(&writer, "email", request->email, false);
    writer_append_property(&writer, "password", request->password, true);
    writer_append_property(&writer, "machineCode", request->machine_code, true);
    writer_append(&writer, ",\"deviceInfo\":{");
    writer_append_property(&writer, "name", "LoimReader", false);
    writer_append_property(&writer, "type", "client", true);
    writer_append_property(&writer, "userAgent", user_agent, true);
    writer_append_property(&writer, "os", request->os_name, true);
    writer_append_property(&writer, "architecture", request->architecture, true);
    writer_append_property(&writer, "appVersion", request->app_version, true);
    writer_append(&writer, "}");
    writer_append_property(&writer, "clientName", "LoimReader", true);
    writer_append_property(&writer, "currentVersion", request->app_version, true);
    writer_append_property(&writer, "platform", request->platform, true);
    writer_append_property(&writer, "architecture", request->architecture, true);
    writer_append(&writer, "}");
    if (writer.failed) {
        output[0] = '\0';
        *out_length = 0U;
        return LOIM_ERROR_OVERFLOW;
    }
    *out_length = writer.length;
    return LOIM_OK;
}

static const char *skip_space(const char *cursor)
{
    while (*cursor != '\0' && isspace((unsigned char)*cursor) != 0) {
        cursor += 1;
    }
    return cursor;
}

static const char *skip_json_string(const char *cursor)
{
    if (*cursor != '"') {
        return NULL;
    }
    cursor += 1;
    while (*cursor != '\0') {
        if (*cursor == '\\') {
            if (cursor[1] == '\0') {
                return NULL;
            }
            cursor += 2;
        } else if (*cursor == '"') {
            return cursor + 1;
        } else {
            cursor += 1;
        }
    }
    return NULL;
}

static const char *find_json_value(const char *json, const char *key)
{
    const char *cursor = json;
    size_t key_length = strlen(key);

    while (*cursor != '\0') {
        const char *end;
        const char *after;

        if (*cursor != '"') {
            cursor += 1;
            continue;
        }
        end = skip_json_string(cursor);
        if (end == NULL) {
            return NULL;
        }
        if ((size_t)(end - cursor) == key_length + 2U &&
            memcmp(cursor + 1, key, key_length) == 0) {
            after = skip_space(end);
            if (*after == ':') {
                return skip_space(after + 1);
            }
        }
        cursor = end;
    }
    return NULL;
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool append_utf8(char *output, size_t capacity, size_t *length, uint32_t codepoint)
{
    unsigned char encoded[3];
    size_t count;

    if (codepoint <= 0x7FU) {
        encoded[0] = (unsigned char)codepoint;
        count = 1U;
    } else if (codepoint <= 0x7FFU) {
        encoded[0] = (unsigned char)(0xC0U | (codepoint >> 6U));
        encoded[1] = (unsigned char)(0x80U | (codepoint & 0x3FU));
        count = 2U;
    } else {
        encoded[0] = (unsigned char)(0xE0U | (codepoint >> 12U));
        encoded[1] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        encoded[2] = (unsigned char)(0x80U | (codepoint & 0x3FU));
        count = 3U;
    }
    if (*length >= capacity || count >= capacity ||
        *length > capacity - 1U - count) {
        return false;
    }
    memcpy(output + *length, encoded, count);
    *length += count;
    return true;
}

static bool extract_json_string(const char *value, char *output, size_t capacity)
{
    const char *cursor = value;
    size_t length = 0U;

    if (capacity == 0U || *cursor != '"') {
        return false;
    }
    cursor += 1;
    while (*cursor != '\0' && *cursor != '"') {
        uint32_t decoded;

        if (*cursor != '\\') {
            if (length >= capacity - 1U) {
                return false;
            }
            output[length] = *cursor;
            length += 1U;
            cursor += 1;
            continue;
        }
        cursor += 1;
        switch (*cursor) {
            case '"': decoded = '"'; break;
            case '\\': decoded = '\\'; break;
            case '/': decoded = '/'; break;
            case 'b': decoded = '\b'; break;
            case 'f': decoded = '\f'; break;
            case 'n': decoded = '\n'; break;
            case 'r': decoded = '\r'; break;
            case 't': decoded = '\t'; break;
            case 'u':
            {
                int first = hex_value(cursor[1]);
                int second = hex_value(cursor[2]);
                int third = hex_value(cursor[3]);
                int fourth = hex_value(cursor[4]);

                if (first < 0 || second < 0 || third < 0 || fourth < 0) {
                    return false;
                }
                decoded = ((uint32_t)first << 12U) | ((uint32_t)second << 8U) |
                    ((uint32_t)third << 4U) | (uint32_t)fourth;
                cursor += 4;
                break;
            }
            default:
                return false;
        }
        if (!append_utf8(output, capacity, &length, decoded)) {
            return false;
        }
        cursor += 1;
    }
    if (*cursor != '"') {
        return false;
    }
    output[length] = '\0';
    return true;
}

static bool extract_optional_string(
    const char *json,
    const char *key,
    char *output,
    size_t capacity)
{
    const char *value = find_json_value(json, key);

    output[0] = '\0';
    return value == NULL || extract_json_string(value, output, capacity);
}

static bool extract_bool(const char *json, const char *key, bool *output)
{
    const char *value = find_json_value(json, key);

    if (value == NULL) {
        return false;
    }
    if (strncmp(value, "true", 4U) == 0) {
        *output = true;
        return true;
    }
    if (strncmp(value, "false", 5U) == 0) {
        *output = false;
        return true;
    }
    return false;
}

loim_status loim_auth_parse_login_response(
    const char *json,
    loim_login_result *out_result)
{
    const char *object;
    const char *end;
    const char *subscription;

    if (json == NULL || out_result == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    object = skip_space(json);
    end = object + strlen(object);
    while (end > object && isspace((unsigned char)end[-1]) != 0) {
        end -= 1;
    }
    if (end <= object || *object != '{' || end[-1] != '}') {
        return LOIM_ERROR_INVALID_DATA;
    }
    if (!extract_bool(json, "success", &out_result->success) ||
        !extract_optional_string(json, "token", out_result->token, sizeof(out_result->token)) ||
        !extract_optional_string(json, "email", out_result->email, sizeof(out_result->email)) ||
        !extract_optional_string(json, "error", out_result->error_code, sizeof(out_result->error_code)) ||
        !extract_optional_string(json, "message", out_result->message, sizeof(out_result->message))) {
        return LOIM_ERROR_INVALID_DATA;
    }
    subscription = find_json_value(json, "subscription");
    if (subscription != NULL && !extract_optional_string(
            subscription,
            "type",
            out_result->subscription_type,
            sizeof(out_result->subscription_type))) {
        return LOIM_ERROR_INVALID_DATA;
    }
    if (find_json_value(json, "deviceLimitExceeded") != NULL &&
        !extract_bool(json, "deviceLimitExceeded", &out_result->device_limit_exceeded)) {
        return LOIM_ERROR_INVALID_DATA;
    }
    if (out_result->success && out_result->token[0] == '\0') {
        return LOIM_ERROR_INVALID_DATA;
    }
    return LOIM_OK;
}

loim_status loim_auth_login(
    const loim_login_request *request,
    loim_auth_post_fn post,
    void *post_context,
    loim_login_result *out_result,
    int *out_http_status)
{
    char request_json[8192];
    char response_json[LOIM_AUTH_RESPONSE_CAPACITY];
    size_t request_length;
    loim_status status;

    if (post == NULL || out_result == NULL || out_http_status == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    status = loim_auth_build_login_json(
        request, request_json, sizeof(request_json), &request_length);
    if (status != LOIM_OK) {
        return status;
    }
    (void)request_length;
    response_json[0] = '\0';
    status = post(
        post_context,
        LOIM_AUTH_LOGIN_URL,
        request_json,
        response_json,
        sizeof(response_json),
        out_http_status);
    if (status != LOIM_OK) {
        return status;
    }
    return loim_auth_parse_login_response(response_json, out_result);
}
