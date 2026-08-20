#ifndef LOIM_MACHINE_CODE_H
#define LOIM_MACHINE_CODE_H

#include <stddef.h>
#include <stdint.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOIM_MACHINE_CODE_CAPACITY 25U

loim_status loim_machine_code_from_bytes(
    const uint8_t *machine_id,
    size_t machine_id_size,
    char *output,
    size_t output_capacity);

#ifdef __cplusplus
}
#endif

#endif
