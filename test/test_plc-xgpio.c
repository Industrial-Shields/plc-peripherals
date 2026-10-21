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
 * plc_normal_gpio_* functions it expects from the library using it are faked
 * by fake-normal-gpio, recording what they get and returning what each test
 * sets.
 */

#include "unity.h"

#include "mock_peripheral-ads101x.h"
#include "mock_peripheral-ltc2309.h"
#include "mock_peripheral-mcp230xx.h"
#include "mock_peripheral-pca9685.h"
#include "fake-normal-gpio.h"
#include "plc-xgpio.h"

#include <errno.h>
#include <string.h>

// Only compared by address, never dereferenced
#define TEST_I2C ((const i2c_interface_t*)(!NULL))
#define TEST_TIMEOUT_MS ((uint32_t)42)

// The first pin type after the last one xgpio knows
#define UNKNOWN_TYPE_PIN ((plc_xgpio_pin_t)(PLC_ADS101X + 1) << 24)

// clang-format off
#define MCP23008_ADDR 0x20
#define MCP23017_ADDR 0x24
#define PCA9685_ADDR  0x40
#define LTC2309_ADDR  0x08
#define ADS101X_ADDR  0x48
#define MISSING_ADDR  0x7F
// clang-format on

// mcp230xx[0] is the MCP23008, mcp230xx[1] the MCP23017
static plc_xgpio_mcp230xx_t mcp230xx[2];
static plc_xgpio_pca9685_t pca9685[1];
static plc_xgpio_ltc2309_t ltc2309[1];
static plc_xgpio_ads101x_t ads101x[1];
static plc_xgpio_peripherals_t peripherals;

const plc_xgpio_peripherals_t* const plc_xgpio_peripherals = &peripherals;
const i2c_interface_t* plc_xgpio_i2c = TEST_I2C;

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

// The type mcp230xx_get_type gives for each entry of mcp230xx
static MCP230XX_TYPE mcp230xx_types[] = { MCP230XX_008, MCP230XX_017 };

static void expect_mcp230xx_get_type(size_t i)
{
	mcp230xx_get_type_ExpectAndReturn(
		TEST_I2C, &mcp230xx[i].handle, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_get_type_IgnoreArg_type();
	mcp230xx_get_type_ReturnThruPtr_type(&mcp230xx_types[i]);
}

static ADS101X_TYPE ads101x_type = ADS101X_015;

static void expect_ads101x_get_type(void)
{
	ads101x_get_type_ExpectAndReturn(
		TEST_I2C, &ads101x[0].handle, NULL, TEST_TIMEOUT_MS, 0);
	ads101x_get_type_IgnoreArg_type();
	ads101x_get_type_ReturnThruPtr_type(&ads101x_type);
}

void setUp(void)
{
	memset(&peripherals, 0, sizeof(peripherals));
	mcp230xx[0].addr = MCP23008_ADDR;
	mcp230xx[0].timeout_ms = TEST_TIMEOUT_MS;
	mcp230xx[1].addr = MCP23017_ADDR;
	mcp230xx[1].timeout_ms = TEST_TIMEOUT_MS;
	pca9685[0].addr = PCA9685_ADDR;
	pca9685[0].timeout_ms = TEST_TIMEOUT_MS;
	ltc2309[0].addr = LTC2309_ADDR;
	ltc2309[0].timeout_ms = TEST_TIMEOUT_MS;
	ads101x[0].addr = ADS101X_ADDR;
	ads101x[0].timeout_ms = TEST_TIMEOUT_MS;

	peripherals.mcp230xx = mcp230xx;
	peripherals.num_mcp230xx = 2;
	peripherals.pca9685 = pca9685;
	peripherals.num_pca9685 = 1;
	peripherals.ltc2309 = ltc2309;
	peripherals.num_ltc2309 = 1;
	peripherals.ads101x = ads101x;
	peripherals.num_ads101x = 1;

	fake_normal_gpio_reset();
	memset(pca_all_values, 0xA5, sizeof(pca_all_values));
	pca_all_ret = 0;
}

void tearDown(void)
{
}

// ---- Pin encoding ----

void test_make_pin_encodes_type_addr_and_index(void)
{
	TEST_ASSERT_EQUAL_HEX32(0x02210006, MAKE_PIN_MCP230XX(0x21, 6));
	TEST_ASSERT_EQUAL_HEX32(0x0220000F, MAKE_PIN_MCP230XX(0x20, 0x0F));
	TEST_ASSERT_EQUAL_HEX32(0x0140000B, MAKE_PIN_PCA9685(0x40, 11));
	TEST_ASSERT_EQUAL_HEX32(0x03080007,
				MAKE_PIN_LTC2309(0x08, LTC2309_CH7));
	TEST_ASSERT_EQUAL_HEX32(0x04480007,
				MAKE_PIN_ADS101X(0x48, ADS101X_P3_GND));
}

void test_make_pin_direct_keeps_a_24_bit_index(void)
{
	TEST_ASSERT_EQUAL_HEX32(0x00000005, MAKE_PIN_DIRECT(5));
	TEST_ASSERT_EQUAL_HEX32(0x00123456, MAKE_PIN_DIRECT(0x123456));
	TEST_ASSERT_EQUAL_HEX32(0x00ABCDEF, MAKE_PIN_DIRECT(0xFFABCDEF));
}

// ---- plc_xgpio_set_pin_mode ----

void test_set_pin_mode_rejects_an_invalid_mode(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
				       PLC_XGPIO_INPUT_PULLUP_INVERTED + 1));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0), 7));
	TEST_ASSERT_EQUAL_UINT32(0, fake_normal_gpio.calls);
}

void test_set_pin_mode_direct_passes_the_pin_and_mode(void)
{
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(0x123456),
						     PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_UINT32(0x123456, fake_normal_gpio.pin);
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_INPUT, fake_normal_gpio.arg);

	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(0x123456),
						     PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_OUTPUT, fake_normal_gpio.arg);
}

void test_set_pin_mode_direct_passes_the_input_modes(void)
{
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
						     PLC_XGPIO_INPUT_PULLUP));
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_INPUT_PULLUP, fake_normal_gpio.arg);

	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
						     PLC_XGPIO_INPUT_INVERTED));
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_INPUT_INVERTED,
				 fake_normal_gpio.arg);

	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
				       PLC_XGPIO_INPUT_PULLUP_INVERTED));
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_INPUT_PULLUP_INVERTED,
				 fake_normal_gpio.arg);
}

void test_set_pin_mode_direct_enotsup_is_not_supported(void)
{
	fake_normal_gpio.ret = -1;
	errno = ENOTSUP;
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
						     PLC_XGPIO_INPUT_INVERTED));
}

void test_set_pin_mode_direct_passes_1_through(void)
{
	fake_normal_gpio.ret = 1;
	TEST_ASSERT_EQUAL_INT(1,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
						     PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_direct_failure(void)
{
	fake_normal_gpio.ret = -1;
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_SET_PIN_MODE_FAIL,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
						     PLC_XGPIO_OUTPUT));

	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(INDEX_OUT_OF_RANGE,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
						     PLC_XGPIO_OUTPUT));

	errno = ENOTSUP;
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5),
						     PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_mcp23008_output(void)
{
	mcp230xx_set_output_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, 3, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 3),
				       PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_mcp23008_input_has_no_pullup(void)
{
	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp230xx[0].handle,
					   7,
					   MCP230XX_NO_PULLUP,
					   TEST_TIMEOUT_MS,
					   0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 7),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_mcp23008_input_with_pullup(void)
{
	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp230xx[0].handle,
					   6,
					   MCP230XX_PULLUP,
					   TEST_TIMEOUT_MS,
					   0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 6),
				       PLC_XGPIO_INPUT_PULLUP));
}

void test_set_pin_mode_mcp23017_input_inverted(void)
{
	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp230xx[1].handle,
					   12,
					   MCP230XX_NO_PULLUP_INVERTED,
					   TEST_TIMEOUT_MS,
					   0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23017_ADDR, 12),
				       PLC_XGPIO_INPUT_INVERTED));
}

void test_set_pin_mode_mcp23017_input_with_pullup_inverted(void)
{
	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp230xx[1].handle,
					   9,
					   MCP230XX_PULLUP_INVERTED,
					   TEST_TIMEOUT_MS,
					   0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23017_ADDR, 9),
				       PLC_XGPIO_INPUT_PULLUP_INVERTED));
}

void test_set_pin_mode_mcp230xx_passes_1_through(void)
{
	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp230xx[1].handle,
					   15,
					   MCP230XX_NO_PULLUP,
					   TEST_TIMEOUT_MS,
					   1);
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23017_ADDR, 15),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_mcp230xx_failures_use_the_chip_code(void)
{
	mcp230xx_set_output_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, 0, TEST_TIMEOUT_MS, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_MCP230XX_SET_PIN_MODE_FAIL,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0),
				       PLC_XGPIO_OUTPUT));

	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp230xx[1].handle,
					   8,
					   MCP230XX_NO_PULLUP,
					   TEST_TIMEOUT_MS,
					   -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_MCP230XX_SET_PIN_MODE_FAIL,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23017_ADDR, 8),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_mcp230xx_index_out_of_range(void)
{
	mcp230xx_set_output_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[0].handle,
					    MCP23008_NUM_GPIOS,
					    TEST_TIMEOUT_MS,
					    -1);
	expect_mcp230xx_get_type(0);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR,
							 MCP23008_NUM_GPIOS),
				       PLC_XGPIO_OUTPUT));

	mcp230xx_set_input_ExpectAndReturn(TEST_I2C,
					   &mcp230xx[1].handle,
					   MCP23017_NUM_GPIOS,
					   MCP230XX_NO_PULLUP,
					   TEST_TIMEOUT_MS,
					   -1);
	expect_mcp230xx_get_type(1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23017_ADDR,
							 MCP23017_NUM_GPIOS),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_mcp230xx_einval_in_range_is_a_chip_failure(void)
{
	// The index is valid, so the driver's EINVAL is about something else
	mcp230xx_set_output_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, 7, TEST_TIMEOUT_MS, -1);
	expect_mcp230xx_get_type(0);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_MCP230XX_SET_PIN_MODE_FAIL,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 7),
				       PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	// Out of an MCP23008's range, but valid on an MCP23017
	mcp230xx_set_output_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[1].handle,
					    MCP23008_NUM_GPIOS,
					    TEST_TIMEOUT_MS,
					    -1);
	expect_mcp230xx_get_type(1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_MCP230XX_SET_PIN_MODE_FAIL,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23017_ADDR,
							 MCP23008_NUM_GPIOS),
				       PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_mcp230xx_get_type_failing_is_a_chip_failure(void)
{
	// Out of an MCP23008's range, but the type to check it against is unknown
	mcp230xx_set_output_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[0].handle,
					    MCP23008_NUM_GPIOS,
					    TEST_TIMEOUT_MS,
					    -1);
	mcp230xx_get_type_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, NULL, TEST_TIMEOUT_MS, -1);
	mcp230xx_get_type_IgnoreArg_type();
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_MCP230XX_SET_PIN_MODE_FAIL,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR,
							 MCP23008_NUM_GPIOS),
				       PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_mcp230xx_unknown_type_is_unsupported(void)
{
	// A type added to the driver after xgpio was built
	MCP230XX_TYPE unknown = (MCP230XX_TYPE)(MCP230XX_017 + 1);

	mcp230xx_set_output_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, 20, TEST_TIMEOUT_MS, -1);
	mcp230xx_get_type_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_get_type_IgnoreArg_type();
	mcp230xx_get_type_ReturnThruPtr_type(&unknown);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		UNSUPPORTED_PERIPHERAL_TYPE,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23017_ADDR, 20),
				       PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_mcp230xx_efault_is_a_pin_without_bus(void)
{
	mcp230xx_set_output_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, 0, TEST_TIMEOUT_MS, -1);
	errno = EFAULT;
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0),
				       PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_mcp230xx_not_in_the_array(void)
{
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MISSING_ADDR, 0),
				       PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_fixed_direction_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_set_pin_mode(MAKE_PIN_PCA9685(PCA9685_ADDR, 15),
				       PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR,
							LTC2309_CH7),
				       PLC_XGPIO_INPUT));
	expect_ads101x_get_type();
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(ADS101X_ADDR,
							ADS101X_P3_GND),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_fixed_direction_pins_reject_the_other_mode(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_set_pin_mode(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				       PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR,
							LTC2309_CH0),
				       PLC_XGPIO_OUTPUT));
	expect_ads101x_get_type();
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(ADS101X_ADDR,
							ADS101X_P0_GND),
				       PLC_XGPIO_OUTPUT));
}

void test_set_pin_mode_fixed_direction_pins_reject_the_input_modes(void)
{
	const uint8_t modes[] = {
		PLC_XGPIO_INPUT_PULLUP,
		PLC_XGPIO_INPUT_INVERTED,
		PLC_XGPIO_INPUT_PULLUP_INVERTED,
	};

	for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
		TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
				      plc_xgpio_set_pin_mode(
					      MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					      modes[i]));
		TEST_ASSERT_EQUAL_INT(
			PIN_NOT_SUPPORTED,
			plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR,
								LTC2309_CH0),
					       modes[i]));
		expect_ads101x_get_type();
		TEST_ASSERT_EQUAL_INT(
			PIN_NOT_SUPPORTED,
			plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(ADS101X_ADDR,
								ADS101X_P0_GND),
					       modes[i]));
	}
}

void test_set_pin_mode_ads101x_unknown_type_is_unsupported(void)
{
	// A type added to the driver after xgpio was built
	ADS101X_TYPE unknown = (ADS101X_TYPE)(ADS101X_015 + 1);

	ads101x_get_type_ExpectAndReturn(
		TEST_I2C, &ads101x[0].handle, NULL, TEST_TIMEOUT_MS, 0);
	ads101x_get_type_IgnoreArg_type();
	ads101x_get_type_ReturnThruPtr_type(&unknown);
	TEST_ASSERT_EQUAL_INT(
		UNSUPPORTED_PERIPHERAL_TYPE,
		plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(ADS101X_ADDR,
							ADS101X_P0_GND),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_ltc2309_differential_is_an_input(void)
{
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309_DIFF(LTC2309_ADDR,
							     LTC2309_P2_N3),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_ltc2309_unknown_mode(void)
{
	// The 1st byte isn't single-ended or differential
	TEST_ASSERT_EQUAL_INT(
		UNKNOWN_MODE,
		plc_xgpio_set_pin_mode(
			_MAKE_PIN_PLC(PLC_LTC2309,
				      LTC2309_ADDR,
				      PLC_XGPIO_LTC2309_DIFFERENTIAL + 1,
				      LTC2309_CH0),
			PLC_XGPIO_INPUT));
}

void test_set_pin_mode_fixed_direction_pins_index_out_of_range(void)
{
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_set_pin_mode(MAKE_PIN_PCA9685(PCA9685_ADDR,
							PCA9685_NUM_OUTPUTS),
				       PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR,
							LTC2309_NUM_INPUTS),
				       PLC_XGPIO_INPUT));
	expect_ads101x_get_type();
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(ADS101X_ADDR,
							ADS1015_NUM_INPUTS),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_fixed_direction_pins_not_in_the_arrays(void)
{
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_PCA9685(MISSING_ADDR, 0),
				       PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309(MISSING_ADDR,
							LTC2309_CH0),
				       PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(MISSING_ADDR,
							ADS101X_P0_GND),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_empty_arrays_find_nothing(void)
{
	memset(&peripherals, 0, sizeof(peripherals));

	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				       PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0),
				       PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR,
							LTC2309_CH0),
				       PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(ADS101X_ADDR,
							ADS101X_P0_GND),
				       PLC_XGPIO_INPUT));
}

void test_set_pin_mode_unknown_pin_type(void)
{
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_set_pin_mode(UNKNOWN_TYPE_PIN,
						     PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_set_pin_mode(0xFF000000,
						     PLC_XGPIO_INPUT));
}

// ---- plc_xgpio_digital_write ----

void test_digital_write_direct(void)
{
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_digital_write(MAKE_PIN_DIRECT(0x123456),
						      PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_UINT32(0x123456, fake_normal_gpio.pin);
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_HIGH, fake_normal_gpio.arg);

	fake_normal_gpio.ret = 1;
	TEST_ASSERT_EQUAL_INT(1,
			      plc_xgpio_digital_write(MAKE_PIN_DIRECT(0x123456),
						      PLC_XGPIO_LOW));
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_LOW, fake_normal_gpio.arg);

	fake_normal_gpio.ret = -1;
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_DIGITAL_WRITE_FAIL,
			      plc_xgpio_digital_write(MAKE_PIN_DIRECT(0x123456),
						      PLC_XGPIO_LOW));

	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(INDEX_OUT_OF_RANGE,
			      plc_xgpio_digital_write(MAKE_PIN_DIRECT(0x123456),
						      PLC_XGPIO_LOW));

	errno = ENOTSUP;
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_digital_write(MAKE_PIN_DIRECT(0x123456),
						      PLC_XGPIO_LOW));
}

void test_digital_write_direct_passes_high_for_any_nonzero_value(void)
{
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_digital_write(MAKE_PIN_DIRECT(9), 5));
	TEST_ASSERT_EQUAL_UINT32(PLC_XGPIO_HIGH, fake_normal_gpio.arg);
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
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_PCA9685_DIGITAL_WRITE_FAIL,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					PLC_XGPIO_HIGH));

	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     PCA9685_NUM_OUTPUTS,
					     PCA9685_OUTPUT_ON,
					     TEST_TIMEOUT_MS,
					     -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR,
							 PCA9685_NUM_OUTPUTS),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(MISSING_ADDR, 0),
					PLC_XGPIO_HIGH));
}

void test_digital_write_mcp230xx(void)
{
	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[0].handle,
					    4,
					    MCP230XX_HIGH,
					    TEST_TIMEOUT_MS,
					    0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_write(MAKE_PIN_MCP230XX(MCP23008_ADDR, 4),
					PLC_XGPIO_HIGH));

	// Already set
	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[1].handle,
					    12,
					    MCP230XX_LOW,
					    TEST_TIMEOUT_MS,
					    1);
	TEST_ASSERT_EQUAL_INT(
		1,
		plc_xgpio_digital_write(MAKE_PIN_MCP230XX(MCP23017_ADDR, 12),
					PLC_XGPIO_LOW));
}

void test_digital_write_mcp230xx_errors(void)
{
	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[0].handle,
					    0,
					    MCP230XX_HIGH,
					    TEST_TIMEOUT_MS,
					    -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_MCP230XX_DIGITAL_WRITE_FAIL,
		plc_xgpio_digital_write(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0),
					PLC_XGPIO_HIGH));

	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[1].handle,
					    0,
					    MCP230XX_HIGH,
					    TEST_TIMEOUT_MS,
					    -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_MCP230XX_DIGITAL_WRITE_FAIL,
		plc_xgpio_digital_write(MAKE_PIN_MCP230XX(MCP23017_ADDR, 0),
					PLC_XGPIO_HIGH));

	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[0].handle,
					    MCP23008_NUM_GPIOS,
					    MCP230XX_HIGH,
					    TEST_TIMEOUT_MS,
					    -1);
	expect_mcp230xx_get_type(0);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_write(MAKE_PIN_MCP230XX(MCP23008_ADDR,
							  MCP23008_NUM_GPIOS),
					PLC_XGPIO_HIGH));

	mcp230xx_write_gpio_ExpectAndReturn(TEST_I2C,
					    &mcp230xx[1].handle,
					    MCP23017_NUM_GPIOS,
					    MCP230XX_HIGH,
					    TEST_TIMEOUT_MS,
					    -1);
	expect_mcp230xx_get_type(1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_write(MAKE_PIN_MCP230XX(MCP23017_ADDR,
							  MCP23017_NUM_GPIOS),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_write(MAKE_PIN_MCP230XX(MISSING_ADDR, 0),
					PLC_XGPIO_HIGH));
}

void test_digital_write_unsupported_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_digital_write(MAKE_PIN_LTC2309(LTC2309_ADDR,
							 LTC2309_CH0),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_digital_write(MAKE_PIN_ADS101X(ADS101X_ADDR,
							 ADS101X_P0_GND),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_digital_write(UNKNOWN_TYPE_PIN,
						      PLC_XGPIO_HIGH));
}

// ---- plc_xgpio_digital_read ----

void test_digital_read_null_read(void)
{
	TEST_ASSERT_EQUAL_INT(NULL_ARGUMENT,
			      plc_xgpio_digital_read(MAKE_PIN_DIRECT(3), NULL));
	TEST_ASSERT_EQUAL_UINT32(0, fake_normal_gpio.calls);
}

void test_digital_read_direct(void)
{
	uint8_t read = UINT8_MAX;

	fake_normal_gpio.level = PLC_XGPIO_LOW;
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_read(MAKE_PIN_DIRECT(0x10203), &read));
	TEST_ASSERT_EQUAL_UINT32(0x10203, fake_normal_gpio.pin);
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_LOW, read);

	fake_normal_gpio.level = PLC_XGPIO_HIGH;
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_read(MAKE_PIN_DIRECT(0x10203), &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_HIGH, read);

	fake_normal_gpio.ret = -1;
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_DIGITAL_READ_FAIL,
			      plc_xgpio_digital_read(MAKE_PIN_DIRECT(3),
						     &read));

	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(INDEX_OUT_OF_RANGE,
			      plc_xgpio_digital_read(MAKE_PIN_DIRECT(3),
						     &read));

	errno = ENOTSUP;
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_digital_read(MAKE_PIN_DIRECT(3),
						     &read));
}

static void expect_pca9685_read(uint8_t index, uint16_t* value, int ret)
{
	pca9685_read_output_ExpectAndReturn(
		TEST_I2C, &pca9685[0].handle, index, NULL, TEST_TIMEOUT_MS, ret);
	pca9685_read_output_IgnoreArg_value();
	pca9685_read_output_ReturnThruPtr_value(value);
}

void test_digital_read_pca9685_high_above_the_threshold(void)
{
	uint16_t at = PLC_XGPIO_PCA9685_DEFAULT_HIGH_THRESHOLD;
	uint16_t above = PCA9685_OUTPUT_ON / 2;
	uint8_t read = UINT8_MAX;

	TEST_ASSERT_GREATER_THAN_UINT16(at, above);

	expect_pca9685_read(3, &at, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(MAKE_PIN_PCA9685(PCA9685_ADDR, 3),
				       &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_LOW, read);

	expect_pca9685_read(3, &above, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(MAKE_PIN_PCA9685(PCA9685_ADDR, 3),
				       &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_HIGH, read);
}

void test_digital_read_pca9685_errors(void)
{
	uint16_t value = PCA9685_OUTPUT_OFF;
	uint8_t read;

	expect_pca9685_read(0, &value, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_PCA9685_DIGITAL_READ_FAIL,
		plc_xgpio_digital_read(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				       &read));

	expect_pca9685_read(PCA9685_NUM_OUTPUTS, &value, -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_read(MAKE_PIN_PCA9685(PCA9685_ADDR,
							PCA9685_NUM_OUTPUTS),
				       &read));

	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_read(MAKE_PIN_PCA9685(MISSING_ADDR, 0),
				       &read));
}

void test_digital_read_mcp230xx(void)
{
	uint8_t read = UINT8_MAX;
	uint8_t high = MCP230XX_HIGH;
	uint8_t low = MCP230XX_LOW;

	mcp230xx_read_gpio_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, 9, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_read_gpio_IgnoreArg_return_value();
	mcp230xx_read_gpio_ReturnThruPtr_return_value(&low);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(MAKE_PIN_MCP230XX(MCP23017_ADDR, 9),
				       &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_LOW, read);

	mcp230xx_read_gpio_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, 9, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_read_gpio_IgnoreArg_return_value();
	mcp230xx_read_gpio_ReturnThruPtr_return_value(&high);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(MAKE_PIN_MCP230XX(MCP23017_ADDR, 9),
				       &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_HIGH, read);
}

void test_digital_read_mcp230xx_errors(void)
{
	uint8_t read;

	mcp230xx_read_gpio_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, 2, NULL, TEST_TIMEOUT_MS, -1);
	mcp230xx_read_gpio_IgnoreArg_return_value();
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_MCP230XX_DIGITAL_READ_FAIL,
		plc_xgpio_digital_read(MAKE_PIN_MCP230XX(MCP23008_ADDR, 2),
				       &read));

	mcp230xx_read_gpio_ExpectAndReturn(TEST_I2C,
					   &mcp230xx[0].handle,
					   MCP23008_NUM_GPIOS,
					   NULL,
					   TEST_TIMEOUT_MS,
					   -1);
	mcp230xx_read_gpio_IgnoreArg_return_value();
	expect_mcp230xx_get_type(0);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_read(MAKE_PIN_MCP230XX(MCP23008_ADDR,
							 MCP23008_NUM_GPIOS),
				       &read));

	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_read(MAKE_PIN_MCP230XX(MISSING_ADDR, 2),
				       &read));
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

// What ads101x_get_continuous_mode gives for the ADS101X in the array
static bool ads101x_continuous_mode = false;

static void expect_ads1015_read(ADS101X_INPUT input, uint16_t* value, int ret)
{
	ads101x_get_continuous_mode_ExpectAndReturn(
		TEST_I2C, &ads101x[0].handle, NULL, TEST_TIMEOUT_MS, 0);
	ads101x_get_continuous_mode_IgnoreArg_continuous_mode();
	ads101x_get_continuous_mode_ReturnThruPtr_continuous_mode(
		&ads101x_continuous_mode);
	ads101x_unsigned_single_read_ExpectAndReturn(
		TEST_I2C, &ads101x[0].handle, input, NULL, TEST_TIMEOUT_MS, ret);
	ads101x_unsigned_single_read_IgnoreArg_return_value();
	ads101x_unsigned_single_read_ReturnThruPtr_return_value(value);
}

void test_digital_read_ltc2309_high_above_the_threshold(void)
{
	uint16_t at = PLC_XGPIO_LTC2309_DEFAULT_HIGH_THRESHOLD;
	uint16_t above = PLC_XGPIO_LTC2309_DEFAULT_HIGH_THRESHOLD + 1;
	uint8_t read = UINT8_MAX;

	expect_ltc2309_read(LTC2309_CH5, &at, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(
			MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH5), &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_LOW, read);

	expect_ltc2309_read(LTC2309_CH5, &above, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(
			MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH5), &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_HIGH, read);
}

static void
expect_ltc2309_diff_read(LTC2309_DIFF_INPUT pair, uint16_t* value, int ret)
{
	ltc2309_unsigned_differential_read_ExpectAndReturn(
		TEST_I2C, &ltc2309[0].handle, pair, NULL, TEST_TIMEOUT_MS, ret);
	ltc2309_unsigned_differential_read_IgnoreArg_read_value();
	ltc2309_unsigned_differential_read_ReturnThruPtr_read_value(value);
}

void test_digital_read_ltc2309_differential_high_above_the_threshold(void)
{
	uint16_t at = PLC_XGPIO_LTC2309_DEFAULT_HIGH_THRESHOLD;
	uint16_t above = PLC_XGPIO_LTC2309_DEFAULT_HIGH_THRESHOLD + 1;
	uint8_t read = UINT8_MAX;

	expect_ltc2309_diff_read(LTC2309_P3_N2, &at, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(MAKE_PIN_LTC2309_DIFF(LTC2309_ADDR,
							     LTC2309_P3_N2),
				       &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_LOW, read);

	expect_ltc2309_diff_read(LTC2309_P3_N2, &above, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(MAKE_PIN_LTC2309_DIFF(LTC2309_ADDR,
							     LTC2309_P3_N2),
				       &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_HIGH, read);
}

void test_digital_read_ads101x_high_above_the_threshold(void)
{
	uint16_t at = PLC_XGPIO_ADS101X_DEFAULT_HIGH_THRESHOLD;
	uint16_t above = PLC_XGPIO_ADS101X_DEFAULT_HIGH_THRESHOLD + 1;
	uint8_t read = UINT8_MAX;

	expect_ads1015_read(ADS101X_P2_GND, &at, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P2_GND), &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_LOW, read);

	expect_ads1015_read(ADS101X_P2_GND, &above, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_digital_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P2_GND), &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_HIGH, read);
}

void test_digital_read_analog_errors(void)
{
	uint16_t value = 0;
	uint8_t read;

	expect_ltc2309_read(LTC2309_CH0, &value, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_LTC2309_DIGITAL_READ_FAIL,
		plc_xgpio_digital_read(
			MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH0), &read));

	expect_ads1015_read(ADS101X_P0_GND, &value, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_ADS101X_DIGITAL_READ_FAIL,
		plc_xgpio_digital_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P0_GND), &read));

	// EINVAL on a valid input is about something else, like the wrong bus
	expect_ltc2309_read(LTC2309_CH7, &value, -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_LTC2309_DIGITAL_READ_FAIL,
		plc_xgpio_digital_read(
			MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH7), &read));

	expect_ads1015_read(ADS101X_P3_GND, &value, -1);
	expect_ads101x_get_type();
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_ADS101X_DIGITAL_READ_FAIL,
		plc_xgpio_digital_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P3_GND), &read));

	expect_ltc2309_read((LTC2309_INPUT)LTC2309_NUM_INPUTS, &value, -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_read(MAKE_PIN_LTC2309(LTC2309_ADDR,
							LTC2309_NUM_INPUTS),
				       &read));

	expect_ads1015_read((ADS101X_INPUT)ADS1015_NUM_INPUTS, &value, -1);
	expect_ads101x_get_type();
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_digital_read(MAKE_PIN_ADS101X(ADS101X_ADDR,
							ADS1015_NUM_INPUTS),
				       &read));

	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_read(
			MAKE_PIN_ADS101X(MISSING_ADDR, ADS101X_P0_GND), &read));
}

void test_digital_read_unknown_pin_type(void)
{
	uint8_t read;

	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_digital_read(UNKNOWN_TYPE_PIN, &read));
}

// ---- plc_xgpio_pwm_write ----

void test_pwm_write_direct(void)
{
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_pwm_write(MAKE_PIN_DIRECT(0x123456), 1234));
	TEST_ASSERT_EQUAL_UINT32(0x123456, fake_normal_gpio.pin);
	TEST_ASSERT_EQUAL_UINT32(1234, fake_normal_gpio.arg);

	fake_normal_gpio.ret = 1;
	TEST_ASSERT_EQUAL_INT(
		1, plc_xgpio_pwm_write(MAKE_PIN_DIRECT(0x123456), 1234));

	fake_normal_gpio.ret = -1;
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_PWM_WRITE_FAIL,
			      plc_xgpio_pwm_write(MAKE_PIN_DIRECT(0x123456),
						  1234));

	errno = ERANGE;
	TEST_ASSERT_EQUAL_INT(DUTY_CYCLE_OUT_OF_RANGE,
			      plc_xgpio_pwm_write(MAKE_PIN_DIRECT(0x123456),
						  1234));

	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(INDEX_OUT_OF_RANGE,
			      plc_xgpio_pwm_write(MAKE_PIN_DIRECT(0x123456),
						  1234));

	errno = ENOTSUP;
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_pwm_write(MAKE_PIN_DIRECT(0x123456),
						  1234));
}

void test_pwm_write_direct_keeps_duty_cycles_above_16_bits(void)
{
	// A 20-bit PWM's full scale
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_pwm_write(MAKE_PIN_DIRECT(0x123456), 0xFFFFF));
	TEST_ASSERT_EQUAL_UINT32(0xFFFFF, fake_normal_gpio.arg);
}

void test_pwm_write_pca9685_passes_the_duty_cycle(void)
{
	pca9685_write_output_ExpectAndReturn(
		TEST_I2C, &pca9685[0].handle, 13, 2048, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 13), 2048));

	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     13,
					     PCA9685_OUTPUT_ON,
					     TEST_TIMEOUT_MS,
					     0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 13),
				    PCA9685_OUTPUT_ON));
}

void test_pwm_write_pca9685_errors(void)
{
	// It would wrap to 100 in the driver's uint16_t
	TEST_ASSERT_EQUAL_INT(
		DUTY_CYCLE_OUT_OF_RANGE,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				    UINT16_MAX + 1 + 100));

	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     0,
					     PCA9685_OUTPUT_ON + 1,
					     TEST_TIMEOUT_MS,
					     -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		DUTY_CYCLE_OUT_OF_RANGE,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				    PCA9685_OUTPUT_ON + 1));

	// The driver's EINVAL at the top of the range is about something else
	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     0,
					     PCA9685_OUTPUT_ON,
					     TEST_TIMEOUT_MS,
					     -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_PCA9685_PWM_WRITE_FAIL,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				    PCA9685_OUTPUT_ON));

	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     0,
					     PCA9685_OUTPUT_ON,
					     TEST_TIMEOUT_MS,
					     -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_PCA9685_PWM_WRITE_FAIL,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				    PCA9685_OUTPUT_ON));

	pca9685_write_output_ExpectAndReturn(TEST_I2C,
					     &pca9685[0].handle,
					     PCA9685_NUM_OUTPUTS,
					     PCA9685_OUTPUT_OFF,
					     TEST_TIMEOUT_MS,
					     -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(PCA9685_ADDR,
						     PCA9685_NUM_OUTPUTS),
				    PCA9685_OUTPUT_OFF));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(MISSING_ADDR, 0),
				    PCA9685_OUTPUT_OFF));
}

void test_pwm_write_unsupported_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pwm_write(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0), 0));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pwm_write(MAKE_PIN_MCP230XX(MCP23017_ADDR, 0), 0));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pwm_write(MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH0),
				    0));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_pwm_write(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P0_GND), 0));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_pwm_write(UNKNOWN_TYPE_PIN, 0));
}

// ---- plc_xgpio_set_pwm_frequency ----

void test_set_pwm_frequency_direct_is_not_range_checked(void)
{
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_DIRECT(0x123456), 100000));
	TEST_ASSERT_EQUAL_UINT32(0x123456, fake_normal_gpio.pin);
	TEST_ASSERT_EQUAL_UINT32(100000, fake_normal_gpio.arg);

	fake_normal_gpio.ret = 1;
	TEST_ASSERT_EQUAL_INT(
		1, plc_xgpio_set_pwm_frequency(MAKE_PIN_DIRECT(0x123456), 1));

	fake_normal_gpio.ret = -1;
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		NORMAL_GPIO_SET_PWM_FREQUENCY_FAIL,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_DIRECT(0x123456), 1));

	errno = ERANGE;
	TEST_ASSERT_EQUAL_INT(
		FREQ_OUT_OF_RANGE,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_DIRECT(0x123456), 1));

	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_DIRECT(0x123456), 1));

	errno = ENOTSUP;
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_DIRECT(0x123456), 1));
}

void test_set_pwm_frequency_pca9685_range_ends(void)
{
	pca9685_set_frequency_ExpectAndReturn(TEST_I2C,
					      &pca9685[0].handle,
					      PCA9685_MIN_FREQ_HZ,
					      TEST_TIMEOUT_MS,
					      0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					    PCA9685_MIN_FREQ_HZ));

	pca9685_set_frequency_ExpectAndReturn(TEST_I2C,
					      &pca9685[0].handle,
					      PCA9685_MAX_FREQ_HZ,
					      TEST_TIMEOUT_MS,
					      0);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					    PCA9685_MAX_FREQ_HZ));
}

void test_set_pwm_frequency_pca9685_already_set_returns_1(void)
{
	pca9685_set_frequency_ExpectAndReturn(
		TEST_I2C, &pca9685[0].handle, 500, TEST_TIMEOUT_MS, 1);
	TEST_ASSERT_EQUAL_INT(1,
			      plc_xgpio_set_pwm_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 500));
}

void test_set_pwm_frequency_pca9685_errors(void)
{
	pca9685_set_frequency_ExpectAndReturn(
		TEST_I2C, &pca9685[0].handle, 500, TEST_TIMEOUT_MS, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(PERIPHERAL_PCA9685_SET_PWM_FREQUENCY_FAIL,
			      plc_xgpio_set_pwm_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 500));
	TEST_ASSERT_EQUAL_INT(
		FREQ_OUT_OF_RANGE,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					    PCA9685_MIN_FREQ_HZ - 1));
	TEST_ASSERT_EQUAL_INT(
		FREQ_OUT_OF_RANGE,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					    PCA9685_MAX_FREQ_HZ + 1));
	// It would wrap to 500 in the driver's uint16_t
	TEST_ASSERT_EQUAL_INT(
		FREQ_OUT_OF_RANGE,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					    UINT16_MAX + 1 + 500));
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_set_pwm_frequency(
			MAKE_PIN_PCA9685(PCA9685_ADDR, PCA9685_NUM_OUTPUTS),
			500));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_set_pwm_frequency(
				      MAKE_PIN_PCA9685(MISSING_ADDR, 0), 500));
}

void test_set_pwm_frequency_unsupported_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0),
					    500));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_set_pwm_frequency(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P0_GND), 500));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_set_pwm_frequency(UNKNOWN_TYPE_PIN,
							  500));
}

// ---- plc_xgpio_analog_read ----

void test_analog_read_null_read(void)
{
	TEST_ASSERT_EQUAL_INT(NULL_ARGUMENT,
			      plc_xgpio_analog_read(MAKE_PIN_DIRECT(36), NULL));
	TEST_ASSERT_EQUAL_UINT32(0, fake_normal_gpio.calls);
}

void test_analog_read_direct(void)
{
	uint32_t read = UINT32_MAX;

	fake_normal_gpio.analog = 3210;
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_analog_read(MAKE_PIN_DIRECT(0x123456), &read));
	TEST_ASSERT_EQUAL_UINT32(0x123456, fake_normal_gpio.pin);
	TEST_ASSERT_EQUAL_UINT32(3210, read);

	fake_normal_gpio.ret = -1;
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(NORMAL_GPIO_ANALOG_READ_FAIL,
			      plc_xgpio_analog_read(MAKE_PIN_DIRECT(0x123456),
						    &read));

	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(INDEX_OUT_OF_RANGE,
			      plc_xgpio_analog_read(MAKE_PIN_DIRECT(0x123456),
						    &read));

	errno = ENOTSUP;
	TEST_ASSERT_EQUAL_INT(PIN_NOT_SUPPORTED,
			      plc_xgpio_analog_read(MAKE_PIN_DIRECT(0x123456),
						    &read));
}

void test_analog_read_direct_keeps_readings_above_16_bits(void)
{
	uint32_t read = 0;

	// A 24-bit ADC's full scale
	fake_normal_gpio.analog = 0xFFFFFF;
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_analog_read(MAKE_PIN_DIRECT(0x123456), &read));
	TEST_ASSERT_EQUAL_UINT32(0xFFFFFF, read);
}

void test_analog_read_ltc2309_passes_the_channel(void)
{
	static const LTC2309_INPUT CHANNELS[LTC2309_NUM_INPUTS] = {
		LTC2309_CH0, LTC2309_CH1, LTC2309_CH2, LTC2309_CH3,
		LTC2309_CH4, LTC2309_CH5, LTC2309_CH6, LTC2309_CH7,
	};
	uint32_t read = UINT32_MAX;
	uint16_t value;
	uint8_t i;

	for (i = 0; i < LTC2309_NUM_INPUTS; i++) {
		value = (uint16_t)(100 + i);
		expect_ltc2309_read(CHANNELS[i], &value, 0);
		TEST_ASSERT_EQUAL_INT(
			0,
			plc_xgpio_analog_read(MAKE_PIN_LTC2309(LTC2309_ADDR,
							       CHANNELS[i]),
					      &read));
		TEST_ASSERT_EQUAL_UINT32(100 + i, read);
	}
}

void test_analog_read_ads101x_passes_the_input(void)
{
	static const ADS101X_INPUT INPUTS[] = {
		ADS101X_P0_GND,
		ADS101X_P1_GND,
		ADS101X_P2_GND,
		ADS101X_P3_GND,
	};
	uint32_t read = UINT32_MAX;
	uint16_t value;
	uint8_t i;

	for (i = 0; i < sizeof(INPUTS) / sizeof(INPUTS[0]); i++) {
		value = (uint16_t)(2000 + i);
		expect_ads1015_read(INPUTS[i], &value, 0);
		TEST_ASSERT_EQUAL_INT(
			0,
			plc_xgpio_analog_read(MAKE_PIN_ADS101X(ADS101X_ADDR,
							       INPUTS[i]),
					      &read));
		TEST_ASSERT_EQUAL_UINT32(2000 + i, read);
	}
}

void test_analog_read_ads101x_unknown_type_is_unsupported(void)
{
	// A type added to the driver after xgpio was built
	ADS101X_TYPE unknown = (ADS101X_TYPE)(ADS101X_015 + 1);
	uint16_t value = 0;
	uint32_t read;

	expect_ads1015_read(ADS101X_P3_GND, &value, -1);
	ads101x_get_type_ExpectAndReturn(
		TEST_I2C, &ads101x[0].handle, NULL, TEST_TIMEOUT_MS, 0);
	ads101x_get_type_IgnoreArg_type();
	ads101x_get_type_ReturnThruPtr_type(&unknown);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		UNSUPPORTED_PERIPHERAL_TYPE,
		plc_xgpio_analog_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P3_GND), &read));
}

void test_analog_read_ltc2309_differential_passes_the_pair(void)
{
	static const LTC2309_DIFF_INPUT PAIRS[] = {
		LTC2309_P0_N1, LTC2309_P2_N3, LTC2309_P4_N5, LTC2309_P6_N7,
		LTC2309_P1_N0, LTC2309_P3_N2, LTC2309_P5_N4, LTC2309_P7_N6,
	};
	uint32_t read = UINT32_MAX;
	uint16_t value;
	uint8_t i;

	for (i = 0; i < sizeof(PAIRS) / sizeof(PAIRS[0]); i++) {
		value = (uint16_t)(300 + i);
		expect_ltc2309_diff_read(PAIRS[i], &value, 0);
		TEST_ASSERT_EQUAL_INT(
			0,
			plc_xgpio_analog_read(
				MAKE_PIN_LTC2309_DIFF(LTC2309_ADDR, PAIRS[i]),
				&read));
		TEST_ASSERT_EQUAL_UINT32(300 + i, read);
	}
}

void test_reads_ltc2309_unknown_mode(void)
{
	// The 1st byte isn't single-ended or differential: no driver call
	const plc_xgpio_pin_t pin =
		_MAKE_PIN_PLC(PLC_LTC2309,
			      LTC2309_ADDR,
			      PLC_XGPIO_LTC2309_DIFFERENTIAL + 1,
			      LTC2309_CH0);
	uint32_t value;
	uint8_t level;

	TEST_ASSERT_EQUAL_INT(UNKNOWN_MODE, plc_xgpio_analog_read(pin, &value));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_MODE,
			      plc_xgpio_digital_read(pin, &level));
}

void test_analog_read_ads101x_continuous_uses_the_continuous_read(void)
{
	uint32_t read = UINT32_MAX;
	bool continuous = true;
	uint16_t value = 1234;

	ads101x_get_continuous_mode_ExpectAndReturn(
		TEST_I2C, &ads101x[0].handle, NULL, TEST_TIMEOUT_MS, 0);
	ads101x_get_continuous_mode_IgnoreArg_continuous_mode();
	ads101x_get_continuous_mode_ReturnThruPtr_continuous_mode(&continuous);
	ads101x_unsigned_continuous_read_ExpectAndReturn(TEST_I2C,
							 &ads101x[0].handle,
							 ADS101X_P2_GND,
							 NULL,
							 TEST_TIMEOUT_MS,
							 0);
	ads101x_unsigned_continuous_read_IgnoreArg_return_value();
	ads101x_unsigned_continuous_read_ReturnThruPtr_return_value(&value);
	TEST_ASSERT_EQUAL_INT(
		0,
		plc_xgpio_analog_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P2_GND), &read));
	TEST_ASSERT_EQUAL_UINT32(1234, read);
}

void test_analog_read_ads101x_get_continuous_mode_failing(void)
{
	// The wrong bus: the type can't be read either, so no index check
	uint32_t read;

	ads101x_get_continuous_mode_ExpectAndReturn(
		TEST_I2C, &ads101x[0].handle, NULL, TEST_TIMEOUT_MS, -1);
	ads101x_get_continuous_mode_IgnoreArg_continuous_mode();
	ads101x_get_type_ExpectAndReturn(
		TEST_I2C, &ads101x[0].handle, NULL, TEST_TIMEOUT_MS, -1);
	ads101x_get_type_IgnoreArg_type();
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_ADS101X_ANALOG_READ_FAIL,
		plc_xgpio_analog_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P2_GND), &read));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_analog_read_errors(void)
{
	uint16_t value = 0;
	uint32_t read;

	expect_ltc2309_read(LTC2309_CH0, &value, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_LTC2309_ANALOG_READ_FAIL,
		plc_xgpio_analog_read(
			MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH0), &read));

	expect_ads1015_read(ADS101X_P0_GND, &value, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_ADS101X_ANALOG_READ_FAIL,
		plc_xgpio_analog_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P0_GND), &read));

	// EINVAL on a valid input is about something else, like the wrong bus
	expect_ltc2309_read(LTC2309_CH7, &value, -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_LTC2309_ANALOG_READ_FAIL,
		plc_xgpio_analog_read(
			MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH7), &read));

	expect_ads1015_read(ADS101X_P3_GND, &value, -1);
	expect_ads101x_get_type();
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		PERIPHERAL_ADS101X_ANALOG_READ_FAIL,
		plc_xgpio_analog_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P3_GND), &read));

	expect_ltc2309_read((LTC2309_INPUT)LTC2309_NUM_INPUTS, &value, -1);
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_analog_read(MAKE_PIN_LTC2309(LTC2309_ADDR,
						       LTC2309_NUM_INPUTS),
				      &read));

	expect_ads1015_read((ADS101X_INPUT)ADS1015_NUM_INPUTS, &value, -1);
	expect_ads101x_get_type();
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(
		INDEX_OUT_OF_RANGE,
		plc_xgpio_analog_read(MAKE_PIN_ADS101X(ADS101X_ADDR,
						       ADS1015_NUM_INPUTS),
				      &read));

	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_analog_read(
			MAKE_PIN_LTC2309(MISSING_ADDR, LTC2309_CH0), &read));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_analog_read(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				      &read));
	TEST_ASSERT_EQUAL_INT(
		PIN_NOT_SUPPORTED,
		plc_xgpio_analog_read(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0),
				      &read));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_analog_read(UNKNOWN_TYPE_PIN, &read));
}

// ---- plc_xgpio_digital_write_all ----

void test_digital_write_all_mcp23008_writes_its_port(void)
{
	expect_mcp230xx_get_type(0);
	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, 0x0013, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_digital_write_all(MCP23008_ADDR, 0x13));
}

void test_digital_write_all_mcp23017_writes_both_ports(void)
{
	expect_mcp230xx_get_type(1);
	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, 0xBEEF, TEST_TIMEOUT_MS, 0);
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write_all(MCP23017_ADDR, 0xBEEF));
}

void test_digital_write_all_mcp230xx_errors(void)
{
	expect_mcp230xx_get_type(0);
	TEST_ASSERT_EQUAL_INT(
		VALUES_OUT_OF_RANGE,
		plc_xgpio_digital_write_all(MCP23008_ADDR,
					    1UL << MCP23008_NUM_GPIOS));

	expect_mcp230xx_get_type(1);
	TEST_ASSERT_EQUAL_INT(
		VALUES_OUT_OF_RANGE,
		plc_xgpio_digital_write_all(MCP23017_ADDR,
					    1UL << MCP23017_NUM_GPIOS));

	expect_mcp230xx_get_type(0);
	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, 0x0013, TEST_TIMEOUT_MS, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(PERIPHERAL_MCP230XX_DIGITAL_WRITE_ALL_FAIL,
			      plc_xgpio_digital_write_all(MCP23008_ADDR, 0x13));

	expect_mcp230xx_get_type(1);
	mcp230xx_write_all_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, 0xBEEF, TEST_TIMEOUT_MS, -1);
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(PERIPHERAL_MCP230XX_DIGITAL_WRITE_ALL_FAIL,
			      plc_xgpio_digital_write_all(MCP23017_ADDR,
							  0xBEEF));
}

void test_digital_write_all_pca9685_bit_i_is_output_i(void)
{
	const uint32_t values = 0x0413;
	uint8_t i;

	pca9685_write_all_Stub(pca9685_write_all_stub);
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write_all(PCA9685_ADDR, values));
	for (i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		TEST_ASSERT_EQUAL_UINT16((values >> i) & 1 ? PCA9685_OUTPUT_ON :
							     PCA9685_OUTPUT_OFF,
					 pca_all_values[i]);
	}
}

void test_digital_write_all_pca9685_errors(void)
{
	TEST_ASSERT_EQUAL_INT(
		VALUES_OUT_OF_RANGE,
		plc_xgpio_digital_write_all(PCA9685_ADDR,
					    1UL << PCA9685_NUM_OUTPUTS));

	pca9685_write_all_Stub(pca9685_write_all_stub);
	pca_all_ret = -1;
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(PERIPHERAL_PCA9685_DIGITAL_WRITE_ALL_FAIL,
			      plc_xgpio_digital_write_all(PCA9685_ADDR,
							  0x0413));
}

void test_digital_write_all_unknown_mcp230xx_type_is_unsupported(void)
{
	// A type added to the driver after xgpio was built
	MCP230XX_TYPE unknown = (MCP230XX_TYPE)(MCP230XX_017 + 1);

	mcp230xx_get_type_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_get_type_IgnoreArg_type();
	mcp230xx_get_type_ReturnThruPtr_type(&unknown);
	TEST_ASSERT_EQUAL_INT(UNSUPPORTED_PERIPHERAL_TYPE,
			      plc_xgpio_digital_write_all(MCP23017_ADDR, 0));
}

void test_digital_write_all_errors(void)
{
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_digital_write_all(LTC2309_ADDR, 0));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_digital_write_all(MISSING_ADDR, 0));
}

// ---- plc_xgpio_digital_read_all ----

void test_digital_read_all_mcp23008_stores_one_byte(void)
{
	uint16_t levels = 0x00C3;
	uint8_t out[2] = { 0xEE, 0xEE };

	expect_mcp230xx_get_type(0);
	mcp230xx_read_all_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, NULL, TEST_TIMEOUT_MS, 0);
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

	expect_mcp230xx_get_type(1);
	mcp230xx_read_all_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_read_all_IgnoreArg_return_value();
	mcp230xx_read_all_ReturnThruPtr_return_value(&levels);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_digital_read_all(MCP23017_ADDR, &out));
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, out);
}

void test_digital_read_all_failures(void)
{
	uint16_t out = 0x5A5A;

	expect_mcp230xx_get_type(0);
	mcp230xx_read_all_ExpectAndReturn(
		TEST_I2C, &mcp230xx[0].handle, NULL, TEST_TIMEOUT_MS, -1);
	mcp230xx_read_all_IgnoreArg_return_value();
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(PERIPHERAL_MCP230XX_DIGITAL_READ_ALL_FAIL,
			      plc_xgpio_digital_read_all(MCP23008_ADDR, &out));

	expect_mcp230xx_get_type(1);
	mcp230xx_read_all_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, NULL, TEST_TIMEOUT_MS, -1);
	mcp230xx_read_all_IgnoreArg_return_value();
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(PERIPHERAL_MCP230XX_DIGITAL_READ_ALL_FAIL,
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
}

void test_digital_read_all_unknown_mcp230xx_type_is_unsupported(void)
{
	// A type added to the driver after xgpio was built
	MCP230XX_TYPE unknown = (MCP230XX_TYPE)(MCP230XX_017 + 1);
	uint16_t out = 0x5A5A;

	mcp230xx_get_type_ExpectAndReturn(
		TEST_I2C, &mcp230xx[1].handle, NULL, TEST_TIMEOUT_MS, 0);
	mcp230xx_get_type_IgnoreArg_type();
	mcp230xx_get_type_ReturnThruPtr_type(&unknown);
	TEST_ASSERT_EQUAL_INT(UNSUPPORTED_PERIPHERAL_TYPE,
			      plc_xgpio_digital_read_all(MCP23017_ADDR, &out));
	TEST_ASSERT_EQUAL_HEX16(0x5A5A, out);
}

// ---- plc_xgpio_pwm_write_all ----

void test_pwm_write_all_pca9685_passes_every_duty_cycle(void)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS];
	uint8_t i;

	for (i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		duty[i] = (uint16_t)(i * 256);
	}

	pca9685_write_all_Stub(pca9685_write_all_stub);
	TEST_ASSERT_EQUAL_INT(0, plc_xgpio_pwm_write_all(PCA9685_ADDR, duty));
	TEST_ASSERT_EQUAL_UINT16_ARRAY(
		duty, pca_all_values, PCA9685_NUM_OUTPUTS);

	pca_all_ret = -1;
	errno = EIO;
	TEST_ASSERT_EQUAL_INT(PERIPHERAL_PCA9685_PWM_WRITE_ALL_FAIL,
			      plc_xgpio_pwm_write_all(PCA9685_ADDR, duty));
}

void test_pwm_write_all_errors(void)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS] = { 0 };

	pca9685_write_all_Stub(pca9685_write_all_stub);
	pca_all_ret = -1;

	// The driver's EINVAL with every duty cycle in range, one at the top
	duty[3] = PCA9685_OUTPUT_ON;
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(PERIPHERAL_PCA9685_PWM_WRITE_ALL_FAIL,
			      plc_xgpio_pwm_write_all(PCA9685_ADDR, duty));

	duty[3] = PCA9685_OUTPUT_ON + 1;
	errno = EINVAL;
	TEST_ASSERT_EQUAL_INT(DUTY_CYCLE_OUT_OF_RANGE,
			      plc_xgpio_pwm_write_all(PCA9685_ADDR, duty));

	TEST_ASSERT_EQUAL_INT(NULL_ARGUMENT,
			      plc_xgpio_pwm_write_all(PCA9685_ADDR, NULL));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_pwm_write_all(MCP23008_ADDR, duty));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_pwm_write_all(MISSING_ADDR, duty));
}

// ---- Empty arrays ----

void test_empty_arrays_find_nothing(void)
{
	uint32_t read;

	memset(&peripherals, 0, sizeof(peripherals));

	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0),
				       PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		I2C_ADDR_NOT_FOUND,
		plc_xgpio_analog_read(
			MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH0), &read));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_NOT_FOUND,
			      plc_xgpio_digital_write_all(MCP23008_ADDR, 0));
}
