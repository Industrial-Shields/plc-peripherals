#ifndef __PERIPHERAL_PCA9685_H__
#define __PERIPHERAL_PCA9685_H__

#include <stdint.h>
#include "../include/i2c-interface.h"

#define PCA9685_NUM_OUTPUTS 16
#define PCA9685_INTERNAL_CLOCK 25000000UL // 25 MHz

#ifdef __cplusplus
extern "C" {
#endif

	int pca9685_init(i2c_interface_t* i2c, uint8_t addr);
	int pca9685_deinit(i2c_interface_t* i2c, uint8_t addr);
	int pca9685_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value);
	int pca9685_write_all(i2c_interface_t* i2c, uint8_t addr, uint16_t values);
	int pca9685_pwm_frequency(i2c_interface_t *i2c, uint8_t addr, uint8_t prescaler_value);
	int pca9685_pwm_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t value);
	int pca9685_pwm_write_all(i2c_interface_t* i2c, uint8_t addr, const uint16_t values[PCA9685_NUM_OUTPUTS]);

#ifdef __cplusplus
}
#endif

#endif
