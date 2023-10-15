#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "utils.h"
#include "wifi_service.h"

#define OPEN_WIFI   1

#define WIFI_CONNECTED_BIT            BIT0
#define WIFI_FAIL_BIT                 BIT1
#define EXAMPLE_ESP_MAXIMUM_RETRY     5
#define WIFI_MAC_LEN                  6

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif


static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num;
static uint8_t cached_mac[WIFI_MAC_LEN];
static bool is_mac_initialized = false;
static uint8_t wifi_ssid[32];
static uint8_t wifi_pw[128];
 const wifi_promiscuous_filter_t filt =
 {
//     .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL | 
//                     WIFI_PROMIS_FILTER_MASK_DATA_MPDU | WIFI_PROMIS_FILTER_MASK_DATA_AMPDU
         .filter_mask =               WIFI_PROMIS_FILTER_MASK_ALL
 };

static bool wifi_srv_conn_event_blocking()
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
    * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ERROR_PRINT("connected to ap SSID:%s\n", wifi_ssid);
        return true;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ERROR_PRINT("Failed to connect to SSID:%s \n", wifi_ssid);
        return false;
    }
    else
    {
        ERROR_PRINT("UNEXPECTED EVENT\n");
        return false;
    }
}

static void wifi_srv_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ERROR_PRINT("retry to connect to the AP\n");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ERROR_PRINT("connect to the AP fail\n");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ERROR_PRINT("got ip:\n" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        INFO_PRINT("event_id=[%d]\n", (int)event_id);
        //xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        //esp_wifi_disconnect();
    }
    else
    {
        INFO_PRINT("event_id=[%d]\n", (int)event_id);
    }
}

bool wifi_srv_station_start(uint8_t *ssid, uint8_t *pw)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));


    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_srv_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_srv_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {0};

    /* OPEN */
    if (pw == NULL)
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        strcpy((char *)wifi_config.sta.ssid, (char *)ssid);
        strcpy((char *)wifi_config.sta.password, "");
        strcpy((char *)wifi_ssid, (char *)ssid);
        strcpy((char *)wifi_pw, "");
    }
    else
    {
        wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
        strcpy((char *)wifi_config.sta.threshold.sae_pwe_h2e, ESP_WIFI_SAE_MODE);
        strcpy((char *)wifi_config.sta.threshold.sae_h2e_identifier, EXAMPLE_H2E_IDENTIFIER);
        strcpy((char *)wifi_config.sta.ssid, (char *)ssid);
        strcpy((char *)wifi_config.sta.password, (char *)pw);
        strcpy((char *)wifi_ssid, (char *)ssid);
        strcpy((char *)wifi_pw, (char *)pw);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    INFO_PRINT("Waiting for wifi connection\n");

    if (wifi_srv_conn_event_blocking() == false)
    {
        return false;
    }
    else
    {
        esp_wifi_get_mac(WIFI_IF_STA, cached_mac);
        return true;
    }
}

uint8_t* get_wifi_srv_mac_address(void)
{
    if (!is_mac_initialized)
    {
        esp_wifi_get_mac(ESP_IF_WIFI_STA, cached_mac);
        is_mac_initialized = true;
    }
    return cached_mac;
}

bool is_current_wifi_srv_mac(const uint8_t *mac)
{
    uint8_t *current_mac = get_wifi_srv_mac_address();
    return memcmp(current_mac, mac, WIFI_MAC_LEN) == 0;
}

bool is_broadcast_address(const uint8_t *mac)
{
    for (int i = 0; i < WIFI_MAC_LEN; i++)
    {
        if (mac[i] != 0xFF)
        {
            return false;
        }
    }
    return true;
}

bool is_multicast_address(const uint8_t *mac)
{
    return (mac[0] & 0x01) == 1;
}

void check_promiscuous_filter(void)
{
    wifi_promiscuous_filter_t filter;
    esp_err_t ret = esp_wifi_get_promiscuous_filter(&filter);

    if (ret == ESP_OK)
    {
        INFO_PRINT("Promiscuous Filter Type: %d\n", (int)filter.filter_mask);

        if (filter.filter_mask & WIFI_PROMIS_FILTER_MASK_MGMT)
        {
            INFO_PRINT("Management frames are being filtered.\n");
        }

        if (filter.filter_mask & WIFI_PROMIS_FILTER_MASK_DATA)
        {
            INFO_PRINT("Data frames are being filtered.\n");
        }

        if (filter.filter_mask & WIFI_PROMIS_FILTER_MASK_CTRL)
        {
            INFO_PRINT("Control frames are being filtered.\n");
        }

        if (filter.filter_mask & WIFI_PROMIS_FILTER_MASK_MISC)
        {
            INFO_PRINT("Misc frames are being filtered.\n");
        }
        
        if (filter.filter_mask & WIFI_PROMIS_FILTER_MASK_DATA_MPDU)
        {
            INFO_PRINT("MPDU frames are being filtered.\n");
        }
        
        if (filter.filter_mask & WIFI_PROMIS_FILTER_MASK_DATA_AMPDU)
        {
            INFO_PRINT("AMPDU frames are being filtered.\n");
        }
    }
    else
    {
        INFO_PRINT("Failed to get promiscuous filter. Error: %d\n", (int)ret);
    }
}


void wifi_srv_pk_sniffer_start(void (*custom_callback)(void *buf, wifi_promiscuous_pkt_type_t type))
{
    if (custom_callback == NULL)
    {
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(custom_callback));

    esp_wifi_set_promiscuous_filter(&filt);
    check_promiscuous_filter();
}

void wifi_srv_pk_sniffer_stop(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(NULL));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
}
