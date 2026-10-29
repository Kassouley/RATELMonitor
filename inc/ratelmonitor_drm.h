#ifndef RATELMONITOR_DRM_H
#define RATELMONITOR_DRM_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t domain;
    uint32_t bus;
    uint32_t device;
    uint32_t function;
} ratelmonitor_dbdf_t;

int ratelmonitor_parse_dbdf_from_sysfs(const char *sysfs_path, ratelmonitor_dbdf_t *dbdf);

uint32_t ratelmonitor_get_location_id_from_dbdf(const ratelmonitor_dbdf_t *dbdf);

bool ratelmonitor_is_card_name(const char *name);

bool ratelmonitor_is_amd_gpu(const char *device_path);

int ratelmonitor_get_canonical_device_path(char **canonical, char device_path[PATH_MAX]);

int ratelmonitor_get_device_path(const char *card_name, char device_path[PATH_MAX]);

#endif // RATELMONITOR_DRM_H