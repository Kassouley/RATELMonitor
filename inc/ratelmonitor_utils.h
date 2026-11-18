#ifndef RATELMONITOR_UTILS_H
#define RATELMONITOR_UTILS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define DRM_CLASS_PATH "/sys/class/drm"
#define KFD_NODES_PATH "/sys/class/kfd/kfd/topology/nodes"
#define AMD_VENDOR_ID  "0x1002"

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define IS_LINK(entry) ((entry)->d_type == DT_LNK || (entry)->d_type == DT_UNKNOWN)


typedef enum {
    RATELMONITOR_STATUS_SUCCESS,
    RATELMONITOR_STATUS_ERROR,
    RATELMONITOR_STATUS_OUT_OF_MEMORY,
    RATELMONITOR_STATUS_INVALID_PTR,
    RATELMONITOR_STATUS_INVALID_VALUE,
    RATELMONITOR_STATUS_OPEN_FAILED,
    RATELMONITOR_STATUS_READ_FAILED,
    RATELMONITOR_STATUS_CLOSE_FAILED,
    RATELMONITOR_STATUS_WRITE_FAILED,
    RATELMONITOR_STATUS_PARSE_FAILED,
    RATELMONITOR_STATUS_NOT_FOUND,
    RATELMONITOR_STATUS_THREAD_FAILED,
    RATELMONITOR_STATUS_NOT_INITIALIZED,
    RATELMONITOR_STATUS_UNLINK_FAILED,
    RATELMONITOR_STATUS_CONTINUE,
    RATELMONITOR_STATUS_STOP,
} ratelmonitor_status_t;


#define MAKE_PATH(DEST, NDEST, DIR, NAME, FMT) do { \
    int wrt = snprintf((DEST), (NDEST), "%s/" FMT, (DIR), (NAME)); \
    if (wrt < 0 || (size_t)wrt >= (NDEST)) { \
        fprintf(stderr, "Error: Failed to create path %s/" FMT "\n", (DEST), (NAME)); \
        exit(EXIT_FAILURE); \
    } \
} while(0)


static inline ratelmonitor_status_t for_each_entry_in_dir(const char *path, int type, ratelmonitor_status_t (*func)(const char *name, void *arg), void *arg)
{
    if (path == NULL || func == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    DIR *dir = opendir(path);
    if (dir == NULL) return RATELMONITOR_STATUS_OPEN_FAILED;

    ratelmonitor_status_t status = RATELMONITOR_STATUS_CONTINUE;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) 
    {
        if (entry->d_type != type) continue;
        if (entry->d_name[0] == '.') continue;

        status = func(entry->d_name, arg);
        if (status != RATELMONITOR_STATUS_CONTINUE) {
            break;
        }
    }
    if (status == RATELMONITOR_STATUS_CONTINUE || 
        status == RATELMONITOR_STATUS_STOP) 
    {
        status = RATELMONITOR_STATUS_SUCCESS;
    }
    if (closedir(dir) != 0)
        return RATELMONITOR_STATUS_CLOSE_FAILED;
    return status;
}


static inline ratelmonitor_status_t parse_unsigned(const char *text, uint64_t *value)
{
    char *end = NULL;
    int base = 10;

    if (text == NULL || value == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;
    if (*text == '\0')
        return RATELMONITOR_STATUS_INVALID_VALUE;

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        base = 16;

    uint64_t parsed = strtoull(text, &end, base);
    if (end == text || *end != '\0') return RATELMONITOR_STATUS_PARSE_FAILED;
    *value = parsed;
    return RATELMONITOR_STATUS_SUCCESS;
}


static inline const char *ratelmonitor_status_string(ratelmonitor_status_t status)
{
    switch (status) {
    case RATELMONITOR_STATUS_SUCCESS: return "success";
    case RATELMONITOR_STATUS_OUT_OF_MEMORY: return "out of memory";
    case RATELMONITOR_STATUS_INVALID_PTR: return "invalid pointer";
    case RATELMONITOR_STATUS_ERROR: return "generic error";
    case RATELMONITOR_STATUS_UNLINK_FAILED: return "unlink failed";
    case RATELMONITOR_STATUS_INVALID_VALUE: return "invalid value";
    case RATELMONITOR_STATUS_OPEN_FAILED: return "open failed";
    case RATELMONITOR_STATUS_READ_FAILED: return "read failed";
    case RATELMONITOR_STATUS_CLOSE_FAILED: return "close failed";
    case RATELMONITOR_STATUS_WRITE_FAILED: return "write failed";
    case RATELMONITOR_STATUS_PARSE_FAILED: return "parse failed";
    case RATELMONITOR_STATUS_NOT_FOUND: return "not found";
    case RATELMONITOR_STATUS_THREAD_FAILED: return "thread operation failed";
    case RATELMONITOR_STATUS_NOT_INITIALIZED: return "not initialized";
    case RATELMONITOR_STATUS_CONTINUE: return "continue";
    case RATELMONITOR_STATUS_STOP: return "stop";
    }
    return "unknown status";
}

#endif // RATELMONITOR_UTILS_H
