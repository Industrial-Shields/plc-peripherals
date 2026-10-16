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

// The accepted modes of plc_xgpio_pin_mode
// clang-format off
#define PLC_XGPIO_INPUT  0
#define PLC_XGPIO_OUTPUT 1
// clang-format on

// The accepted values of plc_xgpio_digital_write/_all
#define PLC_XGPIO_LOW 0
#define PLC_XGPIO_HIGH 1

// The value of plc_xgpio_peripherals.i2c when there is no I2C bus
#define PLC_XGPIO_NO_I2C_BUS NULL

// The default timeout_ms of the entries of the arrays
#define PLC_XGPIO_DEFAULT_TIMEOUT_MS 25

/*
 * xgpio aims to provide a unified interface in which you can interact
 * seamlessly with direct GPIOs of the chip you are using, or expanded GPIOs
 * like the ones provided by MCP23008, LTC2309, etc...
 *
 * Nothing is initialized here. Fill plc_xgpio_peripherals with the arrays of
 * peripherals in use, each handle already initialized, before calling any
 * function.
 */

#include "peripheral-ads101x.h"
#include "peripheral-ltc2309.h"
#include "peripheral-mcp230xx.h"
#include "peripheral-pca9685.h"
#include "plc-peripherals-i2c.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A pin is 32 bits. The MSB is its type, as peripheral_type_t. For the
 * peripherals, the 2nd byte is the I2C address and the 0th byte the GPIO's
 * index in the peripheral. The 1st byte can freely be used by the peripheral.
 */
typedef enum {
	PLC_DIRECT = 0,
	PLC_PCA9685 = 1,
	PLC_MCP23008 = 2,
	PLC_MCP23017 = 3,
	PLC_LTC2309 = 4,
	PLC_ADS1015 = 5,
} peripheral_type_t;

typedef struct {
	mcp230xx_t handle;
	uint32_t timeout_ms;
	uint8_t addr;
} plc_xgpio_mcp230xx_t;

/*
 * TODO: The PCA9685 handle must be initialized as v2.0.0 used it:
 * PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED, allcall and the three subcalls
 * false, and freq_hz = 500.
 */
typedef struct {
	pca9685_t handle;
	uint32_t timeout_ms;
	uint8_t addr;
} plc_xgpio_pca9685_t;

/*
 * TODO: The LTC2309 inputs are always read unipolar. The single-ended ones, as
 * v2.0.0 read them, need COM tied to GND.
 */
typedef struct {
	ltc2309_t handle;
	uint32_t timeout_ms;
	uint8_t addr;
} plc_xgpio_ltc2309_t;

/*
 * TODO: The ADS1015 handle must be initialized as v2.0.0 used it: single-shot
 * (continuous_mode = false), ADS101X_FSR_4_096V and ADS101X_1600SPS.
 */
typedef struct {
	ads101x_t handle;
	uint32_t timeout_ms;
	uint8_t addr;
} plc_xgpio_ads101x_t;

/*
 * The peripherals the pins refer to, and the I2C interface they are on. Each
 * array has its num_* entries. An array can only be NULL if its num_* is 0.
 *
 * i2c can be PLC_XGPIO_NO_I2C_BUS. In that case, the functions fail on any
 * other pin with I2C_PIN_WITHOUT_I2C_BUS, and the *_all ones with
 * I2C_ADDR_WITHOUT_I2C_BUS.
 */
typedef struct {
	const i2c_interface_t* i2c;
	plc_xgpio_mcp230xx_t* mcp23008;
	size_t num_mcp23008;
	plc_xgpio_ads101x_t* ads1015;
	size_t num_ads1015;
	plc_xgpio_pca9685_t* pca9685;
	size_t num_pca9685;
	plc_xgpio_ltc2309_t* ltc2309;
	size_t num_ltc2309;
	plc_xgpio_mcp230xx_t* mcp23017;
	size_t num_mcp23017;
} plc_xgpio_peripherals_t;

#define _MAKE_PIN_PLC(type, byte2, byte3, byte4) \
	(((uint32_t)((type) & 0xFF) << 24) |     \
	 ((uint32_t)((byte2) & 0xFF) << 16) |    \
	 ((uint32_t)((byte3) & 0xFF) << 8) | ((uint32_t)((byte4) & 0xFF)))

// Direct pins can have an index of up to 24 bits
#define MAKE_PIN_DIRECT(index)                    \
	_MAKE_PIN_PLC(PLC_DIRECT,                 \
		      ((index) & 0xFF0000) >> 16, \
		      ((index) & 0xFF00) >> 8,    \
		      (index))

// Macros to distinguish the input type of an LTC2309
// clang-format off
#define PLC_XGPIO_LTC2309_SINGLE_ENDED 0b00
#define PLC_XGPIO_LTC2309_DIFFERENTIAL 0b01
// clang-format on

/*
 * The adapted peripherals until now use the 2nd byte as I2C address,
 * and the 0th byte as the GPIO's index of the peripheral. The 1st
 * byte is left as 0x00 if not used by the peripheral.
 */
#define MAKE_PIN_PCA9685(addr, index) \
	_MAKE_PIN_PLC(PLC_PCA9685, (addr), 0x00, (index))
#define MAKE_PIN_MCP23008(addr, index) \
	_MAKE_PIN_PLC(PLC_MCP23008, (addr), 0x00, (index))
#define MAKE_PIN_MCP23017(addr, index) \
	_MAKE_PIN_PLC(PLC_MCP23017, (addr), 0x00, (index))
#define MAKE_PIN_LTC2309(addr, index) \
	_MAKE_PIN_PLC(                \
		PLC_LTC2309, (addr), PLC_XGPIO_LTC2309_SINGLE_ENDED, (index))
#define MAKE_PIN_LTC2309_DIFF(addr, pair) \
	_MAKE_PIN_PLC(                    \
		PLC_LTC2309, (addr), PLC_XGPIO_LTC2309_DIFFERENTIAL, (pair))
#define MAKE_PIN_ADS1015(addr, index) \
	_MAKE_PIN_PLC(PLC_ADS1015, (addr), 0x00, (index))

extern plc_xgpio_peripherals_t plc_xgpio_peripherals;
#define PLC_XGPIO_ARRAY_MCP23008 plc_xgpio_peripherals.mcp23008
#define PLC_XGPIO_NUM_ARRAY_MCP23008 plc_xgpio_peripherals.num_mcp23008
#define PLC_XGPIO_ARRAY_ADS1015 plc_xgpio_peripherals.ads1015
#define PLC_XGPIO_NUM_ARRAY_ADS1015 plc_xgpio_peripherals.num_ads1015
#define PLC_XGPIO_ARRAY_PCA9685 plc_xgpio_peripherals.pca9685
#define PLC_XGPIO_NUM_ARRAY_PCA9685 plc_xgpio_peripherals.num_pca9685
#define PLC_XGPIO_ARRAY_LTC2309 plc_xgpio_peripherals.ltc2309
#define PLC_XGPIO_NUM_ARRAY_LTC2309 plc_xgpio_peripherals.num_ltc2309
#define PLC_XGPIO_ARRAY_MCP23017 plc_xgpio_peripherals.mcp23017
#define PLC_XGPIO_NUM_ARRAY_MCP23017 plc_xgpio_peripherals.num_mcp23017

/*
 * The following are expected to be provided by the library using this one, to
 * interact with the direct GPIOs of the device (ESP32, Raspberry...). They are
 * only called for PLC_DIRECT pins, and get the direct pin's index (its 24 low
 * bits) as "pin".
 *
 * On failure, they can set errno, depending on the platform. The xgpio
 * function that called them returns with errno as they left it, with some
 * errnos being good default values.
 */

/*
 * The "mode" values plc_normal_gpio_set_pin_mode takes for an input and an
 * output.
 */
extern const uint8_t PLC_NORMAL_GPIO_INPUT;
extern const uint8_t PLC_NORMAL_GPIO_OUTPUT;

/**
 * plc_normal_gpio_set_pin_mode
 *
 * Set a direct GPIO as input or as output.
 *
 * Parameters:
 *   pin (uint32_t)  - The index of the direct GPIO.
 *   mode (uint8_t)  - PLC_NORMAL_GPIO_INPUT or PLC_NORMAL_GPIO_OUTPUT.
 * Returns:
 *   int - 0 if successful, 1 if it already had that mode, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL : mode is neither PLC_NORMAL_GPIO_INPUT nor
 *                PLC_NORMAL_GPIO_OUTPUT.
 *     - Linux specific :
 *         - (others)   : Whatever ioctl(2) reports for the GPIO line request.
 */
extern int plc_normal_gpio_set_pin_mode(uint32_t pin, uint8_t mode);

/**
 * plc_normal_gpio_write
 *
 * Set a direct GPIO high or low.
 *
 * Parameters:
 *   pin (uint32_t)   - The index of the direct GPIO.
 *   value (uint8_t)  - PLC_XGPIO_HIGH or PLC_XGPIO_LOW, as passed to
 *                      plc_xgpio_digital_write.
 * Returns:
 *   int - 0 if successful, 1 if it already had that level, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - Linux specific :
 *         - (others)   : Whatever ioctl(2) reports for the GPIO line request
 *                        (the GPIO is set as output first, if it wasn't), or
 *                        to set its value.
 */
extern int plc_normal_gpio_write(uint32_t pin, uint8_t value);

/**
 * plc_normal_gpio_pwm_set_frequency
 *
 * Set the PWM frequency of a direct GPIO.
 *
 * Parameters:
 *   pin (uint32_t)   - The index of the direct GPIO.
 *   freq (uint32_t)  - The frequency, in Hz, as passed to
 *                      plc_xgpio_analog_write_set_frequency.
 * Returns:
 *   int - 0 if successful, 1 if it already had that frequency, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL  : The GPIO can't be set to that frequency.
 *     - ENOTSUP : The platform has no PWM on its direct GPIOs.
 */
extern int plc_normal_gpio_pwm_set_frequency(uint32_t pin, uint32_t freq);

/**
 * plc_normal_gpio_pwm_write
 *
 * Set the PWM duty cycle of a direct GPIO.
 *
 * Parameters:
 *   pin (uint32_t)    - The index of the direct GPIO.
 *   value (uint32_t)  - The duty cycle, as passed to plc_xgpio_analog_write.
 *                       Its scale is up to this function.
 * Returns:
 *   int - 0 if successful, 1 if it already had that duty cycle, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL  : The GPIO can't be set to that duty cycle.
 *     - ENOTSUP : The platform has no PWM on its direct GPIOs.
 */
extern int plc_normal_gpio_pwm_write(uint32_t pin, uint32_t value);

/**
 * plc_normal_gpio_read
 *
 * Read the level of a direct GPIO.
 *
 * Parameters:
 *   pin (uint32_t)   - The index of the direct GPIO.
 *   read (uint8_t*)  - Where to store the level.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT         : "read" pointer is NULL.
 *     - Linux specific :
 *         - (others)   : Whatever ioctl(2) reports for the GPIO line request
 *                        (the GPIO is set as input first, if it wasn't), or
 *                        to get its value.
 */
extern int plc_normal_gpio_read(uint32_t pin, uint8_t* read);

/**
 * plc_normal_gpio_analog_read
 *
 * Read the analog value of a direct GPIO.
 *
 * Parameters:
 *   pin (uint32_t)    - The index of the direct GPIO.
 *   read (uint32_t*)  - Where to store the reading. Its scale is up to this
 *                       function, and always unsigned.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT  : "read" pointer is NULL.
 *     - ENOTSUP : The platform has no analog reads on its direct GPIOs.
 */
extern int plc_normal_gpio_analog_read(uint32_t pin, uint32_t* read);

// The errors reported by xgpio functions.
enum {
	// clang-format off
	// plc_xgpio_pin_mode
	NORMAL_GPIO_SET_PIN_MODE_FAIL      = -15,
	ARRAY_MCP23008_SET_PIN_MODE_FAIL   = -16,
	ARRAY_MCP23017_SET_PIN_MODE_FAIL   = -17,

	// plc_xgpio_digital_write
	NORMAL_GPIO_WRITE_FAIL             = -18,
	ARRAY_PCA9685_WRITE_FAIL           = -19,
	ARRAY_MCP23008_WRITE_FAIL          = -20,
	ARRAY_MCP23017_WRITE_FAIL          = -21,

	// plc_xgpio_analog_write
	NORMAL_GPIO_PWM_WRITE_FAIL         = -22,
	ARRAY_PCA9685_PWM_WRITE_FAIL       = -23,

	// plc_xgpio_analog_write_set_frequency
	NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL   = -24,
	ARRAY_PCA9685_PWM_CHANGE_FREQ_FAIL = -25,

	// plc_xgpio_digital_write_all
	ARRAY_PCA9685_WRITE_ALL_FAIL       = -26,
	ARRAY_MCP23008_WRITE_ALL_FAIL      = -27,
	ARRAY_MCP23017_WRITE_ALL_FAIL      = -28,

	// plc_xgpio_digital_read_all
	ARRAY_MCP23008_READ_ALL_FAIL       = -29,
	ARRAY_MCP23017_READ_ALL_FAIL       = -30,

	// plc_xgpio_analog_write_all
	ARRAY_PCA9685_PWM_WRITE_ALL_FAIL   = -31,

	// Others
	I2C_PIN_WITHOUT_I2C_BUS            = -32,
	I2C_ADDR_WITHOUT_I2C_BUS           = -34,
	PIN_NOT_SUPPORTED                  = -35,
	I2C_ADDR_NOT_FOUND                 = -36,
	NULL_ARGUMENT                      = -37,
	FREQ_OUT_OF_RANGE                  = -38,
	UNKNOWN_PIN_TYPE                   = -39,
	INDEX_OUT_OF_RANGE                 = -40,
	// clang-format on
};

/**
 * plc_xgpio_pin_mode
 *
 * Set a GPIO as input or as output.
 *
 * Parameters:
 *   pin (uint32_t)  - The pin.
 *   mode (uint8_t)  - PLC_XGPIO_INPUT or PLC_XGPIO_OUTPUT.
 * Returns:
 *   int - 0 if successful, 1 if it already had that mode, otherwise a negative
 *         value:
 *         - NORMAL_GPIO_SET_PIN_MODE_FAIL    : The direct GPIO call failed.
 *         - ARRAY_MCP23008_SET_PIN_MODE_FAIL : The MCP23008 call failed.
 *         - ARRAY_MCP23017_SET_PIN_MODE_FAIL : The MCP23017 call failed.
 *         - I2C_ADDR_NOT_FOUND               : The peripheral isn't in the
 *                                              arrays.
 *         - INDEX_OUT_OF_RANGE               : The peripheral doesn't have the
 *                                              pin's index.
 *         - I2C_PIN_WITHOUT_I2C_BUS          : It isn't a direct pin, and there
 *                                              is no I2C bus.
 *         - PIN_NOT_SUPPORTED                : The mode is invalid, the pin
 *                                              can't have it, or it is an
 *                                              ADS1015 differential pair.
 *         - UNKNOWN_PIN_TYPE                 : The pin type is unknown.
 */
int plc_xgpio_pin_mode(uint32_t pin, uint8_t mode);

/**
 * plc_xgpio_digital_write
 *
 * Set a GPIO high or low.
 *
 * Supported pins: PLC_DIRECT, PLC_MCP23008, PLC_MCP23017 and PLC_PCA9685.
 *
 * Parameters:
 *   pin (uint32_t)   - The pin.
 *   value (uint8_t)  - PLC_XGPIO_HIGH or PLC_XGPIO_LOW.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - NORMAL_GPIO_WRITE_FAIL    : The direct GPIO call failed.
 *         - ARRAY_PCA9685_WRITE_FAIL  : The PCA9685 call failed.
 *         - ARRAY_MCP23008_WRITE_FAIL : The MCP23008 call failed.
 *         - ARRAY_MCP23017_WRITE_FAIL : The MCP23017 call failed.
 *         - I2C_ADDR_NOT_FOUND        : The peripheral isn't in the arrays.
 *         - INDEX_OUT_OF_RANGE        : The peripheral doesn't have the pin's
 *                                       index.
 *         - I2C_PIN_WITHOUT_I2C_BUS   : It isn't a direct pin, and there is no
 *                                       I2C bus.
 *         - PIN_NOT_SUPPORTED         : The pin type can't be written.
 *         - UNKNOWN_PIN_TYPE          : The pin type is unknown.
 */
int plc_xgpio_digital_write(uint32_t pin, uint8_t value);

/**
 * plc_xgpio_digital_read
 *
 * Read the level of a GPIO.
 *
 * Supported pins: PLC_DIRECT, PLC_MCP23008, PLC_MCP23017, PLC_LTC2309 (high
 * above 1636) and PLC_ADS1015 (high above 818).
 *
 * Parameters:
 *   pin (uint32_t) - The pin.
 * Returns:
 *   int - PLC_XGPIO_HIGH or PLC_XGPIO_LOW (for a direct GPIO, what
 *         plc_normal_gpio_read read). PLC_XGPIO_LOW also on failure.
 *         Otherwise, a negative value:
 *         - I2C_PIN_WITHOUT_I2C_BUS : It isn't a direct pin, and there is no
 *                                     I2C bus.
 */
int plc_xgpio_digital_read(uint32_t pin);

/**
 * plc_xgpio_analog_write
 *
 * Set the PWM duty cycle of a GPIO.
 *
 * Supported pins: PLC_DIRECT and PLC_PCA9685.
 *
 * Parameters:
 *   pin (uint32_t)    - The pin.
 *   value (uint32_t)  - The duty cycle. For a PCA9685, from
 *                       PCA9685_OUTPUT_OFF to PCA9685_OUTPUT_ON.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - NORMAL_GPIO_PWM_WRITE_FAIL   : The direct GPIO call failed.
 *         - ARRAY_PCA9685_PWM_WRITE_FAIL : The PCA9685 call failed, or the
 *                                          duty cycle is above
 *                                          PCA9685_OUTPUT_ON.
 *         - I2C_ADDR_NOT_FOUND           : The peripheral isn't in the arrays.
 *         - INDEX_OUT_OF_RANGE           : The peripheral doesn't have the
 *                                          pin's index.
 *         - I2C_PIN_WITHOUT_I2C_BUS      : It isn't a direct pin, and there is
 *                                          no I2C bus.
 *         - PIN_NOT_SUPPORTED            : The pin type has no PWM.
 *         - UNKNOWN_PIN_TYPE             : The pin type is unknown.
 */
int plc_xgpio_analog_write(uint32_t pin, uint32_t value);

/**
 * plc_xgpio_analog_write_set_frequency
 *
 * Set the PWM frequency of a GPIO.
 *
 * Supported pins: PLC_DIRECT and PLC_PCA9685.
 *
 * Parameters:
 *   pin (uint32_t)      - The pin.
 *   freq_hz (uint32_t)  - The frequency. For a PCA9685, from
 *                         PCA9685_MIN_FREQ_HZ to PCA9685_MAX_FREQ_HZ.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL   : The direct GPIO call failed.
 *         - ARRAY_PCA9685_PWM_CHANGE_FREQ_FAIL : The PCA9685 call failed.
 *         - FREQ_OUT_OF_RANGE                  : The frequency is out of the
 *                                                PCA9685's range.
 *         - I2C_ADDR_NOT_FOUND                 : The peripheral isn't in the
 *                                                arrays.
 *         - INDEX_OUT_OF_RANGE                 : The peripheral doesn't have
 *                                                the pin's index.
 *         - I2C_PIN_WITHOUT_I2C_BUS            : It isn't a direct pin, and
 *                                                there is no I2C bus.
 *         - PIN_NOT_SUPPORTED                  : The pin type has no PWM.
 *         - UNKNOWN_PIN_TYPE                   : The pin type is unknown.
 */
int plc_xgpio_analog_write_set_frequency(uint32_t pin, uint32_t freq_hz);

/**
 * plc_xgpio_analog_read
 *
 * Read the analog value of a GPIO.
 *
 * Supported pins: PLC_DIRECT, PLC_LTC2309 (single-ended or differential,
 * unipolar, 0 to 4095) and PLC_ADS1015 (against GND, 0 to 2047).
 *
 * Parameters:
 *   pin (uint32_t) - The pin.
 * Returns:
 *   uint32_t - The reading, 0 on failure, or -I2C_PIN_WITHOUT_I2C_BUS
 *              (positive, as the return is unsigned) for any pin but a
 *              direct one when there is no I2C bus.
 */
uint32_t plc_xgpio_analog_read(uint32_t pin);

/**
 * plc_xgpio_digital_write_all
 *
 * Set every output of the peripheral at "addr" high or low at once. Bit i is
 * output i.
 *
 * Supported peripherals: MCP23008, MCP23017 and PCA9685.
 *
 * Parameters:
 *   addr (uint8_t)     - The I2C address of the peripheral.
 *   values (uint32_t)  - The levels.
 * Returns:
 *   int - 0 if successful, otherwise a negative value:
 *         - ARRAY_PCA9685_WRITE_ALL_FAIL  : The PCA9685 call failed.
 *         - ARRAY_MCP23008_WRITE_ALL_FAIL : The MCP23008 call failed.
 *         - ARRAY_MCP23017_WRITE_ALL_FAIL : The MCP23017 call failed.
 *         - I2C_ADDR_NOT_FOUND            : No MCP23008, PCA9685 or MCP23017 is
 *                                           at "addr".
 *         - I2C_ADDR_WITHOUT_I2C_BUS      : There is no I2C bus.
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
 *         - ARRAY_MCP23008_READ_ALL_FAIL : The MCP23008 call failed.
 *         - ARRAY_MCP23017_READ_ALL_FAIL : The MCP23017 call failed.
 *         - NULL_ARGUMENT                : "values" is NULL.
 *         - I2C_ADDR_NOT_FOUND           : No MCP23008 or MCP23017 is at
 *                                          "addr".
 *         - I2C_ADDR_WITHOUT_I2C_BUS     : There is no I2C bus.
 */
int plc_xgpio_digital_read_all(uint8_t addr, void* values);

/**
 * plc_xgpio_analog_write_all
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
 *         - ARRAY_PCA9685_PWM_WRITE_ALL_FAIL : The PCA9685 call failed.
 *         - NULL_ARGUMENT                    : "values" is NULL.
 *         - I2C_ADDR_NOT_FOUND               : No PCA9685 is at "addr".
 *         - I2C_ADDR_WITHOUT_I2C_BUS         : There is no I2C bus.
 */
int plc_xgpio_analog_write_all(uint8_t addr, const void* values);

#ifdef __cplusplus
}
#endif

#endif // PLC_XGPIO_H_
