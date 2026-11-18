#ifndef RATELMONITOR_KFD_H
#define RATELMONITOR_KFD_H

#include <stdint.h>
#include "ratelmonitor_utils.h"

ratelmonitor_status_t ratelmonitor_read_kfd_property(const char *properties_path, const char *property_name, uint64_t *out);

ratelmonitor_status_t ratelmonitor_read_kfd_location_id(const char *properties_path, uint32_t *location_id);

ratelmonitor_status_t ratelmonitor_read_kfd_domain(const char *properties_path, uint32_t *domain);

ratelmonitor_status_t ratelmonitor_find_kfd_node_id_by_location_id(uint32_t location_id, uint32_t *node_id);

ratelmonitor_status_t ratelmonitor_get_kfd_node_path(uint32_t node_id, char *node_path);

#endif // RATELMONITOR_KFD_H