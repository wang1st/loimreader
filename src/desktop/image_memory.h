#ifndef LOIM_IMAGE_MEMORY_H
#define LOIM_IMAGE_MEMORY_H

#include <stdint.h>

/* Zero means the OS could not report available memory. */
uint64_t loim_available_memory_bytes(void);
uint64_t loim_image_pixel_budget(uint64_t total_bytes, uint64_t available_bytes);

#endif
