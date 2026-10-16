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
    if (request->entitlement_nonce != NULL) {
        writer_append(&writer, ",\"entitlementVersion\":1");
        writer_append_property(&writer, "entitlementKeyId", loim_entitlement_key_id(), true);
        writer_append_property(&writer, "entitlementNonce", request->entitlement_nonce, true);
    }
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

/* Validate the response before extracting scoped members. Unknown fields remain
 * forward compatible, but malformed JSON and duplicate object keys are rejected. */
static const char *skip_json_string(const char *cursor)
{
    if (*cursor++ != '"') return NULL;
    while (*cursor != '\0') {
        if ((unsigned char)*cursor < 0x20U) return NULL;
        if (*cursor == '"') return cursor + 1;
        if (*cursor == '\\') {
            cursor += 1;
            if (*cursor == '\0') return NULL;
            if (*cursor == 'u') {
                unsigned index;
                for (index = 0U; index < 4U; ++index) {
                    cursor += 1;
                    if (!isxdigit((unsigned char)*cursor)) return NULL;
                }
            } else if (strchr("\"\\/bfnrt", *cursor) == NULL) {
                return NULL;
            }
        }
        cursor += 1;
    }
    return NULL;
}

static bool extract_json_string(const char *value, char *output, size_t capacity);

/* Earlier members have already been validated. Skip them without recursively
 * validating again, so nested objects cannot multiply the amount of work. */
static const char *skip_validated_json_value(const char *cursor)
{
    unsigned nesting = 0U;
    cursor = skip_space(cursor);
    if (*cursor == '\"') return skip_json_string(cursor);
    if (*cursor != '{' && *cursor != '[') {
        while (*cursor != '\0' && *cursor != ',' && *cursor != '}' &&
            *cursor != ']' && !isspace((unsigned char)*cursor)) cursor += 1;
        return cursor;
    }
    do {
        if (*cursor == '\"') {
            cursor = skip_json_string(cursor);
            if (cursor == NULL) return NULL;
            continue;
        }
        if (*cursor == '{' || *cursor == '[') nesting += 1U;
        if (*cursor == '}' || *cursor == ']') nesting -= 1U;
        cursor += 1;
    } while (nesting != 0U && *cursor != '\0');
    return nesting == 0U ? cursor : NULL;
}

static const char *skip_json_value(const char *cursor, unsigned depth)
{
    cursor = skip_space(cursor);
    if (depth > 32U) return NULL;
    if (*cursor == '"') return skip_json_string(cursor);
    if (*cursor == '{' || *cursor == '[') {
        bool object = *cursor == '{';
        char close = object ? '}' : ']';
        const char *members = skip_space(cursor + 1);
        cursor = members;
        if (*cursor == close) return cursor + 1;
        for (;;) {
            if (object) {
                const char *key = cursor;
                const char *end = skip_json_string(key);
                const char *previous = members;
                char key_name[256];
                if (end == NULL || !extract_json_string(key, key_name, sizeof(key_name))) return NULL;
                while (previous < key) {
                    const char *previous_end = skip_json_string(previous);
                    if (previous_end == NULL) return NULL;
                    char previous_name[256];
                    if (!extract_json_string(previous, previous_name, sizeof(previous_name)) ||
                        strcmp(key_name, previous_name) == 0) return NULL;
                    previous = skip_space(previous_end);
                    if (*previous != ':') return NULL;
                    previous = skip_validated_json_value(previous + 1);
                    if (previous == NULL) return NULL;
                    previous = skip_space(previous);
                    if (*previous != ',') return NULL;
                    previous = skip_space(previous + 1);
                }
                cursor = skip_space(end);
                if (*cursor != ':') return NULL;
                cursor += 1;
            }
            cursor = skip_json_value(cursor, depth + 1U);
            if (cursor == NULL) return NULL;
            cursor = skip_space(cursor);
            if (*cursor == close) return cursor + 1;
            if (*cursor != ',') return NULL;
            cursor = skip_space(cursor + 1);
        }
    }
    if (strncmp(cursor, "true", 4U) == 0) return cursor + 4;
    if (strncmp(cursor, "false", 5U) == 0) return cursor + 5;
    if (strncmp(cursor, "null", 4U) == 0) return cursor + 4;
    if (*cursor == '-') cursor += 1;
    if (*cursor == '0') {
        cursor += 1;
    } else {
        if (*cursor < '1' || *cursor > '9') return NULL;
        do { cursor += 1; } while (isdigit((unsigned char)*cursor));
    }
    if (*cursor == '.') {
        cursor += 1;
        if (!isdigit((unsigned char)*cursor)) return NULL;
        do { cursor += 1; } while (isdigit((unsigned char)*cursor));
    }
    if (*cursor == 'e' || *cursor == 'E') {
        cursor += 1;
        if (*cursor == '+' || *cursor == '-') cursor += 1;
        if (!isdigit((unsigned char)*cursor)) return NULL;
        do { cursor += 1; } while (isdigit((unsigned char)*cursor));
    }
    return cursor;
}

static const char *find_json_value(const char *json, const char *key)
{
    const char *cursor = skip_space(json);
    if (*cursor != '{') return NULL;
    cursor = skip_space(cursor + 1);
    while (*cursor == '"') {
        const char *end = skip_json_string(cursor);
        const char *value;
        if (end == NULL) return NULL;
        value = skip_space(end);
        if (*value != ':') return NULL;
        value = skip_space(value + 1);
        {
            char name[256];
            if (!extract_json_string(cursor, name, sizeof(name))) return NULL;
            if (strcmp(name, key) == 0) return value;
        }
        cursor = skip_validated_json_value(value);
        if (cursor == NULL) return NULL;
        cursor = skip_space(cursor);
        if (*cursor != ',') return NULL;
        cursor = skip_space(cursor + 1);
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
        if (decoded == 0U || !append_utf8(output, capacity, &length, decoded)) {
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

static bool ascii_equals_ignore_case(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }
        left += 1;
        right += 1;
    }
    return *left == '\0' && *right == '\0';
}

static bool legacy_paid_subscription(const char *type)
{
    static const char *const types[] = {
        "monthly", "yearly", "subscription", "team", "enterprise"
    };
    size_t index;
    for (index = 0U; index < sizeof(types) / sizeof(types[0]); ++index) {
        if (ascii_equals_ignore_case(type, types[index])) return true;
    }
    return false;
}

bool loim_auth_result_is_licensed(const loim_login_result *result)
{
    if (result == NULL || !result->success) {
        return false;
    }
    if (result->subscription_active_present) {
        return result->subscription_active;
    }
    return legacy_paid_subscription(result->subscription_type);
}

static loim_status parse_auth_response(
    const char *json,
    loim_login_result *out_result,
    bool require_token)
{
    const char *object;
    const char *end;
    const char *subscription;
    const char *subscription_active;
    const char *identity;
    const char *expires;

    if (json == NULL || out_result == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    object = skip_space(json);
    end = skip_json_value(object, 0U);
    if (*object != '{' || end == NULL || *skip_space(end) != '\0') {
        return LOIM_ERROR_INVALID_DATA;
    }
    identity = find_json_value(json, "user");
    if (identity == NULL) identity = json;
    if (*identity != '{') return LOIM_ERROR_INVALID_DATA;
    if (!extract_bool(json, "success", &out_result->success) ||
        !extract_optional_string(json, "token", out_result->token, sizeof(out_result->token)) ||
        !extract_optional_string(identity, "email", out_result->email, sizeof(out_result->email)) ||
        !extract_optional_string(json, "error", out_result->error_code, sizeof(out_result->error_code)) ||
        !extract_optional_string(json, "message", out_result->message, sizeof(out_result->message)) ||
        !extract_optional_string(json, "entitlementPayload", out_result->entitlement_payload,
            sizeof(out_result->entitlement_payload)) ||
        !extract_optional_string(json, "entitlementSignature", out_result->entitlement_signature,
            sizeof(out_result->entitlement_signature))) {
        return LOIM_ERROR_INVALID_DATA;
    }
    subscription = find_json_value(identity, "subscription");
    if (subscription != NULL) {
        if (*subscription != '{') return LOIM_ERROR_INVALID_DATA;
        if (!extract_optional_string(subscription, "type",
                out_result->subscription_type, sizeof(out_result->subscription_type))) {
            return LOIM_ERROR_INVALID_DATA;
        }
        expires = find_json_value(subscription, "expiresAt");
        if (expires != NULL && strncmp(expires, "null", 4U) != 0 &&
            !extract_optional_string(subscription, "expiresAt",
                out_result->subscription_expires_at,
                sizeof(out_result->subscription_expires_at))) {
            return LOIM_ERROR_INVALID_DATA;
        }
        subscription_active = find_json_value(subscription, "active");
        if (subscription_active != NULL) {
            out_result->subscription_active_present = true;
            if (!extract_bool(subscription, "active", &out_result->subscription_active)) {
                return LOIM_ERROR_INVALID_DATA;
            }
        }
        if (out_result->success && !out_result->subscription_active_present &&
            !ascii_equals_ignore_case(out_result->subscription_type, "free") &&
            !legacy_paid_subscription(out_result->subscription_type)) {
            return LOIM_ERROR_INVALID_DATA;
        }
    }
    if (find_json_value(json, "deviceLimitExceeded") != NULL &&
        !extract_bool(json, "deviceLimitExceeded", &out_result->device_limit_exceeded)) {
        return LOIM_ERROR_INVALID_DATA;
    }
    if (require_token && out_result->success && out_result->token[0] == '\0') {
        return LOIM_ERROR_INVALID_DATA;
    }
    return LOIM_OK;
}

loim_status loim_auth_parse_login_response(
    const char *json,
    loim_login_result *out_result)
{
    return parse_auth_response(json, out_result, true);
}

static bool session_request_valid(const loim_login_request *request)
{
    return request != NULL && request->machine_code != NULL &&
        strlen(request->machine_code) >= 8U && request->platform != NULL &&
        request->platform[0] != '\0' && request->architecture != NULL &&
        request->architecture[0] != '\0' && request->app_version != NULL &&
        request->app_version[0] != '\0';
}

static loim_status build_session_json(
    const loim_login_request *request,
    char *output,
    size_t output_capacity)
{
    json_writer writer;

    if (!session_request_valid(request) || output == NULL || output_capacity == 0U) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    writer.output = output;
    writer.capacity = output_capacity;
    writer.length = 0U;
    writer.failed = false;
    output[0] = '\0';
    writer_append(&writer, "{");
    writer_append_property(&writer, "machineCode", request->machine_code, false);
    writer_append_property(&writer, "currentVersion", request->app_version, true);
    writer_append_property(&writer, "platform", request->platform, true);
    writer_append_property(&writer, "architecture", request->architecture, true);
    writer_append_property(&writer, "clientName", "LoimReader", true);
    if (request->entitlement_nonce != NULL) {
        writer_append(&writer, ",\"entitlementVersion\":1");
        writer_append_property(&writer, "entitlementKeyId", loim_entitlement_key_id(), true);
        writer_append_property(&writer, "entitlementNonce", request->entitlement_nonce, true);
    }
    writer_append(&writer, "}");
    return writer.failed ? LOIM_ERROR_OVERFLOW : LOIM_OK;
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
    status = loim_auth_parse_login_response(response_json, out_result);
    if (status == LOIM_OK && out_result->success &&
        (*out_http_status < 200 || *out_http_status >= 300)) {
        return LOIM_ERROR_INVALID_DATA;
    }
    return status;
}

loim_status loim_auth_restore_session(
    const loim_login_request *request,
    const char *bearer_token,
    loim_auth_bearer_post_fn post,
    void *post_context,
    loim_login_result *out_result,
    int *out_http_status)
{
    char request_json[2048];
    char response_json[LOIM_AUTH_RESPONSE_CAPACITY];
    loim_status status;

    if (bearer_token == NULL || bearer_token[0] == '\0' || post == NULL ||
        out_result == NULL || out_http_status == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    status = build_session_json(request, request_json, sizeof(request_json));
    if (status != LOIM_OK) {
        return status;
    }
    response_json[0] = '\0';
    status = post(
        post_context,
        LOIM_AUTH_SESSION_URL,
        bearer_token,
        request_json,
        response_json,
        sizeof(response_json),
        out_http_status);
    if (status != LOIM_OK) {
        return status;
    }
    status = parse_auth_response(response_json, out_result, false);
    if (status != LOIM_OK) {
        return status;
    }
    if (out_result->success &&
        (*out_http_status < 200 || *out_http_status >= 300 ||
         out_result->email[0] == '\0' ||
         (!out_result->subscription_active_present &&
          out_result->subscription_type[0] == '\0'))) {
        return LOIM_ERROR_INVALID_DATA;
    }
    return LOIM_OK;
}
