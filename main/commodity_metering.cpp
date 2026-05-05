/*
 * SPDX-FileCopyrightText: 2025 Simon Knott
 *
 * SPDX-License-Identifier: MIT
 *
 * Local CommodityMetering cluster helper for esp_matter.
 *
 * NOTE: This is a stopgap until esp_matter adds built-in CommodityMetering support
 * (like it has for electrical_energy_measurement, flow_measurement, etc.).
 * Once upstream esp_matter ships cluster::commodity_metering, delete this file
 * and commodity_metering.h, then switch to the upstream API.
 */

#include "commodity_metering.h"

#include <esp_log.h>
#include <esp_matter.h>
#include <clusters/CommodityMetering/ClusterId.h>
#include <clusters/CommodityMetering/AttributeIds.h>

static const char *TAG = "commodity_metering";

using namespace chip::app::Clusters;

namespace esp_matter {
namespace cluster {
namespace commodity_metering {

static const uint16_t cluster_revision = 1;

cluster_t *create(endpoint_t *endpoint, config_t *config, uint8_t flags)
{
    cluster_t *cluster = cluster::create(endpoint, CommodityMetering::Id, flags);
    if (!cluster) {
        ESP_LOGE(TAG, "Could not create cluster 0x%08" PRIX32, CommodityMetering::Id);
        return NULL;
    }

    if (flags & CLUSTER_FLAG_SERVER) {
        if (!config) {
            ESP_LOGE(TAG, "Config cannot be NULL");
            cluster::destroy(cluster);
            return NULL;
        }

        /* Global attributes */
        global::attribute::create_cluster_revision(cluster, cluster_revision);
        global::attribute::create_feature_map(cluster, 0);

        /* Cluster-specific attributes */
        attribute::create_metered_quantity(cluster, nullable<int64_t>(0));
        attribute::create_metered_quantity_timestamp(cluster, nullable<uint32_t>());
        /* TODO: MeasurementTypeEnum has no gas/water/thermal values yet (only electrical).
           Update once the spec adds a proper commodity type. */
        attribute::create_measurement_type(cluster, nullable<uint16_t>(0x00 /* kUnspecified */));
    }

    return cluster;
}

namespace attribute {

attribute_t *create_metered_quantity(cluster_t *cluster, nullable<int64_t> value)
{
    return esp_matter::attribute::create(cluster,
        CommodityMetering::Attributes::MeteredQuantity::Id,
        ATTRIBUTE_FLAG_NULLABLE,
        esp_matter_nullable_int64(value));
}

attribute_t *create_metered_quantity_timestamp(cluster_t *cluster, nullable<uint32_t> value)
{
    return esp_matter::attribute::create(cluster,
        CommodityMetering::Attributes::MeteredQuantityTimestamp::Id,
        ATTRIBUTE_FLAG_NULLABLE,
        esp_matter_nullable_uint32(value));
}

attribute_t *create_measurement_type(cluster_t *cluster, nullable<uint16_t> value)
{
    return esp_matter::attribute::create(cluster,
        CommodityMetering::Attributes::MeasurementType::Id,
        ATTRIBUTE_FLAG_NULLABLE,
        esp_matter_nullable_enum16(value));
}

} /* attribute */
} /* commodity_metering */
} /* cluster */

} /* esp_matter */
