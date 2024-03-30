#include "string.h"
// #include "mqtt_client.h"
// #include "esp_log.h"
#include "mqtt.h"
#include "driver/gpio.h"
#define MAX_BROKER_LENGTH 256

char g_broker[MAX_BROKER_LENGTH];
int g_port;
int g_noofrelay;
static const char *TAG = "MQTT_EXAMPLE";

typedef struct {
    int number;
    int pin;
    bool state;  // true for ON, false for OFF
} Relay;

// Assume a maximum number of relays
#define MAX_RELAYS 10
Relay relays[MAX_RELAYS];
// int relay_count = 0;
static int num_relays = 0;


esp_mqtt_client_handle_t client = NULL;

// static const char *TAG = "MQTT_EXAMPLE";

static void update_relay_state(int relay_number, bool state) {
    for (int i = 0; i < num_relays; i++) {
        if (relays[i].number == relay_number) {
            relays[i].state = state;
            gpio_set_level(relays[i].pin, state ? 1 : 0);
            ESP_LOGI(TAG, "Relay %d set to %s", relay_number, state ? "ON" : "OFF");
            break;
        }
    }
}

static void configure_relay(int relay_number, int pin) {
    if (num_relays < MAX_RELAYS) {
        relays[num_relays].number = relay_number;
        relays[num_relays].pin = pin;
        relays[num_relays].state = false;
        // Initialize GPIO
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        gpio_set_level(pin, 0);  // Start with the relay turned off
        num_relays++;
        ESP_LOGI(TAG, "Configured relay %d on pin %d", relay_number, pin);
    }
}




static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}



static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id=0;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "configRelay/rx", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, "controlRelay/rx", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        // ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
            if (strcmp(event->topic, "configRelay/rx") == 0) {
                cJSON *json = cJSON_Parse(event->data);
                cJSON *relays_json = cJSON_GetObjectItem(json, "relays");

                int count = cJSON_GetArraySize(relays_json);
                for (int i = 0; i < count; i++) {
                    cJSON *relay_json = cJSON_GetArrayItem(relays_json, i);
                    int relay_number = cJSON_GetObjectItem(relay_json, "number")->valueint;
                    int pin = cJSON_GetObjectItem(relay_json, "pin")->valueint;
                    configure_relay(relay_number, pin);
                }

                cJSON_Delete(json);
            } else if (strcmp(event->topic, "controlRelay/rx") == 0) {
                cJSON *json = cJSON_Parse(event->data);
                cJSON *relays_json = cJSON_GetObjectItem(json, "relays");

                int count = cJSON_GetArraySize(relays_json);
                for (int i = 0; i < count; i++) {
                    cJSON *relay_json = cJSON_GetArrayItem(relays_json, i);
                    int relay_number = cJSON_GetObjectItem(relay_json, "number")->valueint;
                    bool state = strcmp(cJSON_GetObjectItem(relay_json, "state")->valuestring, "on") == 0;
                    update_relay_state(relay_number, state);
                }

                cJSON_Delete(json);
            }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    char full_broker_uri[MAX_BROKER_LENGTH + 8];  // Extra space for "mqtt://"
    snprintf(full_broker_uri, sizeof(full_broker_uri), "mqtt://%s", g_broker);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = full_broker_uri,
        .broker.address.port = g_port,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_init(const char* broker, int port) {
    // Ensure broker URL starts with mqtt:// or mqtts://
    if (strncmp(broker, "mqtt://", 7) != 0 && strncmp(broker, "mqtts://", 8) != 0) {
        ESP_LOGE(TAG, "Invalid broker URI. It should start with mqtt:// or mqtts://");
        return;
    }
    // Copy the broker, port, and noofrelay to global variables
    strncpy(g_broker, broker + 7, MAX_BROKER_LENGTH - 1); // +7 to skip mqtt://
    g_broker[MAX_BROKER_LENGTH - 1] = '\0'; // Ensure null termination
    g_port = port;
    // g_noofrelay = noofrelay;

    // Existing log and setup code...

    mqtt_app_start();
}
