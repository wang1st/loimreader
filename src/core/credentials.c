#include "loim/credentials.h"

#include <string.h>

static const uint8_t credential_magic[8] = {
    'L', 'O', 'I', 'M', 'C', 'R', 'D', '2'
};

static size_t bounded_length(const char *text, size_t capacity)
{
    size_t length = 0U;

    while (length < capacity && text[length] != '\0') {
        length += 1U;
    }
    return length;
}

bool loim_credentials_can_prefill(const loim_credentials *credentials)
{
    if (credentials == NULL) {
        return false;
    }
    return credentials->email[0] != '\0' &&
        strchr(credentials->email, '@') != NULL &&
        credentials->password[0] != '\0';
}

bool loim_credentials_has_session(const loim_credentials *credentials)
{
    return loim_credentials_can_prefill(credentials) &&
        credentials->token[0] != '\0';
}

loim_status loim_credentials_encode(
    const loim_credentials *credentials,
    uint8_t *output,
    size_t output_capacity,
    size_t *out_length)
{
    size_t email_length;
    size_t password_length;
    size_t token_length;
    size_t required;

    if (credentials == NULL || output == NULL || out_length == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    email_length = bounded_length(
        credentials->email, sizeof(credentials->email));
    password_length = bounded_length(
        credentials->password, sizeof(credentials->password));
    token_length = bounded_length(
        credentials->token, sizeof(credentials->token));
    if (email_length == sizeof(credentials->email) ||
        password_length == sizeof(credentials->password) ||
        token_length == sizeof(credentials->token) ||
        !loim_credentials_can_prefill(credentials)) {
        return LOIM_ERROR_INVALID_DATA;
    }
    required = sizeof(credential_magic) + 6U + email_length + password_length +
        token_length;
    if (required > output_capacity || email_length > UINT16_MAX ||
        password_length > UINT16_MAX || token_length > UINT16_MAX) {
        return LOIM_ERROR_OVERFLOW;
    }
    memcpy(output, credential_magic, sizeof(credential_magic));
    output[8] = (uint8_t)(email_length & 0xFFU);
    output[9] = (uint8_t)(email_length >> 8U);
    output[10] = (uint8_t)(password_length & 0xFFU);
    output[11] = (uint8_t)(password_length >> 8U);
    output[12] = (uint8_t)(token_length & 0xFFU);
    output[13] = (uint8_t)(token_length >> 8U);
    memcpy(output + 14U, credentials->email, email_length);
    memcpy(output + 14U + email_length, credentials->password, password_length);
    memcpy(
        output + 14U + email_length + password_length,
        credentials->token,
        token_length);
    *out_length = required;
    return LOIM_OK;
}

loim_status loim_credentials_decode(
    const uint8_t *encoded,
    size_t encoded_length,
    loim_credentials *out_credentials)
{
    size_t email_length;
    size_t password_length;
    size_t token_length;
    size_t expected;

    if (encoded == NULL || out_credentials == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    memset(out_credentials, 0, sizeof(*out_credentials));
    if (encoded_length < 14U ||
        memcmp(encoded, credential_magic, sizeof(credential_magic)) != 0) {
        return LOIM_ERROR_INVALID_DATA;
    }
    email_length = (size_t)encoded[8] | ((size_t)encoded[9] << 8U);
    password_length = (size_t)encoded[10] | ((size_t)encoded[11] << 8U);
    token_length = (size_t)encoded[12] | ((size_t)encoded[13] << 8U);
    expected = 14U + email_length + password_length + token_length;
    if (expected != encoded_length ||
        email_length >= sizeof(out_credentials->email) ||
        password_length >= sizeof(out_credentials->password) ||
        token_length >= sizeof(out_credentials->token)) {
        return LOIM_ERROR_INVALID_DATA;
    }
    memcpy(out_credentials->email, encoded + 14U, email_length);
    memcpy(
        out_credentials->password,
        encoded + 14U + email_length,
        password_length);
    memcpy(
        out_credentials->token,
        encoded + 14U + email_length + password_length,
        token_length);
    if (!loim_credentials_can_prefill(out_credentials)) {
        loim_credentials_clear(out_credentials);
        return LOIM_ERROR_INVALID_DATA;
    }
    return LOIM_OK;
}

void loim_credentials_clear(loim_credentials *credentials)
{
    volatile unsigned char *bytes;
    size_t index;

    if (credentials == NULL) {
        return;
    }
    bytes = (volatile unsigned char *)credentials;
    for (index = 0U; index < sizeof(*credentials); ++index) {
        bytes[index] = 0U;
    }
}
