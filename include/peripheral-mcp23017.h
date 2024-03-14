#ifndef __PERIPHERAL_MCP23017_H__
#define __PERIPHERAL_MCP23017_H__

#include "../include/i2c-interface.h"

#ifdef __cplusplus
extern "C" {
#endif

        int mcp23017_init(i2c_interface_t* i2c, uint8_t addr);
        int mcp23017_set_pin_mode(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t mode);
	int mcp23017_get_input(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t* value);
        int mcp23017_set_output(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value);

#ifdef __cplusplus
}
#endif


#endif
