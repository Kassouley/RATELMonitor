#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "ratelmonitor_utils.h"
#include "ratelmonitor_kfd.h"

int ratelmonitor_read_kfd_property(const char *properties_path, const char *property_name, uint64_t *out)
{
    FILE *file = fopen(properties_path, "r");
    if (file == NULL) return -1;

    char line[256];
    int res = -1;
    while (fgets(line, sizeof(line), file) != NULL) 
    {
        char key[64];
        char val[128];
        if (sscanf(line, " %63s %127s", key, val) != 2 || strcmp(key, property_name) != 0)
            continue;

        if (parse_unsigned(val, out) == 0) {
            res = 0;
            break;
        }
    }
    fclose(file);
    return res;
}

int ratelmonitor_read_kfd_location_id(const char *properties_path, uint32_t *location_id)
{
    uint64_t value;
    if (ratelmonitor_read_kfd_property(properties_path, "location_id", &value) != 0 ||
        value > UINT32_MAX)
        return -1;
    *location_id = (uint32_t)value;
    return 0;
}

int ratelmonitor_read_kfd_domain(const char *properties_path, uint32_t *domain)
{
    uint64_t value;
    if (ratelmonitor_read_kfd_property(properties_path, "domain", &value) != 0 ||
        value > UINT32_MAX)
        return -1;
    *domain = (uint32_t)value;
    return 0;
}

struct kfd_search_arg_t {
    uint32_t location_id;
    uint32_t *node_id;
};

static bool for_each_callback(const char *d_name, void *arg)
{
    uint32_t candidate_node;
    uint64_t candidate_value;
    if (parse_unsigned(d_name, &candidate_value) != 0 ||
        candidate_value > UINT32_MAX)
        return true;
    candidate_node = (uint32_t)candidate_value;
        
    struct kfd_search_arg_t *search = (struct kfd_search_arg_t *) arg;
    uint32_t location_id = search->location_id;
    char properties_path[PATH_MAX];
    char node_path[PATH_MAX];

    if (make_path(node_path, sizeof(node_path), KFD_NODES_PATH, d_name) != 0 ||
        make_path(properties_path, sizeof(properties_path), node_path, "properties") != 0)
        return true;

    uint32_t candidate_location;
    if (ratelmonitor_read_kfd_location_id(properties_path, &candidate_location) == 0 
        && candidate_location == location_id) 
    {
        *search->node_id = candidate_node;
        return false;
    }
    return true;
}

int ratelmonitor_find_kfd_node_id_by_location_id(uint32_t location_id, uint32_t *node_id)
{
    struct kfd_search_arg_t arg = {
        .location_id = location_id,
        .node_id = node_id
    };
    return for_each_entry_in_dir(KFD_NODES_PATH, DT_DIR, for_each_callback, &arg);
}

int ratelmonitor_get_kfd_node_path(uint32_t node_id, char *node_path)
{
    return make_pathd(node_path, PATH_MAX, KFD_NODES_PATH, (uint64_t) node_id);
}