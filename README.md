# IDE
used IDE: Espressif-IDE (https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/get-started/eclipse-setup.html)
# Project configuration
menuconfig settings:
- websocket server support
- custom partition table csv, partitions.csv
- flash size: 4MB
# Hardware
Tested on "esp32 evb" board
# Features
- esp32 connects to network, either wifi or ethernet depending on defines.h settings
	- wifi network credentials are set in defines.h
	- the IP address is not static, the esp32 is accessible using its hostname "polin_gateway.local"
		- the mdns hostname is set in defines.h (MDNS_HOSTNAME)
- web server
	- stores html page in spiffs file system
	- serve page and allows to toggle relè
- ota
	- automatically downloads firmware (.bin file) if a new firmware version is released
	- firmware version and firmware bin file are set in a file hosted in the cloud
		- see defines.h (OTA_URI_JSON)
- mqtt client
	- connects to server and sends/receives messages
		- the mqtts server used for tests is: https://test.mosquitto.org/
	- mqtt settings in defines.h:
		- MQTT_URI, i.e. protocol + server + port
		- MQTT_USERNAME
		- MQTT_PASSWORD
		- MQTT_SERVER_CERT
			- needed to use MQTT over SSL
	- NB: port 8883 is mqtts, not mqtt
- modbus 
	- modbus settings in components/utils/modbus_utils.h
		- MB_DEV_SPEED, i.e. baudrate
		- MB_TXD, i.e. TX gpio
		- MB_RXD, i.e. RX gpio
		- MB_PORT_NUM, i.e. UART port number
		- MB_DEVICE_ADDR1: master/slave address
- modbus master
	- connect to modbus slave, reads/writes two registers
	- NB: the modbus slave is simulated using ModRSsim2 for windows
- modbus slave:
	- esp32 has two holding registers that can be interrogated by modbus master
	- NB: modbus master simulated using RMMS for windows