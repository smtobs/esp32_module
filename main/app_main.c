#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"

#include "wifi_service.h"
#include "hw_link_ctrl_protocol.h"
#include "ring_buff.h"
#include "utils.h"

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define GPIO_HANDSHAKE 2
#define GPIO_MOSI 12
#define GPIO_MISO 13
#define GPIO_SCLK 15
#define GPIO_CS 14

#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2
#define GPIO_HANDSHAKE 3
#define GPIO_MOSI 7
#define GPIO_MISO 2
#define GPIO_SCLK 6
#define GPIO_CS 10

#elif CONFIG_IDF_TARGET_ESP32C6
#define GPIO_HANDSHAKE 15
#define GPIO_MOSI 19
#define GPIO_MISO 20
#define GPIO_SCLK 18
#define GPIO_CS 9

#elif CONFIG_IDF_TARGET_ESP32H2
#define GPIO_HANDSHAKE 2
#define GPIO_MOSI 5
#define GPIO_MISO 0
#define GPIO_SCLK 4
#define GPIO_CS 1

#elif CONFIG_IDF_TARGET_ESP32S3
#define GPIO_HANDSHAKE 2
#define GPIO_MOSI 11
#define GPIO_MISO 13
#define GPIO_SCLK 12
#define GPIO_CS 10

#endif //CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2

#ifdef CONFIG_IDF_TARGET_ESP32
#define RCV_HOST    HSPI_HOST

#else
#define RCV_HOST    SPI2_HOST

#endif

#define WIFI_SSID                     "your_ssid"
#define WIFI_PASSWORD                 "your_pw"

/* Called after a transaction is queued and ready for pickup by master. We use this to set the handshake line high. */
void my_post_setup_cb(spi_slave_transaction_t *trans)
{
    gpio_set_level(GPIO_HANDSHAKE, 1);
}

/* Called after transaction is sent/received. We use this to set the handshake line low. */
void my_post_trans_cb(spi_slave_transaction_t *trans)
{
    gpio_set_level(GPIO_HANDSHAKE, 0);
}

void spi_init(void)
{
    esp_err_t ret;

    TRACE_FUNC_ENTRY();

    /* Configuration for the SPI bus */
    spi_bus_config_t buscfg =
    {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    /* Configuration for the SPI slave interface */
    spi_slave_interface_config_t slvcfg =
    {
        .mode=0,
        .spics_io_num=GPIO_CS,
        .queue_size=3,
        .flags=0,
        .post_setup_cb=my_post_setup_cb,
        .post_trans_cb=my_post_trans_cb
    };

    /* Configuration for the handshake line */
    gpio_config_t io_conf =
    {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1 << GPIO_HANDSHAKE)
    };

    /* Configure handshake line as output */
    gpio_config(&io_conf);
    /* Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected. */
    gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    /* Initialize SPI slave interface */
    ret = spi_slave_initialize(RCV_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    assert(ret == ESP_OK);

    TRACE_FUNC_EXIT();
}

void promiscuous_callback(void *buff, wifi_promiscuous_pkt_type_t type)
{
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buff;
    uint8_t *pkt_ctrl = pkt->payload;

    if (type == WIFI_PKT_MGMT) /* MGMT Frame */
    {
        if (pkt_ctrl[0] == 0x50 && pkt_ctrl[1] == 0x00)
        {
            uint8_t *da = &pkt_ctrl[4];
            uint8_t *sa = &pkt_ctrl[10];

            if ((is_current_wifi_srv_mac(da)) || 
                (is_broadcast_address(da)))
            {
                tx_buffer_critical_section_lock();
                tx_buffer_enqueue((u8 *)pkt_ctrl, (int)pkt->rx_ctrl.sig_len);
                tx_buffer_critical_section_unlock();

                //INFO_PRINT("Received a probe response packet");
            }
        }
    }
    else if (type == WIFI_PKT_DATA) /* Data Frame */
    {
        bool toDS = pkt_ctrl[1] & 0x01;
        bool fromDS = pkt_ctrl[1] & 0x02;

        if (toDS == false && fromDS == true)
        {
            uint8_t *da = &pkt_ctrl[4];
            if (is_current_wifi_srv_mac(da) || is_broadcast_address(da))
            {
                tx_buffer_critical_section_lock();
                tx_buffer_enqueue((u8 *)pkt_ctrl, (int)pkt->rx_ctrl.sig_len);
                tx_buffer_critical_section_unlock();
                
                //printf("DATA Frame [%d]\n", (int)pkt->rx_ctrl.sig_len);
            }
            else if (is_multicast_address(da))
            {
                printf("muticast add\n");
            }
        }
    }
}

void app_main_loop(void)
{
    esp_err_t ret;
    spi_slave_transaction_t spi_trans;
    struct buffer *tx_buff = NULL;
    static WORD_ALIGNED_ATTR char recvbuf[MAX_BUFFER_SIZE]="";
    static uint8_t sendbuf[MAX_BUFFER_SIZE] = {0};
    int recvbuf_len = MAX_BUFFER_SIZE * 8, actual_send_buf_len;
    u8 *actual_send_buf = NULL;

    TRACE_FUNC_ENTRY();

    memset(&spi_trans, 0x0, sizeof(spi_slave_transaction_t));

    spi_trans.tx_buffer = NULL;
    spi_trans.length    = recvbuf_len;

    while (1)
    {
        memset(recvbuf, 0x0, MAX_BUFFER_SIZE);
        spi_trans.rx_buffer = recvbuf;
        ret = spi_slave_transmit(RCV_HOST, &spi_trans, 500);
        if (ret == ESP_OK)
        {
            u8 *p_recvbuf = (u8 *)recvbuf;
            int recv_len = is_valid_hw_frame(p_recvbuf);

            if (recv_len > 24)
            {
                esp_wifi_80211_tx(WIFI_IF_STA, &p_recvbuf[PAYLOAD_FIELD], recv_len, true);
            }
            else if (recv_len == 2)
            {
                // H/W command
            }
            else
            {
                //failed
            }
        }
        else
        {
            switch (ret)
            {
                case ESP_ERR_INVALID_ARG:
                    ERROR_PRINT("Invalid argument passed to spi_slave_transmit");
                    break;

                case ESP_ERR_TIMEOUT:
                    ERROR_PRINT("SPI transmit timed out");
                    break;

                case ESP_ERR_NO_MEM:
                    ERROR_PRINT("Memory allocation failed for spi_slave_transmit");
                    break;

                default:
                    ERROR_PRINT("SPI transmit failed with error: %d", ret);
                    break;
            }
        }

        tx_buffer_critical_section_lock();
        tx_buff = tx_buffer_dequeue();
        if (tx_buff == BUFFER_EMPTY)
        {
            actual_send_buf_len = 0;
            sendbuf[0] = '\0';
        }
        else
        {
            actual_send_buf_len = tx_buff->len;
            memcpy(sendbuf, tx_buff->buf, tx_buff->len);
        }
        tx_buffer_critical_section_unlock();

        actual_send_buf = hw_frame_assemble(sendbuf, &actual_send_buf_len);
        if (actual_send_buf == NULL)
        {
            spi_trans.tx_buffer = NULL;
        }
        else
        {
            spi_trans.tx_buffer = actual_send_buf;
        }
        spi_trans.length = recvbuf_len;

        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    TRACE_FUNC_EXIT();
}

void app_main(void)
{
    esp_err_t ret;

    TRACE_FUNC_ENTRY();

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    buffer_init();

    while (wifi_srv_station_start((uint8_t *)WIFI_SSID, NULL) == false)
    {
        /* Todo */
    }

    wifi_srv_pk_sniffer_start(promiscuous_callback);
    spi_init();

    /* app main loop */
    app_main_loop();
    
    wifi_srv_pk_sniffer_stop();
    esp_wifi_disconnect();
    buffer_deinit();
    
    TRACE_FUNC_EXIT();
}
