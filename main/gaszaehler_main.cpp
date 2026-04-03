/*
 * SPDX-FileCopyrightText: 2025 Simon Knott
 *
 * SPDX-License-Identifier: MIT
 */

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_matter.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#ifdef CONFIG_ENABLE_ICD_SERVER
#include <app/icd/server/ICDNotifier.h>
#endif

static const char *TAG = "gaszaehler";

/* Custom gas-meter cluster in the vendor-specific test range */
static constexpr uint32_t CLUSTER_ID_GAS_METER = 0xFFF10000;
static constexpr uint32_t ATTR_ID_GAS_COUNT    = 0x00000000;

using namespace esp_matter;
using namespace chip::app::Clusters;

static uint16_t gas_endpoint_id = 0;
static uint16_t power_source_endpoint_id = 0;
static uint32_t gas_count = 0;
static TaskHandle_t counter_task_handle = NULL;

/* ── NVS persistence ──────────────────────────────────────────── */

static esp_err_t nvs_load_gas_count()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("gaszaehler", NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) { gas_count = 0; return ESP_OK; }
    if (err != ESP_OK) return err;
    err = nvs_get_u32(h, "count", &gas_count);
    if (err == ESP_ERR_NVS_NOT_FOUND) { gas_count = 0; err = ESP_OK; }
    nvs_close(h);
    return err;
}

static esp_err_t nvs_save_gas_count()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("gaszaehler", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u32(h, "count", gas_count);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* ── GPIO ISR ─────────────────────────────────────────────────── */

static void IRAM_ATTR reed_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(counter_task_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ── Counter task (debounce → persist → report to Matter) ─────── */

static void gas_counter_task(void *)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        gas_count++;
        ESP_LOGI(TAG, "Pulse! gas_count=%lu", (unsigned long)gas_count);

        nvs_save_gas_count();

        esp_matter_attr_val_t val = esp_matter_uint32(gas_count);
        attribute::update(gas_endpoint_id, CLUSTER_ID_GAS_METER,
                          ATTR_ID_GAS_COUNT, &val);

#ifdef CONFIG_ENABLE_ICD_SERVER
        /* Wake ICD into active mode so the report is sent immediately */
        chip::app::ICDNotifier::GetInstance().NotifyNetworkActivityNotification();
#endif

        /* debounce: ignore further edges for 200 ms */
        vTaskDelay(pdMS_TO_TICKS(200));
        ulTaskNotifyTake(pdTRUE, 0); /* drain anything that arrived */
    }
}

/* ── Battery voltage monitoring (GPIO 0, voltage divider ×2) ── */

#define BAT_ADC_CHANNEL ADC_CHANNEL_0  /* GPIO 0 on ESP32-C6 */

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

static esp_err_t battery_adc_init()
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &adc_handle);
    if (err != ESP_OK) return err;

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) return err;

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = BAT_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &adc_cali_handle);
    return err;
}

static int battery_read_millivolts()
{
    int raw = 0, mv = 0;
    adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw);
    adc_cali_raw_to_voltage(adc_cali_handle, raw, &mv);
    return mv * 2;  /* voltage divider on board halves battery voltage */
}

static void battery_monitor_task(void *)
{
    for (;;) {
        int bat_mv = battery_read_millivolts();
        ESP_LOGI(TAG, "Battery: %d mV", bat_mv);

        /* Update Power Source cluster attributes */
        esp_matter_attr_val_t volt_val = esp_matter_nullable_uint32((uint32_t)bat_mv);
        attribute::update(power_source_endpoint_id, PowerSource::Id,
                          PowerSource::Attributes::BatVoltage::Id, &volt_val);

        /* Rough Li-ion percentage: 3300 mV = 0 %, 4200 mV = 100 %
           Matter uses half-percent units: 0–200 */
        int pct = (bat_mv - 3300) * 200 / (4200 - 3300);
        if (pct < 0) pct = 0;
        if (pct > 200) pct = 200;
        esp_matter_attr_val_t pct_val = esp_matter_nullable_uint8((uint8_t)pct);
        attribute::update(power_source_endpoint_id, PowerSource::Id,
                          PowerSource::Attributes::BatPercentRemaining::Id, &pct_val);

        /* Update charge level: OK / Warning / Critical */
        uint8_t charge_level;
        if (pct > 40)       charge_level = 0; /* OK */
        else if (pct > 20)  charge_level = 1; /* Warning */
        else                charge_level = 2; /* Critical */
        esp_matter_attr_val_t lvl_val = esp_matter_enum8(charge_level);
        attribute::update(power_source_endpoint_id, PowerSource::Id,
                          PowerSource::Attributes::BatChargeLevel::Id, &lvl_val);

        vTaskDelay(pdMS_TO_TICKS(60000));  /* read every 60 s */
    }
}

/* ── Matter callbacks ─────────────────────────────────────────── */

static void app_event_cb(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg)
{
    if (event->Type == chip::DeviceLayer::DeviceEventType::kCommissioningComplete) {
        ESP_LOGI(TAG, "Commissioning complete");
    }
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id,
                                         uint32_t cluster_id, uint32_t attribute_id,
                                         esp_matter_attr_val_t *val, void *priv_data)
{
    return ESP_OK;
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id,
                                       uint8_t effect_id, uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification: type=%u, effect=%u", type, effect_id);
    return ESP_OK;
}

/* ── main ─────────────────────────────────────────────────────── */

extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    /* NVS */
    nvs_flash_init();
    nvs_load_gas_count();
    ESP_LOGI(TAG, "Restored gas_count=%lu", (unsigned long)gas_count);

    /* Matter node */
    node::config_t node_cfg;
    node_t *node = node::create(&node_cfg, app_attribute_update_cb, app_identification_cb);
    if (!node) { ESP_LOGE(TAG, "node::create failed"); return; }

    /* Endpoint (generic, hosts our custom cluster) */
    endpoint_t *ep = endpoint::create(node, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "endpoint::create failed"); return; }
    gas_endpoint_id = endpoint::get_id(ep);

    /* Descriptor cluster (required on every endpoint) */
    cluster::descriptor::config_t desc_cfg;
    cluster::descriptor::create(ep, &desc_cfg, CLUSTER_FLAG_SERVER);

    /* Custom gas-meter cluster */
    cluster_t *cl = cluster::create(ep, CLUSTER_ID_GAS_METER, CLUSTER_FLAG_SERVER);
    if (!cl) { ESP_LOGE(TAG, "cluster::create failed"); return; }

    cluster::global::attribute::create_cluster_revision(cl, 1);
    cluster::global::attribute::create_feature_map(cl, 0);

    /* gas_count attribute – seeded from NVS */
    attribute::create(cl, ATTR_ID_GAS_COUNT, ATTRIBUTE_FLAG_NONE,
                      esp_matter_uint32(gas_count));

    ESP_LOGI(TAG, "Gas meter endpoint %u", gas_endpoint_id);

    /* Power Source endpoint (standard Matter cluster 0x002F) */
    endpoint_t *ps_ep = endpoint::create(node, ENDPOINT_FLAG_NONE, NULL);
    if (!ps_ep) { ESP_LOGE(TAG, "ps endpoint::create failed"); return; }
    power_source_endpoint_id = endpoint::get_id(ps_ep);

    cluster::descriptor::config_t ps_desc_cfg;
    cluster::descriptor::create(ps_ep, &ps_desc_cfg, CLUSTER_FLAG_SERVER);

    cluster::power_source::config_t ps_cfg;
    ps_cfg.status = 1;  /* Active */
    ps_cfg.order  = 0;
    snprintf(ps_cfg.description, sizeof(ps_cfg.description), "Battery");
    ps_cfg.features.battery.bat_charge_level = 0;        /* OK */
    ps_cfg.features.battery.bat_replacement_needed = false;
    ps_cfg.features.battery.bat_replaceability = 2;      /* NotReplaceable */
    ps_cfg.features.rechargeable.bat_charge_state = 0;
    ps_cfg.features.rechargeable.bat_functional_while_charging = true;
    ps_cfg.feature_flags = cluster::power_source::feature::battery::get_id()
                         | cluster::power_source::feature::rechargeable::get_id();

    cluster_t *ps_cl = cluster::power_source::create(ps_ep, &ps_cfg, CLUSTER_FLAG_SERVER);
    if (!ps_cl) { ESP_LOGE(TAG, "power_source::create failed"); return; }

    /* Add optional battery attributes for voltage, percentage & capacity */
    cluster::power_source::attribute::create_bat_voltage(ps_cl, nullable<uint32_t>(0), nullable<uint32_t>(0), nullable<uint32_t>(0));
    cluster::power_source::attribute::create_bat_percent_remaining(ps_cl, nullable<uint8_t>(0), nullable<uint8_t>(0), nullable<uint8_t>(200));
    cluster::power_source::attribute::create_bat_capacity(ps_cl, 2500, 0, 0);

    ESP_LOGI(TAG, "Power source endpoint %u", power_source_endpoint_id);

    /* OpenThread */
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    esp_openthread_platform_config_t ot_cfg = {
        .radio_config = {
            .radio_mode = RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = HOST_CONNECTION_MODE_NONE,
        },
        .port_config = {
            .storage_partition_name = "nvs",
            .netif_queue_size = 10,
            .task_queue_size = 10,
        },
    };
    set_openthread_platform_config(&ot_cfg);
#endif

    /* Start Matter */
    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_matter::start: %d", err); return; }

    /* Counter task + reed-sensor GPIO */
    xTaskCreate(gas_counter_task, "gas_cnt", 4096, NULL, 5, &counter_task_handle);

    gpio_num_t pin = (gpio_num_t)CONFIG_REED_SENSOR_GPIO;
    gpio_config_t io = {
        .pin_bit_mask  = 1ULL << pin,
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_ENABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(pin, reed_isr, NULL);

    /* Battery ADC + monitor task */
    err = battery_adc_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "battery_adc_init: %d", err); }
    else { xTaskCreate(battery_monitor_task, "bat_mon", 2048, NULL, 3, NULL); }

    ESP_LOGI(TAG, "Ready – reed sensor on GPIO %d", (int)pin);
}
