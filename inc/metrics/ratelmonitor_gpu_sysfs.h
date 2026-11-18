#ifndef RATELMONITOR_GPU_SYSFS_H
#define RATELMONITOR_GPU_SYSFS_H

#include <stdbool.h>
#include "ratelmonitor_utils.h"

typedef struct {
    int fd;
    bool ready;
} ratelmonitor_gpu_sysfs_t;


ratelmonitor_status_t ratelmonitor_close_gpu_sysfs(ratelmonitor_gpu_sysfs_t *gpu_sysfs);
ratelmonitor_status_t ratelmonitor_read_gpu_sysfs(ratelmonitor_gpu_sysfs_t *gpu_sysfs, uint8_t buffer[4096], size_t *read);
ratelmonitor_status_t ratelmonitor_open_gpu_sysfs(ratelmonitor_gpu_sysfs_t* gpu_sysfs, const char *device_path, const char *sysfs_file);
ratelmonitor_status_t ratelmonitor_gpu_sysfs_orc(const char *device_path, const char *sysfs_file, uint8_t buffer[4096], size_t *read);

#endif // RATELMONITOR_GPU_SYSFS_H