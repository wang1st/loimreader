#ifndef LOIM_PDF_H
#define LOIM_PDF_H

#include <stddef.h>
#include <stdint.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct loim_pdf_rgb_page {
    uint32_t width_px;
    uint32_t height_px;
    uint32_t media_width_pt;
    uint32_t media_height_pt;
    const uint8_t *rgb;
} loim_pdf_rgb_page;

loim_status loim_pdf_write_rgb(
    const char *path,
    const loim_pdf_rgb_page *pages,
    size_t page_count);

#ifdef __cplusplus
}
#endif

#endif
