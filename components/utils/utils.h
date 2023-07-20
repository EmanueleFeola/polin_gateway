#include "esp_timer.h"
#include "lwip/dns.h"
#include "mdns.h"

unsigned long millis();
void get_ip_bytes(char *ipv4, uint8_t *ip_array);
