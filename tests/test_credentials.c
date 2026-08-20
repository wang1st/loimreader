#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "loim/credentials.h"

static int failures = 0;

#define CHECK(expression)                                                        \
    do {                                                                         \
        if (!(expression)) {                                                     \
            (void)fprintf(                                                       \
                stderr, "%s:%d: check failed: %s\n",                           \
                __FILE__, __LINE__, #expression);                                \
            failures += 1;                                                       \
        }                                                                        \
    } while (0)

int main(void)
{
    loim_credentials input;
    loim_credentials decoded;
    uint8_t encoded[LOIM_CREDENTIALS_ENCODED_CAPACITY];
    size_t encoded_length = 0U;

    memset(&input, 0, sizeof(input));
    memcpy(input.email, "subscriber@example.com", 23U);
    memcpy(input.password, "密码-pass-123", strlen("密码-pass-123"));
    memcpy(input.token, "header.payload.signature", 25U);
    CHECK(loim_credentials_can_prefill(&input));
    CHECK(loim_credentials_has_session(&input));
    CHECK(loim_credentials_encode(
        &input, encoded, sizeof(encoded), &encoded_length) == LOIM_OK);
    CHECK(encoded_length > 14U);
    CHECK(loim_credentials_decode(encoded, encoded_length, &decoded) == LOIM_OK);
    CHECK(strcmp(decoded.email, input.email) == 0);
    CHECK(strcmp(decoded.password, input.password) == 0);
    CHECK(strcmp(decoded.token, input.token) == 0);
    CHECK(loim_credentials_can_prefill(&decoded));
    CHECK(loim_credentials_has_session(&decoded));
    decoded.token[0] = '\0';
    CHECK(loim_credentials_can_prefill(&decoded));
    CHECK(!loim_credentials_has_session(&decoded));

    encoded[0] ^= 0xFFU;
    CHECK(loim_credentials_decode(encoded, encoded_length, &decoded) ==
          LOIM_ERROR_INVALID_DATA);
    loim_credentials_clear(&input);
    CHECK(input.email[0] == '\0');
    CHECK(input.password[0] == '\0');

    if (failures != 0) {
        (void)fprintf(stderr, "%d credential test check(s) failed\n", failures);
        return 1;
    }
    (void)puts("credential tests passed");
    return 0;
}
