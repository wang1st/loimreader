#ifndef LOIM_STATUS_H
#define LOIM_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum loim_status {
    LOIM_OK = 0,
    LOIM_ERROR_INVALID_ARGUMENT,
    LOIM_ERROR_OUT_OF_MEMORY,
    LOIM_ERROR_IO,
    LOIM_ERROR_UNSUPPORTED_FORMAT,
    LOIM_ERROR_CORRUPT_IMAGE,
    LOIM_ERROR_OVERFLOW,
    LOIM_ERROR_NOT_FOUND
} loim_status;

const char *loim_status_string(loim_status status);

#ifdef __cplusplus
}
#endif

#endif
