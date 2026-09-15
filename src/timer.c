#include "timer.h"
#include <esp_wifi.h>
#include <esp_log.h>
#include "esp_timer.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "multi_ap.h"

static const char *TAG = "Timer";

#define REFRESH_TIMER_PERIOD 5 * 60000000

esp_timer_handle_t restart_timer, refresh_timer;

extern bool ap_connect;

static int last_ping_latency_ms = -1;
static int ping_fail_count = 0;

int watchdog_get_last_latency(void)
{
    return last_ping_latency_ms;
}

int watchdog_get_fail_count(void)
{
    return ping_fail_count;
}

static void watchdog_task(void *pvParameters)
{
    ESP_LOGI(TAG, "24/7 Smart Ping Watchdog task started.");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Check every 30 seconds

        if (!ap_connect)
        {
            last_ping_latency_ms = -1;
            continue;
        }

        uint32_t start_time = xTaskGetTickCount();
        esp_http_client_config_t config = {
            .url = "http://connectivitycheck.gstatic.com/generate_204",
            .timeout_ms = 4000,
            .disable_auto_redirect = true
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_err_t err = esp_http_client_perform(client);
        uint32_t duration = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;

        if (err == ESP_OK && (esp_http_client_get_status_code(client) == 204 || esp_http_client_get_status_code(client) == 200))
        {
            last_ping_latency_ms = (int)duration;
            ping_fail_count = 0;
            ESP_LOGD(TAG, "Watchdog ping OK. Latency: %d ms", last_ping_latency_ms);
        }
        else
        {
            ping_fail_count++;
            last_ping_latency_ms = -1;
            ESP_LOGW(TAG, "Watchdog ping failed (%d/3). Error: %s", ping_fail_count, esp_err_to_name(err));

            if (ping_fail_count >= 3)
            {
                ESP_LOGE(TAG, "Internet connection unresponsive (3 consecutive fails). Triggering connection auto-recovery...");
                ping_fail_count = 0;
                ap_connect = false;
                multi_ap_check_and_switch_async();
            }
        }
        esp_http_client_cleanup(client);
    }
}

void initializePingWatchdog(void)
{
    xTaskCreate(watchdog_task, "ping_watchdog", 4096, NULL, 4, NULL);
}

static void restart_timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Restarting now...");
    esp_restart();
}
static void refresh_timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Starting web call for keep alive");
    esp_http_client_config_t config = {
        .url = "https://www.startpage.com/",
        .method = HTTP_METHOD_HEAD,
        .disable_auto_redirect = true};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld ", esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

esp_timer_create_args_t restart_timer_args = {
    .callback = &restart_timer_callback,
    /* argument specified here will be passed to timer callback function */
    .arg = (void *)0,
    .name = "restart_timer"};

esp_timer_create_args_t refresh_timer_args = {
    .callback = &refresh_timer_callback,
    .arg = (void *)0,
    .name = "refresh_timer"};

void initializeRestartTimer()
{
    esp_timer_create(&restart_timer_args, &restart_timer);
}
void initializeKeepAliveTimer()
{
    esp_timer_create(&refresh_timer_args, &refresh_timer);
    esp_timer_start_periodic(refresh_timer, REFRESH_TIMER_PERIOD);
}

void restartByTimer()
{
    esp_timer_start_once(restart_timer, 500000);
}

void restartByTimerinS(int seconds)
{
    esp_timer_start_once(restart_timer, seconds * 1000000);
}
