#ifndef RATELMONITOR_GPU_SYSFS_H
#define RATELMONITOR_GPU_SYSFS_H


typedef struct {
    int fd;
} ratelmonitor_gpu_sysfs_t;


int ratelmonitor_close_gpu_sysfs(ratelmonitor_gpu_sysfs_t *gpu_sysfs);
int ratelmonitor_read_gpu_sysfs(ratelmonitor_gpu_sysfs_t *gpu_sysfs, int64_t *out);
int ratelmonitor_open_gpu_sysfs(ratelmonitor_gpu_sysfs_t* gpu_sysfs, const char *device_path, const char *sysfs_file);
int ratelmonitor_gpu_sysfs_open_read_close(const char *device_path, const char *sysfs_file, int64_t *out);

#endif // RATELMONITOR_GPU_SYSFS_H