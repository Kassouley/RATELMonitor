#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "ratelmonitor_utils.h"
#include "ratelmonitor_drm.h"
#include "ratelmonitor_kfd.h"
#include "ratelmonitor_gpu_metrics.h"
#include "ratelmonitor.h"

typedef struct {
    ratelmonitor_gpu_t* gpus;
    size_t count;
    size_t capacity;
} ratelmonitor_gpu_list_t;

static int append_gpu(ratelmonitor_gpu_list_t *list, const ratelmonitor_gpu_t *gpu)
{
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        ratelmonitor_gpu_t *resized = realloc(list->gpus, new_capacity * sizeof(*list->gpus));
        if (resized == NULL) return -1;
        list->gpus = resized;
        list->capacity = new_capacity;
    }
    list->gpus[list->count++] = *gpu;
    return 0;
}

static bool for_each_callback(const char *d_name, void *arg)
{
    ratelmonitor_gpu_t gpu = {0};
    char *canonical_device;

    if(!ratelmonitor_is_card_name(d_name)) {
        return true;
    }
    if (ratelmonitor_get_device_path(d_name, gpu.device_path) != 0) {
        fprintf(stderr, "warning: cannot construct device path for %s (%s)\n", d_name, strerror(errno));
        return true;
    }

    if (ratelmonitor_get_canonical_device_path(&canonical_device, gpu.device_path) != 0) {
        fprintf(stderr, "warning: cannot resolve %s: %s\n", gpu.device_path, strerror(errno));
        return true;
    }

    if (!ratelmonitor_is_amd_gpu(canonical_device)) {
        fprintf(stderr, "warning: %s is not an AMD GPU\n", canonical_device);
        free(canonical_device);
        return true;
    }

    if (ratelmonitor_parse_dbdf_from_sysfs(canonical_device, &gpu.dbdf) != 0) {
        fprintf(stderr, "warning: cannot parse DBDF from %s\n", canonical_device);
        free(canonical_device);
        return true;
    }
    free(canonical_device);


    uint32_t location_id = ratelmonitor_get_location_id_from_dbdf(&gpu.dbdf);
    if (ratelmonitor_find_kfd_node_id_by_location_id(location_id, &gpu.node_id) != 0) {
        fprintf(stderr, "warning: cannot find KFD node for %s (location_id 0x%08x)\n", d_name, location_id);
        return true;
    }

    if (ratelmonitor_get_kfd_node_path(gpu.node_id, gpu.kdf_path) != 0) {
        fprintf(stderr, "warning: cannot construct KFD node path for %s (node_id 0x%08x)\n", d_name, gpu.node_id);
        return true;
    }

    if (ratelmonitor_open_gpu_metrics(&gpu.gpu_metrics, gpu.device_path) != 0) {
        fprintf(stderr, "warning: cannot open gpu_metrics for %s\n", d_name);
        return true;
    }

    if(ratelmonitor_open_gpu_sysfs(&gpu.gpu_usage, gpu.device_path, "gpu_busy_percent") != 0) {
        fprintf(stderr, "warning: cannot open gpu_usage for %s\n", d_name);
        return true;
    }

    if(ratelmonitor_open_gpu_sysfs(&gpu.vram_used, gpu.device_path, "mem_info_vram_used") != 0) {
        fprintf(stderr, "warning: cannot open gpu_usage for %s\n", d_name);
        return true;
    }

    if(ratelmonitor_open_gpu_sysfs(&gpu.power_avg, gpu.device_path, "hwmon/hwmon1/power1_average") != 0) {
        fprintf(stderr, "warning: cannot open power_avg for %s\n", d_name);
        return true;
    }

    if(ratelmonitor_gpu_sysfs_open_read_close(gpu.device_path, "mem_info_vram_total", (int64_t*)&gpu.vram_total) != 0) {
        fprintf(stderr, "warning: cannot read vram total for %s\n", d_name);
        return true;
    }

    if(ratelmonitor_gpu_sysfs_open_read_close(gpu.device_path, "hwmon/hwmon1/power1_cap", (int64_t*)&gpu.power_cap) != 0) {
        fprintf(stderr, "warning: cannot read power cap for %s\n", d_name);
        return true;
    }
    gpu.power_cap /= 1000000; // convert from uW to W

    if (append_gpu((ratelmonitor_gpu_list_t *)arg, &gpu) != 0) {
        fprintf(stderr, "error: cannot append GPU for %s: %s\n", d_name, strerror(errno));
        return true;
    }

    return true;
}

int ratelmonitor_discover_gpus(ratelmonitor_gpu_t **gpus, size_t *count)
{
    ratelmonitor_gpu_list_t list = {0};
    int result = for_each_entry_in_dir(DRM_CLASS_PATH, DT_LNK, for_each_callback, &list);
    if (result != 0) {
        fprintf(stderr, "error: cannot scan %s: %s\n", DRM_CLASS_PATH, strerror(errno));
        free(list.gpus);
        return result;
    }
    *gpus = list.gpus;
    *count = list.count;
    return 0;
}

int ratelmonitor_fini(ratelmonitor_gpu_t *gpus, size_t count)
{
    if (gpus == NULL) return -1;
    for (size_t i = 0; i < count; ++i) {
        ratelmonitor_close_gpu_metrics(&gpus[i].gpu_metrics);
    }
    free(gpus);
    return 0;
}
