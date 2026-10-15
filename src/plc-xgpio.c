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

#include <stdint.h>
#include <string.h>

#define PIN_PERIPHERAL_TYPE(pin) (((pin) >> 24) & 0xFF)
#define PIN_PERIPHERAL_ADDR(pin) (((pin) >> 16) & 0xFF)
#define PIN_PERIPHERAL_INDEX(pin) ((pin) & 0xFF)
#define PIN_DIRECT_INDEX(pin) ((pin) & 0xFFFFFF)
// The number of GPIOs of the MCP230XX a PLC_MCP23008 or PLC_MCP23017 pin is of
#define PIN_MCP230XX_NUM_GPIOS(pin)                                      \
	(PIN_PERIPHERAL_TYPE(pin) == PLC_MCP23008 ? MCP23008_NUM_GPIOS : \
						    MCP23017_NUM_GPIOS)

// clang-format off
// Above these readings, an analog input reads as high (as in v2.0.0)
#define LTC2309_HIGH_THRESHOLD 1636
#define ADS1015_HIGH_THRESHOLD 818
// clang-format on

#define I2C (plc_xgpio_peripherals.i2c)

// The LTC2309 channel of each input index
static const LTC2309_INPUT LTC2309_CHANNELS[LTC2309_NUM_INPUTS] = {
	LTC2309_CH0, LTC2309_CH1, LTC2309_CH2, LTC2309_CH3,
	LTC2309_CH4, LTC2309_CH5, LTC2309_CH6, LTC2309_CH7,
};

plc_xgpio_peripherals_t plc_xgpio_peripherals;

static plc_xgpio_mcp230xx_t*
find_mcp230xx(plc_xgpio_mcp230xx_t* arr, size_t num, uint8_t addr)
{
	size_t i;

	for (i = 0; i < num; i++) {
		if (arr[i].addr == addr) {
			return &arr[i];
		}
	}

	return NULL;
}

static plc_xgpio_mcp230xx_t* find_pin_mcp230xx(uint32_t pin)
{
	if (PIN_PERIPHERAL_TYPE(pin) == PLC_MCP23008) {
		return find_mcp230xx(PLC_XGPIO_ARRAY_MCP23008,
				     PLC_XGPIO_NUM_ARRAY_MCP23008,
				     PIN_PERIPHERAL_ADDR(pin));
	}

	if (PIN_PERIPHERAL_TYPE(pin) == PLC_MCP23017) {
		return find_mcp230xx(PLC_XGPIO_ARRAY_MCP23017,
				     PLC_XGPIO_NUM_ARRAY_MCP23017,
				     PIN_PERIPHERAL_ADDR(pin));
	}

	return NULL;
}

static void* find_pca9685(uint8_t addr)
{
	plc_xgpio_pca9685_t* arr = PLC_XGPIO_ARRAY_PCA9685;
	size_t num = PLC_XGPIO_NUM_ARRAY_PCA9685;
	size_t i;

	for (i = 0; i < num; i++) {
		if (arr[i].addr == addr) {
			return &arr[i];
		}
	}

	return NULL;
}

static void* find_ltc2309(uint8_t addr)
{
	plc_xgpio_ltc2309_t* arr = PLC_XGPIO_ARRAY_LTC2309;
	size_t num = PLC_XGPIO_NUM_ARRAY_LTC2309;
	size_t i;

	for (i = 0; i < num; i++) {
		if (arr[i].addr == addr) {
			return &arr[i];
		}
	}

	return NULL;
}

static void* find_ads1015(uint8_t addr)
{
	plc_xgpio_ads101x_t* arr = PLC_XGPIO_ARRAY_ADS1015;
	size_t num = PLC_XGPIO_NUM_ARRAY_ADS1015;
	size_t i;

	for (i = 0; i < num; i++) {
		if (arr[i].addr == addr) {
			return &arr[i];
		}
	}

	return NULL;
}

/*
 * plc_xgpio_pin_mode for a pin whose peripheral always has the same mode,
 * "fixed_mode". "find" looks up its entry in the arrays, and is only called
 * once the index and the mode are valid.
 */
static int fixed_pin_mode(uint32_t pin,
			  uint8_t num_gpios,
			  uint8_t mode,
			  uint8_t fixed_mode,
			  void* (*find)(uint8_t addr))
{
	if (PIN_PERIPHERAL_INDEX(pin) >= num_gpios) {
		return INDEX_OUT_OF_RANGE;
	}
	if (mode != fixed_mode) {
		return PIN_NOT_SUPPORTED;
	}

	return find(PIN_PERIPHERAL_ADDR(pin)) != NULL ? 1 : I2C_ADDR_NOT_FOUND;
}

int plc_xgpio_pin_mode(uint32_t pin, uint8_t mode)
{
	plc_xgpio_mcp230xx_t* mcp;
	int code;
	int ret;

	if (mode != PLC_XGPIO_INPUT && mode != PLC_XGPIO_OUTPUT) {
		return PIN_NOT_SUPPORTED;
	}

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	const uint8_t index = PIN_PERIPHERAL_INDEX(pin);

	switch (type) {
	case PLC_PCA9685:
		return fixed_pin_mode(pin,
				      PCA9685_NUM_OUTPUTS,
				      mode,
				      PLC_XGPIO_OUTPUT,
				      find_pca9685);
	case PLC_LTC2309:
		return fixed_pin_mode(pin,
				      LTC2309_NUM_INPUTS,
				      mode,
				      PLC_XGPIO_INPUT,
				      find_ltc2309);
	case PLC_ADS1015:
		// The differential pairs read signed values
		if (index < ADS101X_P0_GND) {
			return PIN_NOT_SUPPORTED;
		}
		return fixed_pin_mode(pin,
				      ADS101X_NUM_INPUTS,
				      mode,
				      PLC_XGPIO_INPUT,
				      find_ads1015);
	default:
		return UNKNOWN_PIN_TYPE;

	case PLC_DIRECT:
		code = NORMAL_GPIO_SET_PIN_MODE_FAIL;
		ret = plc_normal_gpio_set_pin_mode(
			PIN_DIRECT_INDEX(pin),
			mode == PLC_XGPIO_OUTPUT ? PLC_NORMAL_GPIO_OUTPUT :
						   PLC_NORMAL_GPIO_INPUT);
		break;

	case PLC_MCP23008:
	case PLC_MCP23017:
		if (index >= PIN_MCP230XX_NUM_GPIOS(pin)) {
			return INDEX_OUT_OF_RANGE;
		}
		mcp = find_pin_mcp230xx(pin);
		if (mcp == NULL) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = type == PLC_MCP23017 ? ARRAY_MCP23017_SET_PIN_MODE_FAIL :
					      ARRAY_MCP23008_SET_PIN_MODE_FAIL;
		if (mode == PLC_XGPIO_OUTPUT) {
			ret = mcp230xx_set_output(
				I2C, &mcp->handle, index, mcp->timeout_ms);
		} else {
			ret = mcp230xx_set_input(I2C,
						 &mcp->handle,
						 index,
						 MCP230XX_NO_PULLUP,
						 mcp->timeout_ms);
		}
		break;
	}

	return ret < 0 ? code : ret;
}

int plc_xgpio_digital_write(uint32_t pin, uint8_t value)
{
	plc_xgpio_mcp230xx_t* mcp;
	plc_xgpio_pca9685_t* pca;
	int code;
	int ret;

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	const uint8_t index = PIN_PERIPHERAL_INDEX(pin);

	switch (type) {
	case PLC_LTC2309:
	case PLC_ADS1015:
		return PIN_NOT_SUPPORTED;
	default:
		return UNKNOWN_PIN_TYPE;

	case PLC_DIRECT:
		code = NORMAL_GPIO_WRITE_FAIL;
		ret = plc_normal_gpio_write(PIN_DIRECT_INDEX(pin), value);
		break;

	case PLC_PCA9685:
		if (index >= PCA9685_NUM_OUTPUTS) {
			return INDEX_OUT_OF_RANGE;
		}
		pca = find_pca9685(PIN_PERIPHERAL_ADDR(pin));
		if (pca == NULL) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = ARRAY_PCA9685_WRITE_FAIL;
		ret = pca9685_write_output(I2C,
					   &pca->handle,
					   index,
					   value ? PCA9685_OUTPUT_ON :
						   PCA9685_OUTPUT_OFF,
					   pca->timeout_ms);
		break;

	case PLC_MCP23008:
	case PLC_MCP23017:
		if (index >= PIN_MCP230XX_NUM_GPIOS(pin)) {
			return INDEX_OUT_OF_RANGE;
		}
		mcp = find_pin_mcp230xx(pin);
		if (mcp == NULL) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = type == PLC_MCP23017 ? ARRAY_MCP23017_WRITE_FAIL :
					      ARRAY_MCP23008_WRITE_FAIL;
		ret = mcp230xx_write_gpio(
			I2C, &mcp->handle, index, value, mcp->timeout_ms);
		break;
	}

	return ret < 0 ? code : 0;
}

int plc_xgpio_digital_read(uint32_t pin)
{
	plc_xgpio_mcp230xx_t* mcp;
	plc_xgpio_ltc2309_t* ltc;
	plc_xgpio_ads101x_t* ads;
	uint16_t analog;
	uint8_t level;

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	const uint8_t index = PIN_PERIPHERAL_INDEX(pin);

	switch (type) {
	case PLC_DIRECT:
		if (plc_normal_gpio_read(PIN_DIRECT_INDEX(pin), &level) != 0) {
			return 0;
		}
		return level;

	case PLC_MCP23008:
	case PLC_MCP23017:
		mcp = find_pin_mcp230xx(pin);
		if (mcp == NULL || mcp230xx_read_gpio(I2C,
						      &mcp->handle,
						      index,
						      &level,
						      mcp->timeout_ms) != 0) {
			return 0;
		}
		return level ? 1 : 0;

	case PLC_LTC2309:
		ltc = find_ltc2309(PIN_PERIPHERAL_ADDR(pin));
		if (ltc == NULL || index >= LTC2309_NUM_INPUTS ||
		    ltc2309_unsigned_single_ended_read(I2C,
						       &ltc->handle,
						       LTC2309_CHANNELS[index],
						       &analog,
						       ltc->timeout_ms) != 0) {
			return 0;
		}
		return analog > LTC2309_HIGH_THRESHOLD ? 1 : 0;

	case PLC_ADS1015:
		ads = find_ads1015(PIN_PERIPHERAL_ADDR(pin));
		if (ads == NULL || index < ADS101X_P0_GND ||
		    index >= ADS101X_NUM_INPUTS ||
		    ads101x_unsigned_single_read(I2C,
						 &ads->handle,
						 (ADS101X_INPUT)index,
						 &analog,
						 ads->timeout_ms) != 0) {
			return 0;
		}
		return analog > ADS1015_HIGH_THRESHOLD ? 1 : 0;

	default:
		return 0;
	}
}

int plc_xgpio_analog_write(uint32_t pin, uint16_t value)
{
	plc_xgpio_pca9685_t* pca;
	int code;
	int ret;

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	switch (type) {
	case PLC_MCP23008:
	case PLC_MCP23017:
	case PLC_LTC2309:
	case PLC_ADS1015:
		return PIN_NOT_SUPPORTED;
	default:
		return UNKNOWN_PIN_TYPE;

	case PLC_DIRECT:
		code = NORMAL_GPIO_PWM_WRITE_FAIL;
		ret = plc_normal_gpio_pwm_write(PIN_DIRECT_INDEX(pin), value);
		break;

	case PLC_PCA9685:
		if (PIN_PERIPHERAL_INDEX(pin) >= PCA9685_NUM_OUTPUTS) {
			return INDEX_OUT_OF_RANGE;
		}
		pca = find_pca9685(PIN_PERIPHERAL_ADDR(pin));
		if (pca == NULL) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = ARRAY_PCA9685_PWM_WRITE_FAIL;
		ret = pca9685_write_output(I2C,
					   &pca->handle,
					   PIN_PERIPHERAL_INDEX(pin),
					   value,
					   pca->timeout_ms);
		break;
	}

	return ret < 0 ? code : 0;
}

int plc_xgpio_analog_write_set_frequency(uint32_t pin, uint32_t freq_hz)
{
	plc_xgpio_pca9685_t* pca;
	int code;
	int ret;

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	switch (type) {
	case PLC_MCP23008:
	case PLC_MCP23017:
	case PLC_LTC2309:
	case PLC_ADS1015:
		return PIN_NOT_SUPPORTED;
	default:
		return UNKNOWN_PIN_TYPE;

	case PLC_DIRECT:
		code = NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL;
		ret = plc_normal_gpio_pwm_set_frequency(PIN_DIRECT_INDEX(pin),
							freq_hz);
		break;

	case PLC_PCA9685:
		if (PIN_PERIPHERAL_INDEX(pin) >= PCA9685_NUM_OUTPUTS) {
			return INDEX_OUT_OF_RANGE;
		}
		if (freq_hz < PCA9685_MIN_FREQ_HZ ||
		    freq_hz > PCA9685_MAX_FREQ_HZ) {
			return FREQ_OUT_OF_RANGE;
		}
		pca = find_pca9685(PIN_PERIPHERAL_ADDR(pin));
		if (pca == NULL) {
			return I2C_ADDR_NOT_FOUND;
		}

		code = ARRAY_PCA9685_PWM_CHANGE_FREQ_FAIL;
		ret = pca9685_set_frequency(
			I2C, &pca->handle, (uint16_t)freq_hz, pca->timeout_ms);
		break;
	}

	return ret < 0 ? code : 0;
}

uint16_t plc_xgpio_analog_read(uint32_t pin)
{
	plc_xgpio_ltc2309_t* ltc;
	plc_xgpio_ads101x_t* ads;
	uint16_t value;

	const uint8_t type = PIN_PERIPHERAL_TYPE(pin);

	if (type != PLC_DIRECT && I2C == PLC_XGPIO_NO_I2C_BUS) {
		return -I2C_PIN_WITHOUT_I2C_BUS;
	}

	const uint8_t index = PIN_PERIPHERAL_INDEX(pin);

	switch (type) {
	case PLC_DIRECT:
		if (plc_normal_gpio_analog_read(PIN_DIRECT_INDEX(pin),
						&value) != 0) {
			return 0;
		}
		return value;

	case PLC_LTC2309:
		ltc = find_ltc2309(PIN_PERIPHERAL_ADDR(pin));
		if (ltc == NULL || index >= LTC2309_NUM_INPUTS ||
		    ltc2309_unsigned_single_ended_read(I2C,
						       &ltc->handle,
						       LTC2309_CHANNELS[index],
						       &value,
						       ltc->timeout_ms) != 0) {
			return 0;
		}
		return value;

	case PLC_ADS1015:
		ads = find_ads1015(PIN_PERIPHERAL_ADDR(pin));
		if (ads == NULL || index < ADS101X_P0_GND ||
		    index >= ADS101X_NUM_INPUTS ||
		    ads101x_unsigned_single_read(I2C,
						 &ads->handle,
						 (ADS101X_INPUT)index,
						 &value,
						 ads->timeout_ms) != 0) {
			return 0;
		}
		return value;

	default:
		return 0;
	}
}

int plc_xgpio_digital_write_all(uint8_t addr, uint32_t values)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS];
	plc_xgpio_mcp230xx_t* mcp;
	plc_xgpio_pca9685_t* pca;
	int i;

	if (I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_ADDR_WITHOUT_I2C_BUS;
	}

	mcp = find_mcp230xx(
		PLC_XGPIO_ARRAY_MCP23008, PLC_XGPIO_NUM_ARRAY_MCP23008, addr);
	if (mcp != NULL) {
		return mcp230xx_write_all(I2C,
					  &mcp->handle,
					  values & 0xFF,
					  mcp->timeout_ms) == 0 ?
			       0 :
			       ARRAY_MCP23008_WRITE_ALL_FAIL;
	}

	pca = find_pca9685(addr);
	if (pca != NULL) {
		for (i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
			duty[i] = (values & (1UL << i)) ? PCA9685_OUTPUT_ON :
							  PCA9685_OUTPUT_OFF;
		}
		return pca9685_write_all(
			       I2C, &pca->handle, duty, pca->timeout_ms) == 0 ?
			       0 :
			       ARRAY_PCA9685_WRITE_ALL_FAIL;
	}

	mcp = find_mcp230xx(
		PLC_XGPIO_ARRAY_MCP23017, PLC_XGPIO_NUM_ARRAY_MCP23017, addr);
	if (mcp != NULL) {
		return mcp230xx_write_all(I2C,
					  &mcp->handle,
					  values & 0xFFFF,
					  mcp->timeout_ms) == 0 ?
			       0 :
			       ARRAY_MCP23017_WRITE_ALL_FAIL;
	}

	return I2C_ADDR_NOT_FOUND;
}

int plc_xgpio_digital_read_all(uint8_t addr, void* values)
{
	plc_xgpio_mcp230xx_t* mcp;
	uint16_t levels;
	uint8_t low;

	if (I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_ADDR_WITHOUT_I2C_BUS;
	}

	mcp = find_mcp230xx(
		PLC_XGPIO_ARRAY_MCP23008, PLC_XGPIO_NUM_ARRAY_MCP23008, addr);
	if (mcp != NULL) {
		if (values == NULL) {
			return NULL_ARGUMENT;
		}
		if (mcp230xx_read_all(
			    I2C, &mcp->handle, &levels, mcp->timeout_ms) != 0) {
			return ARRAY_MCP23008_READ_ALL_FAIL;
		}
		low = (uint8_t)levels;
		memcpy(values, &low, sizeof(low));
		return 0;
	}

	mcp = find_mcp230xx(
		PLC_XGPIO_ARRAY_MCP23017, PLC_XGPIO_NUM_ARRAY_MCP23017, addr);
	if (mcp != NULL) {
		if (values == NULL) {
			return NULL_ARGUMENT;
		}
		if (mcp230xx_read_all(
			    I2C, &mcp->handle, &levels, mcp->timeout_ms) != 0) {
			return ARRAY_MCP23017_READ_ALL_FAIL;
		}
		memcpy(values, &levels, sizeof(levels));
		return 0;
	}

	return I2C_ADDR_NOT_FOUND;
}

int plc_xgpio_analog_write_all(uint8_t addr, const void* values)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS];
	plc_xgpio_pca9685_t* pca;

	if (I2C == PLC_XGPIO_NO_I2C_BUS) {
		return I2C_ADDR_WITHOUT_I2C_BUS;
	}

	pca = find_pca9685(addr);
	if (pca == NULL) {
		return I2C_ADDR_NOT_FOUND;
	}
	if (values == NULL) {
		return NULL_ARGUMENT;
	}
	memcpy(duty, values, sizeof(duty));
	return pca9685_write_all(I2C, &pca->handle, duty, pca->timeout_ms) ==
			       0 ?
		       0 :
		       ARRAY_PCA9685_PWM_WRITE_ALL_FAIL;
}
