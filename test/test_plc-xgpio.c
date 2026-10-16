/*
 * Copyright (c) 2026 Industrial Shields. All rights reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Tests for src/plc-xgpio.c. The four drivers it calls are mocked, and the
 * plc_normal_gpio_* functions it expects from the library using it are
 * defined here, recording what they get and returning what each test sets.
 */

#include "unity.h"

#include "mock_peripheral-ads101x.h"
#include "mock_peripheral-ltc2309.h"
#include "mock_peripheral-mcp230xx.h"
#include "mock_peripheral-pca9685.h"
#include "plc-xgpio.h"

#include <string.h>

// Only compared by address, never dereferenced
#define TEST_I2C ((const i2c_interface_t*)(!NULL))
#define TEST_TIMEOUT_MS ((uint32_t)42)

// clang-format off
#define MCP23008_ADDR 0x20
#define MCP23017_ADDR 0x24
#define PCA9685_ADDR  0x40
#define LTC2309_ADDR  0x08
#define ADS1015_ADDR  0x48
#define MISSING_ADDR  0x7F
// clang-format on

static plc_xgpio_mcp230xx_t mcp23008[1];
static plc_xgpio_mcp230xx_t mcp23017[1];
static plc_xgpio_pca9685_t pca9685[1];
static plc_xgpio_ltc2309_t ltc2309[1];
static plc_xgpio_ads101x_t ads1015[1];

/*
 * The direct GPIO functions plc-xgpio expects. Each records its arguments and
 * returns normal_ret, and the reads store normal_level or normal_analog.
 */
const uint8_t PLC_NORMAL_GPIO_INPUT = 0x01;
const uint8_t PLC_NORMAL_GPIO_INPUT_PULLUP = 0x05;
const uint8_t PLC_NORMAL_GPIO_INPUT_INVERTED = 0x81;
const uint8_t PLC_NORMAL_GPIO_INPUT_PULLUP_INVERTED = 0x85;
const uint8_t PLC_NORMAL_GPIO_OUTPUT = 0x03;

static uint32_t normal_calls;
static uint32_t normal_pin;
static uint32_t normal_arg;
static uint32_t normal_analog;
static uint8_t normal_level;
static int normal_ret;

int plc_normal_gpio_set_pin_mode(uint32_t pin, uint8_t mode)
{
	normal_calls++;
	normal_pin = pin;
	normal_arg = mode;
	return normal_ret;
}

int plc_normal_gpio_write(uint32_t pin, uint8_t value)
{
	normal_calls++;
	normal_pin = pin;
	normal_arg = value;
	return normal_ret;
}

int plc_normal_gpio_pwm_set_frequency(uint32_t pin, uint32_t freq)
{
	normal_calls++;
	normal_pin = pin;
	normal_arg = freq;
	return normal_ret;
}

int plc_normal_gpio_pwm_write(uint32_t pin, uint32_t value)
{
	normal_calls++;
	normal_pin = pin;
	normal_arg = value;
	return normal_ret;
}

int plc_normal_gpio_read(uint32_t pin, uint8_t* read)
{
	normal_calls++;
	normal_pin = pin;
	*read = normal_level;
	return normal_ret;
}

int plc_normal_gpio_analog_read(uint32_t pin, uint32_t* read)
{
	normal_calls++;
	normal_pin = pin;
	*read = normal_analog;
	return normal_ret;
}

// What the pca9685_write_all stub got, and what it returns
static uint16_t pca_all_values[PCA9685_NUM_OUTPUTS];
static int pca_all_ret;

static int pca9685_write_all_stub(const i2c_interface_t* i2c,
				  pca9685_t* pca,
				  const uint16_t values[PCA9685_NUM_OUTPUTS],
				  uint32_t timeout_ms,
				  int cmock_num_calls)
{
	(void)cmock_num_calls;

	TEST_ASSERT_EQUAL_PTR(TEST_I2C, i2c);
	TEST_ASSERT_EQUAL_PTR(&pca9685[0].handle, pca);
	TEST_ASSERT_EQUAL_UINT32(TEST_TIMEOUT_MS, timeout_ms);
	memcpy(pca_all_values, values, sizeof(pca_all_values));
	return pca_all_ret;
}

void setUp(void)
{
	memset(&plc_xgpio_peripherals, 0, sizeof(plc_xgpio_peripherals));
	mcp23008[0].addr = MCP23008_ADDR;
	mcp23008[0].timeout_ms = TEST_TIMEOUT_MS;
	mcp23017[0].addr = MCP23017_ADDR;
	mcp23017[0].timeout_ms = TEST_TIMEOUT_MS;
	pca9685[0].addr = PCA9685_ADDR;
	pca9685[0].timeout_ms = TEST_TIMEOUT_MS;
	ltc2309[0].addr = LTC2309_ADDR;
	ltc2309[0].timeout_ms = TEST_TIMEOUT_MS;
	ads1015[0].addr = ADS1015_ADDR;
	ads1015[0].timeout_ms = TEST_TIMEOUT_MS;

	plc_xgpio_peripherals.i2c = TEST_I2C;
	PLC_XGPIO_ARRAY_MCP23008 = mcp23008;
	PLC_XGPIO_NUM_ARRAY_MCP23008 = 1;
	PLC_XGPIO_ARRAY_MCP23017 = mcp23017;
	PLC_XGPIO_NUM_ARRAY_MCP23017 = 1;
	PLC_XGPIO_ARRAY_PCA9685 = pca9685;
	PLC_XGPIO_NUM_ARRAY_PCA9685 = 1;
	PLC_XGPIO_ARRAY_LTC2309 = ltc2309;
	PLC_XGPIO_NUM_ARRAY_LTC2309 = 1;
	PLC_XGPIO_ARRAY_ADS1015 = ads1015;
	PLC_XGPIO_NUM_ARRAY_ADS1015 = 1;

	normal_calls = 0;
	normal_pin = 0;
	normal_arg = 0;
	normal_analog = 0;
	normal_level = 0;
	normal_ret = 0;
	memset(pca_all_values, 0xA5, sizeof(pca_all_values));
	pca_all_ret = 0;
}

void tearDown(void)
{
}

// ---- Pin encoding ----

void test_make_pin_encodes_type_addr_and_index(void)
{
	TEST_ASSERT_EQUAL_HEX32(0x02210006, MAKE_PIN_MCP23008(0x21, 6));
	TEST_ASSERT_EQUAL_HEX32(0x0320000F, MAKE_PIN_MCP23017(0x20, 0x0F));
	TEST_ASSERT_EQUAL_HEX32(0x0140000B, MAKE_PIN_PCA9685(0x40, 11));
	TEST_ASSERT_EQUAL_HEX32(0x04080007, MAKE_PIN_LTC2309(0x08, 7));
	TEST_ASSERT_EQUAL_HEX32(0x05480007,
				MAKE_PIN_ADS1015(0x48, ADS101X_P3_GND));
}

void test_make_pin_ltc2309_diff_sets_the_differential_byte(void)
{
	TEST_ASSERT_EQUAL_HEX32(0x04080103,
				MAKE_PIN_LTC2309_DIFF(0x08, LTC2309_P6_N7));
	// A plain LTC2309 pin is single-ended
	TEST_ASSERT_EQUAL_HEX32(0x04080003, MAKE_PIN_LTC2309(0x08, 3));
}

void test_make_pin_direct_keeps_a_24_bit_index(void)
{
	TEST_ASSERT_EQUAL_HEX32(0x00000005, MAKE_PIN_DIRECT(5));
	TEST_ASSERT_EQUAL_HEX32(0x00123456, MAKE_PIN_DIRECT(0x123456));
	TEST_ASSERT_EQUAL_HEX32(0x00ABCDEF, MAKE_PIN_DIRECT(0xFFABCDEF));
}

// ---- plc_xgpio_pin_mode ----

void test_pin_mode_rejects_an_invalid_mode_before_anything_else(void)
{
	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;

	// An unknown bit
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_pin_mode(MAKE_PIN_DIRECT(5), 0x08));
	// An output with the input flags
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_pin_mode(MAKE_PIN_DIRECT(5),
						 PLC_XGPIO_OUTPUT |
							 PLC_XGPIO_PULLUP));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23008(MCP23008_ADDR, 0), 7));
	TEST_ASSERT_EQUAL_UINT32(0, normal_calls);
}

void test_pin_mode_direct_passes_the_platform_modes(void)
{
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_pin_mode(MAKE_PIN_DIRECT(0x123456),
						 PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_UINT32(0x123456, normal_pin);
	TEST_ASSERT_EQUAL_UINT32(PLC_NORMAL_GPIO_OUTPUT, normal_arg);

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_pin_mode(MAKE_PIN_DIRECT(7), PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_UINT32(7, normal_pin);
	TEST_ASSERT_EQUAL_UINT32(PLC_NORMAL_GPIO_INPUT, normal_arg);

	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_pin_mode(MAKE_PIN_DIRECT(7),
						 PLC_XGPIO_INPUT |
							 PLC_XGPIO_PULLUP));
	TEST_ASSERT_EQUAL_UINT32(PLC_NORMAL_GPIO_INPUT_PULLUP, normal_arg);
}

void test_pin_mode_direct_input_flags_map_to_the_platform_modes(void)
{
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_pin_mode(MAKE_PIN_DIRECT(7),
						 PLC_XGPIO_INPUT |
							 PLC_XGPIO_INVERTED));
	TEST_ASSERT_EQUAL_UINT32(PLC_NORMAL_GPIO_INPUT_INVERTED, normal_arg);

	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_pin_mode(MAKE_PIN_DIRECT(7),
						 PLC_XGPIO_INPUT |
							 PLC_XGPIO_PULLUP |
							 PLC_XGPIO_INVERTED));
	TEST_ASSERT_EQUAL_UINT32(PLC_NORMAL_GPIO_INPUT_PULLUP_INVERTED,
				 normal_arg);
}

void test_pin_mode_direct_platform_rejecting_a_flag_fails(void)
{
	// As a platform without inversion does
	normal_ret = -1;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_SET_PIN_MODE_FAIL,
			      plc_xgpio_pin_mode(MAKE_PIN_DIRECT(7),
						 PLC_XGPIO_INPUT |
							 PLC_XGPIO_INVERTED));
}

void test_pin_mode_direct_passes_1_through(void)
{
	normal_ret = 1;
	TEST_ASSERT_EQUAL_INT(
		1, plc_xgpio_pin_mode(MAKE_PIN_DIRECT(5), PLC_XGPIO_OUTPUT));
}

void test_pin_mode_direct_failure(void)
{
	normal_ret = -1;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_SET_PIN_MODE_FAIL,
			      plc_xgpio_pin_mode(MAKE_PIN_DIRECT(5),
						 PLC_XGPIO_OUTPUT));
}

void test_pin_mode_mcp23008_output(void)
{
	mcp230xx_set_output_ExpectAndReturn(
		TEST_I2C, &mcp23008[0].handle, 3, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23008(MCP23008_ADDR, 3),
				   PLC_XGPIO_OUTPUT));
}

void test_pin_mode_mcp230xx_input_flags_map_to_the_input_configs(void)
{
	static const struct {
		uint8_t mode;
		MCP230XX_INPUT_CONFIG config;
	} cases[] = {
		{ PLC_XGPIO_INPUT, MCP230XX_NO_PULLUP },
		{ PLC_XGPIO_INPUT | PLC_XGPIO_PULLUP, MCP230XX_PULLUP },
		{ PLC_XGPIO_INPUT | PLC_XGPIO_INVERTED,
		  MCP230XX_NO_PULLUP_INVERTED },
		{ PLC_XGPIO_INPUT | PLC_XGPIO_PULLUP | PLC_XGPIO_INVERTED,
		  MCP230XX_PULLUP_INVERTED },
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
						   &mcp23017[0].handle,
						   12,
						   cases[i].config,
						   TEST_TIMEOUT_MS,
						   0);
		TEST_ASSERT_EQUAL_INT(
			0,
			plc_xgpio_pin_mode(MAKE_PIN_MCP23017(MCP23017_ADDR, 12),
					   cases[i].mode));
	}
}

void test_pin_mode_fixed_direction_pins_reject_the_input_flags(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pin_mode(MAKE_PIN_ADS1015(ADS1015_ADDR,
						    ADS101X_P0_GND),
				   PLC_XGPIO_INPUT | PLC_XGPIO_PULLUP));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR, 0),
				   PLC_XGPIO_INPUT | PLC_XGPIO_INVERTED));
}

void test_pin_mode_mcp23008_input_has_no_pullup(void)
{
	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp23008[0].handle,
					   7,
					   MCP230XX_NO_PULLUP,
					   TEST_TIMEOUT_MS,
					   0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23008(MCP23008_ADDR, 7),
				   PLC_XGPIO_INPUT));
}

void test_pin_mode_mcp230xx_passes_1_through(void)
{
	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp23017[0].handle,
					   15,
					   MCP230XX_NO_PULLUP,
					   TEST_TIMEOUT_MS,
					   1);
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23017(MCP23017_ADDR, 15),
				   PLC_XGPIO_INPUT));
}

void test_pin_mode_mcp230xx_failures_use_the_chip_code(void)
{
	mcp230xx_set_output_ExpectAndReturn(
		TEST_I2C, &mcp23008[0].handle, 0, TEST_TIMEOUT_MS, -1);
	TEST_ASSERT_EQUAL_INT(
		ARRAY_MCP23008_SET_PIN_MODE_FAIL,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23008(MCP23008_ADDR, 0),
				   PLC_XGPIO_OUTPUT));

	mcp230xx_set_output_ExpectAndReturn(
		TEST_I2C, &mcp23017[0].handle, 8, TEST_TIMEOUT_MS, -1);
	TEST_ASSERT_EQUAL_INT(
		ARRAY_MCP23017_SET_PIN_MODE_FAIL,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23017(MCP23017_ADDR, 8),
				   PLC_XGPIO_OUTPUT));
}

void test_pin_mode_mcp230xx_index_out_of_range(void)
{
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23008(MCP23008_ADDR, 8),
				   PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23017(MCP23017_ADDR, 16),
				   PLC_XGPIO_OUTPUT));
}

void test_pin_mode_mcp230xx_searches_the_array_of_its_type(void)
{
	// The MCP23017 address is not in the MCP23008 array, and vice versa
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23008(MCP23017_ADDR, 0),
				   PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23017(MCP23008_ADDR, 0),
				   PLC_XGPIO_OUTPUT));
}

void test_pin_mode_ltc2309_differential_pins_are_inputs(void)
{
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_pin_mode(MAKE_PIN_LTC2309_DIFF(LTC2309_ADDR,
							 LTC2309_P7_N6),
				   PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pin_mode(MAKE_PIN_LTC2309_DIFF(LTC2309_ADDR,
							 LTC2309_P0_N1),
				   PLC_XGPIO_OUTPUT));
}

void test_pin_mode_ltc2309_rejects_an_unknown_read_byte(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pin_mode(
			_MAKE_PIN_PLC(PLC_LTC2309, LTC2309_ADDR, 0x02, 0),
			PLC_XGPIO_INPUT));
}

void test_pin_mode_fixed_direction_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_pin_mode(MAKE_PIN_PCA9685(PCA9685_ADDR, 15),
				   PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR, 7),
				   PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_pin_mode(MAKE_PIN_ADS1015(ADS1015_ADDR,
						    ADS101X_P3_GND),
				   PLC_XGPIO_INPUT));
}

void test_pin_mode_fixed_direction_pins_reject_the_other_mode(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pin_mode(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				   PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR, 0),
				   PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pin_mode(MAKE_PIN_ADS1015(ADS1015_ADDR,
						    ADS101X_P0_GND),
				   PLC_XGPIO_OUTPUT));
}

void test_pin_mode_fixed_direction_pins_check_the_index_first(void)
{
	// Out of range and with the wrong mode: the index is reported
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_pin_mode(MAKE_PIN_PCA9685(PCA9685_ADDR, 16),
				   PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR, 8),
				   PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_pin_mode(MAKE_PIN_ADS1015(ADS1015_ADDR, 8),
				   PLC_XGPIO_INPUT));
}

void test_pin_mode_fixed_direction_pins_not_in_the_arrays(void)
{
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_pin_mode(MAKE_PIN_PCA9685(MISSING_ADDR, 0),
				   PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_pin_mode(MAKE_PIN_LTC2309(MISSING_ADDR, 0),
				   PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_pin_mode(MAKE_PIN_ADS1015(MISSING_ADDR,
						    ADS101X_P0_GND),
				   PLC_XGPIO_INPUT));
}

void test_pin_mode_unknown_pin_type(void)
{
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_pin_mode(0x06000000, PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_pin_mode(0xFF000000, PLC_XGPIO_INPUT));
}

void test_pin_mode_without_bus_only_handles_direct_pins(void)
{
	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_pin_mode(MAKE_PIN_DIRECT(5), PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_UINT32(1, normal_calls);
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23008(MCP23008_ADDR, 0),
				   PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_pin_mode(MAKE_PIN_ADS1015(ADS1015_ADDR,
						    ADS101X_P0_GND),
				   PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_I2C_BUS,
			      plc_xgpio_pin_mode(0x06000000, PLC_XGPIO_INPUT));
}

// ---- plc_xgpio_digital_write ----

void test_digital_write_direct(void)
{
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write(MAKE_PIN_DIRECT(9), PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_UINT32(9, normal_pin);
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_HIGH, normal_arg);

	normal_ret = 1;
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write(MAKE_PIN_DIRECT(9), PLC_XGPIO_LOW));

	normal_ret = -1;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_WRITE_FAIL,
			      plc_xgpio_digital_write(MAKE_PIN_DIRECT(9),
						      PLC_XGPIO_LOW));
}

void test_digital_write_pca9685_is_fully_on_or_off(void)
{
	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     11,
					     PCA9685_OUTPUT_ON,
					     TEST_TIMEOUT_MS,
					     0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 11),
					PLC_XGPIO_HIGH));

	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     11,
					     PCA9685_OUTPUT_OFF,
					     TEST_TIMEOUT_MS,
					     0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 11),
					PLC_XGPIO_LOW));

	// Any value other than 0 is high
	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     0,
					     PCA9685_OUTPUT_ON,
					     TEST_TIMEOUT_MS,
					     0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 5));
}

void test_digital_write_pca9685_errors(void)
{
	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     0,
					     PCA9685_OUTPUT_ON,
					     TEST_TIMEOUT_MS,
					     -1);
	TEST_ASSERT_EQUAL_INT(
		ARRAY_PCA9685_WRITE_FAIL,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 16),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(MISSING_ADDR, 0),
					PLC_XGPIO_HIGH));
}

void test_digital_write_mcp230xx(void)
{
	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp23008[0].handle,
					    4,
					    PLC_XGPIO_HIGH,
					    TEST_TIMEOUT_MS,
					    0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_write(MAKE_PIN_MCP23008(MCP23008_ADDR, 4),
					PLC_XGPIO_HIGH));

	// Already set
	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp23017[0].handle,
					    12,
					    PLC_XGPIO_LOW,
					    TEST_TIMEOUT_MS,
					    1);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_write(MAKE_PIN_MCP23017(MCP23017_ADDR, 12),
					PLC_XGPIO_LOW));
}

void test_digital_write_mcp230xx_errors(void)
{
	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp23008[0].handle,
					    0,
					    PLC_XGPIO_HIGH,
					    TEST_TIMEOUT_MS,
					    -1);
	TEST_ASSERT_EQUAL_INT(
		ARRAY_MCP23008_WRITE_FAIL,
		plc_xgpio_digital_write(MAKE_PIN_MCP23008(MCP23008_ADDR, 0),
					PLC_XGPIO_HIGH));

	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp23017[0].handle,
					    0,
					    PLC_XGPIO_HIGH,
					    TEST_TIMEOUT_MS,
					    -1);
	TEST_ASSERT_EQUAL_INT(
		ARRAY_MCP23017_WRITE_FAIL,
		plc_xgpio_digital_write(MAKE_PIN_MCP23017(MCP23017_ADDR, 0),
					PLC_XGPIO_HIGH));

	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_write(MAKE_PIN_MCP23008(MCP23008_ADDR, 8),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_write(MAKE_PIN_MCP23017(MCP23017_ADDR, 16),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_write(MAKE_PIN_MCP23008(MISSING_ADDR, 0),
					PLC_XGPIO_HIGH));
}

void test_digital_write_unsupported_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_digital_write(MAKE_PIN_LTC2309(LTC2309_ADDR, 0),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_digital_write(MAKE_PIN_ADS1015(ADS1015_ADDR,
							 ADS101X_P0_GND),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_digital_write(0x06000000,
						      PLC_XGPIO_HIGH));
}

void test_digital_write_without_bus(void)
{
	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write(MAKE_PIN_DIRECT(9), PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					PLC_XGPIO_HIGH));
}

// ---- plc_xgpio_digital_read ----

void test_digital_read_direct(void)
{
	normal_level = PLC_XGPIO_HIGH;
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_HIGH,
			      plc_xgpio_digital_read(MAKE_PIN_DIRECT(0x10203)));
	TEST_ASSERT_EQUAL_UINT32(0x10203, normal_pin);

	normal_ret = -1;
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_LOW,
			      plc_xgpio_digital_read(MAKE_PIN_DIRECT(3)));
}

void test_digital_read_mcp230xx(void)
{
	uint8_t level = 1;

	mcp230xx_read_gpio_ExpectAndReturn(
		TEST_I2C, &mcp23017[0].handle, 9, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_read_gpio_IgnoreArg_return_value();
	mcp230xx_read_gpio_ReturnThruPtr_return_value(&level);
	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_HIGH,
		plc_xgpio_digital_read(MAKE_PIN_MCP23017(MCP23017_ADDR, 9)));

	mcp230xx_read_gpio_ExpectAndReturn(
		TEST_I2C, &mcp23008[0].handle, 2, NULL, TEST_TIMEOUT_MS, -1);
	mcp230xx_read_gpio_IgnoreArg_return_value();
	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_LOW,
		plc_xgpio_digital_read(MAKE_PIN_MCP23008(MCP23008_ADDR, 2)));

	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_LOW,
		plc_xgpio_digital_read(MAKE_PIN_MCP23008(MISSING_ADDR, 2)));
}

static void expect_ltc2309_read(LTC2309_INPUT channel, uint16_t* value, int ret)
{
	ltc2309_unsigned_single_ended_read_ExpectAndReturn(TEST_I2C,
							   &ltc2309[0].handle,
							   channel,
							   NULL,
							   TEST_TIMEOUT_MS,
							   ret);
	ltc2309_unsigned_single_ended_read_IgnoreArg_read_value();
	ltc2309_unsigned_single_ended_read_ReturnThruPtr_read_value(value);
}

static void expect_ads1015_read(ADS101X_INPUT input, uint16_t* value, int ret)
{
	ads101x_unsigned_single_read_ExpectAndReturn(
		TEST_I2C, &ads1015[0].handle, input, NULL, TEST_TIMEOUT_MS, ret);
	ads101x_unsigned_single_read_IgnoreArg_return_value();
	ads101x_unsigned_single_read_ReturnThruPtr_return_value(value);
}

void test_digital_read_ltc2309_threshold_is_above_1636(void)
{
	uint16_t at = 1636;
	uint16_t above = 1637;

	expect_ltc2309_read(LTC2309_CH0, &at, 0);
	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_LOW,
		plc_xgpio_digital_read(MAKE_PIN_LTC2309(LTC2309_ADDR, 0)));

	expect_ltc2309_read(LTC2309_CH0, &above, 0);
	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_HIGH,
		plc_xgpio_digital_read(MAKE_PIN_LTC2309(LTC2309_ADDR, 0)));
}

static void
expect_ltc2309_diff_read(LTC2309_DIFF_INPUT pair, uint16_t* value, int ret)
{
	ltc2309_unsigned_differential_read_ExpectAndReturn(
		TEST_I2C, &ltc2309[0].handle, pair, NULL, TEST_TIMEOUT_MS, ret);
	ltc2309_unsigned_differential_read_IgnoreArg_read_value();
	ltc2309_unsigned_differential_read_ReturnThruPtr_read_value(value);
}

void test_digital_read_ltc2309_differential_threshold_is_above_1636(void)
{
	uint16_t at = 1636;
	uint16_t above = 1637;

	expect_ltc2309_diff_read(LTC2309_P2_N3, &at, 0);
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_LOW,
			      plc_xgpio_digital_read(MAKE_PIN_LTC2309_DIFF(
				      LTC2309_ADDR, LTC2309_P2_N3)));

	expect_ltc2309_diff_read(LTC2309_P2_N3, &above, 0);
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_HIGH,
			      plc_xgpio_digital_read(MAKE_PIN_LTC2309_DIFF(
				      LTC2309_ADDR, LTC2309_P2_N3)));
}

void test_digital_read_ads1015_threshold_is_above_818(void)
{
	uint16_t at = 818;
	uint16_t above = 819;

	expect_ads1015_read(ADS101X_P2_GND, &at, 0);
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_LOW,
			      plc_xgpio_digital_read(MAKE_PIN_ADS1015(
				      ADS1015_ADDR, ADS101X_P2_GND)));

	expect_ads1015_read(ADS101X_P2_GND, &above, 0);
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_HIGH,
			      plc_xgpio_digital_read(MAKE_PIN_ADS1015(
				      ADS1015_ADDR, ADS101X_P2_GND)));
}

void test_digital_read_analog_failures_read_low(void)
{
	uint16_t high = 4000;

	expect_ltc2309_read(LTC2309_CH0, &high, -1);
	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_LOW,
		plc_xgpio_digital_read(MAKE_PIN_LTC2309(LTC2309_ADDR, 0)));

	expect_ads1015_read(ADS101X_P0_GND, &high, -1);
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_LOW,
			      plc_xgpio_digital_read(MAKE_PIN_ADS1015(
				      ADS1015_ADDR, ADS101X_P0_GND)));

	// No driver call for an index out of range or a missing chip
	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_LOW,
		plc_xgpio_digital_read(MAKE_PIN_LTC2309(LTC2309_ADDR, 8)));
	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_LOW,
		plc_xgpio_digital_read(MAKE_PIN_ADS1015(ADS1015_ADDR, 8)));
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_LOW,
			      plc_xgpio_digital_read(MAKE_PIN_ADS1015(
				      MISSING_ADDR, ADS101X_P0_GND)));
}

void test_digital_read_unsupported_pins_read_low(void)
{
	TEST_ASSERT_EQUAL_INT(
		PLC_XGPIO_LOW,
		plc_xgpio_digital_read(MAKE_PIN_PCA9685(PCA9685_ADDR, 0)));
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_LOW,
			      plc_xgpio_digital_read(0x06000000));
}

void test_digital_read_without_bus(void)
{
	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;
	normal_level = PLC_XGPIO_HIGH;

	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_HIGH,
			      plc_xgpio_digital_read(MAKE_PIN_DIRECT(3)));
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_I2C_BUS,
			      plc_xgpio_digital_read(MAKE_PIN_ADS1015(
				      ADS1015_ADDR, ADS101X_P0_GND)));
}

// ---- plc_xgpio_analog_write ----

void test_analog_write_direct(void)
{
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_analog_write(MAKE_PIN_DIRECT(4), 1234));
	TEST_ASSERT_EQUAL_UINT32(4, normal_pin);
	TEST_ASSERT_EQUAL_UINT32(1234, normal_arg);

	normal_ret = 1;
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_analog_write(MAKE_PIN_DIRECT(4), 1234));

	normal_ret = -1;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_PWM_WRITE_FAIL,
			      plc_xgpio_analog_write(MAKE_PIN_DIRECT(4), 1234));
}

void test_analog_write_direct_keeps_duty_cycles_above_16_bits(void)
{
	// A 20-bit PWM's full scale
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_analog_write(MAKE_PIN_DIRECT(4), 0xFFFFF));
	TEST_ASSERT_EQUAL_UINT32(0xFFFFF, normal_arg);
}

void test_analog_write_pca9685_passes_the_duty_cycle(void)
{
	pca9685_write_output_ExpectAndReturn(
		TEST_I2C, &pca9685[0].handle, 13, 2048, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_analog_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 13),
				       2048));

	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     13,
					     PCA9685_OUTPUT_ON,
					     TEST_TIMEOUT_MS,
					     0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_analog_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 13),
				       PCA9685_OUTPUT_ON));
}

void test_analog_write_pca9685_errors(void)
{
	pca9685_write_output_ExpectAndReturn(
		TEST_I2C, &pca9685[0].handle, 0, 2048, TEST_TIMEOUT_MS, -1);
	TEST_ASSERT_EQUAL_INT(ARRAY_PCA9685_PWM_WRITE_FAIL,
			      plc_xgpio_analog_write(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 2048));

	// Above PCA9685_OUTPUT_ON, without calling the driver
	TEST_ASSERT_EQUAL_INT(
		ARRAY_PCA9685_PWM_WRITE_FAIL,
		plc_xgpio_analog_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				       PCA9685_OUTPUT_ON + 1));
	// It would wrap to 100 in the driver's uint16_t
	TEST_ASSERT_EQUAL_INT(
		ARRAY_PCA9685_PWM_WRITE_FAIL,
		plc_xgpio_analog_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				       65536 + 100));
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_analog_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 16), 0));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_analog_write(MAKE_PIN_PCA9685(MISSING_ADDR, 0), 0));
}

void test_analog_write_unsupported_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_analog_write(MAKE_PIN_MCP23008(MCP23008_ADDR, 0), 0));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_analog_write(MAKE_PIN_MCP23017(MCP23017_ADDR, 0), 0));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_analog_write(MAKE_PIN_LTC2309(LTC2309_ADDR, 0), 0));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_analog_write(
			MAKE_PIN_ADS1015(ADS1015_ADDR, ADS101X_P0_GND), 0));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_analog_write(0x06000000, 0));
}

void test_analog_write_without_bus(void)
{
	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;

	TEST_ASSERT_EQUAL_INT(0, plc_xgpio_analog_write(MAKE_PIN_DIRECT(4), 1));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_analog_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 1));
}

// ---- plc_xgpio_analog_write_set_frequency ----

void test_set_frequency_direct_is_not_range_checked(void)
{
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_DIRECT(4), 100000));
	TEST_ASSERT_EQUAL_UINT32(4, normal_pin);
	TEST_ASSERT_EQUAL_UINT32(100000, normal_arg);

	normal_ret = 1;
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_analog_write_set_frequency(MAKE_PIN_DIRECT(4), 1));

	normal_ret = -1;
	TEST_ASSERT_EQUAL_INT(
		NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL,
		plc_xgpio_analog_write_set_frequency(MAKE_PIN_DIRECT(4), 1));
}

void test_set_frequency_pca9685_range_ends(void)
{
	pca9685_set_frequency_ExpectAndReturn(TEST_I2C,
					      &pca9685[0].handle,
					      PCA9685_MIN_FREQ_HZ,
					      TEST_TIMEOUT_MS,
					      0);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				      PCA9685_MIN_FREQ_HZ));

	pca9685_set_frequency_ExpectAndReturn(TEST_I2C,
					      &pca9685[0].handle,
					      PCA9685_MAX_FREQ_HZ,
					      TEST_TIMEOUT_MS,
					      0);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				      PCA9685_MAX_FREQ_HZ));
}

void test_set_frequency_pca9685_already_set_is_a_success(void)
{
	pca9685_set_frequency_ExpectAndReturn(
		TEST_I2C, &pca9685[0].handle, 500, TEST_TIMEOUT_MS, 1);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 500));
}

void test_set_frequency_pca9685_errors(void)
{
	pca9685_set_frequency_ExpectAndReturn(
		TEST_I2C, &pca9685[0].handle, 500, TEST_TIMEOUT_MS, -1);
	TEST_ASSERT_EQUAL_INT(ARRAY_PCA9685_PWM_CHANGE_FREQ_FAIL,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 500));
	TEST_ASSERT_EQUAL_INT(FREQ_OUT_OF_RANGE,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				      PCA9685_MIN_FREQ_HZ - 1));
	TEST_ASSERT_EQUAL_INT(FREQ_OUT_OF_RANGE,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				      PCA9685_MAX_FREQ_HZ + 1));
	// It would wrap to 500 in the driver's uint16_t
	TEST_ASSERT_EQUAL_INT(FREQ_OUT_OF_RANGE,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				      65536 + 500));
	TEST_ASSERT_EQUAL_INT(INDEX_OUT_OF_RANGE,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 16), 500));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(MISSING_ADDR, 0), 500));
}

void test_set_frequency_unsupported_pins(void)
{
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_MCP23008(MCP23008_ADDR, 0),
				      500));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_analog_write_set_frequency(
			MAKE_PIN_ADS1015(ADS1015_ADDR, ADS101X_P0_GND), 500));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_analog_write_set_frequency(0x06000000,
								   500));
}

void test_set_frequency_without_bus(void)
{
	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;

	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_analog_write_set_frequency(MAKE_PIN_DIRECT(4), 500));
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_I2C_BUS,
			      plc_xgpio_analog_write_set_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 500));
}

// ---- plc_xgpio_analog_read ----

void test_analog_read_direct(void)
{
	normal_analog = 3210;
	TEST_ASSERT_EQUAL_UINT32(3210,
				 plc_xgpio_analog_read(MAKE_PIN_DIRECT(36)));
	TEST_ASSERT_EQUAL_UINT32(36, normal_pin);

	normal_ret = -1;
	TEST_ASSERT_EQUAL_UINT32(0, plc_xgpio_analog_read(MAKE_PIN_DIRECT(36)));
}

void test_analog_read_direct_keeps_readings_above_16_bits(void)
{
	// A 24-bit ADC's full scale
	normal_analog = 0xFFFFFF;
	TEST_ASSERT_EQUAL_UINT32(0xFFFFFF,
				 plc_xgpio_analog_read(MAKE_PIN_DIRECT(36)));
}

void test_analog_read_ltc2309_maps_every_index_to_its_channel(void)
{
	static const LTC2309_INPUT CHANNELS[LTC2309_NUM_INPUTS] = {
		LTC2309_CH0, LTC2309_CH1, LTC2309_CH2, LTC2309_CH3,
		LTC2309_CH4, LTC2309_CH5, LTC2309_CH6, LTC2309_CH7,
	};
	uint16_t value;
	uint8_t i;

	for (i = 0; i < LTC2309_NUM_INPUTS; i++) {
		value = (uint16_t)(100 + i);
		expect_ltc2309_read(CHANNELS[i], &value, 0);
		TEST_ASSERT_EQUAL_UINT32(100 + i,
					 plc_xgpio_analog_read(MAKE_PIN_LTC2309(
						 LTC2309_ADDR, i)));
	}
}

void test_analog_read_ltc2309_reads_every_differential_pair(void)
{
	uint16_t value;
	uint8_t i;

	for (i = 0; i < LTC2309_NUM_INPUTS; i++) {
		value = (uint16_t)(300 + i);
		expect_ltc2309_diff_read((LTC2309_DIFF_INPUT)i, &value, 0);
		TEST_ASSERT_EQUAL_UINT32(
			300 + i,
			plc_xgpio_analog_read(
				MAKE_PIN_LTC2309_DIFF(LTC2309_ADDR, i)));
	}
}

void test_analog_read_ltc2309_differential_failures_read_0(void)
{
	uint16_t value = 1234;

	expect_ltc2309_diff_read(LTC2309_P0_N1, &value, -1);
	TEST_ASSERT_EQUAL_UINT32(0,
				 plc_xgpio_analog_read(MAKE_PIN_LTC2309_DIFF(
					 LTC2309_ADDR, LTC2309_P0_N1)));

	/*
	 * No driver call for a pair out of range, a missing chip, or an
	 * unknown read byte
	 */
	TEST_ASSERT_EQUAL_UINT32(
		0,
		plc_xgpio_analog_read(MAKE_PIN_LTC2309_DIFF(LTC2309_ADDR, 8)));
	TEST_ASSERT_EQUAL_UINT32(0,
				 plc_xgpio_analog_read(MAKE_PIN_LTC2309_DIFF(
					 MISSING_ADDR, LTC2309_P0_N1)));
	TEST_ASSERT_EQUAL_UINT32(0,
				 plc_xgpio_analog_read(_MAKE_PIN_PLC(
					 PLC_LTC2309, LTC2309_ADDR, 0x02, 0)));
	TEST_ASSERT_EQUAL_INT(PLC_XGPIO_LOW,
			      plc_xgpio_digital_read(_MAKE_PIN_PLC(
				      PLC_LTC2309, LTC2309_ADDR, 0x02, 0)));
}

void test_analog_read_ads1015_reads_each_input_against_gnd(void)
{
	static const ADS101X_INPUT INPUTS[ADS1015_NUM_INPUTS] = {
		ADS101X_P0_GND,
		ADS101X_P1_GND,
		ADS101X_P2_GND,
		ADS101X_P3_GND,
	};
	uint16_t value;
	uint8_t i;

	for (i = 0; i < ADS1015_NUM_INPUTS; i++) {
		value = (uint16_t)(2000 + i);
		expect_ads1015_read(INPUTS[i], &value, 0);
		TEST_ASSERT_EQUAL_UINT32(2000 + i,
					 plc_xgpio_analog_read(MAKE_PIN_ADS1015(
						 ADS1015_ADDR, INPUTS[i])));
	}
}

void test_analog_read_failures_read_0(void)
{
	uint16_t value = 1234;

	expect_ltc2309_read(LTC2309_CH0, &value, -1);
	TEST_ASSERT_EQUAL_UINT32(
		0, plc_xgpio_analog_read(MAKE_PIN_LTC2309(LTC2309_ADDR, 0)));

	expect_ads1015_read(ADS101X_P0_GND, &value, -1);
	TEST_ASSERT_EQUAL_UINT32(0,
				 plc_xgpio_analog_read(MAKE_PIN_ADS1015(
					 ADS1015_ADDR, ADS101X_P0_GND)));

	TEST_ASSERT_EQUAL_UINT32(
		0, plc_xgpio_analog_read(MAKE_PIN_LTC2309(LTC2309_ADDR, 8)));
	TEST_ASSERT_EQUAL_UINT32(
		0, plc_xgpio_analog_read(MAKE_PIN_ADS1015(ADS1015_ADDR, 8)));
	TEST_ASSERT_EQUAL_UINT32(
		0, plc_xgpio_analog_read(MAKE_PIN_LTC2309(MISSING_ADDR, 0)));
	TEST_ASSERT_EQUAL_UINT32(
		0, plc_xgpio_analog_read(MAKE_PIN_PCA9685(PCA9685_ADDR, 0)));
	TEST_ASSERT_EQUAL_UINT32(
		0, plc_xgpio_analog_read(MAKE_PIN_MCP23008(MCP23008_ADDR, 0)));
	TEST_ASSERT_EQUAL_UINT32(0, plc_xgpio_analog_read(0x06000000));
}

void test_analog_read_without_bus_returns_32(void)
{
	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;
	normal_analog = 77;

	TEST_ASSERT_EQUAL_UINT32(77,
				 plc_xgpio_analog_read(MAKE_PIN_DIRECT(36)));
	TEST_ASSERT_EQUAL_UINT32(-I2C_PIN_WITHOUT_I2C_BUS,
				 plc_xgpio_analog_read(MAKE_PIN_ADS1015(
					 ADS1015_ADDR, ADS101X_P0_GND)));
	TEST_ASSERT_EQUAL_UINT32(
		32, plc_xgpio_analog_read(MAKE_PIN_LTC2309(LTC2309_ADDR, 0)));
}

// ---- plc_xgpio_digital_write_all ----

void test_digital_write_all_mcp23008_keeps_the_low_byte(void)
{
	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp23008[0].handle, 0x00A5, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write_all(MCP23008_ADDR, 0xFFFF12A5));

	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp23008[0].handle, 0x0000, TEST_TIMEOUT_MS, -1);
	TEST_ASSERT_EQUAL_INT(ARRAY_MCP23008_WRITE_ALL_FAIL,
			      plc_xgpio_digital_write_all(MCP23008_ADDR, 0));
}

void test_digital_write_all_mcp23017_writes_both_ports(void)
{
	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp23017[0].handle, 0xBEEF, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write_all(MCP23017_ADDR, 0x1234BEEF));

	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp23017[0].handle, 0x0000, TEST_TIMEOUT_MS, -1);
	TEST_ASSERT_EQUAL_INT(ARRAY_MCP23017_WRITE_ALL_FAIL,
			      plc_xgpio_digital_write_all(MCP23017_ADDR, 0));
}

void test_digital_write_all_pca9685_bit_i_is_output_i(void)
{
	uint8_t i;

	pca9685_write_all_Stub(pca9685_write_all_stub);
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write_all(PCA9685_ADDR, 0xFFFF8421));
	for (i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		TEST_ASSERT_EQUAL_UINT16((0x8421 >> i) & 1 ? PCA9685_OUTPUT_ON :
							     PCA9685_OUTPUT_OFF,
					 pca_all_values[i]);
	}

	pca_all_ret = -1;
	TEST_ASSERT_EQUAL_INT(ARRAY_PCA9685_WRITE_ALL_FAIL,
			      plc_xgpio_digital_write_all(PCA9685_ADDR, 0));
}

void test_digital_write_all_searches_mcp23008_first(void)
{
	// The same address in all three arrays
	mcp23017[0].addr = MCP23008_ADDR;
	pca9685[0].addr = MCP23008_ADDR;

	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp23008[0].handle, 0x0001, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(0, plc_xgpio_digital_write_all(MCP23008_ADDR, 1));
}

void test_digital_write_all_searches_pca9685_before_mcp23017(void)
{
	mcp23017[0].addr = PCA9685_ADDR;

	pca9685_write_all_Stub(pca9685_write_all_stub);
	TEST_ASSERT_EQUAL_INT(0, plc_xgpio_digital_write_all(PCA9685_ADDR, 1));
	TEST_ASSERT_EQUAL_UINT16(PCA9685_OUTPUT_ON, pca_all_values[0]);
}

void test_digital_write_all_errors(void)
{
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_digital_write_all(LTC2309_ADDR, 0));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_digital_write_all(MISSING_ADDR, 0));

	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_I2C_BUS,
			      plc_xgpio_digital_write_all(MCP23008_ADDR, 0));
}

// ---- plc_xgpio_digital_read_all ----

void test_digital_read_all_mcp23008_stores_one_byte(void)
{
	uint16_t levels = 0x12C3;
	uint8_t out[2] = { 0xEE, 0xEE };

	mcp230xx_read_all_ExpectAndReturn(
		TEST_I2C, &mcp23008[0].handle, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_read_all_IgnoreArg_return_value();
	mcp230xx_read_all_ReturnThruPtr_return_value(&levels);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_digital_read_all(MCP23008_ADDR, out));
	TEST_ASSERT_EQUAL_HEX8(0xC3, out[0]);
	TEST_ASSERT_EQUAL_HEX8(0xEE, out[1]);
}

void test_digital_read_all_mcp23017_stores_both_ports(void)
{
	uint16_t levels = 0xBEEF;
	uint16_t out = 0;

	mcp230xx_read_all_ExpectAndReturn(
		TEST_I2C, &mcp23017[0].handle, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_read_all_IgnoreArg_return_value();
	mcp230xx_read_all_ReturnThruPtr_return_value(&levels);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_digital_read_all(MCP23017_ADDR, &out));
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, out);
}

void test_digital_read_all_failures(void)
{
	uint16_t out = 0x5A5A;

	mcp230xx_read_all_ExpectAndReturn(
		TEST_I2C, &mcp23008[0].handle, NULL, TEST_TIMEOUT_MS, -1);
	mcp230xx_read_all_IgnoreArg_return_value();
	TEST_ASSERT_EQUAL_INT(ARRAY_MCP23008_READ_ALL_FAIL,
			      plc_xgpio_digital_read_all(MCP23008_ADDR, &out));

	mcp230xx_read_all_ExpectAndReturn(
		TEST_I2C, &mcp23017[0].handle, NULL, TEST_TIMEOUT_MS, -1);
	mcp230xx_read_all_IgnoreArg_return_value();
	TEST_ASSERT_EQUAL_INT(ARRAY_MCP23017_READ_ALL_FAIL,
			      plc_xgpio_digital_read_all(MCP23017_ADDR, &out));

	// Nothing is stored on failure
	TEST_ASSERT_EQUAL_HEX16(0x5A5A, out);
}

void test_digital_read_all_errors(void)
{
	uint16_t out;

	TEST_ASSERT_EQUAL_INT(NULL_ARGUMENT,
			      plc_xgpio_digital_read_all(MCP23008_ADDR, NULL));
	TEST_ASSERT_EQUAL_INT(NULL_ARGUMENT,
			      plc_xgpio_digital_read_all(MCP23017_ADDR, NULL));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_digital_read_all(PCA9685_ADDR, &out));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_digital_read_all(MISSING_ADDR, &out));

	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_I2C_BUS,
			      plc_xgpio_digital_read_all(MCP23008_ADDR, &out));
}

// ---- plc_xgpio_analog_write_all ----

void test_analog_write_all_pca9685_passes_every_duty_cycle(void)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS];
	uint8_t i;

	for (i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		duty[i] = (uint16_t)(i * 256);
	}

	pca9685_write_all_Stub(pca9685_write_all_stub);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_analog_write_all(PCA9685_ADDR, duty));
	TEST_ASSERT_EQUAL_UINT16_ARRAY(
		duty, pca_all_values, PCA9685_NUM_OUTPUTS);

	pca_all_ret = -1;
	TEST_ASSERT_EQUAL_INT(ARRAY_PCA9685_PWM_WRITE_ALL_FAIL,
			      plc_xgpio_analog_write_all(PCA9685_ADDR, duty));
}

void test_analog_write_all_errors(void)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS] = { 0 };

	TEST_ASSERT_EQUAL_INT(NULL_ARGUMENT,
			      plc_xgpio_analog_write_all(PCA9685_ADDR, NULL));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_analog_write_all(MCP23008_ADDR, duty));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_analog_write_all(MISSING_ADDR, duty));

	plc_xgpio_peripherals.i2c = PLC_XGPIO_NO_I2C_BUS;
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_I2C_BUS,
			      plc_xgpio_analog_write_all(PCA9685_ADDR, duty));
}

// ---- Empty arrays ----

void test_empty_arrays_find_nothing(void)
{
	memset(&plc_xgpio_peripherals, 0, sizeof(plc_xgpio_peripherals));
	plc_xgpio_peripherals.i2c = TEST_I2C;

	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_pin_mode(MAKE_PIN_MCP23008(MCP23008_ADDR, 0),
				   PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_UINT32(
		0, plc_xgpio_analog_read(MAKE_PIN_LTC2309(LTC2309_ADDR, 0)));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_digital_write_all(MCP23008_ADDR, 0));
}
