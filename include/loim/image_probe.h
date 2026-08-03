#ifndef LOIM_IMAGE_PROBE_H
#define LOIM_IMAGE_PROBE_H

#include <stdint.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum loim_image_format {
    LOIM_IMAGE_FORMAT_UNKNOWN = 0,
    LOIM_IMAGE_FORMAT_PNG,
    LOIM_IMAGE_FORMAT_JPEG,
    LOIM_IMAGE_FORMAT_GIF,
    LOIM_IMAGE_FORMAT_BMP
} loim_image_format;

typedef struct loim_image_info {
    loim_image_format format;
    uint32_t width_px;
    uint32_t height_px;
} loim_image_info;

loim_status loim_image_probe_file(const char *path, loim_image_info *out_info);
const char *loim_image_format_name(loim_image_format format);

#ifdef __cplusplus
}
#endif

#endif
