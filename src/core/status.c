#include "loim/status.h"

const char *loim_status_string(loim_status status)
{
    switch (status) {
    case LOIM_OK:
        return "ok";
    case LOIM_ERROR_INVALID_ARGUMENT:
        return "invalid argument";
    case LOIM_ERROR_OUT_OF_MEMORY:
        return "out of memory";
    case LOIM_ERROR_IO:
        return "I/O error";
    case LOIM_ERROR_UNSUPPORTED_FORMAT:
        return "unsupported image format";
    case LOIM_ERROR_CORRUPT_IMAGE:
        return "corrupt image";
    case LOIM_ERROR_OVERFLOW:
        return "integer overflow";
    case LOIM_ERROR_NOT_FOUND:
        return "not found";
    default:
        return "unknown error";
    }
}
