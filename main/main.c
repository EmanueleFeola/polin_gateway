#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_eth.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_https_ota.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "rom/gpio.h"
#include "driver/gpio.h"
#include "esp_http_server.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "protocol_examples_common.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "defines.h"
#include "utils.h"
#include "modbus_utils.h"

#include "esp_log.h"
#include "modbus_params.h"  // for modbus parameters structures
#include "esp_modbus_common.h"
#include "mbcontroller.h"
#include "driver/uart.h"
#include "sdkconfig.h"

static int retry_cnt = 0;
bool connection_ok = false;
bool mqtt_connected = false;
bool rele_state = false;
TaskHandle_t publisher_task_handle = NULL;
esp_mqtt_client_handle_t client = NULL;

httpd_handle_t server = NULL;
char index_html[4096];
char rcv_buffer[1000];
char response_data[4096];

void mdns_set_hostname() {
	esp_err_t err = mdns_init();
	if (err) {
		printf("MDNS Init failed: %d\n", err);
		return;
	}
	mdns_hostname_set(MDNS_HOSTNAME); //set hostname
	mdns_instance_name_set(MDNS_INSTANCE_NAME); //set default instance
}

static void set_static_ip(esp_netif_t *netif) {
	esp_netif_ip_info_t ip_info = { 0 };
	esp_netif_dns_info_t dns_info = { 0 };

	uint8_t ip_arr[4];
	uint8_t ip_dns_arr[4];
	uint8_t ip_gw_arr[4];
	uint8_t ip_mask_arr[4];
	get_ip_bytes((char*) STATIC_IP_ADDR, ip_arr);
	get_ip_bytes((char*) STATIC_GW_ADDR, ip_gw_arr);
	get_ip_bytes((char*) STATIC_DNS_ADDR, ip_dns_arr);
	get_ip_bytes((char*) STATIC_NETMASK_ADDR, ip_mask_arr);

	IP4_ADDR(&ip_info.ip, ip_arr[0], ip_arr[1], ip_arr[2], ip_arr[3]);
	IP4_ADDR(&ip_info.gw, ip_gw_arr[0], ip_gw_arr[1], ip_gw_arr[2], ip_gw_arr[3]);
	IP4_ADDR(&ip_info.netmask, ip_mask_arr[0], ip_mask_arr[1], ip_mask_arr[2], ip_mask_arr[3]);
	IP_ADDR4(&dns_info.ip, ip_dns_arr[0], ip_dns_arr[1], ip_dns_arr[2], ip_dns_arr[3]);
	esp_netif_dhcpc_stop(netif);
	esp_netif_set_ip_info(netif, &ip_info);
	esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);

	mdns_set_hostname();
}

/// ETHERNET START
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	uint8_t mac_addr[6] = { 0 };
	/* we can get the ethernet driver handle from event data */
	esp_eth_handle_t eth_handle = *(esp_eth_handle_t*) event_data;

	switch (event_id) {
	case ETHERNET_EVENT_CONNECTED:
		esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
		ESP_LOGI(TAG, "Ethernet Link Up");
		ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		break;
	case ETHERNET_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "Ethernet Link Down");
		break;
	case ETHERNET_EVENT_START:
		ESP_LOGI(TAG, "Ethernet Started");
		break;
	case ETHERNET_EVENT_STOP:
		ESP_LOGI(TAG, "Ethernet Stopped");
		break;
	default:
		break;
	}
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
	const esp_netif_ip_info_t *ip_info = &event->ip_info;

	ESP_LOGI(TAG, "Ethernet Got IP Address");
	ESP_LOGI(TAG, "~~~~~~~~~~~");
	ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
	ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
	ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
	ESP_LOGI(TAG, "~~~~~~~~~~~");

	connection_ok = true;
}

static void ethernet_init() {
	eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG(); // apply default common MAC configuration
	eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG(); // apply default vendor-specific MAC configuration
	esp32_emac_config.smi_mdc_gpio_num = PIN_ETH_MDC;
	esp32_emac_config.smi_mdio_gpio_num = PIN_ETH_MDIO;
	esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config); // create MAC instance

	eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG(); // apply default PHY configuration
	phy_config.phy_addr = ETH_PHY_ADDR;
	phy_config.reset_gpio_num = PIN_ETH_RESET;
	esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config); // create PHY instance

	esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy); // apply default driver configuration
	esp_eth_handle_t eth_handle = NULL;
	ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle)); // install driver

	/* TCP-IP stack */
	ESP_ERROR_CHECK(esp_netif_init()); // Initialize TCP/IP network interface (should be called only once in application)
	ESP_ERROR_CHECK(esp_event_loop_create_default()); // Create default event loop that running in background

	esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH(); // Create new default instance of esp-netif for Ethernet
	esp_netif_t *eth_netif = esp_netif_new(&cfg);

	/* static ip address */
	set_static_ip(eth_netif);

	ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle))); // attach Ethernet driver to TCP/IP stack
	ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL)); // Register user defined event handers
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL)); // Register user defined event handers

	/* start Ethernet driver state machine */
	ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}

/// ETHERNET END

/// WIFI
static esp_err_t wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	switch (event_id) {
	case WIFI_EVENT_STA_START:
		esp_wifi_connect();
		ESP_LOGI(TAG, "Trying to connect with Wi-Fi");
		break;

	case WIFI_EVENT_STA_CONNECTED:
		ESP_LOGI(TAG, "Wi-Fi connected");
		break;

	case IP_EVENT_STA_GOT_IP:
		ESP_LOGI(TAG, "Wi-Fi got ip");
		connection_ok = true;

		break;

	case WIFI_EVENT_STA_DISCONNECTED:
		ESP_LOGI(TAG, "disconnected: Retrying Wi-Fi\n");
		if (retry_cnt++ < MAX_RETRY) {
			esp_wifi_connect();
		} else
			ESP_LOGI(TAG, "Max Retry Failed: Wi-Fi Connection\n");
		break;

	default:
		break;
	}
	return ESP_OK;
}

void wifi_init(void) {
	printf("[wifi_init]\n");

	esp_event_loop_create_default();

	wifi_config_t wifi_config = { .sta =
			{ .ssid = EXAMPLE_ESP_WIFI_SSID, .password = EXAMPLE_ESP_WIFI_PASS, .threshold.authmode = WIFI_AUTH_WPA2_PSK, }, };
	esp_netif_init();
	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

	set_static_ip(sta_netif);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&cfg);

	esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, sta_netif);
	esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, sta_netif);

	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
	esp_wifi_start();
}
/// WIFI END

/// HTTP
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {

	switch (evt->event_id) {
	case HTTP_EVENT_ERROR:
		break;
	case HTTP_EVENT_ON_CONNECTED:
		break;
	case HTTP_EVENT_HEADER_SENT:
		break;
	case HTTP_EVENT_ON_HEADER:
		break;
	case HTTP_EVENT_ON_DATA:
		if (!esp_http_client_is_chunked_response(evt->client)) {
			strncpy(rcv_buffer, (char*) evt->data, evt->data_len);
		}
		break;
	case HTTP_EVENT_ON_FINISH:
		break;
	case HTTP_EVENT_DISCONNECTED:
		break;
	case HTTP_EVENT_REDIRECT:
		break;
	}
	return ESP_OK;
}
/// HTTP END

/// OTA START
void start_ota_update(char *ota_uri_bin) {
	ESP_LOGI(TAG, "start_ota_update %s\n", ota_uri_bin);

	esp_http_client_config_t config = { .url = ota_uri_bin, .cert_pem = (char*) OTA_SERVER_ROOT_CA, .skip_cert_common_name_check = false };
	esp_https_ota_config_t ota_config = { .http_config = &config, };

	esp_err_t ret = esp_https_ota(&ota_config);
	if (ret == ESP_OK) {
		printf("OTA OK, restarting...\n");
		esp_restart();
	} else {
		printf("OTA failed...\n");
	}
}

char* ota_get_json() {
	ESP_LOGI(TAG, "ota_get_json");

	// configure the esp_http_client
	esp_http_client_config_t config = { .url = OTA_URI_JSON, .event_handler = _http_event_handler, .cert_pem = (char*) OTA_SERVER_ROOT_CA, };
	esp_http_client_handle_t client = esp_http_client_init(&config);

	// download json file (fw version and bin uri)
	esp_err_t err = esp_http_client_perform(client);

	if (err == ESP_OK) {
		cJSON *json = cJSON_Parse(rcv_buffer);
		if (json == NULL) {
			ESP_LOGE(TAG, "cannot parse downloaded json file. abort");
		} else {
			cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
			cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "uri");

			// check the version
			if (!cJSON_IsNumber(version)) {
				ESP_LOGE(TAG, "cannot read version field. abort");
			} else {
				int new_version = (int) version->valuedouble;
				ESP_LOGI(TAG, "current fw ver %d, available fw ver %d", FIRMWARE_VERSION, new_version);

				if (new_version > FIRMWARE_VERSION) {
					if (cJSON_IsString(file) && (file->valuestring != NULL)) {
						ESP_LOGI(TAG, "upgrading. firmware uri: %s", file->valuestring);

						esp_http_client_cleanup(client);
						return (char*) file->valuestring;
					} else {
						ESP_LOGE(TAG, "cannot read uri field. abort");
					}
				} else {
					ESP_LOGI(TAG, "not upgrading. upgrade is not needed.");
				}
			}
		}
	} else {
		ESP_LOGE(TAG, "unable to download json filen");
	}

	esp_http_client_cleanup(client);
	return NULL;
}

static void task_ota(void *pvParameters) {
	while (!connection_ok) {
		// wait for connection
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}

	// once got connection, get json file and perform ota if necessary
	char *ota_uri_bin = ota_get_json();
	if (ota_uri_bin != NULL)
		start_ota_update(ota_uri_bin);

	vTaskDelete(NULL);
}
/// OTA END

static void initi_web_page_buffer(void) {
	esp_vfs_spiffs_conf_t conf = { .base_path = "/spiffs", .partition_label =
	NULL, .max_files = 5, .format_if_mount_failed = true };

	ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

	memset((void*) index_html, 0, sizeof(index_html));
	struct stat st;
	if (stat(INDEX_HTML_PATH, &st)) {
		ESP_LOGE(TAG, "index.html not found");
		return;
	}

	FILE *fp = fopen(INDEX_HTML_PATH, "r");
	if (fread(index_html, st.st_size, 1, fp) == 0) {
		ESP_LOGE(TAG, "fread failed");
	}

//	printf("html page:\n");
//	printf("%s\n", index_html);
	fclose(fp);
}

/// MQTT START
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
	// ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	esp_mqtt_event_handle_t event = event_data;
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	switch ((esp_mqtt_event_id_t) event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		mqtt_connected = true;

		msg_id = esp_mqtt_client_subscribe(client, "/emanuele_topic/#", 0);
		ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
		break;
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		mqtt_connected = false;
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
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
		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		break;
	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
	}
}

static void mqtt_app_start(void) {
	ESP_LOGI(TAG, "STARTING MQTT");

	const esp_mqtt_client_config_t mqttConfig = { .broker = { .address.uri =
	MQTT_URI, .verification.certificate =
	MQTT_SERVER_CERT }, .credentials = { .username = MQTT_USERNAME, .client_id =
	NULL, .authentication = { .password = MQTT_PASSWORD } } };

	client = esp_mqtt_client_init(&mqttConfig);
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
	esp_mqtt_client_start(client);
}

void publisher_task(void *params) {
	while (!connection_ok) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

	mqtt_app_start();

	while (true) {
		if (mqtt_connected) {
			char pub_str[100];
			int max_len = sizeof(pub_str);
			snprintf(pub_str, max_len, "hello world banana %d", (int) millis());
			printf("sending %s\n", pub_str);

			esp_mqtt_client_publish(client, "/emanuele_topic/test3/", pub_str, 0, 0, 0);

			vTaskDelay(5000 / portTICK_PERIOD_MS);
		}
	}
}
/// MQTT END

/// HTTP SERVER + WEBSOCKET START
struct async_resp_arg { // Asynchronous response data structure
	httpd_handle_t hd; // Server instance
	int fd;            // Session socket file descriptor
};

esp_err_t get_req_handler(httpd_req_t *req) {
	printf("[get_req_handler] called");
	int response;
	if (rele_state) {
		sprintf(response_data, index_html, "ON");
	} else {
		sprintf(response_data, index_html, "OFF");
	}
	response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);
	return response;
}

static void ws_async_send(void *arg) {
	httpd_ws_frame_t ws_pkt;
	struct async_resp_arg *resp_arg = arg;
	httpd_handle_t hd = resp_arg->hd;

	rele_state = !rele_state;
	gpio_set_level(RELE_PIN, rele_state);

	char buff[4];
	memset(buff, 0, sizeof(buff));
	sprintf(buff, "%d %d", (int) millis(), rele_state);

	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.payload = (uint8_t*) buff;
	ws_pkt.len = strlen(buff);
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;

	static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
	size_t fds = max_clients;
	int client_fds[max_clients];

	esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);

	if (ret != ESP_OK) {
		return;
	}

	for (int i = 0; i < fds; i++) {
		int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
		if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
			httpd_ws_send_frame_async(hd, client_fds[i], &ws_pkt);
		}
	}
	free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req) {
	struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
	resp_arg->hd = req->handle;
	resp_arg->fd = httpd_req_to_sockfd(req);
	return httpd_queue_work(handle, ws_async_send, resp_arg);
}

static esp_err_t handle_ws_req(httpd_req_t *req) {
	if (req->method == HTTP_GET) {
		ESP_LOGI(TAG, "Handshake done, the new connection was opened");
		return ESP_OK;
	}

	httpd_ws_frame_t ws_pkt;
	uint8_t *buf = NULL;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;
	esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
		return ret;
	}

	if (ws_pkt.len) {
		buf = calloc(1, ws_pkt.len + 1);
		if (buf == NULL) {
			ESP_LOGE(TAG, "Failed to calloc memory for buf");
			return ESP_ERR_NO_MEM;
		}
		ws_pkt.payload = buf;
		ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
			free(buf);
			return ret;
		}
		ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
	}

	ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

	if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp((char*) ws_pkt.payload, "toggle") == 0) {
		free(buf);
		return trigger_async_send(req->handle, req);
	}
	return ESP_OK;
}

void websocket_app_start(void) {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

// Create URI (Uniform Resource Identifier)
// for the server which is added to default gateway
	static const httpd_uri_t uri_handler = { .uri = "/ws", .method = HTTP_GET, .handler = handle_ws_req, .user_ctx =
	NULL, .is_websocket = true };

	static const httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = get_req_handler, .user_ctx =
	NULL };

// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		ESP_LOGI(TAG, "Registering URI handler");
		httpd_register_uri_handler(server, &uri_handler);
		httpd_register_uri_handler(server, &uri_get);
	}
}

static void websocket_task(void *pvParameters) {
	while (!connection_ok) {
		// wait for connection
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}

	websocket_app_start();

	vTaskDelete(NULL);
}
/// HTTP SERVER + WEBSOCKET END

void app_main(void) {
	esp_timer_early_init();
	gpio_set_direction(RELE_PIN, GPIO_MODE_OUTPUT);

	initi_web_page_buffer();

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

#ifdef START_WIFI
	wifi_init();
#endif

#ifdef START_ETH
	ethernet_init();
#endif

	//xTaskCreate(&task_ota, "task_ota", 8192, NULL, 5, NULL); // uncomment to start ota task
	//xTaskCreate(publisher_task, "publisher_task", 1024 * 5, NULL, 5, NULL); // uncomment to start mqtt task
	xTaskCreate(websocket_task, "websocket_task", 1024 * 5, NULL, 5, NULL); // uncomment to start websocket task

#ifdef START_MODBUS_MASTER
	ESP_ERROR_CHECK(modbus_master_init());
	modbus_master_test_read_write();
#endif

#ifdef START_MODBUS_SLAVE
	modbus_slave_init();
	modbus_slave_loop();
#endif

}
