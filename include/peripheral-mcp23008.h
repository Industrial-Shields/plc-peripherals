#ifndef __PERIPHERAL_MCP23008_H__
#define __PERIPHERAL_MCP23008_H__

#include <stdint.h>
#include "../include/i2c-interface.h"

#define MCP23008_NUM_IO 8

#define MCP23008_OUTPUT 0
#define MCP23008_INPUT 1

#ifdef __cplusplus
extern "C" {
#endif

	int mcp23008_init(i2c_interface_t* i2c, uint8_t addr);
	int mcp23008_deinit(i2c_interface_t* i2c, uint8_t addr);
	int mcp23008_set_pin_mode(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t mode);
	int mcp23008_set_pin_mode_all(i2c_interface_t* i2c, uint8_t addr, uint8_t modes);
	int mcp23008_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t* value);
        int mcp23008_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value);
	int mcp23008_read_all(i2c_interface_t* i2c, uint8_t addr, uint8_t* value);
        int mcp23008_write_all(i2c_interface_t* i2c, uint8_t addr, uint8_t value);

#ifdef __cplusplus
}
#endif


#endif
