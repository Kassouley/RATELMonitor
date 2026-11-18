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

static void close_gpu_resources(ratelmonitor_gpu_t *gpu)
{
    ratelmonitor_close_gpu_metrics(&gpu->gpu_metrics);
    ratelmonitor_close_gpu_sysfs(&gpu->gpu_usage);
    ratelmonitor_close_gpu_sysfs(&gpu->vram_used);
    ratelmonitor_close_gpu_sysfs(&gpu->power_avg);
}

static ratelmonitor_status_t append_gpu(ratelmonitor_gpu_list_t *list, const ratelmonitor_gpu_t *gpu)
{
    if (list == NULL || gpu == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        ratelmonitor_gpu_t *resized = realloc(list->gpus, new_capacity * sizeof(*list->gpus));
        if (resized == NULL) return RATELMONITOR_STATUS_OUT_OF_MEMORY;
        list->gpus = resized;
        list->capacity = new_capacity;
    }

    list->gpus[list->count++] = *gpu;
    return RATELMONITOR_STATUS_SUCCESS;
}

#define CALL_TRY(call, on_error) do { \
    ratelmonitor_status_t s = call; \
    if (s != RATELMONITOR_STATUS_SUCCESS) { on_error; } \
} while(0)

#define CALL_STOP(call, on_error) do { \
    ratelmonitor_status_t s = call; \
    if (s != RATELMONITOR_STATUS_SUCCESS) {on_error; return s; } \
} while(0)

static ratelmonitor_status_t for_each_callback(const char *d_name, void *arg)
{
    ratelmonitor_gpu_t gpu = {0};
    ratelmonitor_gpu_list_t *list = arg;
    char *canonical_device;

    uint8_t buffer[4096];
    size_t read = 0;

    if(!ratelmonitor_is_card_name(d_name)) {
        return RATELMONITOR_STATUS_CONTINUE;
    }

    CALL_STOP(ratelmonitor_get_device_path(d_name, gpu.device_path), {});

    CALL_STOP(ratelmonitor_get_canonical_device_path(&canonical_device, gpu.device_path), {});

    if (!ratelmonitor_is_amd_gpu(canonical_device)) {
        free(canonical_device);
        return RATELMONITOR_STATUS_CONTINUE;
    }

    CALL_STOP(ratelmonitor_parse_dbdf_from_sysfs(canonical_device, &gpu.dbdf), {
        free(canonical_device);
    });
       
    free(canonical_device);


    uint32_t location_id = ratelmonitor_get_location_id_from_dbdf(&gpu.dbdf);
    CALL_STOP(ratelmonitor_find_kfd_node_id_by_location_id(location_id, &gpu.node_id), {});

    CALL_STOP(ratelmonitor_get_kfd_node_path(gpu.node_id, gpu.kdf_path), {});

    CALL_TRY(ratelmonitor_open_gpu_metrics(&gpu.gpu_metrics, gpu.device_path), {
        fprintf(stderr, "warning: cannot open gpu_metrics for %s\n", d_name);
    });

    CALL_TRY(ratelmonitor_open_gpu_sysfs(&gpu.gpu_usage, gpu.device_path, "gpu_busy_percent"), {
        fprintf(stderr, "warning: cannot open gpu_usage for %s\n", d_name);
    });

    CALL_TRY(ratelmonitor_open_gpu_sysfs(&gpu.vram_used, gpu.device_path, "mem_info_vram_used"), {
        fprintf(stderr, "warning: cannot open vram_used for %s\n", d_name);
    });

    CALL_TRY(ratelmonitor_open_gpu_sysfs(&gpu.power_avg, gpu.device_path, "hwmon/hwmon1/power1_average"), {
        fprintf(stderr, "warning: cannot open power_avg for %s\n", d_name);
    });

    CALL_TRY(ratelmonitor_gpu_sysfs_orc(gpu.device_path, "mem_info_vram_total", buffer, &read), {
        fprintf(stderr, "warning: cannot read vram total for %s\n", d_name);
    });

    gpu.vram_total = -1;
    if (read != 0) {
        gpu.vram_total = strtoll((const char*)buffer, NULL, 10);
    }

    CALL_TRY(ratelmonitor_gpu_sysfs_orc(gpu.device_path, "hwmon/hwmon1/power1_cap", buffer, &read), {
        fprintf(stderr, "warning: cannot read power cap for %s\n", d_name);
    });

    gpu.power_cap = -1;
    if (read != 0) {
        gpu.power_cap =  strtoll((const char*)buffer, NULL, 10) / 1000000; // convert from uW to W
    }

    CALL_STOP(append_gpu(list, &gpu), {
        fprintf(stderr, "error: cannot append GPU for %s: %s\n", d_name, strerror(errno));
        close_gpu_resources(&gpu);
    });

    return RATELMONITOR_STATUS_SUCCESS;
}

ratelmonitor_status_t ratelmonitor_discover_gpus(ratelmonitor_gpu_t **gpus, size_t *count)
{
    if (gpus == NULL || count == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    *gpus = NULL;
    *count = 0;
    ratelmonitor_gpu_list_t list = {0};
    ratelmonitor_status_t status = for_each_entry_in_dir(DRM_CLASS_PATH, DT_LNK, for_each_callback, &list);
    
    if (status != RATELMONITOR_STATUS_SUCCESS) {
        fprintf(stderr, "error: cannot scan %s: %s\n", DRM_CLASS_PATH, strerror(errno));
        free(list.gpus);
        return status;
    }

    *gpus = list.gpus;
    *count = list.count;
    return RATELMONITOR_STATUS_SUCCESS;
}



ratelmonitor_status_t ratelmonitor_fini(ratelmonitor_gpu_t *gpus, size_t count)
{
    if (gpus == NULL) return RATELMONITOR_STATUS_INVALID_PTR;
    ratelmonitor_status_t status = RATELMONITOR_STATUS_SUCCESS;
    for (size_t i = 0; i < count; ++i) {
        close_gpu_resources(&gpus[i]);
    }
    free(gpus);
    return status;
}
