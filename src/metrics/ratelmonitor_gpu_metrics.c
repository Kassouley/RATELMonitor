#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "ratelmonitor.h"
#include "ratelmonitor_utils.h"
#include "ratelmonitor_gpu_metrics.h"
#include "ratelmonitor_gpu_sysfs.h"

static ratelmonitor_status_t ratelmonitor_gpu_metrics_layout(
    uint8_t format_revision,
    uint8_t content_revision,
    ratelmonitor_gpu_metrics_layout_t *layout)
{
    if (layout == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    switch (format_revision) {
    case 1:
        switch (content_revision) {
			case 0: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V1_0; return RATELMONITOR_STATUS_SUCCESS;
			case 1: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V1_1; return RATELMONITOR_STATUS_SUCCESS;
			case 2: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V1_2; return RATELMONITOR_STATUS_SUCCESS;
			case 3: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V1_3; return RATELMONITOR_STATUS_SUCCESS;
			default: break;
        }
        break;
    case 2:
        switch (content_revision) {
			case 0: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V2_0; return RATELMONITOR_STATUS_SUCCESS;
			case 1: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V2_1; return RATELMONITOR_STATUS_SUCCESS;
			case 2: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V2_2; return RATELMONITOR_STATUS_SUCCESS;
			case 3: *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_V2_3; return RATELMONITOR_STATUS_SUCCESS;
			default: break;
        }
        break;
    default:
        break;
    }

    *layout = (ratelmonitor_gpu_metrics_layout_t)RATELMONITOR_GPU_METRICS_UNKNOWN_LAYOUT;
    return RATELMONITOR_STATUS_INVALID_VALUE;
}


ratelmonitor_status_t ratelmonitor_open_gpu_metrics(ratelmonitor_gpu_metrics_t* gpu_metric, const char *device_path)
{
    ratelmonitor_status_t status = RATELMONITOR_STATUS_SUCCESS;
    gpu_metric->ready = false;

    if (gpu_metric == NULL || device_path == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    status = ratelmonitor_open_gpu_sysfs(&gpu_metric->sysfs, device_path, "gpu_metrics");
    if (status != RATELMONITOR_STATUS_SUCCESS)
        return status;

    int fd = gpu_metric->sysfs.fd;

    uint8_t metrics[4096];
    ssize_t bytes = pread(fd, metrics, sizeof(metrics), 0);
    if (bytes < 4) {
        ratelmonitor_close_gpu_metrics(gpu_metric);
        return RATELMONITOR_STATUS_READ_FAILED;
    }

    /* The first four bytes are structure_size, format_revision, and content_revision. */
    ratelmonitor_gpu_metrics_header_t header;
    memcpy(&header, metrics, sizeof(ratelmonitor_gpu_metrics_header_t));
    if (header.structure_size < 4 || header.structure_size > (size_t)bytes) {
        ratelmonitor_close_gpu_metrics(gpu_metric);
        return RATELMONITOR_STATUS_PARSE_FAILED;
    }
    
    ratelmonitor_gpu_metrics_layout_t layout;
    if (ratelmonitor_gpu_metrics_layout(header.format_revision, header.content_revision, &layout) != RATELMONITOR_STATUS_SUCCESS) {
        ratelmonitor_close_gpu_metrics(gpu_metric);
        return RATELMONITOR_STATUS_INVALID_VALUE;
    }

    gpu_metric->header = header;
    gpu_metric->layout = layout;
    gpu_metric->ready = true;

    return RATELMONITOR_STATUS_SUCCESS;
}


ratelmonitor_status_t ratelmonitor_close_gpu_metrics(ratelmonitor_gpu_metrics_t *gpu_metric)
{
    if (gpu_metric == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;
    gpu_metric->ready = false;
    return ratelmonitor_close_gpu_sysfs(&gpu_metric->sysfs);
}


ratelmonitor_status_t ratelmonitor_read_gpu_metrics(ratelmonitor_gpu_metrics_t *gpu_metric, ratelmonitor_gpu_metrics_snapshot_t* snapshot)
{
    if (gpu_metric == NULL || snapshot == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    snapshot->gpu_metric = gpu_metric;
    snapshot->ext = NULL;
    
    if (!gpu_metric->ready)
        return RATELMONITOR_STATUS_SUCCESS;

    int fd = gpu_metric->sysfs.fd;
    if (fd < 0) return RATELMONITOR_STATUS_NOT_INITIALIZED;

    ssize_t bytes = pread(fd, snapshot->buffer, sizeof(snapshot->buffer), 0);
    if (bytes < 4) return RATELMONITOR_STATUS_READ_FAILED;

    return RATELMONITOR_STATUS_SUCCESS;
}


ratelmonitor_status_t ratelmonitor_get_gpu_socket_power(ratelmonitor_gpu_metrics_snapshot_t* snapshot, uint64_t *power_watts)
{
    *power_watts = (uint64_t) -1;

    if (snapshot == NULL || snapshot->gpu_metric == NULL || power_watts == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    if (!snapshot->gpu_metric->ready)
        return RATELMONITOR_STATUS_SUCCESS;

    ratelmonitor_gpu_metrics_header_t* header = &snapshot->gpu_metric->header;
    ratelmonitor_gpu_metrics_layout_t* layout = &snapshot->gpu_metric->layout;

    if (layout->power_offset + layout->power_size > header->structure_size)
        return RATELMONITOR_STATUS_INVALID_VALUE;

    memcpy(power_watts, snapshot->buffer + layout->power_offset, layout->power_size);
    return RATELMONITOR_STATUS_SUCCESS;
}