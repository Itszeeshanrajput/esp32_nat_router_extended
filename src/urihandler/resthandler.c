#include "handler.h"
#include "router_globals.h"
#include "multi_ap.h"

static const char *TAG = "RestHandler";

const char *JSON_TEMPLATE = "{\"clients\": %d,\"strength\": %s,\"text\": \"%s\"}";

esp_err_t rest_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }
    httpd_resp_set_type(req, "application/json");

    char *db = NULL;
    char *textColor = NULL;
    fillInfoData(&db, &textColor);

    size_t size = strlen(JSON_TEMPLATE) + 5 + strlen(db) + strlen(textColor);
    char *json = malloc(size);
    sprintf(json, JSON_TEMPLATE, getConnectCount(), db, textColor);
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    ESP_LOGD(TAG, "JSON-Response: %s", json);
    free(json);
    free(db);
    return ret;
}

esp_err_t saved_aps_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }
    httpd_resp_set_type(req, "application/json");

    saved_ap_t ap_list[MAX_SAVED_APS];
    multi_ap_load(ap_list);

    char *buf = malloc(2048);
    if (!buf) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    buf[0] = '\0';
    strncat(buf, "[", 2047);
    bool first = true;
    for (int i = 0; i < MAX_SAVED_APS; i++) {
        if (ap_list[i].valid) {
            char item[256];
            snprintf(item, sizeof(item), "%s{\"ssid\": \"%.32s\", \"password\": \"%.64s\"}",
                     first ? "" : ", ", ap_list[i].ssid, ap_list[i].password);

            size_t current_len = strlen(buf);
            size_t remaining = 2048 - current_len - 1;
            strncat(buf, item, remaining);
            first = false;
        }
    }
    size_t current_len = strlen(buf);
    size_t remaining = 2048 - current_len - 1;
    strncat(buf, "]", remaining);

    esp_err_t ret = httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    free(buf);
    return ret;
}

esp_err_t add_ap_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }
    httpd_resp_set_type(req, "application/json");

    char query[256];
    char ssid_buf[32] = {0};
    char pass_buf[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        readUrlParameterIntoBuffer(query, "ssid", ssid_buf, sizeof(ssid_buf));
        readUrlParameterIntoBuffer(query, "password", pass_buf, sizeof(pass_buf));
    }

    if (strlen(ssid_buf) > 0) {
        multi_ap_add(ssid_buf, pass_buf);
        httpd_resp_send(req, "{\"status\": \"ok\"}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID parameter");
    }
    return ESP_OK;
}

esp_err_t delete_ap_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }
    httpd_resp_set_type(req, "application/json");

    char query[256];
    char ssid_buf[32] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        readUrlParameterIntoBuffer(query, "ssid", ssid_buf, sizeof(ssid_buf));
    }

    if (strlen(ssid_buf) > 0) {
        multi_ap_delete(ssid_buf);
        httpd_resp_send(req, "{\"status\": \"ok\"}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID parameter");
    }
    return ESP_OK;
}
