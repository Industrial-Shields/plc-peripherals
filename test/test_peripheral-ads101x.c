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
 * Tests for src/peripheral-ads101x.c. Both of its dependencies are mocked:
 * plc-peripherals-i2c.h (the register-level i2c_write8_16b/i2c_read8_16b
 * calls) and plc-resource-protector.h (the plc_resource_* locking calls).
 * i2c_get_bus is mocked too: ads101x_protect calls it directly, one layer
 * below plc-peripherals-i2c.h.
 *
 * usleep() between a config write and the following conversion read is not
 * mocked; it's a real (short) sleep. Every test that exercises it picks a
 * fast data rate (ADS101X_2400SPS or faster) to keep the suite quick.
 *
 * The NULL/enabled_continuous_mode argument checks in ads101x_single_read and
 * ads101x_continuous_read are gated behind PLC_PERIPHERALS_CHECK_ARGUMENTS,
 * which defaults off and isn't enabled here, matching every other suite in
 * this project.
 *
 * ads101x_init's malloc failure isn't tested, matching the project's existing
 * convention of not testing bare allocation failure (see the resource
 * protector's own malloc/uthash exclusions).
 */

#include "unity.h"

#include "mock_plc-peripherals-i2c-hal.h"
#include "mock_plc-peripherals-i2c.h"
#include "mock_plc-resource-protector.h"
#include "peripheral-ads101x.h"

#include <errno.h>
#include <stdlib.h>

#define TEST_I2C ((i2c_interface_t*)0x1)
#define TEST_ADDR ((plc_i2c_addr_t)0x48)
#define TEST_BUS ((uint8_t)3)
#define TEST_RESOURCE I2C_RESOURCE(TEST_BUS, TEST_ADDR)

// Register addresses (private to peripheral-ads101x.c; mirrored here).
#define CONVERSION_REG 0x00
#define CONFIG_REG 0x01
#define LOW_THRESHOLD_REG 0x02
#define HIGH_THRESHOLD_REG 0x03
#define LOW_THRESHOLD_REG_RESET_VALUE 0x8000
#define HIGH_THRESHOLD_REG_RESET_VALUE 0x7FFF

// A data rate fast enough (~479us) that the real usleep() in
// ads101x_delay_until_conversion doesn't slow the suite down.
#define FAST_DR ADS101X_2400SPS

/*
 * CONFIG_REG value ads101x_init(restart=true) writes for
 * fsr=ADS101X_FSR_4_096V (0b001), dr=FAST_DR (0b101):
 *   OS=1 (0x8000) | MUX=000 (unchanged from reset) | PGA=001<<9 (0x200) |
 *   MODE (0x100 if single, 0 if continuous) | DR=101<<5 (0xA0) |
 *   COMP bits=000_11 (unchanged from CONFIG_REG_RESET_VALUE's 0x3)
 */
#define INIT_RESTART_FSR ADS101X_FSR_4_096V
#define INIT_RESTART_CFG_SINGLE 0x83A3 // 0x8000 | 0x200 | 0x100 | 0xA0 | 3
#define INIT_RESTART_CFG_CONTINUOUS 0x83A3 - 0x100 // MODE bit cleared

static void expect_ads101x_reset_writes(void)
{
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					TEST_ADDR,
					HIGH_THRESHOLD_REG,
					HIGH_THRESHOLD_REG_RESET_VALUE,
					0);
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					TEST_ADDR,
					LOW_THRESHOLD_REG,
					LOW_THRESHOLD_REG_RESET_VALUE,
					0);
}

/*
 * CMock's ReturnThruPtr only stores the pointer we give it; the memcpy into
 * the caller's out-param happens later, when the mocked call actually runs.
 * Every test sets up all its expectations before calling the function under
 * test, so a single shared static would get overwritten by a later call in
 * the same test before any of them fire. Round-robin through a small pool
 * instead, so each pending expectation keeps its own value alive.
 */
#define READ_VALUE_POOL_SIZE 4
static uint16_t read_value_pool[READ_VALUE_POOL_SIZE];
static int read_value_pool_index;

static void expect_i2c_read8_16b(uint8_t reg, uint16_t value, int retval)
{
	i2c_read8_16b_ExpectAndReturn(TEST_I2C, TEST_ADDR, reg, NULL, retval);
	i2c_read8_16b_IgnoreArg_to_read();
	if (retval == 0) {
		TEST_ASSERT_LESS_THAN_INT(READ_VALUE_POOL_SIZE, read_value_pool_index);
		read_value_pool[read_value_pool_index] = value;
		i2c_read8_16b_ReturnThruPtr_to_read(
			&read_value_pool[read_value_pool_index]);
		read_value_pool_index++;
	}
}

// Creates and initializes an ads101x_t via restart=true, fsr=4.096V, dr=fast.
static ads101x_t* create_ads(bool continuous)
{
	expect_ads101x_reset_writes();
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C,
		TEST_ADDR,
		CONFIG_REG,
		continuous ? INIT_RESTART_CFG_CONTINUOUS : INIT_RESTART_CFG_SINGLE,
		0);

	ads101x_t* ads = ads101x_init(
		TEST_I2C, TEST_ADDR, true, continuous, INIT_RESTART_FSR, FAST_DR);
	TEST_ASSERT_NOT_NULL(ads);
	return ads;
}

// create_ads() plus a successful ads101x_protect().
static ads101x_t* create_protected_ads(bool continuous)
{
	ads101x_t* ads = create_ads(continuous);

	i2c_get_bus_ExpectAndReturn(TEST_I2C, NULL, 0);
	i2c_get_bus_IgnoreArg_bus();
	static uint8_t stashed_bus;
	stashed_bus = TEST_BUS;
	i2c_get_bus_ReturnThruPtr_bus(&stashed_bus);
	plc_resource_add_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ads101x_protect(ads));
	return ads;
}

void setUp(void)
{
	read_value_pool_index = 0;
}

void tearDown(void)
{
}

/* ---------------------------- ads101x_init -------------------------------- */

void test_ads101x_init_with_restart_packs_config_reg_in_single_mode(void)
{
	expect_ads101x_reset_writes();
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);

	ads101x_t* ads = ads101x_init(
		TEST_I2C, TEST_ADDR, true, false, INIT_RESTART_FSR, FAST_DR);

	TEST_ASSERT_NOT_NULL(ads);
	free(ads);
}

void test_ads101x_init_with_restart_packs_config_reg_in_continuous_mode(void)
{
	expect_ads101x_reset_writes();
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					TEST_ADDR,
					CONFIG_REG,
					INIT_RESTART_CFG_CONTINUOUS,
					0);

	ads101x_t* ads = ads101x_init(
		TEST_I2C, TEST_ADDR, true, true, INIT_RESTART_FSR, FAST_DR);

	TEST_ASSERT_NOT_NULL(ads);
	free(ads);
}

void test_ads101x_init_without_restart_reads_then_patches_config_reg(void)
{
	// Device already has some COMP bits set; they must survive untouched.
	expect_i2c_read8_16b(CONFIG_REG, 0x0003, 0);
	// MODE(0x100) | PGA(3<<9=0x600) | DR(FAST_DR=5<<5=0xA0) | preserved COMP(0x3)
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x07A3, 0);

	ads101x_t* ads = ads101x_init(TEST_I2C,
				      TEST_ADDR,
				      false,
				      false,
				      ADS101X_FSR_1_024V,
				      FAST_DR);

	TEST_ASSERT_NOT_NULL(ads);
	free(ads);
}

void test_ads101x_init_fails_when_the_high_threshold_reset_fails(void)
{
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					TEST_ADDR,
					HIGH_THRESHOLD_REG,
					HIGH_THRESHOLD_REG_RESET_VALUE,
					-1);

	TEST_ASSERT_NULL(ads101x_init(
		TEST_I2C, TEST_ADDR, true, false, INIT_RESTART_FSR, FAST_DR));
}

void test_ads101x_init_fails_when_the_low_threshold_reset_fails(void)
{
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					TEST_ADDR,
					HIGH_THRESHOLD_REG,
					HIGH_THRESHOLD_REG_RESET_VALUE,
					0);
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					TEST_ADDR,
					LOW_THRESHOLD_REG,
					LOW_THRESHOLD_REG_RESET_VALUE,
					-1);

	TEST_ASSERT_NULL(ads101x_init(
		TEST_I2C, TEST_ADDR, true, false, INIT_RESTART_FSR, FAST_DR));
}

void test_ads101x_init_fails_when_reading_the_config_reg_fails(void)
{
	expect_i2c_read8_16b(CONFIG_REG, 0, -1);

	TEST_ASSERT_NULL(ads101x_init(TEST_I2C,
				      TEST_ADDR,
				      false,
				      false,
				      INIT_RESTART_FSR,
				      FAST_DR));
}

void test_ads101x_init_fails_when_writing_the_config_reg_fails(void)
{
	expect_ads101x_reset_writes();
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, -1);

	TEST_ASSERT_NULL(ads101x_init(
		TEST_I2C, TEST_ADDR, true, false, INIT_RESTART_FSR, FAST_DR));
}

/* --------------------------- ads101x_deinit ------------------------------- */

void test_ads101x_deinit_without_shutdown_just_frees(void)
{
	ads101x_t* ads = create_ads(false);
	TEST_ASSERT_EQUAL_INT(0, ads101x_deinit(ads, false));
}

void test_ads101x_deinit_with_shutdown_sets_the_mode_bit_and_writes_it_back(
	void)
{
	ads101x_t* ads = create_ads(true); // starts in continuous (MODE=0)

	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_CONTINUOUS, 0);
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					TEST_ADDR,
					CONFIG_REG,
					INIT_RESTART_CFG_CONTINUOUS | 0x100,
					0);

	TEST_ASSERT_EQUAL_INT(0, ads101x_deinit(ads, true));
}

void test_ads101x_deinit_fails_when_reading_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0, -1);

	TEST_ASSERT_EQUAL_INT(-1, ads101x_deinit(ads, true));
	free(ads); // deinit bailed out before freeing it
}

void test_ads101x_deinit_fails_when_writing_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					TEST_ADDR,
					CONFIG_REG,
					INIT_RESTART_CFG_SINGLE,
					-1);

	TEST_ASSERT_EQUAL_INT(-1, ads101x_deinit(ads, true));
	free(ads);
}

void test_ads101x_deinit_also_unprotects_when_protected(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ads101x_deinit(ads, false));
}

/* --------------------------- ads101x_protect ------------------------------ */

void test_ads101x_protect_fails_with_einval_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_protect(NULL));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_ads101x_protect_adds_the_resource_for_its_bus_and_address(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_get_bus_ExpectAndReturn(TEST_I2C, NULL, 0);
	i2c_get_bus_IgnoreArg_bus();
	static uint8_t stashed_bus;
	stashed_bus = TEST_BUS;
	i2c_get_bus_ReturnThruPtr_bus(&stashed_bus);
	plc_resource_add_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ads101x_protect(ads));

	free(ads);
}

void test_ads101x_protect_fails_when_the_bus_cant_be_read(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_get_bus_ExpectAndReturn(TEST_I2C, NULL, -1);
	i2c_get_bus_IgnoreArg_bus();

	TEST_ASSERT_EQUAL_INT(-1, ads101x_protect(ads));

	free(ads);
}

void test_ads101x_protect_returns_1_if_already_protected(void)
{
	ads101x_t* ads = create_protected_ads(false);

	i2c_get_bus_ExpectAndReturn(TEST_I2C, NULL, 0);
	i2c_get_bus_IgnoreArg_bus();
	static uint8_t stashed_bus;
	stashed_bus = TEST_BUS;
	i2c_get_bus_ReturnThruPtr_bus(&stashed_bus);
	plc_resource_add_ExpectAndReturn(TEST_RESOURCE, 1);

	TEST_ASSERT_EQUAL_INT(1, ads101x_protect(ads));

	free(ads);
}

/* -------------------------- ads101x_unprotect ------------------------------ */

void test_ads101x_unprotect_fails_with_einval_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_unprotect(NULL));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_ads101x_unprotect_removes_the_resource(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ads101x_unprotect(ads));

	free(ads);
}

void test_ads101x_unprotect_fails_when_the_resource_cant_be_removed(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, -1);

	TEST_ASSERT_EQUAL_INT(-1, ads101x_unprotect(ads));

	free(ads);
}

/* ------------------------- ads101x_single_read ----------------------------- */

void test_ads101x_single_read_returns_a_positive_reading(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0); // channel P0_N1, fast DR
	// CHANGE_CHANNEL(P0_N1=0) keeps MUX=0; OS bit gets set to trigger a
	// conversion.
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x00A0 | 0x8000, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_single_read(ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT16(0x0FF, value);

	free(ads);
}

void test_ads101x_single_read_when_protected_locks_and_unlocks(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 1000, 0);
	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0); // fast DR already set
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x00A0 | 0x8000, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	plc_resource_unlock_ExpectAndReturn(TEST_RESOURCE, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_single_read(ads, ADS101X_P0_N1, &value, 1000));

	free(ads);
}

void test_ads101x_single_read_fails_immediately_when_the_lock_times_out(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 50, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_single_read(ads, ADS101X_P0_N1, &value, 50));

	free(ads);
}

void test_ads101x_single_read_fails_when_reading_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_single_read(ads, ADS101X_P0_N1, &value, 1000));

	free(ads);
}

void test_ads101x_single_read_fails_when_writing_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x0000, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x8000, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_single_read(ads, ADS101X_P0_N1, &value, 1000));

	free(ads);
}

void test_ads101x_single_read_fails_when_reading_the_conversion_reg_fails(
	void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x00A0 | 0x8000, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_single_read(ads, ADS101X_P0_N1, &value, 1000));

	free(ads);
}

/* --------------------- ads101x_unsigned_single_read ------------------------ */

void test_ads101x_unsigned_single_read_passes_through_a_positive_reading(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x00A0 | 0x8000, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0); // -> +255

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_unsigned_single_read(ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(255, value);

	free(ads);
}

void test_ads101x_unsigned_single_read_clamps_a_small_negative_reading_to_0(
	void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x00A0 | 0x8000, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0xFFF0, 0); // -> -1

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_unsigned_single_read(ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(0, value);

	free(ads);
}

void test_ads101x_unsigned_single_read_fails_with_erange_below_minus_8(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x00A0 | 0x8000, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0xFF00, 0); // -> -16

	uint16_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_unsigned_single_read(ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	free(ads);
}

void test_ads101x_unsigned_single_read_propagates_a_single_read_failure(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0, -1);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_unsigned_single_read(ads, ADS101X_P0_N1, &value, 1000));

	free(ads);
}

/* ----------------------- ads101x_continuous_read ---------------------------- */

void test_ads101x_continuous_read_skips_the_write_on_the_same_channel(void)
{
	ads101x_t* ads = create_ads(true);

	// old_cfg's MUX already selects P0_N1 (0): no config write needed.
	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_continuous_read(ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT16(0x0FF, value);

	free(ads);
}

void test_ads101x_continuous_read_writes_and_delays_on_a_channel_change(void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0); // MUX=P0_N1, DR=fast
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x00A0 | 0x1000, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_continuous_read(ads, ADS101X_P1_N3, &value, 1000));

	free(ads);
}

void test_ads101x_continuous_read_when_protected_locks_and_unlocks(void)
{
	ads101x_t* ads = create_protected_ads(true);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 1000, 0);
	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	plc_resource_unlock_ExpectAndReturn(TEST_RESOURCE, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_continuous_read(ads, ADS101X_P0_N1, &value, 1000));

	free(ads);
}

void test_ads101x_continuous_read_fails_when_reading_the_config_reg_fails(
	void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONFIG_REG, 0, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_continuous_read(ads, ADS101X_P0_N1, &value, 1000));

	free(ads);
}

void test_ads101x_continuous_read_fails_when_writing_the_config_reg_fails(
	void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x00A0 | 0x1000, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_continuous_read(ads, ADS101X_P1_N3, &value, 1000));

	free(ads);
}

void test_ads101x_continuous_read_fails_when_reading_the_conversion_reg_fails(
	void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_continuous_read(ads, ADS101X_P0_N1, &value, 1000));

	free(ads);
}

/* -------------------- ads101x_unsigned_continuous_read ---------------------- */

void test_ads101x_unsigned_continuous_read_passes_through_a_positive_reading(
	void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(0,
			       ads101x_unsigned_continuous_read(
				       ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(255, value);

	free(ads);
}

void test_ads101x_unsigned_continuous_read_fails_with_erange_below_minus_8(
	void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0xFF00, 0); // -> -16

	uint16_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			       ads101x_unsigned_continuous_read(
				       ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	free(ads);
}

/* --------------------------- ads101x_get_fs -------------------------------- */

void test_ads101x_get_fs_returns_the_current_data_rate(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x00A0, 0); // DR bits = 101 = 2400SPS

	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr));
	TEST_ASSERT_EQUAL_INT(ADS101X_2400SPS, dr);

	free(ads);
}

void test_ads101x_get_fs_maps_the_0b111_encoding_to_3300sps(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x00E0, 0); // DR bits = 111

	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr));
	TEST_ASSERT_EQUAL_INT(ADS101X_3300SPS, dr);

	free(ads);
}

void test_ads101x_get_fs_fails_when_reading_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0, -1);

	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_get_fs(ads, &dr));

	free(ads);
}

/* --------------------------- ads101x_set_fs -------------------------------- */

void test_ads101x_set_fs_patches_only_the_dr_bits(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x0003, 0); // COMP bits set, DR=0
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x0063, 0); // DR=011<<5 | 0x3

	TEST_ASSERT_EQUAL_INT(
		0, ads101x_set_fs(ads, ADS101X_920SPS, 1000));

	free(ads);
}

void test_ads101x_set_fs_when_protected_locks_and_unlocks(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 1000, 0);
	expect_i2c_read8_16b(CONFIG_REG, 0x0003, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x0063, 0);
	plc_resource_unlock_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(
		0, ads101x_set_fs(ads, ADS101X_920SPS, 1000));

	free(ads);
}

void test_ads101x_set_fs_fails_immediately_when_the_lock_times_out(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 50, -1);

	TEST_ASSERT_EQUAL_INT(-1, ads101x_set_fs(ads, ADS101X_920SPS, 50));

	free(ads);
}

void test_ads101x_set_fs_fails_when_reading_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0, -1);

	TEST_ASSERT_EQUAL_INT(-1, ads101x_set_fs(ads, ADS101X_920SPS, 1000));

	free(ads);
}

void test_ads101x_set_fs_fails_when_writing_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	expect_i2c_read8_16b(CONFIG_REG, 0x0003, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x0063, -1);

	TEST_ASSERT_EQUAL_INT(-1, ads101x_set_fs(ads, ADS101X_920SPS, 1000));

	free(ads);
}
