#include "esp_log.h"
#include "esp_modbus_common.h"
#include "mbcontroller.h"
#define TAG_MODBUS "MODBUS"
#define MB_PORT_NUM UART_NUM_1
#define MB_DEV_SPEED 115200
#define MB_TXD 4
#define MB_RXD 36

/// SLAVE
#define MB_PAR_INFO_GET_TOUT                (10) // Timeout for get parameter info
#define MB_READ_MASK                        (MB_EVENT_INPUT_REG_RD | MB_EVENT_HOLDING_REG_RD | MB_EVENT_DISCRETE_RD | MB_EVENT_COILS_RD)
#define MB_WRITE_MASK                       (MB_EVENT_HOLDING_REG_WR | MB_EVENT_COILS_WR)
#define MB_READ_WRITE_MASK                  (MB_READ_MASK | MB_WRITE_MASK)
#define MB_REG_HOLDING_START_AREA0 0 // indice da cui partono indirizzi modbus
#define MB_REG_HOLD_CNT 2 // numero di registri
#define MB_REG_0_INITIAL_VAL 100 // valore iniziale reg 0
#define MB_REG_1_INITIAL_VAL 201 // valore iniziale reg 1
void modbus_slave_init();
void modbus_slave_setup_initial_values();
void modbus_slave_loop();
static uint16_t holding_reg_area[MB_REG_HOLD_CNT] = { 0 }; // qua ci sono i valori dei registri
static portMUX_TYPE param_lock = portMUX_INITIALIZER_UNLOCKED;

/// MASTER
#define STR(fieldname) ((const char*)( fieldname ))
#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val } // Options can be used as bit masks or parameter limits

esp_err_t read_modbus_parameter(uint16_t cid, uint16_t *par_data);
esp_err_t write_modbus_parameter(uint16_t cid, uint16_t *par_data);
esp_err_t modbus_master_init();
void modbus_master_test_read_write();

enum {
	MB_DEVICE_ADDR1 = 2 // Only one slave device used for the test (add other slave addresses here)
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum {
	HOLD_1 = 0, HOLD_2
};

// { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
static const mb_parameter_descriptor_t device_parameters[] = { { HOLD_1, STR("HOLD 1"), STR(""), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0, // Reg Start, i.e. 0x40000
		1, // Reg Size
		0, // offset
		PARAM_TYPE_FLOAT,
		2, // Data Size
		OPTS( 0, 0, 0 ),
		PAR_PERMS_READ_WRITE_TRIGGER }, { HOLD_2, STR("HOLD 2"), STR(""), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 1, // Reg Start, i.e. 0x40001
		1, // Reg Size
		0, // offset
		PARAM_TYPE_FLOAT,
		2, // Data Size
		OPTS( 0, 0, 0 ),
		PAR_PERMS_READ_WRITE_TRIGGER } };

// Calculate number of parameters in the table
static const uint16_t num_device_parameters = (sizeof(device_parameters) / sizeof(device_parameters[0]));
