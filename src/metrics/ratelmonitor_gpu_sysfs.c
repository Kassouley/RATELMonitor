#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "ratelmonitor.h"
#include "ratelmonitor_utils.h"
#include "ratelmonitor_gpu_sysfs.h"


ratelmonitor_status_t ratelmonitor_open_gpu_sysfs(ratelmonitor_gpu_sysfs_t* gpu_sysfs, const char *device_path, const char *sysfs_file)
{
    gpu_sysfs->ready = false;
    if (gpu_sysfs == NULL || device_path == NULL || sysfs_file == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    char path[PATH_MAX];
    MAKE_PATH(path, PATH_MAX, device_path, sysfs_file, "%s");

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return RATELMONITOR_STATUS_OPEN_FAILED;

    gpu_sysfs->fd = fd;
    gpu_sysfs->ready = true;
    return RATELMONITOR_STATUS_SUCCESS;
}

ratelmonitor_status_t ratelmonitor_close_gpu_sysfs(ratelmonitor_gpu_sysfs_t *gpu_sysfs)
{
    if (gpu_sysfs == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    int fd = gpu_sysfs->fd;
    gpu_sysfs->ready = false;
    gpu_sysfs->fd = -1;
    if (fd < 0)
        return RATELMONITOR_STATUS_NOT_INITIALIZED;

    close(fd);
    return RATELMONITOR_STATUS_SUCCESS;
}

ratelmonitor_status_t ratelmonitor_read_gpu_sysfs(ratelmonitor_gpu_sysfs_t *gpu_sysfs, uint8_t buffer[4096], size_t *read)
{
    *read = 0;

    if (gpu_sysfs == NULL || read == NULL || buffer == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    if (!gpu_sysfs->ready)
        return RATELMONITOR_STATUS_SUCCESS;

    int fd = gpu_sysfs->fd;
    if (fd < 0) return RATELMONITOR_STATUS_NOT_INITIALIZED;

    ssize_t bytes = pread(fd, buffer, 4096, 0);
    if (bytes <= 0) return RATELMONITOR_STATUS_READ_FAILED;

    buffer[bytes] = '\0';
    if (buffer[bytes - 1] == '\n') buffer[bytes - 1] = '\0';

    *read = bytes;

    return RATELMONITOR_STATUS_SUCCESS;
}


ratelmonitor_status_t ratelmonitor_gpu_sysfs_orc(const char *device_path, const char *sysfs_file,
                                                    uint8_t buffer[4096], size_t *read)
{
    *read = 0;
    ratelmonitor_gpu_sysfs_t gpu_sysfs = {0};
    ratelmonitor_status_t status = ratelmonitor_open_gpu_sysfs(&gpu_sysfs, device_path, sysfs_file);
    if (status != RATELMONITOR_STATUS_SUCCESS)
        return status;

    status = ratelmonitor_read_gpu_sysfs(&gpu_sysfs, buffer, read);
    ratelmonitor_status_t close_status = ratelmonitor_close_gpu_sysfs(&gpu_sysfs);
    return status == RATELMONITOR_STATUS_SUCCESS ? close_status : status;
}