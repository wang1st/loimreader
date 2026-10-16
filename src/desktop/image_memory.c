#include "image_memory.h"

#include <stddef.h>
#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

uint64_t loim_available_memory_bytes(void)
{
#if defined(_WIN32)
    MEMORYSTATUSEX memory = {0};
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory)) {
        return 0U;
    }
    return memory.ullAvailPhys != 0U ? (uint64_t)memory.ullAvailPhys : 1U;
#elif defined(__APPLE__)
    mach_port_t host = mach_host_self();
    vm_statistics64_data_t statistics;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t page_size = 0U;
    uint64_t available = 0U;
    if (host_page_size(host, &page_size) == KERN_SUCCESS &&
        host_statistics64(host, HOST_VM_INFO64,
            (host_info64_t)&statistics, &count) == KERN_SUCCESS) {
        /* Inactive pages are reclaimable; do not count active/compressed RAM. */
        uint64_t pages = (uint64_t)statistics.free_count +
            (uint64_t)statistics.inactive_count;
        if (page_size != 0U && pages <= UINT64_MAX / (uint64_t)page_size) {
            available = pages * (uint64_t)page_size;
            if (available == 0U) available = 1U;
        }
    }
    (void)mach_port_deallocate(mach_task_self(), host);
    return available;
#elif defined(__linux__)
    FILE *file = fopen("/proc/meminfo", "r");
    char line[256];
    uint64_t available = 0U;
    if (file == NULL) {
        return 0U;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned long long kib;
        if (sscanf(line, "MemAvailable: %llu kB", &kib) == 1) {
            if (kib <= UINT64_MAX / 1024U) {
                available = (uint64_t)kib * 1024U;
                if (available == 0U) available = 1U;
            }
            break;
        }
    }
    fclose(file);
    return available;
#else
    return 0U;
#endif
}

uint64_t loim_image_pixel_budget(uint64_t total_bytes, uint64_t available_bytes)
{
    /* Leave half the current headroom and use at most a quarter of total RAM.
       12 bytes/pixel covers decoding, RGBA conversion and texture upload copies.
       This is an admission estimate; allocations can still fail afterwards. */
    uint64_t budget = available_bytes != 0U
        ? available_bytes / 2U : total_bytes / 8U;
    if (total_bytes != 0U && budget > total_bytes / 4U) {
        budget = total_bytes / 4U;
    }
    if (budget > SIZE_MAX) {
        budget = SIZE_MAX;
    }
    return budget / 12U;
}
