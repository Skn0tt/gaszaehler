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
#include <clusters/CommodityMetering/Structs.h>

namespace esp_matter {
namespace cluster {
namespace commodity_metering {

typedef struct config {
    config() {}
} config_t;

cluster_t *create(endpoint_t *endpoint, config_t *config, uint8_t flags);

namespace attribute {
attribute_t *create_metered_quantity(cluster_t *cluster, const uint8_t *value, uint16_t length, uint16_t count);
attribute_t *create_metered_quantity_timestamp(cluster_t *cluster, nullable<uint32_t> value);
attribute_t *create_measurement_type(cluster_t *cluster, nullable<uint16_t> value);
} /* attribute */

} /* commodity_metering */
} /* cluster */

/**
 * TLV-encode a single MeteredQuantityStruct { tariffComponentIDs: [], quantity: int64 }
 * wrapped in a single-element list, suitable for attribute::update().
 *
 * @param quantity  The metered quantity value (e.g. gas pulse count).
 * @param buf       Output buffer for TLV bytes.
 * @param buf_size  Size of buf.
 * @param out_len   Receives the number of bytes written.
 * @return ESP_OK on success.
 */
esp_err_t commodity_metering_encode_quantity(int64_t quantity,
                                            uint8_t *buf, size_t buf_size,
                                            uint16_t *out_len);

} /* esp_matter */
