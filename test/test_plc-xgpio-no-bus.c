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
 * Tests for src/plc-xgpio.c without an I2C bus: plc_xgpio_i2c is
 * PLC_XGPIO_NO_I2C_BUS for the whole executable.
 */

#include "unity.h"

#include "mock_peripheral-ads101x.h"
#include "mock_peripheral-ltc2309.h"
#include "mock_peripheral-mcp230xx.h"
#include "mock_peripheral-pca9685.h"
#include "fake-normal-gpio.h"
#include "plc-xgpio.h"

#include <errno.h>

// clang-format off
#define MCP23008_ADDR 0x20
#define PCA9685_ADDR  0x40
#define LTC2309_ADDR  0x08
#define ADS101X_ADDR  0x48
// clang-format on

// The first pin type after the last one xgpio knows
#define UNKNOWN_TYPE_PIN ((plc_xgpio_pin_t)(PLC_ADS101X + 1) << 24)

/*
 * The peripherals are in the arrays, so a lookup alone doesn't tell there is
 * no bus.
 */
static plc_xgpio_mcp230xx_t mcp230xx[1] = { { .addr = MCP23008_ADDR } };
static plc_xgpio_pca9685_t pca9685[1] = { { .addr = PCA9685_ADDR } };
static plc_xgpio_ltc2309_t ltc2309[1] = { { .addr = LTC2309_ADDR } };
static plc_xgpio_ads101x_t ads101x[1] = { { .addr = ADS101X_ADDR } };
static plc_xgpio_peripherals_t peripherals = {
	.pca9685 = pca9685,
	.num_pca9685 = 1,
	.mcp230xx = mcp230xx,
	.num_mcp230xx = 1,
	.ltc2309 = ltc2309,
	.num_ltc2309 = 1,
	.ads101x = ads101x,
	.num_ads101x = 1,
};

const plc_xgpio_peripherals_t* const plc_xgpio_peripherals = &peripherals;
const i2c_interface_t* plc_xgpio_i2c = PLC_XGPIO_NO_I2C_BUS;

// mcp230xx_set_input as the driver behaves with a NULL i2c
static int set_input_without_bus(const i2c_interface_t* i2c,
				 mcp230xx_t* mcp,
				 uint8_t index,
				 MCP230XX_INPUT_CONFIG config,
				 uint32_t timeout_ms,
				 int cmock_num_calls)
{
	(void)mcp;
	(void)index;
	(void)config;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	return 0;
}

// pca9685_write_output as the driver behaves with a NULL i2c
static int write_output_without_bus(const i2c_interface_t* i2c,
				    pca9685_t* pca,
				    uint8_t index,
				    uint16_t value,
				    uint32_t timeout_ms,
				    int cmock_num_calls)
{
	(void)pca;
	(void)index;
	(void)value;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	return 0;
}

// ads101x_get_type as the driver behaves with a NULL i2c
static int ads_get_type_without_bus(const i2c_interface_t* i2c,
				    ads101x_t* ads,
				    ADS101X_TYPE* type,
				    uint32_t timeout_ms,
				    int cmock_num_calls)
{
	(void)ads;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	*type = ADS101X_015;
	return 0;
}

// ads101x_get_continuous_mode as the driver behaves with a NULL i2c
static int get_continuous_mode_without_bus(const i2c_interface_t* i2c,
					   ads101x_t* ads,
					   bool* continuous_mode,
					   uint32_t timeout_ms,
					   int cmock_num_calls)
{
	(void)ads;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	*continuous_mode = false;
	return 0;
}

// ads101x_unsigned_single_read as the driver behaves with a NULL i2c
static int single_read_without_bus(const i2c_interface_t* i2c,
				   ads101x_t* ads,
				   ADS101X_INPUT index,
				   uint16_t* return_value,
				   uint32_t timeout_ms,
				   int cmock_num_calls)
{
	(void)ads;
	(void)index;
	(void)return_value;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	return 0;
}

// pca9685_set_frequency as the driver behaves with a NULL i2c
static int set_frequency_without_bus(const i2c_interface_t* i2c,
				     pca9685_t* pca,
				     uint16_t freq_hz,
				     uint32_t timeout_ms,
				     int cmock_num_calls)
{
	(void)pca;
	(void)freq_hz;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	return 0;
}

// mcp230xx_get_type as the driver behaves with a NULL i2c
static int get_type_without_bus(const i2c_interface_t* i2c,
				mcp230xx_t* mcp,
				MCP230XX_TYPE* type,
				uint32_t timeout_ms,
				int cmock_num_calls)
{
	(void)mcp;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	*type = MCP230XX_008;
	return 0;
}

// mcp230xx_write_all as the driver behaves with a NULL i2c
static int mcp_write_all_without_bus(const i2c_interface_t* i2c,
				     mcp230xx_t* mcp,
				     uint16_t to_write,
				     uint32_t timeout_ms,
				     int cmock_num_calls)
{
	(void)mcp;
	(void)to_write;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	return 0;
}

// mcp230xx_read_all as the driver behaves with a NULL i2c
static int mcp_read_all_without_bus(const i2c_interface_t* i2c,
				    mcp230xx_t* mcp,
				    uint16_t* return_value,
				    uint32_t timeout_ms,
				    int cmock_num_calls)
{
	(void)mcp;
	(void)return_value;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	return 0;
}

// pca9685_write_all as the driver behaves with a NULL i2c
static int pca_write_all_without_bus(const i2c_interface_t* i2c,
				     pca9685_t* pca,
				     const uint16_t values[PCA9685_NUM_OUTPUTS],
				     uint32_t timeout_ms,
				     int cmock_num_calls)
{
	(void)pca;
	(void)values;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	return 0;
}

// ltc2309_unsigned_single_ended_read as the driver behaves with a NULL i2c
static int single_ended_read_without_bus(const i2c_interface_t* i2c,
					 ltc2309_t* ltc,
					 LTC2309_INPUT index,
					 uint16_t* read_value,
					 uint32_t timeout_ms,
					 int cmock_num_calls)
{
	(void)ltc;
	(void)index;
	(void)read_value;
	(void)timeout_ms;
	(void)cmock_num_calls;

	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	return 0;
}

void setUp(void)
{
	fake_normal_gpio_reset();
}

void tearDown(void)
{
}

void test_set_pin_mode_without_bus_only_handles_direct_pins(void)
{
	mcp230xx_set_input_Stub(set_input_without_bus);
	ads101x_get_type_Stub(ads_get_type_without_bus);

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_set_pin_mode(MAKE_PIN_DIRECT(5), PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_UINT32(1, fake_normal_gpio.calls);
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_set_pin_mode(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
				       PLC_XGPIO_OUTPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_set_pin_mode(MAKE_PIN_MCP230XX(MCP23008_ADDR, 0),
				       PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_set_pin_mode(MAKE_PIN_LTC2309(LTC2309_ADDR,
							LTC2309_CH0),
				       PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_set_pin_mode(MAKE_PIN_ADS101X(ADS101X_ADDR,
							ADS101X_P0_GND),
				       PLC_XGPIO_INPUT));
	TEST_ASSERT_EQUAL_INT(UNKNOWN_PIN_TYPE,
			      plc_xgpio_set_pin_mode(UNKNOWN_TYPE_PIN,
						     PLC_XGPIO_INPUT));
}

void test_digital_write_without_bus(void)
{
	pca9685_write_output_Stub(write_output_without_bus);

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_write(MAKE_PIN_DIRECT(9), PLC_XGPIO_HIGH));
	TEST_ASSERT_EQUAL_UINT32(1, fake_normal_gpio.calls);
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_digital_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0),
					PLC_XGPIO_HIGH));
}

void test_digital_read_without_bus(void)
{
	uint8_t read = UINT8_MAX;

	ads101x_get_continuous_mode_Stub(get_continuous_mode_without_bus);
	ads101x_unsigned_single_read_Stub(single_read_without_bus);
	fake_normal_gpio.level = PLC_XGPIO_HIGH;

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_digital_read(MAKE_PIN_DIRECT(3), &read));
	TEST_ASSERT_EQUAL_UINT8(PLC_XGPIO_HIGH, read);
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_digital_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P0_GND), &read));
}

void test_pwm_write_without_bus(void)
{
	pca9685_write_output_Stub(write_output_without_bus);

	TEST_ASSERT_EQUAL_INT(0, plc_xgpio_pwm_write(MAKE_PIN_DIRECT(4), 1));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_pwm_write(MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 1));
}

void test_set_pwm_frequency_without_bus(void)
{
	pca9685_set_frequency_Stub(set_frequency_without_bus);

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_set_pwm_frequency(MAKE_PIN_DIRECT(4), 500));
	TEST_ASSERT_EQUAL_INT(I2C_PIN_WITHOUT_I2C_BUS,
			      plc_xgpio_set_pwm_frequency(
				      MAKE_PIN_PCA9685(PCA9685_ADDR, 0), 500));
}

void test_all_functions_without_bus(void)
{
	uint16_t duty[PCA9685_NUM_OUTPUTS] = { 0 };
	uint16_t levels;

	mcp230xx_get_type_Stub(get_type_without_bus);
	mcp230xx_write_all_Stub(mcp_write_all_without_bus);
	mcp230xx_read_all_Stub(mcp_read_all_without_bus);
	pca9685_write_all_Stub(pca_write_all_without_bus);

	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_I2C_BUS,
			      plc_xgpio_digital_write_all(MCP23008_ADDR, 0));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_I2C_BUS,
			      plc_xgpio_digital_write_all(PCA9685_ADDR, 0));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_I2C_BUS,
			      plc_xgpio_digital_read_all(MCP23008_ADDR,
							 &levels));
	TEST_ASSERT_EQUAL_INT(I2C_ADDR_WITHOUT_I2C_BUS,
			      plc_xgpio_pwm_write_all(PCA9685_ADDR, duty));
}

void test_analog_read_without_bus(void)
{
	uint32_t read = UINT32_MAX;

	ltc2309_unsigned_single_ended_read_Stub(single_ended_read_without_bus);
	ads101x_get_continuous_mode_Stub(get_continuous_mode_without_bus);
	ads101x_unsigned_single_read_Stub(single_read_without_bus);
	fake_normal_gpio.analog = 77;

	TEST_ASSERT_EQUAL_INT(
		0, plc_xgpio_analog_read(MAKE_PIN_DIRECT(36), &read));
	TEST_ASSERT_EQUAL_UINT32(77, read);
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_analog_read(
			MAKE_PIN_LTC2309(LTC2309_ADDR, LTC2309_CH0), &read));
	TEST_ASSERT_EQUAL_INT(
		I2C_PIN_WITHOUT_I2C_BUS,
		plc_xgpio_analog_read(
			MAKE_PIN_ADS101X(ADS101X_ADDR, ADS101X_P0_GND), &read));
}
