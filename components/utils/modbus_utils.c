#include "modbus_utils.h"

/// SLAVE
void modbus_slave_init() {
	mb_communication_info_t comm_info; // Modbus communication parameters
	mb_register_area_descriptor_t reg_area; // Modbus register area descriptor structure

	// Set UART log level
	esp_log_level_set(TAG_MODBUS, ESP_LOG_INFO);
	void *mbc_slave_handler = NULL;

	ESP_ERROR_CHECK(mbc_slave_init(MB_PORT_SERIAL_SLAVE, &mbc_slave_handler)); // Initialization of Modbus controller

	// Setup communication parameters and start stack
	comm_info.mode = MB_MODE_RTU;
	comm_info.slave_addr = MB_DEVICE_ADDR1;
	comm_info.port = MB_PORT_NUM;
	comm_info.baudrate = MB_DEV_SPEED;
	comm_info.parity = MB_PARITY_NONE;
	ESP_ERROR_CHECK(mbc_slave_setup((void* ) &comm_info));

	reg_area.type = MB_PARAM_HOLDING;
	reg_area.start_offset = MB_REG_HOLDING_START_AREA0;
	reg_area.address = (void*) &holding_reg_area[0];
	reg_area.size = sizeof(holding_reg_area);
	ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

	// Starts of modbus controller and stack
	ESP_ERROR_CHECK(mbc_slave_start());

	// Set UART pin numbers and driver mode to Half Duplex
	ESP_ERROR_CHECK(uart_set_pin(MB_PORT_NUM, MB_TXD, MB_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	ESP_ERROR_CHECK(uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));

	ESP_LOGI(TAG_MODBUS, "Modbus slave stack initialized.");
	ESP_LOGI(TAG_MODBUS, "Start modbus test...");

	modbus_slave_setup_initial_values();
}

void modbus_slave_setup_initial_values() {
	holding_reg_area[0] = MB_REG_0_INITIAL_VAL;
	holding_reg_area[1] = MB_REG_1_INITIAL_VAL;
}

void modbus_slave_loop() {
	mb_param_info_t reg_info;

	while (1) {
		ESP_LOGI(TAG_MODBUS, "Waiting for modbus request");
		mb_event_group_t event = mbc_slave_check_event(MB_READ_WRITE_MASK);

		ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
		const char *rw_str = (event & MB_READ_MASK) ? "READ" : "WRITE";

		if (event & (MB_EVENT_HOLDING_REG_WR | MB_EVENT_HOLDING_REG_RD)) {
			if (reg_info.address == &holding_reg_area[0]) {
				// se master legge/scrive registro 0, incremento registro 0 e metto a valore iniziale registro 1
				portENTER_CRITICAL(&param_lock);
				holding_reg_area[0]++;
				holding_reg_area[1] = MB_REG_1_INITIAL_VAL;
				portEXIT_CRITICAL(&param_lock);
			} else if (reg_info.address == &holding_reg_area[1]) {
				// se master legge/scrive registro 1, incremento registro 1 e metto a valore iniziale registro 0
				portENTER_CRITICAL(&param_lock);
				holding_reg_area[0] = MB_REG_0_INITIAL_VAL;
				holding_reg_area[1]++;
				portEXIT_CRITICAL(&param_lock);
			}

			// print all register values
			for (int reg_idx = 0; reg_idx < MB_REG_HOLD_CNT; reg_idx++) {
				printf("register 0x%.4x value is %d \n", &holding_reg_area[reg_idx], holding_reg_area[reg_idx]);
			}

			// info about current reading/writing event
			ESP_LOGI(TAG_MODBUS, "HOLDING %s (%u us), ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u", rw_str, (uint32_t ) reg_info.time_stamp,
					(uint32_t ) reg_info.mb_offset, (uint32_t ) reg_info.type, (uint32_t ) reg_info.address, (uint32_t ) reg_info.size);
		}
	}
}

/// MASTER
esp_err_t read_modbus_parameter(uint16_t cid, uint16_t *par_data) {
	const mb_parameter_descriptor_t *param_descriptor = NULL;

	esp_err_t err = mbc_master_get_cid_info(cid, &param_descriptor);
	if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
		uint8_t type = 0;
		err = mbc_master_get_parameter(cid, (char*) param_descriptor->param_key, (uint8_t*) par_data, &type);
		if (err == ESP_OK) {
			ESP_LOGI(TAG_MODBUS, "Characteristic #%d %s (%s) value = (%05d) parameter read successful.", param_descriptor->cid,
					(char* ) param_descriptor->param_key, (char* ) param_descriptor->param_units, *(uint16_t* ) par_data);
		} else {
			ESP_LOGE(TAG_MODBUS, "Characteristic #%d %s (%s), parameter read fail.", param_descriptor->cid, (char* ) param_descriptor->param_key,
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
		err = mbc_master_set_parameter(cid, (char*) param_descriptor->param_key, (uint8_t*) par_data, &type);
		if (err == ESP_OK) {
			ESP_LOGI(TAG_MODBUS, "Param #%d %s (%s) value = (%05d), write success", param_descriptor->cid, (char* ) param_descriptor->param_key,
					(char* ) param_descriptor->param_units, *(uint16_t* ) par_data);
		} else {
			ESP_LOGE(TAG_MODBUS, "Param #%d (%s) write fail, err = 0x%x (%s).", param_descriptor->cid, (char* ) param_descriptor->param_key,
					(int ) err, (char* ) esp_err_to_name(err));
		}
	}
	return err;
}

void modbus_master_test_read_write() {
	esp_err_t err = ESP_OK;
	uint16_t register_data = 0;

	ESP_LOGI(TAG_MODBUS, "Modbus master test start");

	err = read_modbus_parameter(HOLD_1, &register_data);
	if (err == ESP_OK) {
		register_data += 1;
		err = write_modbus_parameter(HOLD_1, &register_data);
	}

	err = read_modbus_parameter(HOLD_2, &register_data);
	if (err == ESP_OK) {
		register_data += 1;
		err = write_modbus_parameter(HOLD_2, &register_data);
	}

	ESP_LOGI(TAG_MODBUS, "Modbus master test end");
}

esp_err_t modbus_master_init() {
	// Initialize and start Modbus controller
	mb_communication_info_t comm = { .port = MB_PORT_NUM, .mode = MB_MODE_RTU, .baudrate = MB_DEV_SPEED, .parity = MB_PARITY_NONE };
	void *master_handler = NULL;

	esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
	MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG_MODBUS, "mb controller initialization fail.");
	MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG_MODBUS, "mb controller initialization fail, returns(0x%x).", (uint32_t) err);

	err = mbc_master_setup((void*) &comm);
	MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG_MODBUS, "mb controller setup fail, returns(0x%x).", (uint32_t) err);

// Set UART pin numbers
	err = uart_set_pin(MB_PORT_NUM, MB_TXD, MB_RXD, UART_PIN_NO_CHANGE,
	UART_PIN_NO_CHANGE);
	MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG_MODBUS, "mb serial set pin failure, uart_set_pin() returned (0x%x).",
			(uint32_t) err);

	err = mbc_master_start();
	if (err != ESP_OK) {
		ESP_LOGE("master_init", "mb controller start fail, err=%x.", err);
	}

	vTaskDelay(5);
	err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
	MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG_MODBUS, "mb controller set descriptor fail, returns(0x%x).", (uint32_t) err);
	ESP_LOGI("master_init", "Modbus master stack initialized...");
	return err;
}
