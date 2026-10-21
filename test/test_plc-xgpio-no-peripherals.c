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
 * Tests for src/plc-xgpio.c with a NULL plc_xgpio_peripherals. It is fixed at
 * link time, so they can't share an executable with the ones that have
 * peripherals.
 */

#include "unity.h"

#include "mock_peripheral-ads101x.h"
#include "mock_peripheral-ltc2309.h"
#include "mock_peripheral-mcp230xx.h"
#include "mock_peripheral-pca9685.h"
#include "fake-normal-gpio.h"
#include "plc-xgpio.h"

// Only compared by address, never dereferenced
#define TEST_I2C ((const i2c_interface_t*)(!NULL))

const plc_xgpio_peripherals_t* const plc_xgpio_peripherals = NULL;
const i2c_interface_t* plc_xgpio_i2c = TEST_I2C;

void setUp(void)
{
	fake_normal_gpio_reset();
}

void tearDown(void)
{
}

void test_set_pin_mode_without_peripherals_only_handles_direct_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5), PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_UINT32(1, fake_normal_gpio.calls);
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_PERIPHERALS,
			      plc_xgpio_set_pin_mode(MAKE_PIN_PCA9685(0x40, 0),
						     PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_PERIPHERALS,
			      plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(0x20, 0),
						     PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_PERIPHERALS,
		plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309(0x08, LTC2309_CH0),
				       PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_PERIPHERALS,
		plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(0x48, ADS101X_P0_GND),
				       PLC_XGPIO_INPUT));
}

void test_digital_write_without_peripherals_only_handles_direct_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write(MAKE_PIN_DIRECT(5), PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_UINT32(1, fake_normal_gpio.calls);
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_PERIPHERALS,
			      plc_xgpio_digital_write(MAKE_PIN_PCA9685(0x40, 0),
						      PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_PERIPHERALS,
		plc_xgpio_digital_write(MAKE_PIN_MCP230XX(0x20, 0),
					PLC_XGPIO_HIGH));
}

void test_digital_read_without_peripherals_only_handles_direct_pins(void)
{
	uint8_t read;

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_read(MAKE_PIN_DIRECT(5), &read));
	TEST_ASSERT_EQUAL_UINT32(1, fake_normal_gpio.calls);
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_PERIPHERALS,
			      plc_xgpio_digital_read(MAKE_PIN_PCA9685(0x40, 0),
						     &read));
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_PERIPHERALS,
			      plc_xgpio_digital_read(MAKE_PIN_MCP230XX(0x20, 0),
						     &read));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_PERIPHERALS,
		plc_xgpio_digital_read(MAKE_PIN_LTC2309(0x08, LTC2309_CH0),
				       &read));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_PERIPHERALS,
		plc_xgpio_digital_read(MAKE_PIN_ADS101X(0x48, ADS101X_P0_GND),
				       &read));
}

void test_pwm_write_without_peripherals_only_handles_direct_pins(void)
{
	TEST_ASSERT_EQUAL_INT(0, plc_xgpio_pwm_write(MAKE_PIN_DIRECT(5), 1));
	TEST_ASSERT_EQUAL_UINT32(1, fake_normal_gpio.calls);
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_PERIPHERALS,
			      plc_xgpio_pwm_write(MAKE_PIN_PCA9685(0x40, 0),
						  1));
}

void test_set_pwm_frequency_without_peripherals_only_handles_direct_pins(void)
{
	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_set_pwm_frequency(MAKE_PIN_DIRECT(5), 500));
	TEST_ASSERT_EQUAL_UINT32(1, fake_normal_gpio.calls);
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_PERIPHERALS,
		plc_xgpio_set_pwm_frequency(MAKE_PIN_PCA9685(0x40, 0), 500));
}

void test_analog_read_without_peripherals_only_handles_direct_pins(void)
{
	uint16_t read;

	TEST_ASSERT_EQUAL_INT(0,
			      plc_xgpio_analog_read(MAKE_PIN_DIRECT(5), &read));
	TEST_ASSERT_EQUAL_UINT32(1, fake_normal_gpio.calls);
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_PERIPHERALS,
		plc_xgpio_analog_read(MAKE_PIN_LTC2309(0x08, LTC2309_CH0),
				      &read));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_PERIPHERALS,
		plc_xgpio_analog_read(MAKE_PIN_ADS101X(0x48, ADS101X_P0_GND),
				      &read));
}

void test_all_functions_without_peripherals(void)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS] = { 0 };
	uint16_t levels;

	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_PERIPHERALS,
			      plc_xgpio_digital_write_all(0x40, 0));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_PERIPHERALS,
			      plc_xgpio_digital_read_all(0x20, &levels));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_PERIPHERALS,
			      plc_xgpio_pwm_write_all(0x40, duty));
}
