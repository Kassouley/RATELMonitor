#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "ratelmonitor_utils.h"
#include "ratelmonitor_kfd.h"

ratelmonitor_status_t ratelmonitor_read_kfd_property(const char *properties_path, const char *property_name, uint64_t *out)
{
    if (properties_path == NULL || property_name == NULL || out == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    FILE *file = fopen(properties_path, "r");
    if (file == NULL) return RATELMONITOR_STATUS_OPEN_FAILED;

    char line[256];
    ratelmonitor_status_t res = RATELMONITOR_STATUS_NOT_FOUND;
    while (fgets(line, sizeof(line), file) != NULL) 
    {
        char key[64];
        char val[128];
        if (sscanf(line, " %63s %127s", key, val) != 2 || strcmp(key, property_name) != 0)
            continue;

        if (parse_unsigned(val, out) == RATELMONITOR_STATUS_SUCCESS) {
            res = RATELMONITOR_STATUS_SUCCESS;
            break;
        }
    }
    if (ferror(file))
        res = RATELMONITOR_STATUS_READ_FAILED;
    if (fclose(file) != 0 && res == RATELMONITOR_STATUS_SUCCESS)
        res = RATELMONITOR_STATUS_CLOSE_FAILED;
    return res;
}

ratelmonitor_status_t ratelmonitor_read_kfd_location_id(const char *properties_path, uint32_t *location_id)
{
    if (location_id == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;
    uint64_t value;
    if (ratelmonitor_read_kfd_property(properties_path, "location_id", &value) != RATELMONITOR_STATUS_SUCCESS ||
        value > UINT32_MAX)
        return RATELMONITOR_STATUS_PARSE_FAILED;
    *location_id = (uint32_t)value;
    return RATELMONITOR_STATUS_SUCCESS;
}

ratelmonitor_status_t ratelmonitor_read_kfd_domain(const char *properties_path, uint32_t *domain)
{
    if (domain == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;
    uint64_t value;
    if (ratelmonitor_read_kfd_property(properties_path, "domain", &value) != RATELMONITOR_STATUS_SUCCESS ||
        value > UINT32_MAX)
        return RATELMONITOR_STATUS_PARSE_FAILED;
    *domain = (uint32_t)value;
    return RATELMONITOR_STATUS_SUCCESS;
}

struct kfd_search_arg_t {
    uint32_t location_id;
    uint32_t *node_id;
};

static ratelmonitor_status_t for_each_callback(const char *d_name, void *arg)
{
    uint32_t candidate_node;
    uint64_t candidate_value;
    
    if (parse_unsigned(d_name, &candidate_value) != RATELMONITOR_STATUS_SUCCESS ||
        candidate_value > UINT32_MAX)
        return RATELMONITOR_STATUS_PARSE_FAILED;
    candidate_node = (uint32_t)candidate_value;
        
    struct kfd_search_arg_t *search = (struct kfd_search_arg_t *) arg;
    uint32_t location_id = search->location_id;

    char properties_path[PATH_MAX];
    char node_path[PATH_MAX];
    MAKE_PATH(node_path, sizeof(node_path), KFD_NODES_PATH, d_name, "%s");
    MAKE_PATH(properties_path, sizeof(properties_path), node_path, "properties", "%s");

    uint32_t candidate_location;
    if (ratelmonitor_read_kfd_location_id(properties_path, &candidate_location) == RATELMONITOR_STATUS_SUCCESS
        && candidate_location == location_id) 
    {
        *search->node_id = candidate_node;
        return RATELMONITOR_STATUS_SUCCESS;
    }
    return RATELMONITOR_STATUS_NOT_FOUND;
}

ratelmonitor_status_t ratelmonitor_find_kfd_node_id_by_location_id(uint32_t location_id, uint32_t *node_id)
{
    if (node_id == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;
    struct kfd_search_arg_t arg = {
        .location_id = location_id,
        .node_id = node_id
    };
    return for_each_entry_in_dir(KFD_NODES_PATH, DT_DIR, for_each_callback, &arg);
}

ratelmonitor_status_t ratelmonitor_get_kfd_node_path(uint32_t node_id, char *node_path)
{
    if (node_path == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    MAKE_PATH(node_path, PATH_MAX, KFD_NODES_PATH, node_id, "%u");
    return RATELMONITOR_STATUS_SUCCESS;
}