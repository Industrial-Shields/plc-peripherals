#ifndef __PERIPHERAL_LTC2309_H__
#define __PERIPHERAL_LTC2309_H__

#include <stdint.h>
#include "../include/i2c-interface.h"

#define LTC2309_NUM_INPUTS 8

#ifdef __cplusplus
extern "C" {
#endif

	int ltc2309_init(i2c_interface_t* i2c, uint8_t addr);
	int ltc2309_deinit(i2c_interface_t* i2c, uint8_t addr);
	int ltc2309_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t* read_value);

#ifdef __cplusplus
}
#endif

#endif
