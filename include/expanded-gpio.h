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

#ifndef __EXPANDED_GPIO_H__
#define __EXPANDED_GPIO_H__

/*
 * expanded-gpio aims to provide a unified interface in which you can
 * interact seamlessly with direct GPIOs of the chip you are using, or
 * expanded GPIOs like the ones provided by MCP23008, LTC2309, etc...
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define INPUT 0
#define OUTPUT 1

#define LOW 0
#define HIGH 1

// Macros to extract device address and index from a pin number
#define pinToDeviceAddress(pin) (((pin) >> 8) & 0xff)
#define pinToDeviceIndex(pin) ((pin)&0xff)

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * Those marked with extern are the variables that are expected
	 * to be provided by other library. This is used to define which
	 * I2C bus to use, available peripherals, and basic functions to
	 * interact with the GPIOs of the device (ESP32, Raspberry...).
	 * Without these functions the library won't compile, but you can
	 * compile this library without the capabilities provided by
	 * expanded-gpio.
	 */
	
	extern const uint8_t I2C_BUS;

	extern const uint8_t NORMAL_GPIO_INPUT;
	extern const uint8_t NORMAL_GPIO_OUTPUT;

	extern const uint8_t MCP23008[];
	extern const size_t NUM_MCP23008;

	extern const uint8_t ADS1015[];
	extern const size_t NUM_ADS1015;

	extern const uint8_t PCA9685[];
	extern const size_t NUM_PCA9685;

	extern const uint8_t LTC2309[];
	extern const size_t NUM_LTC2309;

	extern const uint8_t MCP23017[];
	extern const size_t NUM_MCP23017;


	extern int normal_gpio_init(void);
	extern int normal_gpio_deinit(void);

	extern int normal_gpio_set_pin_mode(uint32_t pin, uint8_t mode);

	extern int normal_gpio_write(uint32_t pin, uint8_t value);

	extern int normal_gpio_pwm_frequency(uint32_t pin, uint32_t freq);

	extern int normal_gpio_pwm_write(uint32_t pin, uint16_t value);

	extern int normal_gpio_read(uint32_t pin, uint8_t* read);

	extern int normal_gpio_analog_read(uint32_t pin, uint16_t* read);


	#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	void delay(uint32_t milliseconds);

	void delayMicroseconds(uint32_t micros);
	#endif


	// Error codes for error handling
	enum {
		// initExpandedGPIO & deinitExpandedGPIO
		I2C_ALREADY_INITIALIZED = 1,
		I2C_ALREADY_DEINITIALIZED,
		NORMAL_GPIO_INIT_FAIL,
		PCA9685_INIT_FAIL,
		ADS1015_INIT_FAIL,
		MCP23008_INIT_FAIL,
		LTC2309_INIT_FAIL,
		MCP23017_INIT_FAIL,
		NORMAL_GPIO_DEINIT_FAIL,
		PCA9685_DEINIT_FAIL,
		ADS1015_DEINIT_FAIL,
		MCP23008_DEINIT_FAIL,
		LTC2309_DEINIT_FAIL,
		MCP23017_DEINIT_FAIL,
		// pinMode
		NORMAL_GPIO_SET_PIN_MODE_FAIL,
		MCP23008_SET_PIN_MODE_FAIL,
		MCP23017_SET_PIN_MODE_FAIL,
		// digitalWrite
		NORMAL_GPIO_WRITE_FAIL,
		PCA9685_WRITE_FAIL,
		MCP23008_WRITE_FAIL,
		MCP23017_WRITE_FAIL,
		// digitalRead
		// analogWrite
		NORMAL_GPIO_PWM_WRITE_FAIL,
		PCA9685_PWM_WRITE_FAIL,
		// analogWriteSetFrequency
		NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL,
		PCA9685_PWM_CHANGE_FREQ_FAIL,
		// analogRead
		// digitalWriteAll
		PCA9685_WRITE_ALL_FAIL,
		MCP23008_WRITE_ALL_FAIL,
		MCP23017_WRITE_ALL_FAIL,
		// digitalReadAll
		MCP23008_READ_ALL_FAIL,
		MCP23017_READ_ALL_FAIL,
		// analogWriteAll
		PCA9685_PWM_WRITE_ALL_FAIL
	};

	// Exported functions, useful for Arduino and other environments

	/**
	 * @brief Initializes the expanded GPIO devices.
	 *
	 * This function initializes all the GPIOs defined in the library.
	 *
	 * @param restart_peripherals Flag indicating whether to restart the peripherals on initialization failure.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int initExpandedGPIO(bool restart_peripherals);

	/**
	 * @brief De-initializes the expanded GPIO devices.
	 *
	 * This function de-initializes all the GPIOs defined in the library.
	 *
	 * @param restart_peripherals Flag indicating whether to restart the peripherals on initialization failure.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int deinitExpandedGPIO(void);

	/**
	 * @brief Sets the pin mode for a specified pin.
	 *
	 * @param pin The pin number.
	 * @param mode The mode to set (INPUT or OUTPUT).
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int pinMode(uint32_t pin, uint8_t mode);

	/**
	 * @brief Writes a digital value to the specified pin.
	 *
	 * @param pin The pin number.
	 * @param value The digital value to write (LOW or HIGH).
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int digitalWrite(uint32_t pin, uint8_t value);

	/**
	 * @brief Reads the digital value from the specified pin.
	 *
	 * @param pin The pin number.
	 * @return The digital value read from the pin (LOW or HIGH).
	 */
	int digitalRead(uint32_t pin);

	/**
	 * @brief Writes an analog value to the specified pin.
	 *
	 * @param pin The pin number.
	 * @param value The analog (or PWM) value to write (0-4095).
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int analogWrite(uint32_t pin, uint16_t value);

	/**
	 * @brief Sets the PWM frequency for the specified pin.
	 *
	 * @param pin The pin number.
	 * @param desired_freq The desired PWM frequency in Hertz.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int analogWriteSetFrequency(uint32_t pin, uint32_t desired_freq);

	/**
	 * @brief Reads the analog value from the specified pin.
	 *
	 * @param pin The pin number.
	 * @return The analog value read from the pin (0-4095).
	 */
	uint16_t analogRead(uint32_t pin);

	/**
	 * @brief Writes digital values to all pins of a specified device.
	 *
	 * @param addr The device address.
	 * @param values A bitmask representing the digital values to write to each pin.
	 *               The format will depend on the peripheral used.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int digitalWriteAll(uint8_t addr, uint32_t values);

	/**
	 * @brief Reads digital values from all pins of a specified device.
	 *
	 * @param addr The device address.
	 * @param values Pointer to an array where the read values will be stored.
	 *               The format will depend on the peripheral used.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int digitalReadAll(uint8_t addr, void* values);

	/**
	 * @brief Writes analog values (PWM) to all pins of a specified device.
	 *
	 * @param addr The device address.
	 * @param values Pointer to an array containing the analog values to write (0-4095).
	 *               The format will depend on the peripheral used.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int analogWriteAll(uint8_t addr, const void* values);

#ifdef __cplusplus
}
#endif


#endif // __EXPANDED_GPIO_H__
