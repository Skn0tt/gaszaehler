/*
 * SPDX-FileCopyrightText: 2025 Simon Knott
 *
 * SPDX-License-Identifier: MIT
 *
 * Local CommodityMetering cluster helper for esp_matter.
 * Follows the same patterns as esp_matter's built-in cluster helpers
 * (e.g. electrical_energy_measurement) so this can be contributed upstream.
 *
 * NOTE: This is a stopgap until esp_matter adds built-in CommodityMetering support.
 * Once upstream esp_matter ships cluster::commodity_metering, delete this file
 * and commodity_metering.cpp, then switch to the upstream API.
 */

#pragma once

#include <esp_matter.h>
#include <clusters/CommodityMetering/ClusterId.h>
#include <clusters/CommodityMetering/AttributeIds.h>

namespace esp_matter {
namespace cluster {
namespace commodity_metering {

typedef struct config {
    config() {}
} config_t;

cluster_t *create(endpoint_t *endpoint, config_t *config, uint8_t flags);

namespace attribute {
attribute_t *create_metered_quantity(cluster_t *cluster, nullable<int64_t> value);
attribute_t *create_metered_quantity_timestamp(cluster_t *cluster, nullable<uint32_t> value);
attribute_t *create_measurement_type(cluster_t *cluster, nullable<uint16_t> value);
} /* attribute */

} /* commodity_metering */
} /* cluster */

} /* esp_matter */
