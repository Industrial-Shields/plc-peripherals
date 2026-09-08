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
 * Tests for src/peripheral-ltc2309.c. Both of its dependencies are mocked:
 * plc-peripherals-i2c.h (the level i2c_write/i2c_read calls) and
 * plc-resource-protector.h (the plc_resource_* locking calls).  i2c_get_bus is
 * mocked too: ltc2309_protect calls it directly, one layer below
 * plc-peripherals-i2c.h.
 *
 * ltc2309_init() always sleeps a real, fixed 200ms (tREFWAKE) with no
 * data-rate-style knob to shorten it -- unlike ADS101X, whose tests pick a
 * fast data rate to keep the suite quick. Every test that needs an
 * initialized ltc2309_t pays this once; the suite is measurably slower than
 * ADS101X's as a result, but there's nothing to fake without changing the
 * driver itself.
 */

#include "unity.h"

#include "fake-i2c.h"
#include "mock_plc-peripherals-i2c-hal.h"
#include "mock_plc-resource-protector.h"
#include "peripheral-ltc2309.h"

#include <errno.h>
#include <stdlib.h>
#include <time.h>

#define TEST_I2C FAKE_I2C_IFACE
#define TEST_ADDR ((plc_i2c_addr_t)0x08)
#define TEST_BUS ((uint8_t)3)
#define TEST_RESOURCE I2C_RESOURCE(TEST_BUS, TEST_ADDR)

// clang-format off
#define INITIAL_STATE                                                      0x00
#define SHUTDOWN                                                           0x04

#define COMMAND_BYTE_SD                                                    0x80
#define COMMAND_BYTE_SGL                                                   0x80
#define COMMAND_BYTE_DIFF                                                  0x00
#define COMMAND_BYTE_CHANNEL_SHIFT                                            4
#define COMMAND_BYTE_UNI                                                   0x08
#define COMMAND_BYTE_BIP                                                   0x00
// clang-format on

static long elapsed_ms(struct timespec start, struct timespec end)
{
	return (end.tv_sec - start.tv_sec) * 1000L +
	       (end.tv_nsec - start.tv_nsec) / 1000000L;
}

static void assert_last_write_was(uint8_t expected_byte)
{
	TEST_ASSERT_EQUAL_UINT(1, fake_i2c_write_op.len);
	TEST_ASSERT_EQUAL_HEX8(expected_byte, fake_i2c_write_op.bytes[0]);
}

// Asserts exactly one more i2c_write happened since calls_before, with cmd.
static void assert_wrote_cmd(uint32_t calls_before, uint8_t expected_cmd)
{
	TEST_ASSERT_EQUAL_UINT32(calls_before + 1, fake_i2c_write_op.calls);
	assert_last_write_was(expected_cmd);
}

// Asserts no additional i2c_write happened since calls_before.
static void assert_skipped_write(uint32_t calls_before)
{
	TEST_ASSERT_EQUAL_UINT32(calls_before, fake_i2c_write_op.calls);
}

// Creates and initializes an ltc2309_t; always writes INITIAL_STATE.
static ltc2309_t* create_ltc(bool bip)
{
	fake_i2c_write_op.retval = 1;

	ltc2309_t* ltc = ltc2309_init(TEST_I2C, TEST_ADDR, bip);
	TEST_ASSERT_NOT_NULL(ltc);
	assert_last_write_was(INITIAL_STATE);
	return ltc;
}

// create_ltc() plus a successful ltc2309_protect().
static ltc2309_t* create_protected_ltc(bool bip)
{
	ltc2309_t* ltc = create_ltc(bip);

	i2c_get_bus_ExpectAndReturn(TEST_I2C, NULL, 0);
	i2c_get_bus_IgnoreArg_bus();
	static uint8_t stashed_bus;
	stashed_bus = TEST_BUS;
	i2c_get_bus_ReturnThruPtr_bus(&stashed_bus);
	plc_resource_add_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ltc2309_protect(ltc));
	return ltc;
}

static void destroy_ltc(ltc2309_t* ltc)
{
	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(ltc, false));
}

static void destroy_protected_ltc(ltc2309_t* ltc)
{
	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(ltc, false));
}

void setUp(void)
{
	fake_i2c_reset();
	fake_i2c_expected_addr = TEST_ADDR;
	i2c_write_Stub(fake_i2c_write);
	i2c_read_Stub(fake_i2c_read);
}

void tearDown(void)
{
}

/* ---------------------------- ltc2309_init -------------------------------- */

void test_ltc2309_init_writes_initial_state_and_waits_trefwake(void)
{
	fake_i2c_write_op.retval = 1;

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	ltc2309_t* ltc = ltc2309_init(TEST_I2C, TEST_ADDR, true);

	clock_gettime(CLOCK_MONOTONIC, &end);

	TEST_ASSERT_NOT_NULL(ltc);
	assert_last_write_was(INITIAL_STATE);

	long ms = elapsed_ms(start, end);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(200, ms);
	TEST_ASSERT_LESS_THAN_INT(260, ms);

	destroy_ltc(ltc);
}

void test_ltc2309_init_fails_with_efault_when_i2c_is_null(void)
{
	errno = 0;
	TEST_ASSERT_NULL(ltc2309_init(NULL, TEST_ADDR, true));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ltc2309_init_fails_when_the_initial_write_fails(void)
{
	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_NULL(ltc2309_init(TEST_I2C, TEST_ADDR, true));
}

/* --------------------------- ltc2309_deinit ------------------------------- */

void test_ltc2309_deinit_without_shutdown_just_frees(void)
{
	ltc2309_t* ltc = create_ltc(true);
	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(ltc, false));
}

void test_ltc2309_deinit_with_shutdown_writes_the_shutdown_byte(void)
{
	ltc2309_t* ltc = create_ltc(true);

	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(ltc, true));
	assert_last_write_was(SHUTDOWN);
}

void test_ltc2309_deinit_fails_when_the_shutdown_write_fails(void)
{
	ltc2309_t* ltc = create_ltc(true);

	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_EQUAL_INT(-1, ltc2309_deinit(ltc, true));
	free(ltc); // deinit bailed out before freeing it
}

void test_ltc2309_deinit_also_unprotects_when_protected(void)
{
	ltc2309_t* ltc = create_protected_ltc(true);

	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(ltc, false));
}

void test_ltc2309_deinit_fails_when_the_unprotect_fails(void)
{
	ltc2309_t* ltc = create_protected_ltc(true);

	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, -1);

	TEST_ASSERT_EQUAL_INT(-1, ltc2309_deinit(ltc, false));

	free(ltc); // deinit bailed out before freeing it
}

void test_ltc2309_deinit_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_deinit(NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

/* --------------------------- ltc2309_protect ------------------------------ */

void test_ltc2309_protect_fails_with_einval_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_protect(NULL));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_ltc2309_protect_adds_the_resource_for_its_bus_and_address(void)
{
	ltc2309_t* ltc = create_ltc(true);

	i2c_get_bus_ExpectAndReturn(TEST_I2C, NULL, 0);
	i2c_get_bus_IgnoreArg_bus();
	static uint8_t stashed_bus;
	stashed_bus = TEST_BUS;
	i2c_get_bus_ReturnThruPtr_bus(&stashed_bus);
	plc_resource_add_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ltc2309_protect(ltc));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_protect_fails_when_the_bus_cant_be_read(void)
{
	ltc2309_t* ltc = create_ltc(true);

	i2c_get_bus_ExpectAndReturn(TEST_I2C, NULL, -1);
	i2c_get_bus_IgnoreArg_bus();

	TEST_ASSERT_EQUAL_INT(-1, ltc2309_protect(ltc));

	destroy_ltc(ltc);
}

void test_ltc2309_protect_returns_1_if_already_protected(void)
{
	ltc2309_t* ltc = create_protected_ltc(true);

	i2c_get_bus_ExpectAndReturn(TEST_I2C, NULL, 0);
	i2c_get_bus_IgnoreArg_bus();
	static uint8_t stashed_bus;
	stashed_bus = TEST_BUS;
	i2c_get_bus_ReturnThruPtr_bus(&stashed_bus);
	plc_resource_add_ExpectAndReturn(TEST_RESOURCE, 1);

	TEST_ASSERT_EQUAL_INT(1, ltc2309_protect(ltc));

	destroy_protected_ltc(ltc);
}

/* -------------------------- ltc2309_unprotect ------------------------------ */

void test_ltc2309_unprotect_fails_with_einval_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_unprotect(NULL));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_ltc2309_unprotect_removes_the_resource(void)
{
	ltc2309_t* ltc = create_protected_ltc(true);

	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, 0);

	TEST_ASSERT_EQUAL_INT(0, ltc2309_unprotect(ltc));

	destroy_ltc(ltc);
}

void test_ltc2309_unprotect_fails_when_the_resource_cant_be_removed(void)
{
	ltc2309_t* ltc = create_protected_ltc(true);

	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, -1);

	TEST_ASSERT_EQUAL_INT(-1, ltc2309_unprotect(ltc));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_unprotect_returns_1_if_already_unprotected(void)
{
	ltc2309_t* ltc = create_protected_ltc(true);

	plc_resource_remove_ExpectAndReturn(TEST_RESOURCE, 1);

	TEST_ASSERT_EQUAL_INT(1, ltc2309_unprotect(ltc));

	destroy_ltc(ltc);
}

/* ------------------------- ltc2309_read_signed ----------------------------- */

void test_ltc2309_read_signed_returns_a_positive_reading(void)
{
	ltc2309_t* ltc = create_ltc(true); // starts on P0_N1 (cmd=0x00)
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint32_t writes_before = fake_i2c_write_op.calls;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 1000));

	assert_skipped_write(writes_before); // P0_N1 is already the cached cmd
	TEST_ASSERT_EQUAL_INT16(255, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_returns_the_maximally_negative_reading(void)
{
	// Regression test for avoiding an implementation-defined right shift
	// on a negative int16_t: 0x8000 must map to -2048, the most negative
	// value the 12-bit 2's complement conversion result can represent.
	ltc2309_t* ltc = create_ltc(true);
	static const uint8_t reading[2] = { 0x80, 0x00 };
	fake_i2c_read_answers(reading, 2);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT16(-2048, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_writes_on_a_pair_change(void)
{
	ltc2309_t* ltc = create_ltc(true);
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint32_t writes_before = fake_i2c_write_op.calls;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_signed(ltc, LTC2309_P2_N3, &value, 1000));

	assert_wrote_cmd(writes_before,
			 LTC2309_P2_N3 << COMMAND_BYTE_CHANNEL_SHIFT);

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_skips_the_write_on_the_same_pair(void)
{
	ltc2309_t* ltc = create_ltc(true);
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_signed(ltc, LTC2309_P2_N3, &value, 1000));

	uint32_t writes_before = fake_i2c_write_op.calls;
	fake_i2c_read_answers(reading, 2);
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_signed(ltc, LTC2309_P2_N3, &value, 1000));

	assert_skipped_write(writes_before);

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_clears_high_channel_bit_on_a_low_pair_switch(void)
{
	ltc2309_t* ltc = create_ltc(true);
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	fake_i2c_read_answers(reading, 2);
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_signed(ltc, LTC2309_P1_N0, &value, 1000));

	fake_i2c_read_answers(reading, 2);
	uint32_t writes_before = fake_i2c_write_op.calls;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 1000));

	assert_wrote_cmd(writes_before,
			 LTC2309_P0_N1 << COMMAND_BYTE_CHANNEL_SHIFT);

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_selects_every_differential_pair(void)
{
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	for (uint8_t idx = 0; idx <= 0b111; idx++) {
		ltc2309_t* ltc = create_ltc(true);
		fake_i2c_read_answers(reading, 2);

		uint32_t writes_before = fake_i2c_write_op.calls;
		int16_t value;
		TEST_ASSERT_EQUAL_INT(
			0,
			ltc2309_read_signed(
				ltc, (LTC2309_DIFF_INPUT)idx, &value, 1000));

		if (idx == 0) {
			// P0_N1 (index 0) is already the cmd byte cmd starts at.
			assert_skipped_write(writes_before);
		} else {
			assert_wrote_cmd(
				writes_before,
				(uint8_t)(idx << COMMAND_BYTE_CHANNEL_SHIFT));
		}

		destroy_ltc(ltc);
	}
}

void test_ltc2309_read_signed_when_protected_locks_and_unlocks(void)
{
	ltc2309_t* ltc = create_protected_ltc(true);
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 1000, 0);
	fake_i2c_read_answers(reading, 2);
	plc_resource_unlock_ExpectAndReturn(TEST_RESOURCE, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 1000));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_signed_fails_immediately_when_the_lock_times_out(void)
{
	ltc2309_t* ltc = create_protected_ltc(true);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 50, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 50));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_signed_fails_when_writing_the_command_byte_fails(void)
{
	ltc2309_t* ltc = create_ltc(true);

	fake_i2c_write_op.retval = -1;

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_signed(ltc, LTC2309_P2_N3, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_fails_when_reading_the_conversion_fails(void)
{
	ltc2309_t* ltc = create_ltc(true);

	fake_i2c_read_op.retval = -1;

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_fails_with_erange_for_a_bad_low_nibble(void)
{
	ltc2309_t* ltc = create_ltc(true);
	static const uint8_t reading[2] = { 0x0F, 0xF1 }; // low nibble != 0
	fake_i2c_read_answers(reading, 2);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_fails_with_efault_for_null_read_value(void)
{
	ltc2309_t* ltc = create_ltc(true);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_signed(ltc, LTC2309_P0_N1, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_fails_with_einval_when_device_is_unipolar(void)
{
	ltc2309_t* ltc = create_ltc(false);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_ltc(ltc);
}

void test_ltc2309_read_signed_unlocks_when_bip_mismatched_and_protected(void)
{
	// Regression test: the EINVAL bip-mismatch branch must release the
	// lock it already took before returning, not bail out early.
	ltc2309_t* ltc = create_protected_ltc(false);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 1000, 0);
	plc_resource_unlock_ExpectAndReturn(TEST_RESOURCE, 0);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_signed(ltc, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_protected_ltc(ltc);
}

/* ------------------------ ltc2309_read_unsigned ---------------------------- */

void test_ltc2309_read_unsigned_returns_a_positive_reading(void)
{
	ltc2309_t* ltc = create_ltc(false); // starts unipolar (cmd=0x08)
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint32_t writes_before = fake_i2c_write_op.calls;
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));

	// SD bit must flip to single-ended: cmd changes even on channel 0.
	assert_wrote_cmd(writes_before, COMMAND_BYTE_SGL | COMMAND_BYTE_UNI);
	TEST_ASSERT_EQUAL_UINT16(255, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_never_sign_extends_the_top_bit(void)
{
	// Straight binary: a top-bit-set code is still a large positive
	// value, unlike read_signed's 2's complement interpretation of the
	// same wire bytes.
	ltc2309_t* ltc = create_ltc(false);
	static const uint8_t reading[2] = { 0xFF, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(4095, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_skips_the_write_on_the_same_channel(void)
{
	ltc2309_t* ltc = create_ltc(false);
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));

	uint32_t writes_before = fake_i2c_write_op.calls;
	fake_i2c_read_answers(reading, 2);
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));

	assert_skipped_write(writes_before);

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_clears_high_channel_bit_on_a_low_channel_switch(
	void)
{
	// Mirror of the read_signed regression above: switching from a
	// high-index channel (CH1, mux_field=4) to a low-index one (CH0,
	// mux_field=0) on the SAME handle must actually clear mux bit 2
	// (0x40) in the command byte, not leave it stuck from the OR-only
	// rebuild.
	ltc2309_t* ltc = create_ltc(false);
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	fake_i2c_read_answers(reading, 2);
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_unsigned(ltc, LTC2309_CH1, &value, 1000));

	fake_i2c_read_answers(reading, 2);
	uint32_t writes_before = fake_i2c_write_op.calls;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));

	assert_wrote_cmd(
		writes_before,
		(uint8_t)(COMMAND_BYTE_SGL | COMMAND_BYTE_UNI |
			  (LTC2309_CH0 << COMMAND_BYTE_CHANNEL_SHIFT)));

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_selects_every_channel(void)
{
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	for (uint8_t idx = 0; idx <= 0b111; idx++) {
		ltc2309_t* ltc = create_ltc(false);
		fake_i2c_read_answers(reading, 2);

		uint32_t writes_before = fake_i2c_write_op.calls;
		uint16_t value;
		TEST_ASSERT_EQUAL_INT(
			0,
			ltc2309_read_unsigned(
				ltc, (LTC2309_INPUT)idx, &value, 1000));

		// SGL|UNI must always be set, so every channel writes, even 0.
		assert_wrote_cmd(
			writes_before,
			(uint8_t)(COMMAND_BYTE_SGL | COMMAND_BYTE_UNI |
				  (idx << COMMAND_BYTE_CHANNEL_SHIFT)));

		destroy_ltc(ltc);
	}
}

void test_ltc2309_read_unsigned_when_protected_locks_and_unlocks(void)
{
	ltc2309_t* ltc = create_protected_ltc(false);
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 1000, 0);
	fake_i2c_read_answers(reading, 2);
	plc_resource_unlock_ExpectAndReturn(TEST_RESOURCE, 0);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_unsigned_fails_immediately_when_the_lock_times_out(void)
{
	ltc2309_t* ltc = create_protected_ltc(false);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 50, -1);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 50));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_unsigned_fails_when_writing_the_command_byte_fails(void)
{
	ltc2309_t* ltc = create_ltc(false);

	fake_i2c_write_op.retval = -1;

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_fails_when_reading_the_conversion_fails(void)
{
	ltc2309_t* ltc = create_ltc(false);

	fake_i2c_read_op.retval = -1;

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_fails_with_erange_for_a_bad_low_nibble(void)
{
	ltc2309_t* ltc = create_ltc(false);
	static const uint8_t reading[2] = { 0x0F, 0xF1 }; // low nibble != 0
	fake_i2c_read_answers(reading, 2);

	errno = 0;
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_fails_with_efault_for_null_read_value(void)
{
	ltc2309_t* ltc = create_ltc(false);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_unsigned(ltc, LTC2309_CH0, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_fails_with_einval_when_device_is_bipolar(void)
{
	// Per the datasheet's Output Data Format: the conversion is only
	// straight binary in unipolar range, so read_unsigned refuses a
	// device initialized with bip=true (LTC2309_IS_BIPOLAR gated check).
	ltc2309_t* ltc = create_ltc(true);

	errno = 0;
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_ltc(ltc);
}

void test_ltc2309_read_unsigned_unlocks_when_bip_mismatched_and_protected(void)
{
	// Regression test: the EINVAL bip-mismatch branch must release the
	// lock it already took before returning, not bail out early. If it
	// doesn't, plc_resource_unlock_ExpectAndReturn below goes unmet and
	// CMock fails this test.
	ltc2309_t* ltc = create_protected_ltc(true);

	plc_resource_lock_ExpectAndReturn(TEST_RESOURCE, 1000, 0);
	plc_resource_unlock_ExpectAndReturn(TEST_RESOURCE, 0);

	errno = 0;
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_read_unsigned(ltc, LTC2309_CH0, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_protected_ltc(ltc);
}
