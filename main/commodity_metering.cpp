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
#include <clusters/CommodityMetering/Structs.h>
#include <app/AttributeAccessInterface.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/reporting/reporting.h>
#include <platform/PlatformManager.h>
#include <inttypes.h>

static const char *TAG = "commodity_metering";

using namespace chip::app::Clusters;

/* Current metered quantity.  Written from the gas counter task (under CHIP
 * stack lock), read from the AAI Read() callback (also under CHIP stack lock). */
static int64_t  s_quantity       = 0;
static bool     s_quantity_valid = false;
static uint16_t s_endpoint_id    = 0xFFFF;

/*
 * CHIP AttributeAccessInterface for MeteredQuantity.
 *
 * EmberAttributeDataBuffer (CHIP's ZCL↔TLV bridge) has no case for
 * ZCL_ARRAY_ATTRIBUTE_TYPE: it always hits "Attribute type 0x48 not handled".
 * Registering an AAI for this cluster intercepts reads BEFORE that code path
 * is ever reached.  CHIP calls Read() directly and we write proper TLV via
 * AttributeValueEncoder — no ZCL array handling involved.
 *
 * For attributes other than MeteredQuantity, Read() returns CHIP_NO_ERROR
 * without calling encoder, which causes CHIP to fall through to the normal
 * esp-matter / ember path.
 */
class MeteredQuantityAAI : public chip::app::AttributeAccessInterface {
public:
    explicit MeteredQuantityAAI(chip::EndpointId endpoint_id)
        : AttributeAccessInterface(chip::MakeOptional(endpoint_id), CommodityMetering::Id) {}

    CHIP_ERROR Read(const chip::app::ConcreteReadAttributePath & path,
                    chip::app::AttributeValueEncoder & encoder) override
    {
        if (path.mAttributeId != CommodityMetering::Attributes::MeteredQuantity::Id) {
            return CHIP_NO_ERROR; /* let CHIP fall through to esp-matter for other attrs */
        }
        if (!s_quantity_valid) {
            return encoder.EncodeNull();
        }
        int64_t qty = s_quantity;
        return encoder.EncodeList([qty](const auto & listEncoder) -> CHIP_ERROR {
            CommodityMetering::Structs::MeteredQuantityStruct::Type entry;
            entry.tariffComponentIDs = chip::app::DataModel::List<const uint32_t>();
            entry.quantity = qty;
            return listEncoder.Encode(entry);
        });
    }
};

static MeteredQuantityAAI *s_aai = nullptr;

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

    s_endpoint_id = endpoint::get_id(endpoint);

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
        attribute::create_metered_quantity(cluster);
        attribute::create_metered_quantity_timestamp(cluster, nullable<uint32_t>());
        /* Attribute 0x0002: TariffUnit (Matter spec v1.5.1 §9.11.5.3).
           The CHIP SDK calls this MeasurementType — same attribute ID, renamed in spec v1.5. */
        attribute::create_measurement_type(cluster, nullable<uint16_t>());
    }

    /* Register our AttributeAccessInterface so CHIP routes MeteredQuantity reads
     * to Read() above, bypassing EmberAttributeDataBuffer entirely. */
    if (!s_aai) {
        s_aai = new MeteredQuantityAAI(s_endpoint_id);
        if (!chip::app::AttributeAccessInterfaceRegistry::Instance().Register(s_aai)) {
            ESP_LOGE(TAG, "Failed to register MeteredQuantity AttributeAccessInterface");
            delete s_aai;
            s_aai = nullptr;
        }
    }

    return cluster;
}

namespace attribute {

attribute_t *create_metered_quantity(cluster_t *cluster)
{
    /*
     * Placeholder value — the AAI (registered in create()) handles all reads,
     * so this stored value is never served over the wire.  It exists solely so
     * that CHIP's data model knows the attribute is present on this endpoint.
     * Use an empty nullable array to set the correct ZCL type (0x48) in the
     * attribute metadata.
     */
    return esp_matter::attribute::create(cluster,
        CommodityMetering::Attributes::MeteredQuantity::Id,
        ATTRIBUTE_FLAG_NULLABLE,
        esp_matter_array(nullptr, 0, 0));
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

void commodity_metering_set_quantity(int64_t quantity)
{
    s_quantity       = quantity;
    s_quantity_valid = true;

    /*
     * Notify CHIP that MeteredQuantity changed so active subscriptions receive
     * a report.  The AAI Read() callback will be called during report encoding
     * and will encode the updated s_quantity directly as TLV.
     *
     * Guard: MatterReportingAttributeChangeCallback must only be called while
     * the CHIP stack is locked.  The NVS seed call (before esp_matter::start)
     * is not under the lock, so we skip reporting there; the initial value is
     * read from s_quantity when the first subscription is established.
     */
    if (chip::DeviceLayer::PlatformMgr().IsChipStackLockedByCurrentThread()) {
        MatterReportingAttributeChangeCallback(
            s_endpoint_id,
            CommodityMetering::Id,
            CommodityMetering::Attributes::MeteredQuantity::Id);
    }
}

} /* esp_matter */
