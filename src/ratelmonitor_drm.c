#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>

#include "ratelmonitor_utils.h"
#include "ratelmonitor_drm.h"
#include "ratelmonitor_gpu_metrics.h"

ratelmonitor_status_t ratelmonitor_parse_dbdf_from_sysfs(const char *sysfs_path, ratelmonitor_dbdf_t *dbdf)
{
    if (sysfs_path == NULL || dbdf == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;
    const char *last_slash = strrchr(sysfs_path, '/');
    const char *component = last_slash == NULL ? sysfs_path : last_slash + 1;

    char trailing;
    if (sscanf(component, "%4x:%2x:%2x.%1x%c", 
        &dbdf->domain, &dbdf->bus, &dbdf->device, &dbdf->function, &trailing) != 4 ||
        dbdf->domain > 0xffffU || dbdf->bus > 0xffU || dbdf->device > 0x1fU || dbdf->function > 7U) 
    {
        return RATELMONITOR_STATUS_PARSE_FAILED;
    }
    return RATELMONITOR_STATUS_SUCCESS;
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
    if (device_path == NULL) return false;

    uint8_t vendor[4096] = {0};
    size_t readed;

    if (ratelmonitor_gpu_sysfs_orc(device_path, "vendor", vendor, &readed) != RATELMONITOR_STATUS_SUCCESS)
        return false;

    return memcmp(vendor, AMD_VENDOR_ID, strlen(AMD_VENDOR_ID)) == 0;
}


ratelmonitor_status_t ratelmonitor_get_device_path(const char *card_name, char device_path[PATH_MAX])
{
    if (card_name == NULL) return RATELMONITOR_STATUS_INVALID_PTR;

    char card_path[PATH_MAX];

    MAKE_PATH(card_path, PATH_MAX, DRM_CLASS_PATH, card_name, "%s");
    MAKE_PATH(device_path, PATH_MAX, card_path, "device", "%s");

    return RATELMONITOR_STATUS_SUCCESS;
}


ratelmonitor_status_t ratelmonitor_get_canonical_device_path(char **canonical, char device_path[PATH_MAX])
{
    if (canonical == NULL || device_path == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;
    *canonical = realpath(device_path, NULL);
    if (*canonical == NULL) return RATELMONITOR_STATUS_OUT_OF_MEMORY;
    return RATELMONITOR_STATUS_SUCCESS;
}
