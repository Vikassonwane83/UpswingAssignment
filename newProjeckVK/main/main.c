#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <ctype.h>
#include "mqtt.h"
#define CONFIG_ESP_WIFI_SSID "UPSWING"
#define CONFIG_ESP_WIFI_PASSWORD "upswing@123"
#define CONFIG_ESP_MAX_STA_CONN 4  
static const char *TAG = "SoftAP";

static const char *html_form = 
    "<html>"
    "<head><title>ESP32 JSON Receiver</title></head>"
    "<body>"
    "<h1>ESP32 JSON Receiver</h1>"
    "<form method=\"post\" action=\"/json\">"
    "JSON Data:<br>"
    "<textarea name=\"json_data\" rows=\"5\" cols=\"30\"></textarea><br>"
    "<input type=\"submit\" value=\"Send\">"
    "</form>"
    "</body>"
    "</html>";

// Function Prototypes
esp_err_t http_server_init(void);
esp_err_t http_get_handler(httpd_req_t *req);
esp_err_t http_post_handler(httpd_req_t *req);

void app_main(void) {
    esp_err_t ret = nvs_flash_init();  //init the storage system
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", CONFIG_ESP_WIFI_SSID);
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = { // wifi configuration
        .ap = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .ssid_len = strlen(CONFIG_ESP_WIFI_SSID),
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .max_connection = CONFIG_ESP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(CONFIG_ESP_WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP configured, starting HTTP server...");
    ESP_ERROR_CHECK(http_server_init());
}

esp_err_t http_server_init(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 8192;
    ESP_LOGI(TAG, "Starting HTTP Server...");
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_uri);

    httpd_uri_t post_uri = {
        .uri = "/json",
        .method = HTTP_POST,
        .handler = http_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &post_uri);

    return ESP_OK;
}

esp_err_t http_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
static void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';

            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}
esp_err_t http_post_handler(httpd_req_t *req) {
    char content[1024];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';
    ESP_LOGI(TAG, "Received content: %s", content);
     char decoded[1024];
    url_decode(decoded, content);
    ESP_LOGI(TAG, "Decoded content: %s", decoded);

    char *json_start = strstr(decoded, "json_data=");
    if (!json_start) {
        ESP_LOGE(TAG, "JSON data not found in the request");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    json_start += strlen("json_data=");



    cJSON *json = cJSON_Parse(json_start);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
    cJSON *password = cJSON_GetObjectItem(json, "password");
    cJSON *broker = cJSON_GetObjectItem(json, "broker");
    cJSON *port = cJSON_GetObjectItem(json, "port");

    if (ssid && password) {
        ESP_LOGI(TAG, "Received SSID: %s, Password: %s", ssid->valuestring, password->valuestring);
        // Connect to the received WiFi credentials...
                // Disconnect and stop the AP mode
        esp_wifi_disconnect();
        esp_wifi_stop();
        esp_wifi_deinit();

        // Initialize and start the STA mode
        esp_netif_create_default_wifi_sta();
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config = {};
        strncpy((char *)wifi_config.sta.ssid, ssid->valuestring, sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password, password->valuestring, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait a few seconds for connection to establish
        // ESP_ERROR_CHECK(esp_wifi_connect());
        esp_wifi_connect();
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        mqtt_init(broker->valuestring, port->valueint);
        
    } else {
        ESP_LOGE(TAG, "Invalid JSON data");
    }

    cJSON_Delete(json);

    // httpd_resp_send(req, "Configuration received", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
