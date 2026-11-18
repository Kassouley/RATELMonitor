#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>

#include "ratelmonitor_sampling.h"
#include "ratelmonitor.h"

static inline uint64_t get_time_ns() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline struct timespec timespec_from_ms(uint64_t ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000ULL);
    return ts;
}

ratelmonitor_status_t ratelmonitor_sample_once(ratelmonitor_sampling_context_t* ctx)
{
    if (ctx == NULL) {
        return RATELMONITOR_STATUS_INVALID_PTR;
    }

    ratelmonitor_status_t status = RATELMONITOR_STATUS_SUCCESS;

    uint8_t buffer[4096];
    size_t read = 0;

    ratelmonitor_sample_t sample = {0};
    ratelmonitor_gpu_t *gpu = NULL;

    for (size_t i = 0; i < ctx->gpu_count; ++i) {
        gpu = &ctx->gpus[i];

        sample = (ratelmonitor_sample_t){0};
        sample.gpu_index = i;
        sample.gpu = gpu;
        sample.gpu_power_watts = -1;
        sample.gpu_vram_used = -1;
        sample.gpu_usage_percent = -1;
        sample.timestamp = get_time_ns();

        status = RATELMONITOR_STATUS_SUCCESS;

        if (ctx->enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_GPU_USAGE_PERCENT]) {
            status = ratelmonitor_read_gpu_sysfs(&gpu->gpu_usage, buffer, &read);
            if (status != RATELMONITOR_STATUS_SUCCESS) return status;

            if (read > 0) {
                sample.gpu_usage_percent = strtoll((const char*)buffer, NULL, 10);
            }
        }

        if (ctx->enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_GPU_VRAM_USED]) {
            status = ratelmonitor_read_gpu_sysfs(&gpu->vram_used, buffer, &read);
            if (status != RATELMONITOR_STATUS_SUCCESS) return status;

            if (read > 0) {
                sample.gpu_vram_used = strtoll((const char*)buffer, NULL, 10);
            }
        }


        if (ctx->enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_POWER]) {
            status = ratelmonitor_read_gpu_sysfs(&gpu->power_avg, buffer, &read);
            if (status != RATELMONITOR_STATUS_SUCCESS) return status;

            if (read > 0) {
                sample.gpu_power_watts = strtoll((const char*)buffer, NULL, 10);
                sample.gpu_power_watts /= 1000000;
            }
        }

        if (ctx->callback != NULL) {
            ctx->callback(&sample, ctx->user_data);
        }
    }
    return status;
}

ratelmonitor_status_t ratelmonitor_sampling_loop(ratelmonitor_sampling_context_t* ctx, bool (*cb)(ratelmonitor_sampling_context_t* ctx))
{
    if (ctx == NULL) {
        return RATELMONITOR_STATUS_INVALID_PTR;
    }
    struct timespec sleep_time = timespec_from_ms(ctx->interval_ms);

    ctx->sampling_active = true;
    while (ctx->sampling_active) {
        ratelmonitor_status_t status = ratelmonitor_sample_once(ctx);
        if (status != RATELMONITOR_STATUS_SUCCESS) {
            return status;
        }

        if(cb) {
            bool stop = cb(ctx);
            if(stop) {
                ctx->sampling_active = false;
                break;
            }
        }

        nanosleep(&sleep_time, NULL);
    }
    return RATELMONITOR_STATUS_SUCCESS;
}

ratelmonitor_status_t ratelmonitor_sampling_init(ratelmonitor_sampling_context_t* ctx, 
            ratelmonitor_gpu_t* gpus, size_t gpu_count,
            uint64_t interval_ms,
            void (*callback)(ratelmonitor_sample_t*, void*), void* user_data,
            bool enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_COUNT])
{
    if (ctx == NULL || gpus == NULL) {
        return RATELMONITOR_STATUS_INVALID_PTR;
    }
    if (gpu_count == 0) {
        return RATELMONITOR_STATUS_INVALID_VALUE;
    }

    ctx->gpus = gpus;
    ctx->gpu_count = gpu_count;
    ctx->sampling_active = false;
    ctx->interval_ms = interval_ms;
    ctx->callback = callback;
    ctx->user_data = user_data;
    memcpy(ctx->enabled_sampling_types, enabled_sampling_types, sizeof(ctx->enabled_sampling_types));

    return RATELMONITOR_STATUS_SUCCESS;
}

ratelmonitor_status_t ratelmonitor_sampling_stop(ratelmonitor_sampling_context_t* ctx)
{
    if (ctx == NULL) {
        return RATELMONITOR_STATUS_INVALID_PTR;
    }
    ctx->sampling_active = false;
    return RATELMONITOR_STATUS_SUCCESS;
}