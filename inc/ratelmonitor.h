#ifndef RATELMONITOR_H
#define RATELMONITOR_H

#include <limits.h>
#include <stdint.h>

typedef struct ratelmonitor_gpu_s ratelmonitor_gpu_t;

#include "ratelmonitor_drm.h"
#include "ratelmonitor_gpu_metrics.h"
#include "ratelmonitor_gpu_sysfs.h"
#include "ratelmonitor_sampling.h"

struct ratelmonitor_gpu_s {
    ratelmonitor_dbdf_t dbdf;
    uint32_t node_id;
    char kdf_path[PATH_MAX];
    char device_path[PATH_MAX];
    ratelmonitor_gpu_metrics_t gpu_metrics;
    ratelmonitor_gpu_sysfs_t gpu_usage;
    ratelmonitor_gpu_sysfs_t vram_used;
    ratelmonitor_gpu_sysfs_t power_avg;
    uint64_t vram_total;
    uint64_t power_cap;
};

int ratelmonitor_discover_gpus(ratelmonitor_gpu_t **gpus, size_t *count);

int ratelmonitor_fini(ratelmonitor_gpu_t *gpus, size_t count);

#endif // RATELMONITOR_H