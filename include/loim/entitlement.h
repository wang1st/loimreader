#ifndef LOIM_ENTITLEMENT_H
#define LOIM_ENTITLEMENT_H
#include <stdbool.h>
#include <stdint.h>
#include "loim/status.h"
#define LOIM_ENTITLEMENT_PAYLOAD_CAPACITY 2048U
#define LOIM_ENTITLEMENT_SIGNATURE_CAPACITY 129U
#define LOIM_ENTITLEMENT_NONCE_CAPACITY 65U
#define LOIM_ENTITLEMENT_MAX_SECONDS 604800ULL

typedef struct loim_entitlement {
    char account[256];
    char device[128];
    uint64_t issued_at;
    uint64_t expires_at;
    bool licensed;
} loim_entitlement;

const char *loim_entitlement_key_id(void);

/* A live response requires the request nonce; an offline cache instead checks
 * wall-clock expiry. Neither verification path trusts response-supplied keys. */
loim_status loim_entitlement_verify(
    const char *payload, const char *signature_hex,
    const char *device, const char *account, const char *request_nonce,
    uint64_t now, loim_entitlement *out);
#endif
