#include "../include/expanded-gpio.h"

#include <stdio.h>
#include <errno.h>

#include "../include/peripheral-ads1015.h"
#include "../include/peripheral-mcp23008.h"
#include "../include/peripheral-pca9685.h"
#include "../include/peripheral-ltc2309.h"

// TODO: Portable logging methods

static i2c_interface_t* i2c = NULL;

static int isAddressIntoArray(uint8_t addr, const uint8_t* arr, uint8_t len) {
	while (len--) {
		if (*arr++ == addr) {
			return 1;
		}
	}
	return 0;
}

int initPins(void) {
	if (i2c != NULL) {
		errno = EALREADY;
		return I2C_ALREADY_INITIALIZED;
	}

	i2c = i2c_init(I2C_BUS);
	if (i2c == NULL) {
		return -1;
	}


	int i2c_ret;

        i2c_ret = normal_gpio_init();
	if (i2c_ret < 0) {
		return NORMAL_GPIO_INIT_FAIL;
	}


	for (size_t i = 0; i < NUM_PCA9685; ++i) {
		i2c_ret = pca9685_init(i2c, PCA9685[i]);
		if (i2c_ret < 0) {
		        return PCA9685_INIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_ADS1015; ++i) {
		i2c_ret = ads1015_init(i2c, ADS1015[i]);
		if (i2c_ret < 0) {
			return ADS1015_INIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_MCP23008; ++i) {
		i2c_ret = mcp23008_init(i2c, MCP23008[i]);
		if (i2c_ret < 0) {
			return MCP23008_INIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_LTC2309; ++i) {
		i2c_ret = ltc2309_init(i2c, LTC2309[i]);
		if (i2c_ret < 0) {
			return LTC2309_INIT_FAIL;
		}
	}

	return 0;
}

int deinitPins(void) {
	if (i2c == NULL) {
		errno = EALREADY;
		return -1;
	}


	int i2c_ret;

        i2c_ret = normal_gpio_deinit();
	if (i2c_ret < 0) {
		return NORMAL_GPIO_DEINIT_FAIL;
	}


	for (size_t i = 0; i < NUM_PCA9685; ++i) {
		i2c_ret = pca9685_deinit(i2c, PCA9685[i]);
		if (i2c_ret < 0) {
		        return PCA9685_DEINIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_ADS1015; ++i) {
		i2c_ret = ads1015_deinit(i2c, ADS1015[i]);
		if (i2c_ret < 0) {
			return ADS1015_DEINIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_MCP23008; ++i) {
		i2c_ret = mcp23008_deinit(i2c, MCP23008[i]);
		if (i2c_ret < 0) {
			return MCP23008_DEINIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_LTC2309; ++i) {
		i2c_ret = ltc2309_deinit(i2c, LTC2309[i]);
		if (i2c_ret < 0) {
			return LTC2309_DEINIT_FAIL;
		}
	}

        return i2c_deinit(&i2c);
}

int pinMode(uint32_t pin, uint8_t mode) {
	int i2c_ret;


	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);


	if (addr == 0) {
		i2c_ret = normal_gpio_set_pin_mode(index, mode == OUTPUT ? NORMAL_GPIO_OUTPUT : NORMAL_GPIO_INPUT);
		if (i2c_ret < 0) {
			return NORMAL_GPIO_SET_PIN_MODE_FAIL;
		}

	}

	else if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008)) {
		i2c_ret = mcp23008_set_pin_mode(i2c, addr, index, mode == OUTPUT ? MCP23008_OUTPUT : MCP23008_INPUT);
		if (i2c_ret < 0) {
			return MCP23008_SET_PIN_MODE_FAIL;
		}
	}

	return 0;
}

int digitalWrite(uint32_t pin, uint8_t value) {
	int i2c_ret;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);


	if (addr == 0) {
		i2c_ret = normal_gpio_write(index, value);
		if (i2c_ret < 0) {
			return NORMAL_GPIO_WRITE_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685)) {
		if (value) {
			if (pca9685_set_out_on(i2c, addr, index) < 0) {
				return PCA9685_SET_OUT_FAIL;
			}
		}
		else {
			if (pca9685_set_out_off(i2c, addr, index) < 0) {
				return PCA9685_SET_OUT_FAIL;
			}
		}
	}

	else if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008)) {
		if (mcp23008_write(i2c, addr, index, value) < 0) {
			return MCP23008_SET_OUT_FAIL;
		}
	}

	return 0;
}

int digitalRead(uint32_t pin) {
	uint16_t value;
	int i2c_ret;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);


	if (addr == 0) {
		i2c_ret = normal_gpio_read(pin, (uint8_t*) &value);
		if (i2c_ret < 0) {
			return 0;
		}
	}

	else if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008)) {
		i2c_ret = mcp23008_read(i2c, addr, index, (uint8_t*) &value);
		if (i2c_ret < 0) {
			return 0;
		}
	}
	else if (isAddressIntoArray(addr, LTC2309, NUM_LTC2309)) {
		i2c_ret = ltc2309_read(i2c, addr, index, &value) > 2047 ? 1 : 0;
		if (i2c_ret < 0) {
			return 0;
		}
        }

	else if (isAddressIntoArray(addr, ADS1015, NUM_ADS1015)) {
		i2c_ret = ads1015_read(i2c, addr, index, &value) > 1023 ? 1 : 0;
		if (i2c_ret < 0) {
		        value = 0;
		}
	}

	return 0;
}

int analogWrite(uint32_t pin, uint16_t value) {
	int i2c_ret;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);


	if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685)) {
		i2c_ret = pca9685_set_out_pwm(i2c, addr, index, value);
		if (i2c_ret < 0) {
			return PCA9685_SET_OUT_PWM_FAIL;
		}
	}

	return 0;
}

uint16_t analogRead(uint32_t pin) {
	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	uint16_t value;
	int i2c_ret;

	if (isAddressIntoArray(addr, ADS1015, NUM_ADS1015)) {
		i2c_ret = ads1015_read(i2c, addr, index, &value) > 1023 ? 1 : 0;
		if (i2c_ret < 0) {
			return 0;
		}
	}
	else if (isAddressIntoArray(addr, LTC2309, NUM_LTC2309)) {
		i2c_ret = ltc2309_read(i2c, addr, index, &value) > 2047 ? 1 : 0;
		if (i2c_ret < 0) {
		        return 0;
		}
        }

	return value;
}

int digitalWriteAll(uint8_t addr, uint32_t values) {
	int i2c_ret;

	if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008)) {
		i2c_ret = mcp23008_write_all(i2c, addr, values);
		if (i2c_ret < 0) {
			return PCA9685_WRITE_ALL_FAIL;
		}
	} else if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685)) {
		i2c_ret = pca9685_set_all_digital(i2c, addr, values);
		if (i2c_ret < 0) {
			return MCP23008_WRITE_ALL_FAIL;
		}
	}

	return 0;
}

int digitalReadAll(uint8_t addr, uint8_t* values) {
	int i2c_ret;

	if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008)) {
	        i2c_ret = mcp23008_read_all(i2c, addr, values);
	        if (i2c_ret < 0) {
		        return MCP23008_READ_ALL_FAIL;
	        }
	}

	return 0;
}

int analogWriteAll(uint8_t addr, const uint16_t* values) {
	int i2c_ret;

	if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685)) {
		i2c_ret = pca9685_set_all_analog(i2c, addr, values);
		if (i2c_ret < 0) {
			return PCA9685_SET_ALL_FAIL;
		}
	}

	return 0;
}
