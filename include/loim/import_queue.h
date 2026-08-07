#ifndef LOIM_IMPORT_QUEUE_H
#define LOIM_IMPORT_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct loim_import_queue {
    char **paths;
    size_t count;
    size_t capacity;
    bool collecting;
} loim_import_queue;

void loim_import_queue_init(loim_import_queue *queue);
void loim_import_queue_destroy(loim_import_queue *queue);
loim_status loim_import_queue_begin(loim_import_queue *queue);
loim_status loim_import_queue_add(loim_import_queue *queue, const char *path);
loim_status loim_import_queue_complete(loim_import_queue *queue);
const char *loim_import_queue_path(const loim_import_queue *queue, size_t index);

#ifdef __cplusplus
}
#endif

#endif
