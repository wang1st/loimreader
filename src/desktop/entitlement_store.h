#ifndef LOIM_ENTITLEMENT_STORE_H
#define LOIM_ENTITLEMENT_STORE_H
#include "loim/entitlement.h"
bool loim_entitlement_nonce(char output[LOIM_ENTITLEMENT_NONCE_CAPACITY]);
bool loim_entitlement_cache_save(const char *payload, const char *signature);
bool loim_entitlement_cache_load(const char *device, const char *account,
    uint64_t now, loim_entitlement *out);
void loim_entitlement_cache_clear(void);
#endif
