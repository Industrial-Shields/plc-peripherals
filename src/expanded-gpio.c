/*
 * Copyright (c) 2024 Industrial Shields. All rights reserved
 *
 * This file is part of plc-peripherals.
 *
 * plc-peripherals is free software: you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version
 * 3 of the License, or (at your option) any later version.
 *
 * plc-peripherals is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <expanded-gpio.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <i2c-interface.h>
#include <peripheral-ads1015.h>
#include <peripheral-mcp23008.h>
#include <peripheral-pca9685.h>
#include <peripheral-ltc2309.h>
#include <peripheral-mcp23017.h>

#include <assert.h>

// TODO: Portable logging methods

static i2c_interface_t* i2c = NULL;

/**
 * @brief Checks if an address exists in an array of addresses.
 *
 * @param addr The address to check.
 * @param arr Pointer to the array of addresses.
 * @param len Length of the array.
 * @return 0 if the address is found, -1 otherwise.
 */
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

/**
 * @brief Initializes a device with error handling and optional restart capability.
 *
 * This function initializes a device with error handling and optional restart capability.
 *
 * @param init_fun Pointer to the initialization function of the device.
 * @param deinit_fun Pointer to the deinitialization function of the device.
 * @param devices Array of device addresses.
 * @param num_devices Number of devices in the array.
 * @param restart Flag indicating whether to restart the device on initialization failure.
 * @return INIT_SUCCESS if successful, appropriate error code otherwise.
 */
static init_fail_type_t init_device(int (*init_fun)(i2c_interface_t*, uint8_t), int (*deinit_fun)(i2c_interface_t*, uint8_t), const uint8_t* devices, size_t num_devices, bool restart) {
	for (size_t i = 0; i < num_devices; i++) {
		int ret = init_fun(i2c, devices[i]);;
		if (ret < 0) {
			return FIRST_INIT;
		}

		else if (restart) {
			ret = deinit_fun(i2c, devices[i]);
			if (ret != 0) {
				return RESTART_DEINIT;
			}
			ret = init_fun(i2c, devices[i]);
			if (ret != 0) {
				return RESTART_INIT;
			}
		}
	}

	return INIT_SUCCESS;
}

int initExpandedGPIO(bool restart_peripherals) {
	int ret = -1;

	ret = normal_gpio_init();
	if (ret != 0) {
		return NORMAL_GPIO_INIT_FAIL;
	}


	if (I2C_BUS != PERIPHERALS_NO_I2C_BUS) {
		if (i2c != NULL) {
			return I2C_ALREADY_INITIALIZED;
		}

		i2c = i2c_init(I2C_BUS);
		if (i2c == NULL) {
			return -1;
		}

		ret = init_device(pca9685_init, pca9685_deinit, ARRAY_PCA9685, NUM_ARRAY_PCA9685, restart_peripherals);
		assert(ret != FIRST_INIT);
		assert(ret != RESTART_DEINIT);
		assert(ret != RESTART_INIT);
		if (ret != INIT_SUCCESS) {
			return ARRAY_PCA9685_INIT_FAIL;
		}

		ret = init_device(ads1015_init, ads1015_deinit, ARRAY_ADS1015, NUM_ARRAY_ADS1015, restart_peripherals);
		assert(ret != FIRST_INIT);
		assert(ret != RESTART_DEINIT);
		assert(ret != RESTART_INIT);
		if (ret != INIT_SUCCESS) {
			return ARRAY_ADS1015_INIT_FAIL;
		}

		ret = init_device(mcp23008_init, mcp23008_deinit, ARRAY_MCP23008, NUM_ARRAY_MCP23008, restart_peripherals);
		assert(ret != FIRST_INIT);
		assert(ret != RESTART_DEINIT);
		assert(ret != RESTART_INIT);
		if (ret != INIT_SUCCESS) {
			return ARRAY_MCP23008_INIT_FAIL;
		}

		ret = init_device(ltc2309_init, ltc2309_deinit, ARRAY_LTC2309, NUM_ARRAY_LTC2309, restart_peripherals);
		assert(ret != FIRST_INIT);
		assert(ret != RESTART_DEINIT);
		assert(ret != RESTART_INIT);
		if (ret != INIT_SUCCESS) {
			return ARRAY_LTC2309_INIT_FAIL;
		}

		/**
		 * Introduce a delay to allow the MCP23017 devices to stabilize after power-up and reset.
		 * After a power-on reset, the device may not respond to I2C commands. Without this
		 * delay, attempting to communicate with the devices immediately after a reset
		 * may result in a NACK, causing the program to immediately fail.
		 */
		usleep(100 * 1000);

		ret = init_device(mcp23017_init, mcp23017_deinit, ARRAY_MCP23017, NUM_ARRAY_MCP23017, restart_peripherals);
		assert(ret != FIRST_INIT);
		assert(ret != RESTART_DEINIT);
		assert(ret != RESTART_INIT);
		if (ret != INIT_SUCCESS) {
			return ARRAY_MCP23017_INIT_FAIL;
		}
	}

	return 0;
}

int deinitExpandedGPIO(void) {
	int ret = -1;

	ret = normal_gpio_deinit();
	if (ret != 0) {
		return NORMAL_GPIO_DEINIT_FAIL;
	}


	if (I2C_BUS != PERIPHERALS_NO_I2C_BUS) {
		if (i2c == NULL) {
			return I2C_ALREADY_DEINITIALIZED;
		}

		for (size_t i = 0; i < NUM_ARRAY_PCA9685; ++i) {
			ret = pca9685_deinit(i2c, ARRAY_PCA9685[i]);
			if (ret != 0) {
				return ARRAY_PCA9685_DEINIT_FAIL;
			}
		}

		for (size_t i = 0; i < NUM_ARRAY_ADS1015; ++i) {
			ret = ads1015_deinit(i2c, ARRAY_ADS1015[i]);
			if (ret != 0) {
				return ARRAY_ADS1015_DEINIT_FAIL;
			}
		}

		for (size_t i = 0; i < NUM_ARRAY_MCP23008; ++i) {
			ret = mcp23008_deinit(i2c, ARRAY_MCP23008[i]);
			if (ret != 0) {
				return ARRAY_MCP23008_DEINIT_FAIL;
			}
		}

		for (size_t i = 0; i < NUM_ARRAY_LTC2309; ++i) {
			ret = ltc2309_deinit(i2c, ARRAY_LTC2309[i]);
			if (ret != 0) {
				return ARRAY_LTC2309_DEINIT_FAIL;
			}
		}

		for (size_t i = 0; i < NUM_ARRAY_MCP23017; ++i) {
			ret = mcp23017_deinit(i2c, ARRAY_MCP23017[i]);
			if (ret != 0) {
				return ARRAY_MCP23017_DEINIT_FAIL;
			}
		}

		return i2c_deinit(&i2c);
	}

	else {
		return 0;
	}
}

int pinMode(uint32_t pin, uint8_t mode) {
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	if (addr == 0) {
		ret = normal_gpio_set_pin_mode(index, mode == OUTPUT ? NORMAL_GPIO_OUTPUT : NORMAL_GPIO_INPUT);
		return ret == 0 ? 0 : NORMAL_GPIO_SET_PIN_MODE_FAIL;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}


        if (isAddressIntoArray(addr, ARRAY_MCP23008, NUM_ARRAY_MCP23008) == 0) {
		ret = mcp23008_set_pin_mode(i2c, addr, index, mode == OUTPUT ? MCP23008_OUTPUT : MCP23008_INPUT);
		if (ret != 0) {
			return ARRAY_MCP23008_SET_PIN_MODE_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, ARRAY_MCP23017, NUM_ARRAY_MCP23017) == 0) {
		ret = mcp23017_set_pin_mode(i2c, addr, index, mode == OUTPUT ? MCP23017_OUTPUT : MCP23017_INPUT);
		if (ret != 0) {
			return ARRAY_MCP23017_SET_PIN_MODE_FAIL;
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
		return ret == 0 ? 0 : NORMAL_GPIO_WRITE_FAIL;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}


        if (isAddressIntoArray(addr, ARRAY_PCA9685, NUM_ARRAY_PCA9685) == 0) {
		ret = pca9685_write(i2c, addr, index, value);
		if (ret != 0) {
			return ARRAY_PCA9685_WRITE_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, ARRAY_MCP23008, NUM_ARRAY_MCP23008) == 0) {
		ret = mcp23008_write(i2c, addr, index, value);
		if (ret != 0) {
			return ARRAY_MCP23008_WRITE_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, ARRAY_MCP23017, NUM_ARRAY_MCP23017) == 0) {
		ret = mcp23017_write(i2c, addr, index, value);
		if (ret != 0) {
			return ARRAY_MCP23017_WRITE_FAIL;
		}
	}

	return 0;
}

int digitalRead(uint32_t pin) {
	uint16_t value = 0;
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	if (addr == 0) {
		ret = normal_gpio_read(index, (uint8_t*) &value);
		assert(ret == 0);
		return ret == 0 ? (int) value : 0;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}


	assert(i2c);

	if (isAddressIntoArray(addr, ARRAY_MCP23008, NUM_ARRAY_MCP23008) == 0) {
		ret = mcp23008_read(i2c, addr, index, (uint8_t*) &value);
		assert(ret == 0);
		if (ret != 0) {
			return 0;
		}
	}

	else if (isAddressIntoArray(addr, ARRAY_LTC2309, NUM_ARRAY_LTC2309) == 0) {
		ret = ltc2309_read(i2c, addr, index, &value);
		assert(ret == 0);
		if (ret != 0) {
			return 0;
		}
		value = value > 1636 ? 1 : 0;
	}

	else if (isAddressIntoArray(addr, ARRAY_ADS1015, NUM_ARRAY_ADS1015) == 0) {
		ret = ads1015_unsigned_read(i2c, addr, index, &value);
		assert(ret == 0);
		if (ret != 0) {
			return 0;
		}
		value = value > 818 ? 1 : 0;
	}

	else if (isAddressIntoArray(addr, ARRAY_MCP23017, NUM_ARRAY_MCP23017) == 0) {
		ret = mcp23017_read(i2c, addr, index, (uint8_t*) &value);
		assert(ret == 0);
		if (ret != 0) {
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
	        ret = normal_gpio_pwm_write(index, value);
	        return ret == 0 ? 0 : NORMAL_GPIO_PWM_WRITE_FAIL;
        }

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}


        if (isAddressIntoArray(addr, ARRAY_PCA9685, NUM_ARRAY_PCA9685) == 0) {
		ret = pca9685_pwm_write(i2c, addr, index, value);
		if (ret != 0) {
			return ARRAY_PCA9685_PWM_WRITE_FAIL;
		}
	}

	return ret;
}

int analogWriteSetFrequency(uint32_t pin, uint32_t desired_freq) {
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	if (addr == 0) {
	        ret = normal_gpio_pwm_frequency(index, desired_freq);
	        return ret == 0 ? 0 : NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL;
        }

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}


        if (isAddressIntoArray(addr, ARRAY_PCA9685, NUM_ARRAY_PCA9685) == 0) {
	        assert(desired_freq >= 24 && desired_freq <= 1526);
		if (desired_freq < 24 || desired_freq > 1526) {
			errno = ERANGE;
			return -1;
		}

		uint64_t prescaler_value = ((((PCA9685_INTERNAL_CLOCK << 4) / (4096 * desired_freq))) >> 4) - 1;
		if (prescaler_value > 255) {
			errno = EOVERFLOW;
			return -1;
		}

		ret = pca9685_pwm_frequency(i2c, addr, prescaler_value);
	        assert(ret == 0);
	        if (ret != 0) {
		        return NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL;
	        }
	}

	return ret;
}

uint16_t analogRead(uint32_t pin) {
	uint16_t value = 0;
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	if (addr == 0) {
		ret = normal_gpio_analog_read(index, &value);
		assert(ret == 0);
		return ret == 0 ? value : 0;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}


	assert(i2c);

	if (isAddressIntoArray(addr, ARRAY_ADS1015, NUM_ARRAY_ADS1015) == 0) {
		ret = ads1015_unsigned_read(i2c, addr, index, &value);
		assert(ret == 0);
		if (ret != 0) {
			return 0;
		}
	}

	else if (isAddressIntoArray(addr, ARRAY_LTC2309, NUM_ARRAY_LTC2309) == 0) {
		ret = ltc2309_read(i2c, addr, index, &value);
		assert(ret == 0);
		if (ret != 0) {
			return 0;
		}
	}

	return value;
}

int digitalWriteAll(uint8_t addr, uint32_t values) {
	int ret = -1;


	if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		errno = ENOTSUP;
		return ret;
	}


	if (isAddressIntoArray(addr, ARRAY_MCP23008, NUM_ARRAY_MCP23008) == 0) {
		ret = mcp23008_write_all(i2c, addr, values);
		if (ret != 0) {
			return ARRAY_MCP23008_WRITE_ALL_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, ARRAY_PCA9685, NUM_ARRAY_PCA9685) == 0) {
		ret = pca9685_write_all(i2c, addr, values);
		if (ret != 0) {
			return ARRAY_PCA9685_WRITE_ALL_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, ARRAY_MCP23017, NUM_ARRAY_MCP23017) == 0) {
		ret = mcp23017_write_all(i2c, addr, values);
		if (ret != 0) {
			return ARRAY_MCP23017_WRITE_ALL_FAIL;
		}
	}

	return ret;
}

int digitalReadAll(uint8_t addr, void* values) {
	int ret = -1;


	if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		errno = ENOTSUP;
		return ret;
	}


	if (isAddressIntoArray(addr, ARRAY_MCP23008, NUM_ARRAY_MCP23008) == 0) {
		ret = mcp23008_read_all(i2c, addr, (uint8_t*) values);
		if (ret != 0) {
			return ARRAY_MCP23008_READ_ALL_FAIL;
		}
	}

	else if (isAddressIntoArray(addr, ARRAY_MCP23017, NUM_ARRAY_MCP23017) == 0) {
		ret = mcp23017_read_all(i2c, addr, (uint16_t*) values);
		if (ret != 0) {
			return ARRAY_MCP23017_READ_ALL_FAIL;
		}
	}

	return ret;
}

int analogWriteAll(uint8_t addr, const void* values) {
	int ret = -1;


	if (I2C_BUS == PERIPHERALS_NO_I2C_BUS) {
		errno = ENOTSUP;
		return ret;
	}


	if (isAddressIntoArray(addr, ARRAY_PCA9685, NUM_ARRAY_PCA9685) == 0) {
		ret = pca9685_pwm_write_all(i2c, addr, (uint16_t*) values);
		if (ret != 0) {
			return ARRAY_PCA9685_PWM_WRITE_ALL_FAIL;
		}
	}

	return ret;
}
