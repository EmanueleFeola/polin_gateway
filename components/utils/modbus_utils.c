#include "modbus_utils.h"

esp_err_t read_modbus_parameter(uint16_t cid, uint16_t *par_data) {
	const mb_parameter_descriptor_t *param_descriptor = NULL;

	esp_err_t err = mbc_master_get_cid_info(cid, &param_descriptor);
	if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
		uint8_t type = 0;
		err = mbc_master_get_parameter(cid, (char*) param_descriptor->param_key,
				(uint8_t*) par_data, &type);
		if (err == ESP_OK) {
			ESP_LOGI(TAG_MODBUS,
					"Characteristic #%d %s (%s) value = (%05d) parameter read successful.",
					param_descriptor->cid, (char* ) param_descriptor->param_key,
					(char* ) param_descriptor->param_units,
					*(uint16_t* ) par_data);
		} else {
			ESP_LOGE(TAG_MODBUS,
					"Characteristic #%d %s (%s), parameter read fail.",
					param_descriptor->cid, (char* ) param_descriptor->param_key,
					(char* ) param_descriptor->param_units);
		}
	}
	return err;
}

esp_err_t write_modbus_parameter(uint16_t cid, uint16_t *par_data) {
	const mb_parameter_descriptor_t *param_descriptor = NULL;

	esp_err_t err = mbc_master_get_cid_info(cid, &param_descriptor);
	if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
		uint8_t type = 0; // type of parameter from dictionary
		err = mbc_master_set_parameter(cid, (char*) param_descriptor->param_key,
				(uint8_t*) par_data, &type);
		if (err == ESP_OK) {
			ESP_LOGI(TAG_MODBUS,
					"Param #%d %s (%s) value = (%05d), write success",
					param_descriptor->cid, (char* ) param_descriptor->param_key,
					(char* ) param_descriptor->param_units,
					*(uint16_t* ) par_data);
		} else {
			ESP_LOGE(TAG_MODBUS,
					"Param #%d (%s) write fail, err = 0x%x (%s).",
					param_descriptor->cid, (char* ) param_descriptor->param_key,
					(int ) err, (char* ) esp_err_to_name(err));
		}
	}
	return err;
}

// Modbus master initialization
esp_err_t master_init() {
	// Initialize and start Modbus controller
	mb_communication_info_t comm = { .port = MB_PORT_NUM, .mode = MB_MODE_RTU,
			.baudrate = MB_DEV_SPEED, .parity = MB_PARITY_NONE };
	void *master_handler = NULL;

	esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
	MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG_MODBUS,
			"mb controller initialization fail.");
	MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG_MODBUS,
			"mb controller initialization fail, returns(0x%x).",
			(uint32_t) err);

	err = mbc_master_setup((void*) &comm);
	MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG_MODBUS,
			"mb controller setup fail, returns(0x%x).", (uint32_t) err);

	// Set UART pin numbers
	err = uart_set_pin(MB_PORT_NUM, MB_TXD, MB_RXD, UART_PIN_NO_CHANGE,
	UART_PIN_NO_CHANGE);
	MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG_MODBUS,
			"mb serial set pin failure, uart_set_pin() returned (0x%x).",
			(uint32_t) err);

	err = mbc_master_start();
	if (err != ESP_OK) {
		ESP_LOGE("master_init", "mb controller start fail, err=%x.", err);
	}

	vTaskDelay(5);
	err = mbc_master_set_descriptor(&device_parameters[0],
			num_device_parameters);
	MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG_MODBUS,
			"mb controller set descriptor fail, returns(0x%x).",
			(uint32_t) err);
	ESP_LOGI("master_init", "Modbus master stack initialized...");
	return err;
}
