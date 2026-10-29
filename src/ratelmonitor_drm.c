#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "ratelmonitor_utils.h"
#include "ratelmonitor_drm.h"
#include "ratelmonitor_gpu_metrics.h"

int ratelmonitor_parse_dbdf_from_sysfs(const char *sysfs_path, ratelmonitor_dbdf_t *dbdf)
{
    const char *last_slash = strrchr(sysfs_path, '/');
    const char *component = last_slash == NULL ? sysfs_path : last_slash + 1;

    char trailing;
    if (sscanf(component, "%4x:%2x:%2x.%1x%c", 
        &dbdf->domain, &dbdf->bus, &dbdf->device, &dbdf->function, &trailing) != 4 ||
        dbdf->domain > 0xffffU || dbdf->bus > 0xffU || dbdf->device > 0x1fU || dbdf->function > 7U) 
    {
        return -1;
    }
    return 0;
}


uint32_t ratelmonitor_get_location_id_from_dbdf(const ratelmonitor_dbdf_t *dbdf)
{
    return (uint32_t)((dbdf->bus << 8) | (dbdf->device << 3) | dbdf->function);
}


bool ratelmonitor_is_card_name(const char *name)
{
    if (strncmp(name, "card", 4) != 0 || name[4] == '\0')
        return false;
    const unsigned char *cursor = (const unsigned char *)name + 4;
    for (; *cursor != '\0'; cursor++) {
        if (!isdigit(*cursor)) return false;
    }
    return true;
}


bool ratelmonitor_is_amd_gpu(const char *device_path)
{
    char vendor_path[PATH_MAX];
    char vendor[32];

    if (make_path(vendor_path, sizeof(vendor_path), device_path, "vendor")) return false;
    if (read_sysfs_line(vendor_path, vendor, sizeof(vendor))) return false;

    return memcmp(vendor, AMD_VENDOR_ID, strlen(AMD_VENDOR_ID)) == 0 ? true : false;
}


int ratelmonitor_get_device_path(const char *card_name, char device_path[PATH_MAX])
{
    char card_path[PATH_MAX];
    if (make_path(card_path, PATH_MAX, DRM_CLASS_PATH, card_name) != 0) return -1;
    if (make_path(device_path, PATH_MAX, card_path, "device") != 0) return -1;
    return 0;
}


int ratelmonitor_get_canonical_device_path(char **canonical, char device_path[PATH_MAX])
{
    *canonical = realpath(device_path, NULL);
    if (*canonical == NULL) return -1;
    return 0;
}
