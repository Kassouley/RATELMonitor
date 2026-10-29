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

static void* ratelmonitor_sampling_thread(void*arg)
{
    ratelmonitor_sampling_context_t* ctx = arg;
    if (ctx == NULL) {
        return NULL;
    }

    ratelmonitor_sample_t sample = {0};
    ratelmonitor_gpu_t *gpu = NULL;

    struct timespec interval;
    interval.tv_sec = (time_t)(ctx->interval_ms / 1000);
    interval.tv_nsec = (long)((ctx->interval_ms % 1000) * 1000000ULL);

    while (ctx->sampling_active) {
        for (size_t i = 0; i < ctx->gpu_count; ++i) {
            sample = (ratelmonitor_sample_t){0};
            sample.gpu_index = i;
            gpu = &ctx->gpus[i];

            sample.gpu = gpu;

            sample.timestamp = get_time_ns();

            if (ctx->enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_GPU_USAGE_PERCENT]) {
                ratelmonitor_read_gpu_sysfs(&gpu->gpu_usage, (int64_t*)&sample.gpu_usage_percent);
            }
            if (ctx->enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_GPU_VRAM_USED]) {
                ratelmonitor_read_gpu_sysfs(&gpu->vram_used, &sample.gpu_vram_used);
            }

            if (ctx->enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_POWER]) {
                ratelmonitor_read_gpu_sysfs(&gpu->power_avg, (int64_t*)&sample.gpu_power_watts);
                sample.gpu_power_watts /= 1000000; // Convert from microwatts to watts
            }

            if (ctx->callback != NULL) {
                ctx->callback(&sample, ctx);
            }
        }

        if (ctx->interval_ms != 0) {
            nanosleep(&interval, NULL);
        }
    }

    return NULL;
}

int ratelmonitor_sampling_init(ratelmonitor_sampling_context_t* ctx, ratelmonitor_gpu_t* gpus, size_t gpu_count)
{
    if (ctx == NULL || gpus == NULL || gpu_count == 0) {
        return -1;
    }

    ctx->gpus = gpus;
    ctx->gpu_count = gpu_count;
    ctx->sampling_active = false;
    ctx->interval_ms = 0;
    ctx->callback = NULL;
    ctx->user_data = NULL;
    memset(ctx->enabled_sampling_types, 0, sizeof(ctx->enabled_sampling_types));

    return 0;
}

int ratelmonitor_sampling_start(ratelmonitor_sampling_context_t* ctx, uint64_t interval_ms, 
    void (*callback)(ratelmonitor_sample_t*, void*), void* user_data,
    bool enabled_sampling_types[RATELMONITOR_SAMPLING_TYPE_COUNT])
{
    int result = 0;

    if (ctx == NULL) {
        return -1;
    }

    ctx->interval_ms = interval_ms;
    ctx->callback = callback;
    ctx->user_data = user_data;
    memcpy(ctx->enabled_sampling_types, enabled_sampling_types, sizeof(ctx->enabled_sampling_types));
    ctx->sampling_active = true;

    result = pthread_create(&ctx->sampling_thread, NULL, ratelmonitor_sampling_thread, ctx);
    if (result != 0) {
        ctx->sampling_active = false;
    }

    return result;
}

int ratelmonitor_sampling_wait(ratelmonitor_sampling_context_t* ctx)
{
    if (ctx == NULL) {
        return -1;
    }

    pthread_join(ctx->sampling_thread, NULL);

    return 0;
}

int ratelmonitor_sampling_stop(ratelmonitor_sampling_context_t* ctx)
{
    if (ctx == NULL) {
        return -1;
    }

    ctx->sampling_active = false;
    pthread_join(ctx->sampling_thread, NULL);

    return 0;
}