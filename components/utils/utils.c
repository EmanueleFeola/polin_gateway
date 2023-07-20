#include "utils.h"

unsigned long millis() {
	return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

void get_ip_bytes(char *ipv4, uint8_t *ip_array) {
	/*
	 * input: "192.178.168.1"
	 * output: [192, 178, 168, 1]
	 * output[0] = 192
	 * */
	uint32_t ip = ipaddr_addr(ipv4);
//	uint8_t ip_array[4];

	ip_array[0] = ip & 0xff;
	ip_array[1] = (ip >> 8) & 0xff;
	ip_array[2] = (ip >> 16) & 0xff;
	ip_array[3] = (ip >> 24) & 0xff;
}
