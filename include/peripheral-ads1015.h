#ifndef __PERIPHERAL_ADS1015_H__
#define __PERIPHERAL_ADS1015_H__

#include <stdint.h>
#include "../include/i2c-interface.h"

#define ADS1015_NUM_INPUTS 4

#ifdef __cplusplus
extern "C" {
#endif

	int ads1015_init(i2c_interface_t* i2c, uint8_t addr);
	int ads1015_deinit(i2c_interface_t* i2c, uint8_t addr);
	int ads1015_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, int16_t* read_value);

	int ads1015_se_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t* read_value);

#ifdef __cplusplus
}
#endif

#endif
