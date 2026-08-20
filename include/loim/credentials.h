#ifndef LOIM_CREDENTIALS_H
#define LOIM_CREDENTIALS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOIM_CREDENTIAL_EMAIL_CAPACITY 256U
#define LOIM_CREDENTIAL_PASSWORD_CAPACITY 256U
#define LOIM_CREDENTIAL_TOKEN_CAPACITY 1024U
#define LOIM_CREDENTIALS_ENCODED_CAPACITY 1548U

typedef struct loim_credentials {
    char email[LOIM_CREDENTIAL_EMAIL_CAPACITY];
    char password[LOIM_CREDENTIAL_PASSWORD_CAPACITY];
    char token[LOIM_CREDENTIAL_TOKEN_CAPACITY];
} loim_credentials;

bool loim_credentials_can_prefill(const loim_credentials *credentials);
bool loim_credentials_has_session(const loim_credentials *credentials);

loim_status loim_credentials_encode(
    const loim_credentials *credentials,
    uint8_t *output,
    size_t output_capacity,
    size_t *out_length);

loim_status loim_credentials_decode(
    const uint8_t *encoded,
    size_t encoded_length,
    loim_credentials *out_credentials);

void loim_credentials_clear(loim_credentials *credentials);

#ifdef __cplusplus
}
#endif

#endif
