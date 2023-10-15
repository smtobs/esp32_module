#ifndef _WIFI_SERVICE_H
#define _WIFI_SERVICE_H

wifi_promiscuous_pkt_type_t;

bool wifi_srv_station_start(uint8_t *, uint8_t *);
uint8_t* get_wifi_srv_mac_address(void);
bool is_current_wifi_srv_mac(const uint8_t *);
bool is_broadcast_address(const uint8_t *);
bool is_multicast_address(const uint8_t *);
void wifi_srv_pk_sniffer_start(void (*custom_callback)(void *, wifi_promiscuous_pkt_type_t));
void wifi_srv_pk_sniffer_stop(void);

#endif