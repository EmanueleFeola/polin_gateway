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
- esp32 connects to network with static ip, either wifi or ethernet depending on defines.h settings
	- static ip address and wifi network credentials are set in defines.h
- web server
	- stores html page in spiffs file system
	- serve page and allows to toggle relè
- ota
	- automatically downloads firmware (.bin file) if a new firmware version is released
	- firmware version and firmware bin file are set in a file hosted in the cloud
		- see defines.h (OTA_URI_JSON)
- mqtt client
	- connects to server and sends/receive messages
	- mqtt server uri is set in defines.h (MQTT_URI)
