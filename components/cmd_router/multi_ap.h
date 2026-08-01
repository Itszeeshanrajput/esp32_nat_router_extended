#ifndef MULTI_AP_H
#define MULTI_AP_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define MAX_SAVED_APS 5

typedef struct {
    char ssid[32];
    char password[64];
    uint8_t valid;
} saved_ap_t;

// Load saved APs list from NVS
esp_err_t multi_ap_load(saved_ap_t *ap_list);

// Save saved APs list to NVS
esp_err_t multi_ap_save(const saved_ap_t *ap_list);

// Add a new AP network to the saved list
esp_err_t multi_ap_add(const char *ssid, const char *password);

// Delete an AP network from the saved list by SSID
esp_err_t multi_ap_delete(const char *ssid);

// Automatically check and switch connection to any available saved AP
void multi_ap_check_and_switch(void);

// Non-blocking asynchronous wrapper to avoid deadlocking the main event loop
void multi_ap_check_and_switch_async(void);

// Manually switch connection to the saved AP in the specified slot
esp_err_t multi_ap_switch_to(int slot_index);

#endif // MULTI_AP_H
