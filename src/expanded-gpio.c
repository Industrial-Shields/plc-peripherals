#include "../include/expanded-gpio.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "../include/i2c-interface.h"
#include "../include/peripheral-ads1015.h"
#include "../include/peripheral-mcp23008.h"
#include "../include/peripheral-pca9685.h"
#include "../include/peripheral-ltc2309.h"
#include "../include/peripheral-mcp23017.h"

#include <assert.h>

// TODO: Portable logging methods

static i2c_interface_t* i2c = NULL;

static int isAddressIntoArray(uint8_t addr, const uint8_t* arr, uint8_t len) {
	while (len--) {
		if (*arr++ == addr) {
			return 0;
		}
	}

	return -1;
}

typedef enum {
	INIT_SUCCESS = 0,
	FIRST_INIT,
	RESTART_DEINIT,
	RESTART_INIT
} init_fail_type_t;

static init_fail_type_t init_device(int (*init_fun)(i2c_interface_t*, uint8_t), int (*deinit_fun)(i2c_interface_t*, uint8_t), const uint8_t* devices, size_t num_devices, bool restart) {
	for (size_t i = 0; i < num_devices; i++) {
		int ret = init_fun(i2c, devices[i]);
		if (ret == 0) continue;
		else if (ret != 1 && errno != EALREADY) {
			return FIRST_INIT;
		}

		if (restart) {
			ret = deinit_fun(i2c, devices[i]);
			if (ret < 0) {
				return RESTART_DEINIT;
			}
			ret = init_fun(i2c, devices[i]);
			if (ret < 0) {
				return RESTART_INIT;
			}
		}
	}

	return INIT_SUCCESS;
}

int initExpandedGPIO(bool restart_peripherals) {
	if (i2c != NULL) {
		errno = EALREADY;
		return I2C_ALREADY_INITIALIZED;
	}

	i2c = i2c_init(I2C_BUS);
	if (i2c == NULL) {
		return -1;
	}


	int ret;

	ret = normal_gpio_init();
	assert(ret == 0);
	if (ret < 0) {
		return NORMAL_GPIO_INIT_FAIL;
	}

	ret = init_device(pca9685_init, pca9685_deinit, PCA9685, NUM_PCA9685, restart_peripherals);
	assert(ret != FIRST_INIT);
	assert(ret != RESTART_DEINIT);
	assert(ret != RESTART_INIT);
	if (ret != INIT_SUCCESS) {
		return PCA9685_INIT_FAIL;
	}

        ret = init_device(ads1015_init, ads1015_deinit, ADS1015, NUM_ADS1015, restart_peripherals);
	assert(ret != FIRST_INIT);
	assert(ret != RESTART_DEINIT);
	assert(ret != RESTART_INIT);
	if (ret != INIT_SUCCESS) {
		return ADS1015_INIT_FAIL;
	}

	ret = init_device(mcp23008_init, mcp23008_deinit, MCP23008, NUM_MCP23008, restart_peripherals);
	assert(ret != FIRST_INIT);
	assert(ret != RESTART_DEINIT);
	assert(ret != RESTART_INIT);
	if (ret != INIT_SUCCESS) {
		return MCP23008_INIT_FAIL;
	}
	
	ret = init_device(ltc2309_init, ltc2309_deinit, LTC2309, NUM_LTC2309, restart_peripherals);
	assert(ret != FIRST_INIT);
	assert(ret != RESTART_DEINIT);
	assert(ret != RESTART_INIT);
	if (ret != INIT_SUCCESS) {
		return LTC2309_INIT_FAIL;
	}
	
	/*
	 * Introduce a delay to allow the MCP23017 devices to stabilize after power-up and reset.
	 * After a power-on reset, the device may not respond to I2C commands. Without this
	 * delay, attempting to communicate with the devices immediately after a reset
	 * may result in a NACK, causing the program to immediately fail.
	 */
	usleep(80 * 1000);

	ret = init_device(mcp23017_init, mcp23017_deinit, MCP23017, NUM_MCP23017, restart_peripherals);
	assert(ret != FIRST_INIT);
	assert(ret != RESTART_DEINIT);
	assert(ret != RESTART_INIT);
	if (ret != INIT_SUCCESS) {
		return MCP23017_INIT_FAIL;
	}
	
	return 0;
}

int deinitExpandedGPIO(void) {
	if (i2c == NULL) {
		errno = EALREADY;
		return -1;
	}


	int ret;

	ret = normal_gpio_deinit();
	assert(ret == 0);
	if (ret < 0) {
		return NORMAL_GPIO_DEINIT_FAIL;
	}

	for (size_t i = 0; i < NUM_PCA9685; ++i) {
		ret = pca9685_deinit(i2c, PCA9685[i]);
		assert(ret == 0);
		if (ret < 0) {
			return PCA9685_DEINIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_ADS1015; ++i) {
		ret = ads1015_deinit(i2c, ADS1015[i]);
		assert(ret == 0);
		if (ret < 0) {
			return ADS1015_DEINIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_MCP23008; ++i) {
		ret = mcp23008_deinit(i2c, MCP23008[i]);
		assert(ret == 0);
		if (ret < 0) {
			return MCP23008_DEINIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_LTC2309; ++i) {
		ret = ltc2309_deinit(i2c, LTC2309[i]);
		assert(ret == 0);
		if (ret < 0) {
			return LTC2309_DEINIT_FAIL;
		}
	}

	for (size_t i = 0; i < NUM_MCP23017; ++i) {
		ret = mcp23017_deinit(i2c, MCP23017[i]);
		assert(ret == 0);
		if (ret < 0) {
			return MCP23017_DEINIT_FAIL;
		}
	}

	return i2c_deinit(&i2c);
}

int pinMode(uint32_t pin, uint8_t mode) {
	int ret = -1;


	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);


	if (addr == 0) {
		ret = normal_gpio_set_pin_mode(index, mode == OUTPUT ? NORMAL_GPIO_OUTPUT : NORMAL_GPIO_INPUT);
		if (ret < 0) {
			return NORMAL_GPIO_SET_PIN_MODE_FAIL;
		}

	}

	else if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008) == 0) {
		ret = mcp23008_set_pin_mode(i2c, addr, index, mode == OUTPUT ? MCP23008_OUTPUT : MCP23008_INPUT);
		if (ret < 0) {
			return MCP23008_SET_PIN_MODE_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, MCP23017, NUM_MCP23017) == 0) {
		ret = mcp23017_set_pin_mode(i2c, addr, index, mode == OUTPUT ? MCP23017_OUTPUT : MCP23017_INPUT);
		if (ret < 0) {
			return MCP23017_SET_PIN_MODE_FAIL;
		}
	}

	return 0;
}

int digitalWrite(uint32_t pin, uint8_t value) {
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);


	if (addr == 0) {
		ret = normal_gpio_write(index, value);
		if (ret < 0) {
			return NORMAL_GPIO_WRITE_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685) == 0) {
		if (pca9685_write(i2c, addr, index, value) != 0) {
			return PCA9685_WRITE_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008) == 0) {
		if (mcp23008_write(i2c, addr, index, value) < 0) {
			return MCP23008_WRITE_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, MCP23017, NUM_MCP23017) == 0) {
		if (mcp23017_write(i2c, addr, index, value) < 0) {
			return MCP23017_WRITE_FAIL;
		}
	}

	return 0;
}

int digitalRead(uint32_t pin) {
	uint16_t value = 0;
	int ret;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);


	if (addr == 0) {
		ret = normal_gpio_read(pin, (uint8_t*) &value);
		assert(ret == 0);
		if (ret < 0) {
			return 0;
		}
	}

	else if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008) == 0) {
		ret = mcp23008_read(i2c, addr, index, (uint8_t*) &value);
		assert(ret == 0);
		if (ret < 0) {
			return 0;
		}
	}

	else if (isAddressIntoArray(addr, LTC2309, NUM_LTC2309) == 0) {
		ret = ltc2309_read(i2c, addr, index, &value);
		assert(ret == 0);
		if (ret < 0) {
			return 0;
		}
		value = value > 1636 ? 1 : 0;
	}

	else if (isAddressIntoArray(addr, ADS1015, NUM_ADS1015) == 0) {
		ret = ads1015_se_read(i2c, addr, index, &value);
		assert(ret == 0);
		if (ret < 0) {
			return 0;
		}
		value = value > 818 ? 1 : 0;
	}

	else if (isAddressIntoArray(addr, MCP23017, NUM_MCP23017) == 0) {
		ret = mcp23017_read(i2c, addr, index, (uint8_t*) &value);
		assert(ret == 0);
		if (ret < 0) {
			return 0;
		}
	}

	return value;
}

int analogWrite(uint32_t pin, uint16_t value) {
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

        if (addr == 0) {
	        return normal_gpio_pwm_write(pin, value);
        }

        else if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685) == 0) {
		ret = pca9685_pwm_write(i2c, addr, index, value);
		assert(ret == 0);
		if (ret < 0) {
			return PCA9685_PWM_WRITE_FAIL;
		}
	}

	return ret;
}

int analogWriteSetFrequency(uint32_t pin, uint32_t desired_freq) {
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);

        if (addr == 0) {
	        return normal_gpio_pwm_frequency(pin, desired_freq);
        }

        if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685) == 0) {
		if (desired_freq < 24 || desired_freq > 1526) {
			errno = ERANGE;
			return -1;
		}

		uint64_t prescaler_value = ((((PCA9685_INTERNAL_CLOCK << 4) / (4096 * desired_freq))) >> 4) - 1;
		if (prescaler_value > 255) {
			errno = EOVERFLOW;
			return -1;
		}

		return pca9685_pwm_frequency(i2c, addr, prescaler_value);
	}

	return ret;
}

uint16_t analogRead(uint32_t pin) {
	uint16_t value = 0;
	int ret;


	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	if (addr == 0) {
		ret = normal_gpio_analog_read(pin, &value);
		assert(ret == 0);
		if (ret < 0) {
			return 0;
		}
	}

	if (isAddressIntoArray(addr, ADS1015, NUM_ADS1015) == 0) {
		ret = ads1015_se_read(i2c, addr, index, &value);
		assert(ret == 0);
		if (ret < 0) {
			return 0;
		}
	}

	else if (isAddressIntoArray(addr, LTC2309, NUM_LTC2309) == 0) {
		ret = ltc2309_read(i2c, addr, index, &value);
		assert(ret == 0);
		if (ret < 0) {
			return 0;
		}
	}

	return value;
}

int digitalWriteAll(uint8_t addr, uint32_t values) {
	int ret = -1;

	if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008) == 0) {
		ret = mcp23008_write_all(i2c, addr, values);
		if (ret < 0) {
			return MCP23008_WRITE_ALL_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685) == 0) {
		ret = pca9685_write_all(i2c, addr, values);
		if (ret < 0) {
			return PCA9685_WRITE_ALL_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, MCP23017, NUM_MCP23017) == 0) {
		ret = mcp23017_write_all(i2c, addr, values);
		if (ret < 0) {
			return MCP23017_WRITE_ALL_FAIL;
		}
	}

	return 0;
}

int digitalReadAll(uint8_t addr, uint16_t* values) {
	int ret = -1;

	if (isAddressIntoArray(addr, MCP23008, NUM_MCP23008) == 0) {
		ret = mcp23008_read_all(i2c, addr, (uint8_t*) values);
		if (ret < 0) {
			return MCP23008_READ_ALL_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, MCP23017, NUM_MCP23017) == 0) {
		ret = mcp23017_read_all(i2c, addr, values);
		if (ret < 0) {
			return MCP23017_READ_ALL_FAIL;
		}
	}

	return 0;
}

int analogWriteAll(uint8_t addr, const uint16_t* values) {
	int ret = -1;

	if (isAddressIntoArray(addr, PCA9685, NUM_PCA9685) == 0) {
		ret = pca9685_pwm_write_all(i2c, addr, values);
		if (ret < 0) {
			return PCA9685_PWM_WRITE_ALL_FAIL;
		}
	}

	return 0;
}
