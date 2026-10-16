#include "loim/entitlement.h"
#include "monocypher-ed25519.h"
#if defined(LOIM_ENTITLEMENT_TEST_VECTORS)
#include "../../tests/entitlement_vectors.h"
#else
#include "entitlement_key.h"
#endif
#include <stddef.h>
#include <string.h>

const char *loim_entitlement_key_id(void)
{
    return LOIM_ENTITLEMENT_KEY_ID;
}

static int hex_digit(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static bool decode_hex(const char *text, uint8_t *bytes, size_t count)
{
    size_t i;
    if (strlen(text) != count * 2U) return false;
    for (i = 0U; i < count; ++i) {
        int high = hex_digit(text[i * 2U]);
        int low = hex_digit(text[i * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        bytes[i] = (uint8_t)(high * 16 + low);
    }
    return true;
}

static bool read_line(const char **cursor, const char *prefix, char *value, size_t capacity)
{
    size_t prefix_size = strlen(prefix);
    const char *end;
    size_t size;
    if (strncmp(*cursor, prefix, prefix_size) != 0) return false;
    *cursor += prefix_size;
    end = strchr(*cursor, '\n');
    if (end == NULL) return false;
    size = (size_t)(end - *cursor);
    if (size == 0U || size >= capacity) return false;
    memcpy(value, *cursor, size);
    value[size] = '\0';
    *cursor = end + 1;
    return true;
}

static bool read_number(const char **cursor, const char *prefix, uint64_t *number)
{
    char value[24];
    size_t i;
    *number = 0U;
    if (!read_line(cursor, prefix, value, sizeof(value))) return false;
    if (value[0] == '0' && value[1] != '\0') return false;
    for (i = 0U; value[i] != '\0'; ++i) {
        unsigned digit;
        if (value[i] < '0' || value[i] > '9') return false;
        digit = (unsigned)(value[i] - '0');
        if (*number > (UINT64_MAX - digit) / 10U) return false;
        *number = *number * 10U + digit;
    }
    return true;
}

static void normalize_account(const char *source, char *output, size_t capacity)
{
    size_t length = strlen(source);
    size_t i;
    while (length > 0U && (source[length - 1U] == ' ' || source[length - 1U] == '\t' ||
        source[length - 1U] == '\r' || source[length - 1U] == '\n')) --length;
    while (length > 0U && (*source == ' ' || *source == '\t' || *source == '\r' || *source == '\n')) {
        ++source;
        --length;
    }
    if (length >= capacity) { output[0] = '\0'; return; }
    for (i = 0U; i < length; ++i) {
        char ch = source[i];
        output[i] = ch >= 'A' && ch <= 'Z' ? (char)(ch + ('a' - 'A')) : ch;
    }
    output[length] = '\0';
}

loim_status loim_entitlement_verify(
    const char *payload, const char *signature_hex,
    const char *device, const char *account, const char *request_nonce,
    uint64_t now, loim_entitlement *out)
{
    uint8_t signature[64];
    uint8_t nonce_bytes[32];
    char key[64];
    char account_hex[511];
    char nonce[65];
    char licensed[4];
    char normalized[256];
    const char *cursor = payload;
    loim_entitlement verified = {0};
    size_t account_bytes;
    size_t i;
    if (out == NULL) return LOIM_ERROR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (payload == NULL || signature_hex == NULL || device == NULL || account == NULL ||
        strlen(payload) >= LOIM_ENTITLEMENT_PAYLOAD_CAPACITY ||
        !decode_hex(signature_hex, signature, sizeof(signature))) return LOIM_ERROR_INVALID_DATA;
    if (crypto_ed25519_check(signature, loim_entitlement_public_key,
            (const uint8_t *)payload, strlen(payload)) != 0) return LOIM_ERROR_INVALID_DATA;
    if (strncmp(cursor, "LoimReader-License-1\n", 21U) != 0) return LOIM_ERROR_INVALID_DATA;
    cursor += 21U;
    if (!read_line(&cursor, "key=", key, sizeof(key)) || strcmp(key, LOIM_ENTITLEMENT_KEY_ID) != 0 ||
        !read_line(&cursor, "account=", account_hex, sizeof(account_hex)) ||
        !read_line(&cursor, "device=", verified.device, sizeof(verified.device)) ||
        !read_line(&cursor, "nonce=", nonce, sizeof(nonce)) ||
        !decode_hex(nonce, nonce_bytes, sizeof(nonce_bytes)) ||
        !read_number(&cursor, "issued=", &verified.issued_at) ||
        !read_number(&cursor, "expires=", &verified.expires_at) ||
        !read_line(&cursor, "licensed=", licensed, sizeof(licensed)) || *cursor != '\0' ||
        (strcmp(licensed, "0") != 0 && strcmp(licensed, "1") != 0)) return LOIM_ERROR_INVALID_DATA;
    account_bytes = strlen(account_hex) / 2U;
    if (account_bytes == 0U || account_bytes >= sizeof(verified.account) ||
        !decode_hex(account_hex, (uint8_t *)verified.account, account_bytes)) return LOIM_ERROR_INVALID_DATA;
    for (i = 0U; i < account_bytes; ++i) {
        if ((unsigned char)verified.account[i] < 0x20U) return LOIM_ERROR_INVALID_DATA;
    }
    normalize_account(account, normalized, sizeof(normalized));
    if (normalized[0] == '\0' || strcmp(verified.account, normalized) != 0 ||
        strcmp(verified.device, device) != 0 || verified.issued_at == 0U ||
        verified.expires_at <= verified.issued_at ||
        verified.expires_at - verified.issued_at > LOIM_ENTITLEMENT_MAX_SECONDS) return LOIM_ERROR_INVALID_DATA;
    if (request_nonce != NULL) {
        if (strcmp(nonce, request_nonce) != 0) return LOIM_ERROR_INVALID_DATA;
    } else if (now == 0U || now < verified.issued_at || now >= verified.expires_at) {
        return LOIM_ERROR_INVALID_DATA;
    }
    verified.licensed = licensed[0] == '1';
    *out = verified;
    return LOIM_OK;
}
