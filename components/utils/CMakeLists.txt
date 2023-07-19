idf_component_register(SRCS "utils.c" "modbus_utils.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer esp-modbus)
                    
set(EXTRA_COMPONENT_DIRS 
		$ENV{IDF_PATH}/examples/common_components/protocol_examples_common
		$ENV{IDF_PATH}/examples/protocols/modbus/mb_example_common
	)
              
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")      