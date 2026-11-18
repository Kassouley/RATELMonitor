#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "ratelmonitor.h"
#include "ratelmonitor_daemon.h"

static atomic_bool worker_stopped = ATOMIC_VAR_INIT(false);

static void stop_worker(void *user_data)
{
    (void)user_data;
    atomic_store(&worker_stopped, true);
}

static bool worker_should_stop(ratelmonitor_sampling_context_t *ctx)
{
    (void)ctx;
    return atomic_load(&worker_stopped);
}

static void write_sample(ratelmonitor_sample_t *sample, void *user_data)
{
    FILE *csv = (FILE *) user_data;
    if (sample == NULL || sample->gpu == NULL || csv == NULL)
        return;

    fprintf(csv, "%" PRIu64 ",%zu,%u,%d,%" PRId64 ",%" PRId64 "\n",
            sample->timestamp, sample->gpu_index, sample->gpu->node_id,
            sample->gpu_usage_percent, sample->gpu_vram_used,
            sample->gpu_power_watts);
    fflush(csv);
}

static ratelmonitor_status_t monitor_worker(void *user_data)
{
    (void)user_data;

    const char *output_path = getenv("RATELMONITOR_CSV");
    if (output_path == NULL || output_path[0] == '\0')
        output_path = "ratelmonitor.csv";


    FILE *csv = fopen(output_path, "w");
    if (csv == NULL) {
        fprintf(stderr, "ratelmonitor: cannot open CSV %s: %s\n",
                output_path, strerror(errno));
        return RATELMONITOR_STATUS_OPEN_FAILED;
    }
    fprintf(csv, "timestamp_ns,gpu_index,node_id,gpu_usage_percent,"
                 "vram_used_bytes,power_watts\n");
    fflush(csv);

    ratelmonitor_gpu_t *gpus = NULL;
    size_t gpu_count = 0;
    ratelmonitor_status_t status = ratelmonitor_discover_gpus(&gpus, &gpu_count);
    if (status == RATELMONITOR_STATUS_SUCCESS && gpu_count > 0) {
        const char *interval_text = getenv("RATELMONITOR_INTERVAL_MS");
        uint64_t interval_ms = 1000;
        if (interval_text != NULL) {
            char *end = NULL;
            interval_ms = strtoull(interval_text, &end, 10);
            if (end == interval_text || *end != '\0' || interval_ms == 0)
                status = RATELMONITOR_STATUS_INVALID_VALUE;
        }

        ratelmonitor_sampling_context_t ctx = {0};
        if (status == RATELMONITOR_STATUS_SUCCESS)
            status = ratelmonitor_sampling_init(
                &ctx, gpus, gpu_count, interval_ms, write_sample, csv,
                (bool[]){true, true, true});
        if (status == RATELMONITOR_STATUS_SUCCESS)
            status = ratelmonitor_sampling_loop(&ctx, worker_should_stop);
    }

    if (gpus != NULL)
        ratelmonitor_fini(gpus, gpu_count);
    fclose(csv);
    return status;
}


__attribute__((constructor))
static void ratelmonitor_preload_start(void)
{
    ratelmonitor_worker_on_exit_t *on_exit_data = malloc(sizeof(ratelmonitor_worker_on_exit_t));
    if (on_exit_data == NULL) {
        fprintf(stderr, "ratelmonitor: failed to allocate memory for on_exit_data\n");
        return;
    }
    on_exit_data->on_exit = stop_worker;
    on_exit_data->user_data = NULL;

    ratelmonitor_status_t status = ratelmonitor_daemon_start(monitor_worker, NULL, on_exit_data);
    if (status != RATELMONITOR_STATUS_SUCCESS)
        fprintf(stderr, "ratelmonitor: daemon start failed: %s\n",
                ratelmonitor_status_string(status));
}

__attribute__((destructor))
static void ratelmonitor_preload_stop(void)
{
    ratelmonitor_status_t status = ratelmonitor_daemon_stop();
    if (status != RATELMONITOR_STATUS_SUCCESS)
        fprintf(stderr, "ratelmonitor: daemon stop failed: %s\n",
                ratelmonitor_status_string(status));
}
