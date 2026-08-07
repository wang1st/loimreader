#include "loim/import_queue.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void loim_import_queue_clear_paths(loim_import_queue *queue)
{
    size_t index;

    for (index = 0U; index < queue->count; ++index) {
        free(queue->paths[index]);
    }
    queue->count = 0U;
}

void loim_import_queue_init(loim_import_queue *queue)
{
    if (queue != NULL) {
        memset(queue, 0, sizeof(*queue));
    }
}

void loim_import_queue_destroy(loim_import_queue *queue)
{
    if (queue == NULL) {
        return;
    }
    loim_import_queue_clear_paths(queue);
    free(queue->paths);
    memset(queue, 0, sizeof(*queue));
}

loim_status loim_import_queue_begin(loim_import_queue *queue)
{
    if (queue == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    loim_import_queue_clear_paths(queue);
    queue->collecting = true;
    return LOIM_OK;
}

loim_status loim_import_queue_add(loim_import_queue *queue, const char *path)
{
    size_t length;
    char *copy;

    if (queue == NULL || path == NULL || path[0] == '\0') {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    if (!queue->collecting) {
        loim_status status = loim_import_queue_begin(queue);

        if (status != LOIM_OK) {
            return status;
        }
    }
    if (queue->count == queue->capacity) {
        size_t next_capacity = queue->capacity == 0U ? 8U : queue->capacity * 2U;
        char **grown;

        if (next_capacity < queue->capacity ||
            next_capacity > SIZE_MAX / sizeof(*queue->paths)) {
            return LOIM_ERROR_OVERFLOW;
        }
        grown = realloc(queue->paths, next_capacity * sizeof(*queue->paths));
        if (grown == NULL) {
            return LOIM_ERROR_OUT_OF_MEMORY;
        }
        queue->paths = grown;
        queue->capacity = next_capacity;
    }
    length = strlen(path);
    if (length == SIZE_MAX) {
        return LOIM_ERROR_OVERFLOW;
    }
    copy = malloc(length + 1U);
    if (copy == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    memcpy(copy, path, length + 1U);
    queue->paths[queue->count] = copy;
    queue->count += 1U;
    return LOIM_OK;
}

loim_status loim_import_queue_complete(loim_import_queue *queue)
{
    if (queue == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    queue->collecting = false;
    return LOIM_OK;
}

const char *loim_import_queue_path(const loim_import_queue *queue, size_t index)
{
    if (queue == NULL || index >= queue->count) {
        return NULL;
    }
    return queue->paths[index];
}
