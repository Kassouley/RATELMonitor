#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "ratelmonitor.h"
#include "ratelmonitor_utils.h"
#include "ratelmonitor_gpu_sysfs.h"


int ratelmonitor_open_gpu_sysfs(ratelmonitor_gpu_sysfs_t* gpu_sysfs, const char *device_path, const char *sysfs_file)
{
    if (gpu_sysfs == NULL || device_path == NULL || sysfs_file == NULL)
        return -1;
    char path[PATH_MAX];
    if (make_path(path, PATH_MAX, device_path, sysfs_file) != 0) {
        return -1;
    }

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    gpu_sysfs->fd = fd;
    return 0;
}

int ratelmonitor_close_gpu_sysfs(ratelmonitor_gpu_sysfs_t *gpu_sysfs)
{
    if (gpu_sysfs == NULL)
        return -1;

    int fd = gpu_sysfs->fd;
    gpu_sysfs->fd = -1;
    return close(fd);
}

int ratelmonitor_read_gpu_sysfs(ratelmonitor_gpu_sysfs_t *gpu_sysfs, int64_t *out)
{
    if (gpu_sysfs == NULL || out == NULL)
        return -1;

    int fd = gpu_sysfs->fd;
    if (fd < 0) return -1;

    uint8_t buffer[4096];
    ssize_t bytes = pread(fd, buffer, sizeof(buffer), 0);
    if (bytes < 0) return -1;
    buffer[bytes - 1] = '\0';
    parse_unsigned((const char *)buffer, (uint64_t *)out);

    return 0;
}


int ratelmonitor_gpu_sysfs_open_read_close(const char *device_path, const char *sysfs_file, int64_t *out)
{
    ratelmonitor_gpu_sysfs_t gpu_sysfs = {0};
    if (ratelmonitor_open_gpu_sysfs(&gpu_sysfs, device_path, sysfs_file) != 0)
        return -1;

    int res = ratelmonitor_read_gpu_sysfs(&gpu_sysfs, out);
    ratelmonitor_close_gpu_sysfs(&gpu_sysfs);
    return res;
}