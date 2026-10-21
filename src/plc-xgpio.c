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

#include <plc-xgpio.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define PIN_PERIPHERAL_TYPE(pin) (((pin) >> 24) & 0xFF)
#define PIN_PERIPHERAL_ADDR(pin) (((pin) >> 16) & 0xFF)
#define PIN_PERIPHERAL_MODE(pin) (((pin) >> 8) & 0xFF)
#define PIN_PERIPHERAL_INDEX(pin) ((pin) & 0xFF)
#define PIN_DIRECT_INDEX(pin) ((pin) & 0xFFFFFF)

#define I2C plc_xgpio_i2c

#define FIND_IN_ARRAY(arr, num)                      \
	do {                                         \
		assert((num) == 0 || (arr) != NULL); \
                                                     \
		for (size_t i = 0; i < (num); i++) { \
			if ((arr)[i].addr == addr) { \
				return &(arr[i]);    \
			}                            \
		}                                    \
                                                     \
		return NULL;                         \
	} while (0)

static plc_xgpio_pca9685_t* find_pca9685(uint8_t addr)
{
	FIND_IN_ARRAY(PLC_XGPIO_ARRAY_PCA9685, PLC_XGPIO_NUM_ARRAY_PCA9685);
}

static plc_xgpio_mcp230xx_t* find_mcp230xx(uint8_t addr)
{
	FIND_IN_ARRAY(PLC_XGPIO_ARRAY_MCP230XX, PLC_XGPIO_NUM_ARRAY_MCP230XX);
}

static plc_xgpio_ltc2309_t* find_ltc2309(uint8_t addr)
{
	FIND_IN_ARRAY(PLC_XGPIO_ARRAY_LTC2309, PLC_XGPIO_NUM_ARRAY_LTC2309);
}

static plc_xgpio_ads101x_t* find_ads101x(uint8_t addr)
{
	FIND_IN_ARRAY(PLC_XGPIO_ARRAY_ADS101X, PLC_XGPIO_NUM_ARRAY_ADS101X);
}

/*
 * plc_xgpio_set_pin_mode for a pin whose peripheral always has the same mode,
 * "fixed_mode". "found" is whether the pin's peripheral is in the arrays.
 */
static int fixed_pin_mode(plc_xgpio_pin_t pin,
			  uint8_t num_gpios,
			  uint8_t given_mode,
			  uint8_t fixed_mode,
			  bool found)
{
	assert(fixed_mode == PLC_XGPIO_INPUT || fixed_mode == PLC_XGPIO_OUTPUT);

	if (!found) {
		return I2C_ADDR_NOT_FOUND;
	}
	if (I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}
	if (PIN_PERIPHERAL_INDEX(pin) >= num_gpios) {
		return INDEX_OUT_OF_RANGE;
	}
	if (given_mode != fixed_mode) {
		return PIN_NOT_SUPPORTED;
	}

	return 1;
}

static int direct_pin_errors(int code)
{
	switch (errno) {
	case EINVAL:
		return INDEX_OUT_OF_RANGE;
	case ENOTSUP:
		return PIN_NOT_SUPPORTED;
	}

	return code;
}

static int pca9685_error(int code, plc_xgpio_pin_t pin)
{
	switch (errno) {
	case EFAULT:
		return I2C_PIN_WITHOUT_I2C_BUS;
	case EINVAL:
		if (PIN_PERIPHERAL_INDEX(pin) >= PCA9685_NUM_OUTPUTS) {
			return INDEX_OUT_OF_RANGE;
		}
		break;
	}

	return code;
}

static int
mcp230xx_error(int code, plc_xgpio_pin_t pin, plc_xgpio_mcp230xx_t* mcp)
{
	MCP230XX_TYPE mcp_type;
	uint8_t num_gpios;
	int local_errno;
	int get_type_ret;

	switch (errno) {
	case EFAULT:
		return I2C_PIN_WITHOUT_I2C_BUS;
	case EINVAL:
		local_errno = errno;
		get_type_ret = mcp230xx_get_type(
			I2C, &mcp->handle, &mcp_type, mcp->timeout_ms);
		errno = local_errno;
		if (get_type_ret == 0) {
			switch (mcp_type) {
			case MCP230XX_008:
				num_gpios = MCP23008_NUM_GPIOS;
				break;
			case MCP230XX_017:
				num_gpios = MCP23017_NUM_GPIOS;
				break;
			default:
				return UNSUPPORTED_PERIPHERAL_TYPE;
			}

			if (PIN_PERIPHERAL_INDEX(pin) >= num_gpios) {
				return INDEX_OUT_OF_RANGE;
			}
		}
		break;
	}

	return code;
}

static int ltc2309_error(int code, plc_xgpio_pin_t pin)
{
	switch (errno) {
	case EFAULT:
		return I2C_PIN_WITHOUT_I2C_BUS;
	case EINVAL:
		if (PIN_PERIPHERAL_INDEX(pin) >= LTC2309_NUM_INPUTS) {
			return INDEX_OUT_OF_RANGE;
		}
		break;
	}

	return code;
}

static int
ads101x_error(int code, plc_xgpio_pin_t pin, plc_xgpio_ads101x_t* ads)
{
	ADS101X_TYPE ads_type;
	uint8_t num_inputs;
	int local_errno;
	int get_type_ret;

	switch (errno) {
	case EFAULT:
		return I2C_PIN_WITHOUT_I2C_BUS;
	case EINVAL:
		local_errno = errno;
		get_type_ret = ads101x_get_type(
			I2C, &ads->handle, &ads_type, ads->timeout_ms);
		errno = local_errno;
		if (get_type_ret == 0) {
			switch (ads_type) {
			case ADS101X_015:
				num_inputs = ADS1015_NUM_INPUTS;
				break;
			default:
				return UNSUPPORTED_PERIPHERAL_TYPE;
			}

			if (PIN_PERIPHERAL_INDEX(pin) >= num_inputs) {
				return INDEX_OUT_OF_RANGE;
			}
		}
		break;
	}

	return code;
}

// Whether the 1st byte of an LTC2309 pin is a mode xgpio knows
static bool ltc2309_mode_is_known(plc_xgpio_pin_t pin)
{
	return PIN_PERIPHERAL_MODE(pin) == PLC_XGPIO_LTC2309_SINGLE_ENDED ||
	       PIN_PERIPHERAL_MODE(pin) == PLC_XGPIO_LTC2309_DIFFERENTIAL;
}

/*
 * Read the LTC2309 or ADS101X input of "pin" into "read". Returns 0, or a
 * negative error, "code" when errno doesn't tell more.
 */
static int read_ltc2309(plc_xgpio_pin_t pin, uint16_t* read, int code)
{
	plc_xgpio_ltc2309_t* ltc;
	int ret;

	ltc = find_ltc2309(PIN_PERIPHERAL_ADDR(pin));
	if (!ltc) {
		return I2C_ADDR_NOT_FOUND;
	}

	if (!ltc2309_mode_is_known(pin)) {
		return UNKNOWN_MODE;
	}

	if (PIN_PERIPHERAL_MODE(pin) == PLC_XGPIO_LTC2309_SINGLE_ENDED) {
		ret = ltc2309_unsigned_single_ended_read(
			I2C,
			&ltc->handle,
			PIN_PERIPHERAL_INDEX(pin),
			read,
			ltc->timeout_ms);
	} else {
		ret = ltc2309_unsigned_differential_read(
			I2C,
			&ltc->handle,
			PIN_PERIPHERAL_INDEX(pin),
			read,
			ltc->timeout_ms);
	}

	return ret == 0 ? ret : ltc2309_error(code, pin);
}

static int read_ads101x(plc_xgpio_pin_t pin, uint16_t* read, int code)
{
	plc_xgpio_ads101x_t* ads;
	int get_continuous_ret;
	bool is_continuous;
	int ret;

	ads = find_ads101x(PIN_PERIPHERAL_ADDR(pin));
	if (!ads) {
		return I2C_ADDR_NOT_FOUND;
	}

	get_continuous_ret = ads101x_get_continuous_mode(
		I2C, &ads->handle, &is_continuous, ads->timeout_ms);
	if (get_continuous_ret == 0) {
		if (is_continuous) {
			ret = ads101x_unsigned_continuous_read(
				I2C,
				&ads->handle,
				PIN_PERIPHERAL_INDEX(pin),
				read,
				ads->timeout_ms);
		} else {
			ret = ads101x_unsigned_single_read(
				I2C,
				&ads->handle,
				PIN_PERIPHERAL_INDEX(pin),
				read,
				ads->timeout_ms);
		}

		return ret == 0 ? ret : ads101x_error(code, pin, ads);
	}

	return ads101x_error(code, pin, ads);
}

int plc_xgpio_set_pin_mode(plc_xgpio_pin_t pin, uint8_t mode)
{
	plc_xgpio_mcp230xx_t* mcp;
	int code;
	int ret;

	if (mode != PLC_XGPIO_INPUT && mode != PLC_XGPIO_OUTPUT) {
		return PIN_NOT_SUPPORTED;
	}

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && plc_xgpio_peripherals == NULL) {
		return I2C_PIN_WITHOUT_PERIPHERALS;
	}

	switch (type) {
	case PLC_DIRECT:
		code = NORMAL_GPIO_SET_PIN_MODE_FAIL;
		ret = plc_normal_gpio_set_pin_mode(PIN_DIRECT_INDEX(pin), mode);
		assert(ret >= -1 && ret <= 1);

		if (ret < 0) {
			code = direct_pin_errors(code);
		}
		break;

	case PLC_PCA9685:
		return fixed_pin_mode(pin,
				      PCA9685_NUM_OUTPUTS,
				      mode,
				      PLC_XGPIO_OUTPUT,
				      find_pca9685(PIN_PERIPHERAL_ADDR(pin)) !=
					      NULL);

	case PLC_MCP230XX:
		mcp = find_mcp230xx(PIN_PERIPHERAL_ADDR(pin));
		if (!mcp) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = PERIPHERAL_MCP230XX_SET_PIN_MODE_FAIL;
		if (mode == PLC_XGPIO_INPUT) {
			ret = mcp230xx_set_input(I2C,
						 &mcp->handle,
						 PIN_PERIPHERAL_INDEX(pin),
						 MCP230XX_NO_PULLUP,
						 mcp->timeout_ms);
		} else {
			ret = mcp230xx_set_output(I2C,
						  &mcp->handle,
						  PIN_PERIPHERAL_INDEX(pin),
						  mcp->timeout_ms);
		}

		if (ret < 0) {
			code = mcp230xx_error(code, pin, mcp);
		}
		break;

	case PLC_LTC2309:
		if (!ltc2309_mode_is_known(pin)) {
			return UNKNOWN_MODE;
		}

		return fixed_pin_mode(pin,
				      LTC2309_NUM_INPUTS,
				      mode,
				      PLC_XGPIO_INPUT,
				      find_ltc2309(PIN_PERIPHERAL_ADDR(pin)) !=
					      NULL);

	case PLC_ADS101X:
		return fixed_pin_mode(pin,
				      ADS1015_NUM_INPUTS,
				      mode,
				      PLC_XGPIO_INPUT,
				      find_ads101x(PIN_PERIPHERAL_ADDR(pin)) !=
					      NULL);

	default:
		return UNKNOWN_PIN_TYPE;
	}

	return ret >= 0 ? ret : code;
}

int plc_xgpio_digital_write(plc_xgpio_pin_t pin, uint8_t value)
{
	plc_xgpio_mcp230xx_t* mcp;
	plc_xgpio_pca9685_t* pca;
	int code;
	int ret;

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && plc_xgpio_peripherals == NULL) {
		return I2C_PIN_WITHOUT_PERIPHERALS;
	}

	switch (type) {
	case PLC_DIRECT:
		code = NORMAL_GPIO_DIGITAL_WRITE_FAIL;
		ret = plc_normal_gpio_digital_write(PIN_DIRECT_INDEX(pin),
						    value ? PLC_XGPIO_HIGH :
							    PLC_XGPIO_LOW);
		assert(ret >= -1 && ret <= 1);

		if (ret < 0) {
			code = direct_pin_errors(code);
		}
		break;

	case PLC_PCA9685:
		pca = find_pca9685(PIN_PERIPHERAL_ADDR(pin));
		if (!pca) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = PERIPHERAL_PCA9685_DIGITAL_WRITE_FAIL;
		ret = pca9685_write_output(I2C,
					   &pca->handle,
					   PIN_PERIPHERAL_INDEX(pin),
					   value ? PCA9685_OUTPUT_ON :
						   PCA9685_OUTPUT_OFF,
					   pca->timeout_ms);

		if (ret < 0) {
			code = pca9685_error(code, pin);
		}
		break;

	case PLC_MCP230XX:
		mcp = find_mcp230xx(PIN_PERIPHERAL_ADDR(pin));
		if (!mcp) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = PERIPHERAL_MCP230XX_DIGITAL_WRITE_FAIL;
		ret = mcp230xx_write_gpio(I2C,
					  &mcp->handle,
					  PIN_PERIPHERAL_INDEX(pin),
					  value ? MCP230XX_HIGH : MCP230XX_LOW,
					  mcp->timeout_ms);

		if (ret < 0) {
			code = mcp230xx_error(code, pin, mcp);
		}
		break;

	case PLC_LTC2309:
		return PIN_NOT_SUPPORTED;

	case PLC_ADS101X:
		return PIN_NOT_SUPPORTED;

	default:
		return UNKNOWN_PIN_TYPE;
	}

	return ret >= 0 ? ret : code;
}

int plc_xgpio_digital_read(plc_xgpio_pin_t pin, uint8_t* read)
{
	plc_xgpio_mcp230xx_t* mcp;
	plc_xgpio_pca9685_t* pca;
	uint16_t read_16;
	int code;
	int ret;

	/*
	 * Check if read is NULL, because a driver will not distinguish a NULL
	 * I2C bus from a NULL read pointer.
	 */
	if (read == NULL) {
		return NULL_ARGUMENT;
	}

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && plc_xgpio_peripherals == NULL) {
		return I2C_PIN_WITHOUT_PERIPHERALS;
	}

	switch (type) {
	case PLC_DIRECT:
		code = NORMAL_GPIO_DIGITAL_READ_FAIL;
		ret = plc_normal_gpio_digital_read(PIN_DIRECT_INDEX(pin), read);
		assert(ret == 0 || ret == -1);
		assert(ret != 0 || *read == PLC_XGPIO_HIGH ||
		       *read == PLC_XGPIO_LOW);

		if (ret < 0) {
			code = direct_pin_errors(code);
		}
		break;

	case PLC_PCA9685:
		pca = find_pca9685(PIN_PERIPHERAL_ADDR(pin));
		if (!pca) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = PERIPHERAL_PCA9685_DIGITAL_READ_FAIL;
		ret = pca9685_read_output(I2C,
					  &pca->handle,
					  PIN_PERIPHERAL_INDEX(pin),
					  &read_16,
					  pca->timeout_ms);

		if (ret < 0) {
			code = pca9685_error(code, pin);
		} else {
			*read = read_16 > PLC_XGPIO_PCA9685_DEFAULT_HIGH_THRESHOLD ?
					PLC_XGPIO_HIGH :
					PLC_XGPIO_LOW;
		}
		break;

	case PLC_MCP230XX:
		mcp = find_mcp230xx(PIN_PERIPHERAL_ADDR(pin));
		if (!mcp) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = PERIPHERAL_MCP230XX_DIGITAL_READ_FAIL;
		ret = mcp230xx_read_gpio(I2C,
					 &mcp->handle,
					 PIN_PERIPHERAL_INDEX(pin),
					 read,
					 mcp->timeout_ms);

		if (ret < 0) {
			code = mcp230xx_error(code, pin, mcp);
		} else {
			*read = *read == MCP230XX_LOW ? PLC_XGPIO_LOW :
							PLC_XGPIO_HIGH;
		}
		break;

	case PLC_LTC2309:
		ret = read_ltc2309(
			pin, &read_16, PERIPHERAL_LTC2309_DIGITAL_READ_FAIL);
		if (ret == 0) {
			*read = read_16 > PLC_XGPIO_LTC2309_DEFAULT_HIGH_THRESHOLD ?
					PLC_XGPIO_HIGH :
					PLC_XGPIO_LOW;
		}
		return ret;

	case PLC_ADS101X:
		ret = read_ads101x(
			pin, &read_16, PERIPHERAL_ADS101X_DIGITAL_READ_FAIL);
		if (ret == 0) {
			*read = read_16 > PLC_XGPIO_ADS101X_DEFAULT_HIGH_THRESHOLD ?
					PLC_XGPIO_HIGH :
					PLC_XGPIO_LOW;
		}
		return ret;

	default:
		return UNKNOWN_PIN_TYPE;
	}

	return ret >= 0 ? ret : code;
}

int plc_xgpio_pwm_write(plc_xgpio_pin_t pin, uint32_t value)
{
	plc_xgpio_pca9685_t* pca;
	int code;
	int ret;

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && plc_xgpio_peripherals == NULL) {
		return I2C_PIN_WITHOUT_PERIPHERALS;
	}

	switch (type) {
	case PLC_DIRECT:
		code = NORMAL_GPIO_PWM_WRITE_FAIL;
		ret = plc_normal_gpio_pwm_write(PIN_DIRECT_INDEX(pin), value);
		assert(ret >= -1 && ret <= 1);

		if (ret < 0) {
			code = errno == ERANGE ? DUTY_CYCLE_OUT_OF_RANGE :
						 direct_pin_errors(code);
		}
		break;

	case PLC_PCA9685:
		pca = find_pca9685(PIN_PERIPHERAL_ADDR(pin));
		if (!pca) {
			return I2C_ADDR_NOT_FOUND;
		}

		// It would wrap into range in the driver's uint16_t
		if (value > UINT16_MAX) {
			return DUTY_CYCLE_OUT_OF_RANGE;
		}

		code = PERIPHERAL_PCA9685_PWM_WRITE_FAIL;
		ret = pca9685_write_output(I2C,
					   &pca->handle,
					   PIN_PERIPHERAL_INDEX(pin),
					   (uint16_t)value,
					   pca->timeout_ms);

		if (ret < 0) {
			if (errno == EINVAL && value > PCA9685_OUTPUT_ON) {
				code = DUTY_CYCLE_OUT_OF_RANGE;
			} else {
				code = pca9685_error(code, pin);
			}
		}
		break;

	case PLC_MCP230XX:
		return PIN_NOT_SUPPORTED;

	case PLC_LTC2309:
		return PIN_NOT_SUPPORTED;

	case PLC_ADS101X:
		return PIN_NOT_SUPPORTED;

	default:
		return UNKNOWN_PIN_TYPE;
	}

	return ret >= 0 ? ret : code;
}

int plc_xgpio_set_pwm_frequency(plc_xgpio_pin_t pin, uint32_t freq_hz)
{
	plc_xgpio_pca9685_t* pca;
	int code;
	int ret;

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && plc_xgpio_peripherals == NULL) {
		return I2C_PIN_WITHOUT_PERIPHERALS;
	}

	switch (type) {
	case PLC_DIRECT:
		code = NORMAL_GPIO_SET_PWM_FREQUENCY_FAIL;
		ret = plc_normal_gpio_set_pwm_frequency(PIN_DIRECT_INDEX(pin),
							freq_hz);
		assert(ret >= -1 && ret <= 1);

		if (ret < 0) {
			code = errno == ERANGE ? FREQ_OUT_OF_RANGE :
						 direct_pin_errors(code);
		}
		break;

	case PLC_PCA9685:
		pca = find_pca9685(PIN_PERIPHERAL_ADDR(pin));
		if (!pca) {
			return I2C_ADDR_NOT_FOUND;
		}

		/*
		 * Checked before the call, because:
		 * - The driver never gets the index, so we need to discard it
		 *   first.
		 * - A frequency above UINT16_MAX would wrap in its uint16_t.
		 */
		if (PIN_PERIPHERAL_INDEX(pin) >= PCA9685_NUM_OUTPUTS) {
			return INDEX_OUT_OF_RANGE;
		}
		if (freq_hz < PCA9685_MIN_FREQ_HZ ||
		    freq_hz > PCA9685_MAX_FREQ_HZ) {
			return FREQ_OUT_OF_RANGE;
		}

		code = PERIPHERAL_PCA9685_SET_PWM_FREQUENCY_FAIL;
		ret = pca9685_set_frequency(
			I2C, &pca->handle, (uint16_t)freq_hz, pca->timeout_ms);

		if (ret < 0) {
			code = pca9685_error(code, pin);
		}
		break;

	case PLC_MCP230XX:
		return PIN_NOT_SUPPORTED;

	case PLC_LTC2309:
		return PIN_NOT_SUPPORTED;

	case PLC_ADS101X:
		return PIN_NOT_SUPPORTED;

	default:
		return UNKNOWN_PIN_TYPE;
	}

	return ret >= 0 ? ret : code;
}

int plc_xgpio_analog_read(plc_xgpio_pin_t pin, uint32_t* read)
{
	uint16_t read_16;
	int code;
	int ret;

	/*
	 * Check if read is NULL, because a driver will not distinguish a NULL
	 * I2C bus from a NULL read pointer.
	 */
	if (read == NULL) {
		return NULL_ARGUMENT;
	}

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && plc_xgpio_peripherals == NULL) {
		return I2C_PIN_WITHOUT_PERIPHERALS;
	}

	switch (type) {
	case PLC_DIRECT:
		code = NORMAL_GPIO_ANALOG_READ_FAIL;
		ret = plc_normal_gpio_analog_read(PIN_DIRECT_INDEX(pin), read);
		assert(ret == 0 || ret == -1);

		if (ret < 0) {
			code = direct_pin_errors(code);
		}
		break;

	case PLC_PCA9685:
		return PIN_NOT_SUPPORTED;

	case PLC_MCP230XX:
		return PIN_NOT_SUPPORTED;

	case PLC_LTC2309:
		ret = read_ltc2309(
			pin, &read_16, PERIPHERAL_LTC2309_ANALOG_READ_FAIL);
		if (ret == 0) {
			*read = (uint32_t)read_16;
		}
		return ret;

	case PLC_ADS101X:
		ret = read_ads101x(
			pin, &read_16, PERIPHERAL_ADS101X_ANALOG_READ_FAIL);
		if (ret == 0) {
			*read = (uint32_t)read_16;
		}
		return ret;

	default:
		return UNKNOWN_PIN_TYPE;
	}

	return ret >= 0 ? ret : code;
}

int plc_xgpio_digital_write_all(uint8_t addr, uint32_t values)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS];
	plc_xgpio_mcp230xx_t* mcp;
	plc_xgpio_pca9685_t* pca;
	MCP230XX_TYPE type;
	uint8_t num_gpios;

	if (plc_xgpio_peripherals == NULL) {
		return I2C_ADDR_WITHOUT_PERIPHERALS;
	}

	// Try to find a PCA9685 first
	pca = find_pca9685(addr);
	if (pca != NULL) {
		if (values >> PCA9685_NUM_OUTPUTS) {
			return VALUES_OUT_OF_RANGE;
		}

		for (size_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
			duty[i] = (values & (1UL << i)) ? PCA9685_OUTPUT_ON :
							  PCA9685_OUTPUT_OFF;
		}
		if (pca9685_write_all(
			    I2C, &pca->handle, duty, pca->timeout_ms) == 0) {
			return 0;
		}
		return errno == EFAULT ?
			       I2C_ADDR_WITHOUT_I2C_BUS :
			       PERIPHERAL_PCA9685_DIGITAL_WRITE_ALL_FAIL;
	}

	// Then try to find an MCP230XX
	mcp = find_mcp230xx(addr);
	if (mcp != NULL) {
		if (mcp230xx_get_type(
			    I2C, &mcp->handle, &type, mcp->timeout_ms) != 0) {
			return errno == EFAULT ?
				       I2C_ADDR_WITHOUT_I2C_BUS :
				       PERIPHERAL_MCP230XX_DIGITAL_WRITE_ALL_FAIL;
		}
		switch (type) {
		case MCP230XX_008:
			num_gpios = MCP23008_NUM_GPIOS;
			break;
		case MCP230XX_017:
			num_gpios = MCP23017_NUM_GPIOS;
			break;
		default:
			return UNSUPPORTED_PERIPHERAL_TYPE;
		}

		if (values >> num_gpios) {
			return VALUES_OUT_OF_RANGE;
		}

		if (mcp230xx_write_all(I2C,
				       &mcp->handle,
				       (uint16_t)values,
				       mcp->timeout_ms) == 0) {
			return 0;
		}
		return errno == EFAULT ?
			       I2C_ADDR_WITHOUT_I2C_BUS :
			       PERIPHERAL_MCP230XX_DIGITAL_WRITE_ALL_FAIL;
	}

	return I2C_ADDR_NOT_FOUND;
}

int plc_xgpio_digital_read_all(uint8_t addr, void* values)
{
	plc_xgpio_mcp230xx_t* mcp;
	MCP230XX_TYPE type;
	uint16_t levels;
	uint8_t low;

	if (plc_xgpio_peripherals == NULL) {
		return I2C_ADDR_WITHOUT_PERIPHERALS;
	}

	if (values == NULL) {
		return NULL_ARGUMENT;
	}

	mcp = find_mcp230xx(addr);
	if (mcp != NULL) {
		if (mcp230xx_get_type(
			    I2C, &mcp->handle, &type, mcp->timeout_ms) != 0) {
			return errno == EFAULT ?
				       I2C_ADDR_WITHOUT_I2C_BUS :
				       PERIPHERAL_MCP230XX_DIGITAL_READ_ALL_FAIL;
		}
		// A type added to the driver after this was built has an unknown size
		if (type != MCP230XX_008 && type != MCP230XX_017) {
			return UNSUPPORTED_PERIPHERAL_TYPE;
		}

		if (mcp230xx_read_all(
			    I2C, &mcp->handle, &levels, mcp->timeout_ms) != 0) {
			return errno == EFAULT ?
				       I2C_ADDR_WITHOUT_I2C_BUS :
				       PERIPHERAL_MCP230XX_DIGITAL_READ_ALL_FAIL;
		}

		switch (type) {
		case MCP230XX_008:
			low = (uint8_t)levels;
			memcpy(values, &low, sizeof(low));
			break;
		case MCP230XX_017:
			memcpy(values, &levels, sizeof(levels));
			break;
		}

		return 0;
	}

	return I2C_ADDR_NOT_FOUND;
}

int plc_xgpio_pwm_write_all(uint8_t addr, const void* values)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS];
	plc_xgpio_pca9685_t* pca;

	if (plc_xgpio_peripherals == NULL) {
		return I2C_ADDR_WITHOUT_PERIPHERALS;
	}

	if (values == NULL) {
		return NULL_ARGUMENT;
	}

	pca = find_pca9685(addr);
	if (pca != NULL) {
		memcpy(duty, values, sizeof(duty));
		if (pca9685_write_all(
			    I2C, &pca->handle, duty, pca->timeout_ms) == 0) {
			return 0;
		}

		if (errno == EFAULT) {
			return I2C_ADDR_WITHOUT_I2C_BUS;
		}
		if (errno == EINVAL) {
			for (size_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
				if (duty[i] > PCA9685_OUTPUT_ON) {
					return DUTY_CYCLE_OUT_OF_RANGE;
				}
			}
		}

		return PERIPHERAL_PCA9685_PWM_WRITE_ALL_FAIL;
	}

	return I2C_ADDR_NOT_FOUND;
}
