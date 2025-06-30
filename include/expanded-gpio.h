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

#define PERIPHERALS_NO_I2C_BUS -1

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * The new system makes full use of the 32 bits we already had
	 * for our pins. The MSB is used to select what type of pin it
	 * is (direct, or peripheral and what type). The other bytes are
	 * used as the implementor sees fit.
	 * This change enables other libraries (that use this as a base)
	 * to select the used peripherals at both run-time or (with some
	 * pre-processor tricks) at compile-time. Plus, normal operations
	 * should be faster due to not searching an array every time an
	 * access is made.
	 */

	typedef enum {
		PLC_DIRECT = 0,
		PLC_PCA9685,
		PLC_MCP23008,
		PLC_MCP23017,
		PLC_LTC2309,
		PLC_ADS1015
	} peripheral_type_t;

        struct peripherals_t {
		const uint8_t* arrayMCP23008;
		size_t numArrayMCP23008;
		const uint8_t* arrayADS1015;
		size_t numArrayADS1015;
		const uint8_t* arrayPCA9685;
		size_t numArrayPCA9685;
		const uint8_t* arrayLTC2309;
		size_t numArrayLTC2309;
		const uint8_t* arrayMCP23017;
		size_t numArrayMCP23017;
	};

#define _MAKE_PIN_PLC(type, byte2, byte3, byte4) (((uint32_t)(type & 0xFF) << 24) | \
                                                  ((uint32_t)(byte2 & 0xFF) << 16) | \
                                                  ((uint32_t)(byte3 & 0xFF) << 8) | \
                                                  ((uint32_t)(byte4 & 0xFF)))

	// Direct pins can have a direction of up to 24 bits
#define MAKE_PIN_DIRECT(index) _MAKE_PIN_PLC(PLC_DIRECT, (index && 0xFF0000) >> 16, (index && 0xFF00) >> 8, index)
	/*
	 * The adapted peripherals until now use the 2th byte as I2C address,
	 * and the 0th byte as the GPIO's index of the peripheral. The 1th
	 * byte is left as 0x00
	 */
#define MAKE_PIN_PCA9685(addr, index) _MAKE_PIN_PLC(PLC_PCA9685, addr, 0x00, index)
#define MAKE_PIN_MCP23008(addr, index) _MAKE_PIN_PLC(PLC_MCP23008, addr, 0x00, index)
#define MAKE_PIN_MCP23017(addr, index) _MAKE_PIN_PLC(PLC_MCP23017, addr, 0x00, index)
#define MAKE_PIN_LTC2309(addr, index) _MAKE_PIN_PLC(PLC_LTC2309, addr, 0x00, index)
#define MAKE_PIN_ADS1015(addr, index) _MAKE_PIN_PLC(PLC_ADS1015, addr, 0x00, index)

	extern struct peripherals_t _peripherals_struct;
#define ARRAY_MCP23008 _peripherals_struct.arrayMCP23008
#define NUM_ARRAY_MCP23008 _peripherals_struct.numArrayMCP23008
#define ARRAY_ADS1015 _peripherals_struct.arrayADS1015
#define NUM_ARRAY_ADS1015 _peripherals_struct.numArrayADS1015
#define ARRAY_PCA9685 _peripherals_struct.arrayPCA9685
#define NUM_ARRAY_PCA9685 _peripherals_struct.numArrayPCA9685
#define ARRAY_LTC2309 _peripherals_struct.arrayLTC2309
#define NUM_ARRAY_LTC2309 _peripherals_struct.numArrayLTC2309
#define ARRAY_MCP23017 _peripherals_struct.arrayMCP23017
#define NUM_ARRAY_MCP23017 _peripherals_struct.numArrayMCP23017

	/*
	 * The following marked with extern are the variables that are
	 * expected to be provided by other library. This is used to
	 * define which I2C bus to use and basic functions to interact
	 * with the GPIOs of the device (ESP32, Raspberry...). Without
	 * these functions the library won't compile, but you can
	 * compile this library without the capabilities provided by
	 * expanded-gpio.
	 */

	extern const int I2C_BUS;

	extern const uint8_t NORMAL_GPIO_INPUT;
	extern const uint8_t NORMAL_GPIO_OUTPUT;

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
		// Initialization and Deinitialization Errors
		I2C_ALREADY_INITIALIZED               = 1,
		I2C_ALREADY_DEINITIALIZED             = 2,
		NORMAL_GPIO_INIT_FAIL                 = 3,
		ARRAY_PCA9685_INIT_FAIL               = 4,
		ARRAY_ADS1015_INIT_FAIL               = 5,
		ARRAY_MCP23008_INIT_FAIL              = 6,
		ARRAY_LTC2309_INIT_FAIL               = 7,
		ARRAY_MCP23017_INIT_FAIL              = 8,
		NORMAL_GPIO_DEINIT_FAIL               = 9,
		ARRAY_PCA9685_DEINIT_FAIL             = 10,
		ARRAY_ADS1015_DEINIT_FAIL             = 11,
		ARRAY_MCP23008_DEINIT_FAIL            = 12,
		ARRAY_LTC2309_DEINIT_FAIL             = 13,
		ARRAY_MCP23017_DEINIT_FAIL            = 14,
		PLC_PERIHPERALS_STRUCT_INVALID        = 33,


		// pinMode Errors
		NORMAL_GPIO_SET_PIN_MODE_FAIL         = 15,
		ARRAY_MCP23008_SET_PIN_MODE_FAIL      = 16,
		ARRAY_MCP23017_SET_PIN_MODE_FAIL      = 17,

		// digitalWrite Errors
		NORMAL_GPIO_WRITE_FAIL                = 18,
		ARRAY_PCA9685_WRITE_FAIL              = 19,
		ARRAY_MCP23008_WRITE_FAIL             = 20,
		ARRAY_MCP23017_WRITE_FAIL             = 21,

		// PWM Errors
		NORMAL_GPIO_PWM_WRITE_FAIL            = 22,
		ARRAY_PCA9685_PWM_WRITE_FAIL          = 23,

		// PWM Frequency Change Errors
		NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL      = 24,
		ARRAY_PCA9685_PWM_CHANGE_FREQ_FAIL    = 25,

		// digitalWriteAll Errors
		ARRAY_PCA9685_WRITE_ALL_FAIL          = 26,
		ARRAY_MCP23008_WRITE_ALL_FAIL         = 27,
		ARRAY_MCP23017_WRITE_ALL_FAIL         = 28,

		// digitalReadAll Errors
		ARRAY_MCP23008_READ_ALL_FAIL          = 29,
		ARRAY_MCP23017_READ_ALL_FAIL          = 30,

		// analogWriteAll Errors
		ARRAY_PCA9685_PWM_WRITE_ALL_FAIL      = 31,

		// Others
		I2C_PIN_WITHOUT_I2C_BUS               = 32
	};


	// Exported functions, useful for Arduino and other environments

	/**
	 * @brief Initializes the expanded GPIO devices.
	 *
	 * This function initializes all the GPIOs defined in the passed array.
	 *
	 * @param peripherals Struct that indicates which peripherals should be initialized.
	 * @param restart Flag indicating whether to restart the peripherals on initialization failure.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int initExpandedGPIO(bool restart);

	/**
	 * @brief De-initializes the expanded GPIO devices.
	 *
	 * This function de-initializes all the GPIOs defined in the library.
	 *
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int deinitExpandedGPIO(void);

	/**
	 * @brief De-initializes the expanded GPIO devices.
	 *
	 * This function de-initializes all the GPIOs defined in the library without
	 * resetting the peripherals associated.
	 *
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int deinitExpandedGPIONoReset(void);

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
