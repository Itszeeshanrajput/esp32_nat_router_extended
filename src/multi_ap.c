#include "multi_ap.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "router_globals.h"
#include "esp_wifi.h"

static const char *TAG = "Multi_AP";

esp_err_t multi_ap_load(saved_ap_t *ap_list)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        memset(ap_list, 0, sizeof(saved_ap_t) * MAX_SAVED_APS);
        return err;
    }

    size_t required_size = sizeof(saved_ap_t) * MAX_SAVED_APS;
    err = nvs_get_blob(nvs, "saved_aps", ap_list, &required_size);
    if (err != ESP_OK) {
        // Not found, initialize empty
        memset(ap_list, 0, sizeof(saved_ap_t) * MAX_SAVED_APS);
    }
    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t multi_ap_save(const saved_ap_t *ap_list)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    size_t size = sizeof(saved_ap_t) * MAX_SAVED_APS;
    err = nvs_set_blob(nvs, "saved_aps", ap_list, size);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t multi_ap_add(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) return ESP_ERR_INVALID_ARG;

    saved_ap_t ap_list[MAX_SAVED_APS];
    multi_ap_load(ap_list);

    // Check if it already exists to update password
    for (int i = 0; i < MAX_SAVED_APS; i++) {
        if (ap_list[i].valid && strcmp(ap_list[i].ssid, ssid) == 0) {
            strncpy(ap_list[i].password, password ? password : "", sizeof(ap_list[i].password) - 1);
            ESP_LOGI(TAG, "Updated password for saved AP: %s", ssid);
            return multi_ap_save(ap_list);
        }
    }

    // Find an empty slot
    for (int i = 0; i < MAX_SAVED_APS; i++) {
        if (!ap_list[i].valid) {
            strncpy(ap_list[i].ssid, ssid, sizeof(ap_list[i].ssid) - 1);
            strncpy(ap_list[i].password, password ? password : "", sizeof(ap_list[i].password) - 1);
            ap_list[i].valid = 1;
            ESP_LOGI(TAG, "Added new saved AP: %s at slot %d", ssid, i);
            return multi_ap_save(ap_list);
        }
    }

    // List full, replace first slot as a fallback
    ESP_LOGW(TAG, "Saved AP list full, replacing slot 0");
    strncpy(ap_list[0].ssid, ssid, sizeof(ap_list[0].ssid) - 1);
    strncpy(ap_list[0].password, password ? password : "", sizeof(ap_list[0].password) - 1);
    ap_list[0].valid = 1;
    return multi_ap_save(ap_list);
}

esp_err_t multi_ap_delete(const char *ssid)
{
    if (!ssid) return ESP_ERR_INVALID_ARG;

    saved_ap_t ap_list[MAX_SAVED_APS];
    multi_ap_load(ap_list);

    for (int i = 0; i < MAX_SAVED_APS; i++) {
        if (ap_list[i].valid && strcmp(ap_list[i].ssid, ssid) == 0) {
            ap_list[i].valid = 0;
            memset(&ap_list[i], 0, sizeof(saved_ap_t));
            ESP_LOGI(TAG, "Deleted saved AP: %s", ssid);
            return multi_ap_save(ap_list);
        }
    }
    return ESP_ERR_NOT_FOUND;
}

// Global variable representing if we are currently switching APs to prevent nested loops
static bool is_switching = false;

void multi_ap_check_and_switch(void)
{
    if (is_switching) return;
    is_switching = true;

    saved_ap_t ap_list[MAX_SAVED_APS];
    multi_ap_load(ap_list);

    // Count valid APs
    int valid_count = 0;
    for (int i = 0; i < MAX_SAVED_APS; i++) {
        if (ap_list[i].valid) valid_count++;
    }

    if (valid_count == 0) {
        is_switching = false;
        return;
    }

    ESP_LOGI(TAG, "Starting scan to match with saved APs...");

    // Scan for available networks
    uint16_t number = 20;
    wifi_ap_record_t ap_records[20];
    memset(ap_records, 0, sizeof(ap_records));

    // Stop active STA connections to perform scan safely
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
        is_switching = false;
        esp_wifi_connect(); // reconnect to whatever we had
        return;
    }

    err = esp_wifi_scan_get_ap_records(&number, ap_records);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(err));
        is_switching = false;
        esp_wifi_connect();
        return;
    }
    ESP_LOGI(TAG, "Scan complete. Found %d APs", number);

    // Find the strongest available saved AP
    int best_saved_index = -1;
    int strongest_rssi = -1000;

    for (int i = 0; i < number; i++) {
        char *scanned_ssid = (char *)ap_records[i].ssid;
        for (int j = 0; j < MAX_SAVED_APS; j++) {
            if (ap_list[j].valid && strcmp(ap_list[j].ssid, scanned_ssid) == 0) {
                if (ap_records[i].rssi > strongest_rssi) {
                    strongest_rssi = ap_records[i].rssi;
                    best_saved_index = j;
                }
            }
        }
    }

    if (best_saved_index != -1) {
        ESP_LOGI(TAG, "Matched strongest saved AP: %s (RSSI: %d)", ap_list[best_saved_index].ssid, strongest_rssi);

        // Write the selected AP parameters to the router STA configurations so that esp_wifi re-uses them
        nvs_handle_t nvs;
        if (nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_str(nvs, "ssid", ap_list[best_saved_index].ssid);
            nvs_set_str(nvs, "passwd", ap_list[best_saved_index].password);
            nvs_commit(nvs);
            nvs_close(nvs);
        }

        // Apply dynamically
        extern char *ssid;
        extern char *passwd;
        if (ssid) free(ssid);
        if (passwd) free(passwd);
        ssid = strdup(ap_list[best_saved_index].ssid);
        passwd = strdup(ap_list[best_saved_index].password);

        wifi_config_t wifi_config = {0};
        strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, passwd, sizeof(wifi_config.sta.password));
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);

        ESP_LOGI(TAG, "Reconnecting to selected AP SSID: %s", ssid);
        esp_wifi_connect();
    } else {
        ESP_LOGI(TAG, "No matched saved APs found in scan. Reconnecting with existing config.");
        esp_wifi_connect();
    }

    is_switching = false;
}

static void multi_ap_task(void *pvParameters)
{
    multi_ap_check_and_switch();
    vTaskDelete(NULL);
}

void multi_ap_check_and_switch_async(void)
{
    xTaskCreate(multi_ap_task, "multi_ap_task", 4096, NULL, 3, NULL);
}
