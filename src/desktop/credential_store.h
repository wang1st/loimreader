#ifndef LOIM_CREDENTIAL_STORE_H
#define LOIM_CREDENTIAL_STORE_H

#include <stdbool.h>

#include "loim/credentials.h"

bool loim_credential_store_load(loim_credentials *out_credentials);
bool loim_credential_store_save(const loim_credentials *credentials);
bool loim_credential_store_clear(void);

#endif
