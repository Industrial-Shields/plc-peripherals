#ifndef __PERIPHERAL_MCP23017_H__
#define __PERIPHERAL_MCP23017_H__

#include <stdint.h>
#include "i2c-interface.h"

#define MCP23017_OUTPUT 0
#define MCP23017_INPUT 1

#ifdef __cplusplus
extern "C" {
#endif

	int mcp23017_init(i2c_interface_t* i2c, uint8_t addr);
	int mcp23017_deinit(i2c_interface_t* i2c, uint8_t addr);
	int mcp23017_set_pin_mode(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t mode);
	int mcp23017_set_pin_mode_all(i2c_interface_t* i2c, uint8_t addr, uint16_t modes);
	int mcp23017_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t* value);
        int mcp23017_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value);
	int mcp23017_read_all(i2c_interface_t* i2c, uint8_t addr, uint16_t* value);
        int mcp23017_write_all(i2c_interface_t* i2c, uint8_t addr, uint16_t value);

#ifdef __cplusplus
}
#endif


#endif
