/*
 * Copyright (c) 2026 Industrial Shields. All rights reserved
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

#ifndef PLC_XGPIO_H_
#define PLC_XGPIO_H_

/*
 * xgpio aims to provide a unified interface in which you can interact
 * seamlessly with direct GPIOs of the chip you are using, or expanded GPIOs
 * like the ones provided by MCP23008, LTC2309, etc...
 *
 * Nothing is initialized here. Define plc_xgpio_i2c, and plc_xgpio_peripherals
 * pointing to a plc_xgpio_peripherals_t with the arrays of peripherals in use,
 * each handle already initialized before calling any function.
 *
 * This module cannot be compiled by itself. It needs a supplementary library
 * that provides the methods to interact with the direct (aka normal) GPIOs
 * of the platform.
 */

#include "peripheral-ads101x.h"
#include "peripheral-ltc2309.h"
#include "peripheral-mcp230xx.h"
#include "peripheral-pca9685.h"

#include <stddef.h>
#include <stdint.h>

// The accepted modes of plc_xgpio_set_pin_mode
// clang-format off
#define PLC_XGPIO_INPUT  0
#define PLC_XGPIO_OUTPUT 1
// clang-format on

// The accepted values of plc_xgpio_digital_write/_all
#define PLC_XGPIO_LOW 0
#define PLC_XGPIO_HIGH 1

// The value of plc_xgpio_i2c when there is no I2C bus
#define PLC_XGPIO_NO_I2C_BUS NULL

// A default timeout value you can use for waiting peripherals locks.
#define PLC_XGPIO_DEFAULT_TIMEOUT_MS 25

// clang-format off
/*
 * Above these values, plc_xgpio_digital_read reads a pin as high. The LTC2309
 * and ADS101X ones are as in v2.0.0.
 */
#define PLC_XGPIO_PCA9685_DEFAULT_HIGH_THRESHOLD 0
#define PLC_XGPIO_LTC2309_DEFAULT_HIGH_THRESHOLD 1636
#define PLC_XGPIO_ADS101X_DEFAULT_HIGH_THRESHOLD 818

// The modes of an LTC2309 pin, in its 1st byte
#define PLC_XGPIO_LTC2309_SINGLE_ENDED           0
#define PLC_XGPIO_LTC2309_DIFFERENTIAL           1
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A pin is 32 bits. The MSB is its type, as defined by peripheral_type_t
 * enum. The other three bytes can be freely used by the xgpio module.
 */
typedef uint32_t plc_xgpio_pin_t;

typedef enum {
	PLC_DIRECT = 0,
	PLC_PCA9685 = 1,
	PLC_MCP230XX = 2,
	PLC_LTC2309 = 3,
	PLC_ADS101X = 4,
} peripheral_type_t;

typedef struct {
	mcp230xx_t handle;
	uint32_t timeout_ms;
	uint8_t addr;
} plc_xgpio_mcp230xx_t;

typedef struct {
	pca9685_t handle;
	uint32_t timeout_ms;
	uint8_t addr;
} plc_xgpio_pca9685_t;

typedef struct {
	ltc2309_t handle;
	uint32_t timeout_ms;
	uint8_t addr;
} plc_xgpio_ltc2309_t;

typedef struct {
	ads101x_t handle;
	uint32_t timeout_ms;
	uint8_t addr;
} plc_xgpio_ads101x_t;

/*
 * The peripherals the pins refer to, all on plc_xgpio_i2c. Each array has its
 * num_* entries. An array can only be NULL if its num_* is 0.
 */
typedef struct {
	plc_xgpio_pca9685_t* pca9685;
	size_t num_pca9685;
	plc_xgpio_mcp230xx_t* mcp230xx;
	size_t num_mcp230xx;
	plc_xgpio_ltc2309_t* ltc2309;
	size_t num_ltc2309;
	plc_xgpio_ads101x_t* ads101x;
	size_t num_ads101x;
} plc_xgpio_peripherals_t;

/*
 * The peripherals xgpio uses. The library that uses this module must
 * define this pointer to the struct at start-up.
 *
 * It can be NULL. In that case, the functions fail on any pin but a direct one
 * with I2C_PIN_WITHOUT_PERIPHERALS.
 */
extern const plc_xgpio_peripherals_t* const plc_xgpio_peripherals;

/*
 * The I2C interface the peripherals are on.
 *
 * It can be PLC_XGPIO_NO_I2C_BUS. In that case, the calls that reach a
 * peripheral fail with I2C_PIN_WITHOUT_I2C_BUS, and the *_all ones with
 * I2C_ADDR_WITHOUT_I2C_BUS.
 */
extern const i2c_interface_t* plc_xgpio_i2c;
#define PLC_XGPIO_ARRAY_PCA9685 (plc_xgpio_peripherals->pca9685)
#define PLC_XGPIO_NUM_ARRAY_PCA9685 (plc_xgpio_peripherals->num_pca9685)
#define PLC_XGPIO_ARRAY_MCP230XX (plc_xgpio_peripherals->mcp230xx)
#define PLC_XGPIO_NUM_ARRAY_MCP230XX (plc_xgpio_peripherals->num_mcp230xx)
#define PLC_XGPIO_ARRAY_LTC2309 (plc_xgpio_peripherals->ltc2309)
#define PLC_XGPIO_NUM_ARRAY_LTC2309 (plc_xgpio_peripherals->num_ltc2309)
#define PLC_XGPIO_ARRAY_ADS101X (plc_xgpio_peripherals->ads101x)
#define PLC_XGPIO_NUM_ARRAY_ADS101X (plc_xgpio_peripherals->num_ads101x)

/*
 * Pin macros to define an xgpio pin.
 *
 * Direct pins can use up to 24 bits to address a GPIO.
 *
 * The adapted peripherals until now use the 2nd byte as the I2C address, and
 * the 0th byte as the GPIO's index of the peripheral. The 1st byte is the mode
 * of the pin, 0x00 for every peripheral but the LTC2309:
 *   - PLC_XGPIO_LTC2309_SINGLE_ENDED : The index is an LTC2309_INPUT.
 *   - PLC_XGPIO_LTC2309_DIFFERENTIAL : The index is an LTC2309_DIFF_INPUT.
 */

#define _MAKE_PIN_PLC(type, addr, index, pin) \
	(((uint32_t)((type) & 0xFF) << 24) |  \
	 ((uint32_t)((addr) & 0xFF) << 16) |  \
	 ((uint32_t)((index) & 0xFF) << 8) | ((uint32_t)((pin) & 0xFF)))

#define MAKE_PIN_DIRECT(index)                    \
	_MAKE_PIN_PLC(PLC_DIRECT,                 \
		      ((index) & 0xFF0000) >> 16, \
		      ((index) & 0xFF00) >> 8,    \
		      (index))

#define MAKE_PIN_PCA9685(addr, index) \
	_MAKE_PIN_PLC(PLC_PCA9685, (addr), 0x00, (index))
#define MAKE_PIN_MCP230XX(addr, index) \
	_MAKE_PIN_PLC(PLC_MCP230XX, (addr), 0x00, (index))
#define MAKE_PIN_LTC2309(addr, index) \
	_MAKE_PIN_PLC(                \
		PLC_LTC2309, (addr), PLC_XGPIO_LTC2309_SINGLE_ENDED, (index))
#define MAKE_PIN_LTC2309_DIFF(addr, index) \
	_MAKE_PIN_PLC(                     \
		PLC_LTC2309, (addr), PLC_XGPIO_LTC2309_DIFFERENTIAL, (index))
#define MAKE_PIN_ADS101X(addr, index) \
	_MAKE_PIN_PLC(PLC_ADS101X, (addr), 0x00, (index))

/*
 * The following externs are expected to be provided by the library using this
 * one, to interact with the direct GPIOs of the device (ESP32,
 * Raspberry...). They are only called for PLC_DIRECT pins, and get the direct
 * pin's index (its 24 low bits) as "pin".
 *
 * On failure they must set errno. The xgpio function that called them returns
 * with errno as they left it.
 */

/**
 * plc_normal_gpio_set_pin_mode
 *
 * Set a direct GPIO as input or as output.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The index of the direct GPIO.
 *   mode (uint8_t)         - PLC_XGPIO_INPUT or PLC_XGPIO_OUTPUT.
 * Returns:
 *   int - 0 if successful, 1 if it already had that mode, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL  : "pin" is not an existent pin, or "mode" is neither
 *                 PLC_XGPIO_INPUT nor PLC_XGPIO_OUTPUT.
 *     - ENOTSUP : "mode" is not supported.
 *     - Linux specific :
 *         - (others)   : Whatever ioctl(2) reports for the GPIO line request.
 */
extern int plc_normal_gpio_set_pin_mode(plc_xgpio_pin_t pin, uint8_t mode);

/**
 * plc_normal_gpio_digital_write
 *
 * Set a direct GPIO high or low.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The index of the direct GPIO.
 *   value (uint8_t)        - PLC_XGPIO_HIGH or PLC_XGPIO_LOW.
 * Returns:
 *   int - 0 if successful, 1 if it already had that level, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL  : "pin" is not an existent pin, or "value" is neither
 *                 PLC_XGPIO_HIGH nor PLC_XGPIO_LOW.
 *     - ENOTSUP : "pin" can't be an output.
 *     - Linux specific :
 *         - (others)   : Whatever ioctl(2) reports for the GPIO line request
 *                        (the GPIO is set as output first, if it wasn't), or
 *                        to set its value.
 */
extern int plc_normal_gpio_digital_write(plc_xgpio_pin_t pin, uint8_t value);

/**
 * plc_normal_gpio_digital_read
 *
 * Read the level of a direct GPIO.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The index of the direct GPIO.
 *   read (uint8_t*)        - Where to store the level.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT  : "read" is NULL.
 *     - EINVAL  : "pin" is not an existent pin.
 *     - ENOTSUP : "pin" can't be an input.
 *     - Linux specific :
 *         - (others)   : Whatever ioctl(2) reports for the GPIO line request
 *                        (the GPIO is set as input first, if it wasn't), or
 *                        to get its value.
 */
extern int plc_normal_gpio_digital_read(plc_xgpio_pin_t pin, uint8_t* read);

/**
 * plc_normal_gpio_pwm_write
 *
 * Set the PWM duty cycle of a direct GPIO.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The index of the direct GPIO.
 *   value (uint32_t)       - The duty cycle, as passed to
 *                            plc_xgpio_pwm_write. Its scale is up to this
 *                            function.
 * Returns:
 *   int - 0 if successful, 1 if it already had that duty cycle, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL  : "pin" is not an existent pin.
 *     - ERANGE  : "value" is out of the pin's range. The minimum and maximum
 *                 duty cycles are platform dependent.
 *     - ENOTSUP : "pin" doesn't support PWM.
 */
extern int plc_normal_gpio_pwm_write(plc_xgpio_pin_t pin, uint32_t value);

/**
 * plc_normal_gpio_set_pwm_frequency
 *
 * Set the PWM frequency of a direct GPIO.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The index of the direct GPIO.
 *   freq (uint32_t)        - The frequency, in Hz, as passed to
 *                            plc_xgpio_set_pwm_frequency.
 * Returns:
 *   int - 0 if successful, 1 if it already had that frequency, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL  : "pin" is not an existent pin.
 *     - ERANGE  : "freq" is out of the pin's range. The minimum and maximum
 *                 frequencies are platform dependent.
 *     - ENOTSUP : "pin" doesn't support PWM.
 */
extern int plc_normal_gpio_set_pwm_frequency(plc_xgpio_pin_t pin,
					     uint32_t freq);

/**
 * plc_normal_gpio_analog_read
 *
 * Read the analog value of a direct GPIO.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The index of the direct GPIO.
 *   read (uint32_t*)       - Where to store the reading. Its scale is up to
 *                            this function, and always unsigned.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT  : "read" is NULL.
 *     - EINVAL  : "pin" is not an existent pin.
 *     - ENOTSUP : "pin" has no analog reads.
 */
extern int plc_normal_gpio_analog_read(plc_xgpio_pin_t pin, uint32_t* read);

/*
 * The errors reported by xgpio functions.
 *
 * When one fails because a driver or plc_normal_gpio_* call failed, it returns
 * with errno as that call left it.
 */
enum {
	// clang-format off
	// plc_xgpio_set_pin_mode
	NORMAL_GPIO_SET_PIN_MODE_FAIL              = -1,
	PERIPHERAL_MCP230XX_SET_PIN_MODE_FAIL      = -2,
	PERIPHERAL_ADS101X_SET_PIN_MODE_FAIL       = -3,

	// plc_xgpio_digital_write
	NORMAL_GPIO_DIGITAL_WRITE_FAIL             = -4,
	PERIPHERAL_PCA9685_DIGITAL_WRITE_FAIL      = -5,
	PERIPHERAL_MCP230XX_DIGITAL_WRITE_FAIL     = -6,

	// plc_xgpio_digital_read
	NORMAL_GPIO_DIGITAL_READ_FAIL              = -7,
	PERIPHERAL_PCA9685_DIGITAL_READ_FAIL       = -8,
	PERIPHERAL_MCP230XX_DIGITAL_READ_FAIL      = -9,
	PERIPHERAL_LTC2309_DIGITAL_READ_FAIL       = -10,
	PERIPHERAL_ADS101X_DIGITAL_READ_FAIL       = -11,

	// plc_xgpio_pwm_write
	NORMAL_GPIO_PWM_WRITE_FAIL                 = -12,
	PERIPHERAL_PCA9685_PWM_WRITE_FAIL          = -13,

	// plc_xgpio_set_pwm_frequency
	NORMAL_GPIO_SET_PWM_FREQUENCY_FAIL         = -14,
	PERIPHERAL_PCA9685_SET_PWM_FREQUENCY_FAIL  = -15,

	// plc_xgpio_analog_read
	NORMAL_GPIO_ANALOG_READ_FAIL               = -16,
	PERIPHERAL_LTC2309_ANALOG_READ_FAIL        = -17,
	PERIPHERAL_ADS101X_ANALOG_READ_FAIL        = -18,

	// plc_xgpio_digital_write_all
	PERIPHERAL_PCA9685_DIGITAL_WRITE_ALL_FAIL  = -19,
	PERIPHERAL_MCP230XX_DIGITAL_WRITE_ALL_FAIL = -20,

	// plc_xgpio_digital_read_all
	PERIPHERAL_MCP230XX_DIGITAL_READ_ALL_FAIL  = -21,

	// plc_xgpio_pwm_write_all
	PERIPHERAL_PCA9685_PWM_WRITE_ALL_FAIL      = -22,

	// Others
	I2C_PIN_WITHOUT_I2C_BUS                    = -23,
	I2C_ADDR_WITHOUT_I2C_BUS                   = -24,
	PIN_NOT_SUPPORTED                          = -25,
	I2C_ADDR_NOT_FOUND                         = -26,
	NULL_ARGUMENT                              = -27,
	FREQ_OUT_OF_RANGE                          = -28,
	DUTY_CYCLE_OUT_OF_RANGE                    = -29,
	UNKNOWN_PIN_TYPE                           = -30,
	INDEX_OUT_OF_RANGE                         = -31,
	I2C_PIN_WITHOUT_PERIPHERALS                = -32,
	UNSUPPORTED_PERIPHERAL_TYPE                = -33,
	I2C_ADDR_WITHOUT_PERIPHERALS               = -34,
	VALUES_OUT_OF_RANGE                        = -35,
	UNKNOWN_MODE                               = -36,
	// clang-format on
};

/**
 * plc_xgpio_set_pin_mode
 *
 * Set a GPIO as input or as output.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The pin.
 *   mode (uint8_t)         - PLC_XGPIO_INPUT or PLC_XGPIO_OUTPUT.
 * Returns:
 *   int - 0 if successful, 1 if it already had that mode, otherwise a negative
 *         value:
 *         - NORMAL_GPIO_SET_PIN_MODE_FAIL         : The direct GPIO call
 *                                                   failed.
 *         - PERIPHERAL_MCP230XX_SET_PIN_MODE_FAIL : The MCP230XX call failed.
 *         - PERIPHERAL_ADS101X_SET_PIN_MODE_FAIL  : The ADS101X call failed.
 *         - I2C_ADDR_NOT_FOUND                    : The peripheral isn't in the
 *                                                   arrays.
 *         - INDEX_OUT_OF_RANGE                    : The peripheral doesn't have
 *                                                   the pin's index, or the
 *                                                   direct GPIO doesn't exist.
 *         - I2C_PIN_WITHOUT_PERIPHERALS           : It isn't a direct pin, and
 *                                                   plc_xgpio_peripherals is
 *                                                   NULL.
 *         - I2C_PIN_WITHOUT_I2C_BUS               : It isn't a direct pin, and
 *                                                   there is no I2C bus.
 *         - PIN_NOT_SUPPORTED                     : The mode is invalid, or the
 *                                                   pin can't have it.
 *         - UNSUPPORTED_PERIPHERAL_TYPE           : The MCP230XX or ADS101X is
 *                                                   a type xgpio doesn't know.
 *         - UNKNOWN_MODE                          : The pin mode is unknown.
 *         - UNKNOWN_PIN_TYPE                      : The pin type is unknown.
 */
int plc_xgpio_set_pin_mode(plc_xgpio_pin_t pin, uint8_t mode);

/**
 * plc_xgpio_digital_write
 *
 * Set a GPIO high or low.
 *
 * Supported pins: PLC_DIRECT, PLC_PCA9685 and PLC_MCP230XX.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The pin.
 *   value (uint8_t)        - PLC_XGPIO_HIGH or PLC_XGPIO_LOW. Any other
 *                            non-zero value is PLC_XGPIO_HIGH.
 * Returns:
 *   int - 0 if successful, 1 if it already had that level, otherwise a
 *         negative value:
 *         - NORMAL_GPIO_DIGITAL_WRITE_FAIL         : The direct GPIO call
 *                                                    failed.
 *         - PERIPHERAL_PCA9685_DIGITAL_WRITE_FAIL  : The PCA9685 call failed.
 *         - PERIPHERAL_MCP230XX_DIGITAL_WRITE_FAIL : The MCP230XX call failed.
 *         - I2C_ADDR_NOT_FOUND                     : The peripheral isn't in
 *                                                    the arrays.
 *         - INDEX_OUT_OF_RANGE                     : The peripheral doesn't
 *                                                    have the pin's index, or
 *                                                    the direct GPIO doesn't
 *                                                    exist.
 *         - I2C_PIN_WITHOUT_I2C_BUS                : It isn't a direct pin, and
 *                                                    there is no I2C bus.
 *         - I2C_PIN_WITHOUT_PERIPHERALS            : It isn't a direct pin, and
 *                                                    plc_xgpio_peripherals is
 *                                                    NULL.
 *         - PIN_NOT_SUPPORTED                      : The pin can't be written.
 *         - UNSUPPORTED_PERIPHERAL_TYPE            : The MCP230XX is a type
 *                                                    xgpio doesn't know.
 *         - UNKNOWN_PIN_TYPE                       : The pin type is unknown.
 */
int plc_xgpio_digital_write(plc_xgpio_pin_t pin, uint8_t value);

/**
 * plc_xgpio_digital_read
 *
 * Read the level of a GPIO.
 *
 * Supported pins: PLC_DIRECT, PLC_PCA9685 (high above 0), PLC_MCP230XX,
 * PLC_LTC2309 (single-ended or differential, unipolar, high above 1636) and
 * PLC_ADS101X (single-shot or continuous, high above 818).
 *
 * WARNING: An ADS101X differential pair whose positive input is under the
 * negative one will fail, because the reading is unsigned.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The pin.
 *   read (uint8_t*)        - Where to store PLC_XGPIO_HIGH or PLC_XGPIO_LOW.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - NORMAL_GPIO_DIGITAL_READ_FAIL         : The direct GPIO call
 *                                                   failed.
 *         - PERIPHERAL_PCA9685_DIGITAL_READ_FAIL  : The PCA9685 call failed.
 *         - PERIPHERAL_MCP230XX_DIGITAL_READ_FAIL : The MCP230XX call failed.
 *         - PERIPHERAL_LTC2309_DIGITAL_READ_FAIL  : The LTC2309 call failed.
 *         - PERIPHERAL_ADS101X_DIGITAL_READ_FAIL  : The ADS101X call failed.
 *         - I2C_ADDR_NOT_FOUND                    : The peripheral isn't in the
 *                                                   arrays.
 *         - INDEX_OUT_OF_RANGE                    : The peripheral doesn't have
 *                                                   the pin's index, or the
 *                                                   direct GPIO doesn't exist.
 *         - I2C_PIN_WITHOUT_I2C_BUS               : It isn't a direct pin, and
 *                                                   there is no I2C bus.
 *         - I2C_PIN_WITHOUT_PERIPHERALS           : It isn't a direct pin, and
 *                                                   plc_xgpio_peripherals is
 *                                                   NULL.
 *         - NULL_ARGUMENT                         : "read" is NULL.
 *         - PIN_NOT_SUPPORTED                     : The pin can't be read.
 *         - UNSUPPORTED_PERIPHERAL_TYPE           : The MCP230XX or ADS101X is
 *                                                   a type xgpio doesn't know.
 *         - UNKNOWN_MODE                          : The pin mode is unknown.
 *         - UNKNOWN_PIN_TYPE                      : The pin type is unknown.
 */
int plc_xgpio_digital_read(plc_xgpio_pin_t pin, uint8_t* read);

/**
 * plc_xgpio_pwm_write
 *
 * Set the PWM duty cycle of a GPIO.
 *
 * Supported pins: PLC_DIRECT and PLC_PCA9685.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The pin.
 *   value (uint32_t)       - The duty cycle. For a PCA9685, from
 *                            PCA9685_OUTPUT_OFF to PCA9685_OUTPUT_ON.
 * Returns:
 *   int - 0 if successful, 1 if it already had that duty cycle, otherwise a
 *         negative value:
 *         - NORMAL_GPIO_PWM_WRITE_FAIL        : The direct GPIO call failed.
 *         - PERIPHERAL_PCA9685_PWM_WRITE_FAIL : The PCA9685 call failed.
 *         - DUTY_CYCLE_OUT_OF_RANGE           : The duty cycle is out of the
 *                                               pin's range.
 *         - I2C_ADDR_NOT_FOUND                : The peripheral isn't in the
 *                                               arrays.
 *         - INDEX_OUT_OF_RANGE                : The peripheral doesn't have the
 *                                               pin's index, or the direct GPIO
 *                                               doesn't exist.
 *         - I2C_PIN_WITHOUT_I2C_BUS           : It isn't a direct pin, and
 *                                               there is no I2C bus.
 *         - I2C_PIN_WITHOUT_PERIPHERALS       : It isn't a direct pin, and
 *                                               plc_xgpio_peripherals is NULL.
 *         - PIN_NOT_SUPPORTED                 : The pin doesn't support PWM.
 *         - UNKNOWN_PIN_TYPE                  : The pin type is unknown.
 */
int plc_xgpio_pwm_write(plc_xgpio_pin_t pin, uint32_t value);

/**
 * plc_xgpio_set_pwm_frequency
 *
 * Set the PWM frequency of a GPIO.
 *
 * Supported pins: PLC_DIRECT and PLC_PCA9685.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The pin.
 *   freq_hz (uint32_t)     - The frequency. For a PCA9685, from
 *                            PCA9685_MIN_FREQ_HZ to PCA9685_MAX_FREQ_HZ.
 * Returns:
 *   int - 0 if successful, 1 if it already had that frequency, otherwise a
 *         negative value:
 *         - NORMAL_GPIO_SET_PWM_FREQUENCY_FAIL        : The direct GPIO call
 *                                                       failed.
 *         - PERIPHERAL_PCA9685_SET_PWM_FREQUENCY_FAIL : The PCA9685 call
 *                                                       failed.
 *         - FREQ_OUT_OF_RANGE                         : The frequency is out of
 *                                                       the pin's range.
 *         - I2C_ADDR_NOT_FOUND                        : The peripheral isn't in
 *                                                       the arrays.
 *         - INDEX_OUT_OF_RANGE                        : The peripheral doesn't
 *                                                       have the pin's index,
 *                                                       or the direct GPIO
 *                                                       doesn't exist.
 *         - I2C_PIN_WITHOUT_I2C_BUS                   : It isn't a direct pin,
 *                                                       and there is no I2C
 *                                                       bus.
 *         - I2C_PIN_WITHOUT_PERIPHERALS               : It isn't a direct pin,
 *                                                       and
 *                                                       plc_xgpio_peripherals
 *                                                       is NULL.
 *         - PIN_NOT_SUPPORTED                         : The pin doesn't support
 *                                                       PWM.
 *         - UNKNOWN_PIN_TYPE                          : The pin type is
 *                                                       unknown.
 */
int plc_xgpio_set_pwm_frequency(plc_xgpio_pin_t pin, uint32_t freq_hz);

/**
 * plc_xgpio_analog_read
 *
 * Read the analog value of a GPIO.
 *
 * Supported pins: PLC_DIRECT, PLC_LTC2309 (single-ended or differential,
 * unipolar, 0 to 4095) and PLC_ADS101X (single-shot or continuous, 0 to 2047).
 *
 * WARNING: An ADS101X differential pair whose positive input is under the
 * negative one will fail, because the reading is unsigned.
 *
 * Parameters:
 *   pin (plc_xgpio_pin_t)  - The pin.
 *   read (uint32_t*)       - Where to store the reading.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - NORMAL_GPIO_ANALOG_READ_FAIL        : The direct GPIO call failed.
 *         - PERIPHERAL_LTC2309_ANALOG_READ_FAIL : The LTC2309 call failed.
 *         - PERIPHERAL_ADS101X_ANALOG_READ_FAIL : The ADS101X call failed.
 *         - I2C_ADDR_NOT_FOUND                  : The peripheral isn't in the
 *                                                 arrays.
 *         - INDEX_OUT_OF_RANGE                  : The peripheral doesn't have
 *                                                 the pin's index, or the
 *                                                 direct GPIO doesn't exist.
 *         - I2C_PIN_WITHOUT_I2C_BUS             : It isn't a direct pin, and
 *                                                 there is no I2C bus.
 *         - I2C_PIN_WITHOUT_PERIPHERALS         : It isn't a direct pin, and
 *                                                 plc_xgpio_peripherals is
 *                                                 NULL.
 *         - NULL_ARGUMENT                       : "read" is NULL.
 *         - PIN_NOT_SUPPORTED                   : The pin doesn't support
 *                                                 analog reads.
 *         - UNSUPPORTED_PERIPHERAL_TYPE         : The ADS101X is a type xgpio
 *                                                 doesn't know.
 *         - UNKNOWN_MODE                        : The pin mode is unknown.
 *         - UNKNOWN_PIN_TYPE                    : The pin type is unknown.
 */
int plc_xgpio_analog_read(plc_xgpio_pin_t pin, uint32_t* read);

/**
 * plc_xgpio_digital_write_all
 *
 * Set every output of the peripheral at "addr" high or low at once. Bit i is
 * output i.
 *
 * Supported peripherals: PCA9685 and MCP230XX, searched in that order.
 *
 * Parameters:
 *   addr (uint8_t)     - The I2C address of the peripheral.
 *   values (uint32_t)  - The levels.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - PERIPHERAL_PCA9685_DIGITAL_WRITE_ALL_FAIL  : The PCA9685 call
 *                                                        failed.
 *         - PERIPHERAL_MCP230XX_DIGITAL_WRITE_ALL_FAIL : The MCP230XX call
 *                                                        failed.
 *         - I2C_ADDR_NOT_FOUND                         : No PCA9685 or MCP230XX
 *                                                        is at "addr".
 *         - VALUES_OUT_OF_RANGE                        : A bit is set for an
 *                                                        output the peripheral
 *                                                        doesn't have.
 *         - UNSUPPORTED_PERIPHERAL_TYPE                : The MCP230XX at "addr"
 *                                                        is a type xgpio
 *                                                        doesn't know.
 *         - I2C_ADDR_WITHOUT_I2C_BUS                   : There is no I2C bus.
 *         - I2C_ADDR_WITHOUT_PERIPHERALS               : plc_xgpio_peripherals
 *                                                        is NULL.
 */
int plc_xgpio_digital_write_all(uint8_t addr, uint32_t values);

/**
 * plc_xgpio_digital_read_all
 *
 * Read the level of every GPIO of the peripheral at "addr" at once. Bit i is
 * GPIO i.
 *
 * Supported peripherals: MCP23008 and MCP23017.
 *
 * Parameters:
 *   addr (uint8_t)  - The I2C address of the peripheral.
 *   values (void*)  - Where to store the levels: a uint8_t for an MCP23008, a
 *                     uint16_t for an MCP23017.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - PERIPHERAL_MCP230XX_DIGITAL_READ_ALL_FAIL : The MCP230XX call
 *                                                       failed.
 *         - NULL_ARGUMENT                             : "values" is NULL.
 *         - I2C_ADDR_NOT_FOUND                        : No MCP23008 or MCP23017
 *                                                       is at "addr".
 *         - UNSUPPORTED_PERIPHERAL_TYPE               : The MCP230XX at "addr"
 *                                                       is a type xgpio doesn't
 *                                                       know.
 *         - I2C_ADDR_WITHOUT_I2C_BUS                  : There is no I2C bus.
 *         - I2C_ADDR_WITHOUT_PERIPHERALS              : plc_xgpio_peripherals
 *                                                       is NULL.
 */
int plc_xgpio_digital_read_all(uint8_t addr, void* values);

/**
 * plc_xgpio_pwm_write_all
 *
 * Set the PWM duty cycle of every output of the peripheral at "addr" at once.
 *
 * Supported peripherals: PCA9685.
 *
 * Parameters:
 *   addr (uint8_t)        - The I2C address of the peripheral.
 *   values (const void*)  - PCA9685_NUM_OUTPUTS uint16_t duty cycles.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - PERIPHERAL_PCA9685_PWM_WRITE_ALL_FAIL : The PCA9685 call failed.
 *         - DUTY_CYCLE_OUT_OF_RANGE               : A duty cycle is above
 *                                                   PCA9685_OUTPUT_ON.
 *         - NULL_ARGUMENT                         : "values" is NULL.
 *         - I2C_ADDR_NOT_FOUND                    : No PCA9685 is at "addr".
 *         - I2C_ADDR_WITHOUT_I2C_BUS              : There is no I2C bus.
 *         - I2C_ADDR_WITHOUT_PERIPHERALS          : plc_xgpio_peripherals is
 *                                                   NULL.
 */
int plc_xgpio_pwm_write_all(uint8_t addr, const void* values);

#ifdef __cplusplus
}
#endif

#endif // PLC_XGPIO_H_
