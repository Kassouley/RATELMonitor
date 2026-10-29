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
#include "ratelmonitor_gpu_metrics.h"

static int ratelmonitor_gpu_metrics_layout(
    uint8_t format_revision,
    uint8_t content_revision,
    ratelmonitor_gpu_metrics_layout_t *layout)
{
    if (layout == NULL)
        return -1;

    switch (format_revision) {
    case 1:
        switch (content_revision) {
			case 0: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V1_0; return 0;
			case 1: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V1_1; return 0;
			case 2: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V1_2; return 0;
			case 3: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V1_3; return 0;
			default: break;
        }
        break;
    case 2:
        switch (content_revision) {
			case 0: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V2_0; return 0;
			case 1: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V2_1; return 0;
			case 2: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V2_2; return 0;
			case 3: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V2_3; return 0;
			default: break;
        }
        break;
    default:
        break;
    }

    *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_UNKNOWN_LAYOUT;
    return -1;
}



int ratelmonitor_open_gpu_metrics(ratelmonitor_gpu_metrics_t* gpu_metric, const char *device_path)
{
    if (gpu_metric == NULL || device_path == NULL)
        return -1;

    char metrics_path[PATH_MAX];
    if (make_path(metrics_path, PATH_MAX, device_path, "gpu_metrics") != 0) {
        return -1;
    }


    int fd = open(metrics_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    gpu_metric->fd = fd;

    uint8_t metrics[4096];
    ssize_t bytes = pread(fd, metrics, sizeof(metrics), 0);
    if (bytes < 4) {
        ratelmonitor_close_gpu_metrics(gpu_metric);
        return -1;
    }

    /* The first four bytes are structure_size, format_revision, and content_revision. */
    ratelmonitor_gpu_metrics_header_t header;
    memcpy(&header, metrics, sizeof(ratelmonitor_gpu_metrics_header_t));
    if (header.structure_size < 4 || header.structure_size > (size_t)bytes) {
        ratelmonitor_close_gpu_metrics(gpu_metric);
        return -1;
    }
    
    ratelmonitor_gpu_metrics_layout_t layout;
    if (ratelmonitor_gpu_metrics_layout(header.format_revision, header.content_revision, &layout) != 0) {
        ratelmonitor_close_gpu_metrics(gpu_metric);
        return -1;
    }

    gpu_metric->header = header;
    gpu_metric->layout = layout;

    return 0;
}

int ratelmonitor_close_gpu_metrics(ratelmonitor_gpu_metrics_t *gpu_metric)
{
    if (gpu_metric == NULL)
        return -1;

    int fd = gpu_metric->fd;
    gpu_metric->fd = -1;
    return close(fd);
}



int ratelmonitor_read_gpu_metrics(ratelmonitor_gpu_metrics_t *gpu_metric, ratelmonitor_gpu_metrics_snapshot_t* snapshot)
{
    if (gpu_metric == NULL || snapshot == NULL)
        return -1;

    int fd = gpu_metric->fd;
    if (fd < 0) return -1;

    snapshot->gpu_metric = gpu_metric;
    snapshot->ext = NULL;
    ssize_t bytes = pread(fd, snapshot->buffer, sizeof(snapshot->buffer), 0);
    if (bytes < 4) return -1;

    return 0;
}


int ratelmonitor_get_gpu_socket_power(ratelmonitor_gpu_metrics_snapshot_t* snapshot, uint64_t *power_watts)
{
    if (snapshot == NULL || snapshot->gpu_metric == NULL || power_watts == NULL)
        return -1;

    ratelmonitor_gpu_metrics_header_t* header = &snapshot->gpu_metric->header;
    ratelmonitor_gpu_metrics_layout_t* layout = &snapshot->gpu_metric->layout;

    if (layout->power_offset + layout->power_size > header->structure_size)
        return -1;

    *power_watts = 0;
    memcpy(power_watts, snapshot->buffer + layout->power_offset, layout->power_size);
    return 0;
}