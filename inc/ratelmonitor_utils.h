#ifndef RATELMONITOR_UTILS_H
#define RATELMONITOR_UTILS_H

#define _GNU_SOURCE

#define DRM_CLASS_PATH "/sys/class/drm"
#define KFD_NODES_PATH "/sys/class/kfd/kfd/topology/nodes"
#define AMD_VENDOR_ID  "0x1002"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define IS_LINK(entry) ((entry)->d_type == DT_LNK || (entry)->d_type == DT_UNKNOWN)

static inline int for_each_entry_in_dir(const char *path, int type, bool (*func)(const char *name, void *arg), void *arg)
{
    DIR *dir = opendir(path);
    if (dir == NULL) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) 
    {
        if (entry->d_type != type) continue;
        if (entry->d_name[0] == '.') continue;

        bool cont = func(entry->d_name, arg);
        if (!cont) break;
    }
    closedir(dir);
    return 0;
}

static inline int make_path(char *dest, size_t ndest, const char *dir, const char *name)
{
    int wrt = snprintf(dest, ndest, "%s/%s", dir, name);
    if (wrt < 0 || (size_t)wrt >= ndest) return -1;
    return 0;
}


static inline int make_pathd(char *dest, size_t ndest, const char *dir, const uint64_t name)
{
    int wrt = snprintf(dest, ndest, "%s/%lu", dir, name);
    if (wrt < 0 || (size_t)wrt >= ndest) return -1;
    return 0;
}

static inline int parse_unsigned(const char *text, uint64_t *value)
{
    char *end = NULL;
    int base = 10;

    if (text == NULL || *text == '\0')
        return -1;

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        base = 16;

    uint64_t parsed = strtoull(text, &end, base);
    if (end == text || *end != '\0') return -2;
    *value = parsed;
    return 0;
}

static inline int read_sysfs_line(const char *path, char *buffer, size_t buffer_size)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) return -1;

    if (fgets(buffer, buffer_size, file) == NULL) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}


#endif // RATELMONITOR_UTILS_H
