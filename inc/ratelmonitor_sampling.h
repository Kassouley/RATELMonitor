#ifndef RATELMONITOR_SAMPLING_H
#define RATELMONITOR_SAMPLING_H

#include <stdint.h>
#include <stdbool.h>
#include "ratelmonitor.h"

typedef enum {
    RATELMONITOR_SAMPLING_TYPE_POWER,
    RATELMONITOR_SAMPLING_TYPE_GPU_USAGE_PERCENT,
    RATELMONITOR_SAMPLING_TYPE_GPU_VRAM_USED,
    RATELMONITOR_SAMPLING_TYPE_COUNT
} ratelmonitor_sampling_type_t;

typedef struct ratelmonitor_sample_s {
    uint64_t timestamp;
    ratelmonitor_gpu_t* gpu;
    size_t gpu_index;
    int8_t gpu_usage_percent;
    int64_t gpu_power_watts;
    int64_t gpu_vram_used;

} ratelmonitor_sample_t;

typedef struct ratelmonitor_sampling_context_s {
    bool sampling_active;
    uint64_t interval_ms;
    void (*callback)(ratelmonitor_sample_t*, void*);
    void* user_data;
    ratelmonitor_gpu_t* gpus;
    size_t gpu_count;
    bool enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_COUNT];
} ratelmonitor_sampling_context_t;



ratelmonitor_status_t ratelmonitor_sampling_init(ratelmonitor_sampling_context_t* ctx, 
            ratelmonitor_gpu_t* gpus, size_t gpu_count,
            uint64_t interval_ms,
            void (*callback)(ratelmonitor_sample_t*, void*), void* user_data,
            bool enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_COUNT]);
ratelmonitor_status_t ratelmonitor_sampling_loop(ratelmonitor_sampling_context_t* ctx, bool (*cb)(ratelmonitor_sampling_context_t* ctx));
ratelmonitor_status_t ratelmonitor_sample_once(ratelmonitor_sampling_context_t* ctx);
ratelmonitor_status_t ratelmonitor_sampling_stop(ratelmonitor_sampling_context_t* ctx);

#endif // RATELMONITOR_SAMPLING_H