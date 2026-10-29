#ifndef RATELMONITOR_GPU_METRICS_H
#define RATELMONITOR_GPU_METRICS_H

#include <stddef.h>
#include <stdint.h>


#define RATELMONITOR_GPU_METRICS_UNKNOWN_LAYOUT {0}

typedef struct {
    size_t power_offset;
    size_t power_size;
    size_t energy_offset;
    size_t energy_size;
    int power_is_average;
} ratelmonitor_gpu_metrics_layout_t;


typedef struct ratelmonitor_gpu_metrics_header_s {
	uint16_t structure_size;
	uint8_t  format_revision;
	uint8_t  content_revision;
} ratelmonitor_gpu_metrics_header_t;


typedef struct {
    int fd;
    ratelmonitor_gpu_metrics_header_t header;
    ratelmonitor_gpu_metrics_layout_t layout;

} ratelmonitor_gpu_metrics_t;


typedef struct {
    uint8_t buffer[4096];
    ratelmonitor_gpu_metrics_t* gpu_metric;
    void* ext;
} ratelmonitor_gpu_metrics_snapshot_t;

#include "ratelmonitor.h"

/*
 * Offsets are for the packed AMDGPU gpu_metrics table. Unknown revisions
 * intentionally use the template values below until their layout is added.
 */
#define RATELMONITOR_GPU_METRICS_V1_0 \
    { \
		.power_offset  = 30, .power_size  = sizeof(uint16_t), \
		.energy_offset = 32, .energy_size = sizeof(uint32_t), \
		.power_is_average = 1 \
	}
#define RATELMONITOR_GPU_METRICS_V1_1 \
    { \
		.power_offset = 22,  .power_size = sizeof(uint16_t), \
		.energy_offset = 24, .energy_size = sizeof(uint64_t), \
		.power_is_average = 1 \
	}
#define RATELMONITOR_GPU_METRICS_V1_2 \
    { \
		.power_offset = 22,  .power_size = sizeof(uint16_t), \
		.energy_offset = 24, .energy_size = sizeof(uint64_t), \
		.power_is_average = 1 \
	}
#define RATELMONITOR_GPU_METRICS_V1_3 \
    { \
		.power_offset = 22,  .power_size = sizeof(uint16_t), \
		.energy_offset = 24, .energy_size = sizeof(uint64_t), \
		.power_is_average = 1 \
	}

/* v2.0 has no energy accumulator; its socket power follows the utilization fields. */
#define RATELMONITOR_GPU_METRICS_V2_0 \
    { \
		.power_offset = 40, .power_size = sizeof(uint16_t), \
		.energy_offset = 0, .energy_size = 0, \
		.power_is_average = 1 \
	}
#define RATELMONITOR_GPU_METRICS_V2_1 \
    { \
		.power_offset = 40, .power_size = sizeof(uint16_t), \
		.energy_offset = 0, .energy_size = 0, \
		.power_is_average = 1 \
	}
#define RATELMONITOR_GPU_METRICS_V2_2 \
    { \
		.power_offset = 40, .power_size = sizeof(uint16_t), \
		.energy_offset = 0, .energy_size = 0, \
		.power_is_average = 1 \
	}
#define RATELMONITOR_GPU_METRICS_V2_3 \
    { \
		.power_offset = 40, .power_size = sizeof(uint16_t), \
		.energy_offset = 0, .energy_size = 0, \
		.power_is_average = 1 \
	}

int ratelmonitor_read_gpu_metrics(ratelmonitor_gpu_metrics_t *gpu_metric, ratelmonitor_gpu_metrics_snapshot_t* snapshot);
int ratelmonitor_get_gpu_socket_power(ratelmonitor_gpu_metrics_snapshot_t* snapshot, uint64_t *power_watts);
int ratelmonitor_close_gpu_metrics(ratelmonitor_gpu_metrics_t *gpu_metric);
int ratelmonitor_open_gpu_metrics(ratelmonitor_gpu_metrics_t *gpu_metric, const char *device_path);

#endif // RATELMONITOR_GPU_METRICS_H
