#define TAG "MAIN"
#define FIRMWARE_VERSION 1
#define START_WIFI // uncomment to connect to wifi
//#define START_ETH // uncomment to connect to ethernet

// NETWORK
#define STATIC_IP_ADDR "192.168.178.15"
#define STATIC_NETMASK_ADDR "255.255.255.0"
#define STATIC_GW_ADDR "192.168.178.1"
#define STATIC_DNS_ADDR "192.168.178.1"
#define STATIC_DNS_BACKUP_ADDR "8.8.8.8"

// WIFI
#define EXAMPLE_ESP_WIFI_SSID "nome della wifi"
#define EXAMPLE_ESP_WIFI_PASS "password della wifi"
// #define EXAMPLE_ESP_WIFI_SSID "Pixel_8801"
// #define EXAMPLE_ESP_WIFI_PASS "franzogna"
#define MAX_RETRY 10

// ETHERNET
#define PIN_ETH_MDC 23
#define PIN_ETH_MDIO 18
#define PIN_ETH_RESET -1
#define ETH_PHY_ADDR 0

// GPIO
#define OLIMEX_BUT_PIN 34
#define RELE_PIN 32

// SPIFFS
#define INDEX_HTML_PATH "/spiffs/index.html"

// MQTT
#define MQTT_URI "mqtt://broker.hivemq.com:1883"

// OTA
#define OTA_URI_JSON "https://raw.githubusercontent.com/EmanueleFeola/espidf_examples/main/ota_folder/ota_fw_version.json"
#define OTA_SERVER_ROOT_CA "-----BEGIN CERTIFICATE-----\n"\
    "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"\
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"\
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"\
    "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"\
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"\
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n"\
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n"\
    "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n"\
    "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n"\
    "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n"\
    "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n"\
    "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n"\
    "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n"\
    "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n"\
    "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n"\
    "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n"\
    "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n"\
    "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n"\
    "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n"\
    "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n"\
    "-----END CERTIFICATE-----\n"
