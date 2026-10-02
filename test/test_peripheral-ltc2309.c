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
 * Tests for src/peripheral-ltc2309.c. All of its dependencies are mocked:
 * plc-peripherals-i2c.h and plc-peripherals-i2c-hal.h (the calls it makes are
 * stubbed by fake-i2c), plc-mutex.h (the mutex ltc2309_protect embeds in the
 * handle), and plc-delay.h (fake-delay records every delay the driver asks
 * for, so the tests check the delays instead of the time a call took).
 */

#include "unity.h"

#include "mock_plc-mutex.h"

#include "fake-i2c.h"
#include "mock_plc-peripherals-i2c-hal.h"
#include "mock_plc-peripherals-i2c.h"
#include "peripheral-ltc2309.h"

#include "mock_plc-delay.h"

#include "fake-delay.h"

#include <errno.h>
#include <stdlib.h>

#define TEST_I2C FAKE_I2C_IFACE
#define TEST_ADDR ((plc_i2c_addr_t)0x08)
#define TEST_BUS ((uint8_t)3)

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

#define TREFWAKE_US 200000
#define CMD_DELAY_US 2

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
static ltc2309_t* create_ltc(void)
{
	fake_i2c_write_op.retval = 1;

	ltc2309_t* ltc = ltc2309_init(TEST_I2C, TEST_ADDR);
	TEST_ASSERT_NOT_NULL(ltc);
	assert_last_write_was(INITIAL_STATE);
	return ltc;
}

static void destroy_ltc(ltc2309_t* ltc)
{
	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(TEST_I2C, ltc, false));
}

#define TEST_SCOPE PLC_MUTEX_SCOPE_PRIVATE

static ltc2309_t* create_protected_ltc(void)
{
	ltc2309_t* ltc = create_ltc();

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(0, ltc2309_protect(TEST_I2C, ltc, TEST_SCOPE));
	return ltc;
}

static void expect_mutex_destroyed(int retval)
{
	plc_mutex_static_destroy_ExpectAndReturn(NULL, retval);
	plc_mutex_static_destroy_IgnoreArg_mutex();
}

static void destroy_protected_ltc(ltc2309_t* ltc)
{
	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(TEST_I2C, ltc, false));
}

static void expect_mutex_released(void)
{
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();
}

static int acquire_after_owner_died(plc_mutex_t* mutex,
				    uint32_t timeout_ms,
				    int cmock_num_calls)
{
	(void)mutex;
	(void)timeout_ms;

	if (cmock_num_calls == 0) {
		errno = EOWNERDEAD;
		return -1;
	}
	return 0;
}

static int
acquire_times_out(plc_mutex_t* mutex, uint32_t timeout_ms, int cmock_num_calls)
{
	(void)mutex;
	(void)timeout_ms;
	(void)cmock_num_calls;

	errno = EBUSY;
	return -1;
}

void setUp(void)
{
	fake_delay_reset();
	plc_delay_us_Stub(fake_plc_delay_us);

	fake_i2c_reset();
	fake_i2c_expected_addr = TEST_ADDR;
	i2c_write_Stub(fake_i2c_write);
	i2c_read_Stub(fake_i2c_read);

	fake_i2c_bus = TEST_BUS;
	fake_i2c_bus_retval = 0;
	i2c_get_bus_Stub(fake_i2c_get_bus);
	i2c_check_bus_Stub(fake_i2c_check_bus);
}

void tearDown(void)
{
}

/* ---------------------------- ltc2309_init -------------------------------- */

void test_ltc2309_init_writes_initial_state_and_waits_trefwake(void)
{
	fake_i2c_write_op.retval = 1;

	ltc2309_t* ltc = ltc2309_init(TEST_I2C, TEST_ADDR);

	TEST_ASSERT_NOT_NULL(ltc);
	assert_last_write_was(INITIAL_STATE);
	TEST_ASSERT_EQUAL_size_t(1, fake_delay.len);
	TEST_ASSERT_EQUAL_UINT32(TREFWAKE_US, fake_delay.us[0]);

	destroy_ltc(ltc);
}

void test_ltc2309_init_fails_with_efault_when_i2c_is_null(void)
{
	fake_i2c_bus_retval = -1;

	errno = 0;
	TEST_ASSERT_NULL(ltc2309_init(NULL, TEST_ADDR));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ltc2309_init_fails_when_the_initial_write_fails(void)
{
	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_NULL(ltc2309_init(TEST_I2C, TEST_ADDR));
}

/* --------------------------- ltc2309_deinit ------------------------------- */

void test_ltc2309_deinit_without_shutdown_just_frees(void)
{
	ltc2309_t* ltc = create_ltc();
	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(TEST_I2C, ltc, false));
}

void test_ltc2309_deinit_with_shutdown_writes_the_shutdown_byte(void)
{
	ltc2309_t* ltc = create_ltc();

	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(TEST_I2C, ltc, true));
	assert_last_write_was(SHUTDOWN);
}

void test_ltc2309_deinit_fails_when_the_shutdown_write_fails(void)
{
	ltc2309_t* ltc = create_ltc();

	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_EQUAL_INT(-1, ltc2309_deinit(TEST_I2C, ltc, true));
	free(ltc); // deinit bailed out before freeing it
}

void test_ltc2309_deinit_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_deinit(TEST_I2C, NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

/* ------------------ ltc2309_static_init / static_deinit ------------------- */

// Rounds arena up to the next LTC2309_ALIGN boundary.
static unsigned char* align_up(unsigned char* arena)
{
	uintptr_t base = (uintptr_t)arena;

	return (unsigned char*)((base + LTC2309_ALIGN - 1) &
				~(uintptr_t)(LTC2309_ALIGN - 1));
}

void test_ltc2309_static_init_and_static_deinit_use_the_callers_storage(void)
{
	// Static storage: if static_deinit tried to free it, glibc would abort.
	static ltc2309_t storage;
	fake_i2c_write_op.retval = 1;

	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_static_init(TEST_I2C, &storage, TEST_ADDR));
	assert_last_write_was(INITIAL_STATE);

	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, &storage, LTC2309_CH0, &value, 0));
	TEST_ASSERT_EQUAL_UINT16(255, value);

	TEST_ASSERT_EQUAL_INT(0,
			      ltc2309_static_deinit(TEST_I2C, &storage, true));
	assert_last_write_was(SHUTDOWN);
}

void test_ltc2309_static_init_accepts_an_aligned_address_in_a_buffer(void)
{
	// The embedded case: a handle carved out of a region the caller owns.
	static unsigned char arena[sizeof(ltc2309_t) + LTC2309_ALIGN];
	ltc2309_t* aligned = (ltc2309_t*)align_up(arena);
	fake_i2c_write_op.retval = 1;

	TEST_ASSERT_EQUAL_INT(0, (uintptr_t)aligned % LTC2309_ALIGN);

	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_static_init(TEST_I2C, aligned, TEST_ADDR));
	TEST_ASSERT_EQUAL_INT(0,
			      ltc2309_static_deinit(TEST_I2C, aligned, false));
}

void test_ltc2309_static_init_fails_with_efault_for_misaligned_storage(void)
{
	// One byte past an aligned address is never aligned (LTC2309_ALIGN > 1).
	static unsigned char arena[sizeof(ltc2309_t) + 2 * LTC2309_ALIGN];
	ltc2309_t* misaligned = (ltc2309_t*)(align_up(arena) + 1);

	TEST_ASSERT_NOT_EQUAL_INT(0, (uintptr_t)misaligned % LTC2309_ALIGN);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_static_init(TEST_I2C, misaligned, TEST_ADDR));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_ltc2309_static_init_fails_with_efault_for_null_storage(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      ltc2309_static_init(TEST_I2C, NULL, TEST_ADDR));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_ltc2309_static_init_fails_when_the_initial_write_fails(void)
{
	static ltc2309_t storage;
	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_EQUAL_INT(
		-1, ltc2309_static_init(TEST_I2C, &storage, TEST_ADDR));
}

void test_ltc2309_static_deinit_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_static_deinit(TEST_I2C, NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ltc2309_static_deinit_fails_with_einval_for_another_bus(void)
{
	static ltc2309_t storage;
	fake_i2c_write_op.retval = 1;

	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_static_init(TEST_I2C, &storage, TEST_ADDR));

	fake_i2c_bus = TEST_BUS + 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      ltc2309_static_deinit(TEST_I2C, &storage, false));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	fake_i2c_bus = TEST_BUS;
	TEST_ASSERT_EQUAL_INT(0,
			      ltc2309_static_deinit(TEST_I2C, &storage, false));
}

/* ------------------------------ bus checks -------------------------------- */

void test_ltc2309_rejects_an_interface_for_another_bus(void)
{
	ltc2309_t* ltc = create_ltc();

	// Same interface pointer, but it now reports a different bus.
	fake_i2c_bus = TEST_BUS + 1;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &(int16_t){ 0 }, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_deinit(TEST_I2C, ltc, false));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	// Back on the right bus, the same handle still works.
	fake_i2c_bus = TEST_BUS;
	destroy_ltc(ltc);
}

/* ------------------------- ltc2309_read_differential_signed ----------------------------- */

void test_ltc2309_read_differential_signed_fails_with_efault_for_null_ltc(void)
{
	int16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, NULL, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ltc2309_read_differential_signed_returns_a_positive_reading(void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint32_t writes_before = fake_i2c_write_op.calls;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));

	// P0_N1 bipolar is the same byte init wrote, and it is sent anyway.
	assert_wrote_cmd(writes_before, INITIAL_STATE);
	TEST_ASSERT_EQUAL_INT16(255, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_signed_returns_the_maximally_negative_reading(
	void)
{
	// Regression test for avoiding an implementation-defined right shift
	// on a negative int16_t: 0x8000 must map to -2048, the most negative
	// value the 12-bit 2's complement conversion result can represent.
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x80, 0x00 };
	fake_i2c_read_answers(reading, 2);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT16(-2048, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_signed_writes_on_a_pair_change(void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint32_t writes_before = fake_i2c_write_op.calls;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P2_N3, &value, 1000));

	assert_wrote_cmd(writes_before,
			 LTC2309_P2_N3 << COMMAND_BYTE_CHANNEL_SHIFT);

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_signed_writes_the_command_again_on_the_same_pair(
	void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P2_N3, &value, 1000));
	const uint8_t cmd = fake_i2c_write_op.bytes[0];

	uint32_t writes_before = fake_i2c_write_op.calls;
	fake_i2c_read_answers(reading, 2);
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P2_N3, &value, 1000));

	assert_wrote_cmd(writes_before, cmd);

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_signed_clears_high_channel_bit_on_a_low_pair_switch(
	void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	fake_i2c_read_answers(reading, 2);
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P1_N0, &value, 1000));

	fake_i2c_read_answers(reading, 2);
	uint32_t writes_before = fake_i2c_write_op.calls;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));

	assert_wrote_cmd(writes_before,
			 LTC2309_P0_N1 << COMMAND_BYTE_CHANNEL_SHIFT);

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_signed_selects_every_differential_pair(void)
{
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	for (uint8_t idx = 0; idx <= 0b111; idx++) {
		ltc2309_t* ltc = create_ltc();
		fake_i2c_read_answers(reading, 2);

		uint32_t writes_before = fake_i2c_write_op.calls;
		int16_t value;
		TEST_ASSERT_EQUAL_INT(0,
				      ltc2309_read_differential_signed(
					      TEST_I2C,
					      ltc,
					      (LTC2309_DIFF_INPUT)idx,
					      &value,
					      1000));

		assert_wrote_cmd(writes_before,
				 (uint8_t)(idx << COMMAND_BYTE_CHANNEL_SHIFT));

		destroy_ltc(ltc);
	}
}

void test_ltc2309_read_differential_signed_fails_when_writing_the_command_byte_fails(
	void)
{
	ltc2309_t* ltc = create_ltc();

	fake_i2c_write_op.retval = -1;

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P2_N3, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_read_sends_the_command_again_after_a_failed_write(void)
{
	// The chip may have got P2_N3 even though the write failed, so going
	// back to P0_N1, the byte init wrote, must send it again.
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	fake_i2c_write_op.retval = -1;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P2_N3, &value, 1000));

	fake_i2c_write_op.retval = 1;
	fake_i2c_read_answers(reading, 2);
	uint32_t writes_before = fake_i2c_write_op.calls;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));
	assert_wrote_cmd(writes_before, INITIAL_STATE);

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_signed_fails_when_reading_the_conversion_fails(
	void)
{
	ltc2309_t* ltc = create_ltc();

	fake_i2c_read_op.retval = -1;

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_signed_fails_with_erange_for_a_bad_low_nibble(
	void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF1 }; // low nibble != 0
	fake_i2c_read_answers(reading, 2);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_signed_fails_with_efault_for_null_read_value(
	void)
{
	ltc2309_t* ltc = create_ltc();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ltc(ltc);
}

/* ------------------------ ltc2309_read_single_ended_unsigned ---------------------------- */

void test_ltc2309_read_single_ended_unsigned_fails_with_efault_for_null_ltc(void)
{
	uint16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, NULL, LTC2309_CH0, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ltc2309_read_single_ended_unsigned_returns_a_positive_reading(void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint32_t writes_before = fake_i2c_write_op.calls;
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));

	// SD bit must flip to single-ended, even on channel 0.
	assert_wrote_cmd(writes_before, COMMAND_BYTE_SGL | COMMAND_BYTE_UNI);
	TEST_ASSERT_EQUAL_UINT16(255, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_single_ended_unsigned_never_sign_extends_the_top_bit(void)
{
	// Straight binary: a top-bit-set code is still a large positive
	// value, unlike read_differential_signed's 2's complement interpretation of the
	// same wire bytes.
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0xFF, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(4095, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_single_ended_unsigned_writes_the_command_again_on_the_same_channel(
	void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));

	uint32_t writes_before = fake_i2c_write_op.calls;
	fake_i2c_read_answers(reading, 2);
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));

	assert_wrote_cmd(writes_before, COMMAND_BYTE_SGL | COMMAND_BYTE_UNI);

	destroy_ltc(ltc);
}

void test_ltc2309_read_single_ended_unsigned_clears_high_channel_bit_on_a_low_channel_switch(
	void)
{
	// Mirror of the read_differential_signed regression above: switching from a
	// high-index channel (CH1, mux_field=4) to a low-index one (CH0,
	// mux_field=0) on the SAME handle must actually clear mux bit 2
	// (0x40) in the command byte, not leave it stuck from the OR-only
	// rebuild.
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	fake_i2c_read_answers(reading, 2);
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH1, &value, 1000));

	fake_i2c_read_answers(reading, 2);
	uint32_t writes_before = fake_i2c_write_op.calls;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));

	assert_wrote_cmd(
		writes_before,
		(uint8_t)(COMMAND_BYTE_SGL | COMMAND_BYTE_UNI |
			  (LTC2309_CH0 << COMMAND_BYTE_CHANNEL_SHIFT)));

	destroy_ltc(ltc);
}

void test_ltc2309_read_single_ended_unsigned_selects_every_channel(void)
{
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	for (uint8_t idx = 0; idx <= 0b111; idx++) {
		ltc2309_t* ltc = create_ltc();
		fake_i2c_read_answers(reading, 2);

		uint32_t writes_before = fake_i2c_write_op.calls;
		uint16_t value;
		TEST_ASSERT_EQUAL_INT(
			0,
			ltc2309_read_single_ended_unsigned(TEST_I2C,
							   ltc,
							   (LTC2309_INPUT)idx,
							   &value,
							   1000));

		// SGL|UNI must always be set, so every channel writes, even 0.
		assert_wrote_cmd(
			writes_before,
			(uint8_t)(COMMAND_BYTE_SGL | COMMAND_BYTE_UNI |
				  (idx << COMMAND_BYTE_CHANNEL_SHIFT)));

		destroy_ltc(ltc);
	}
}

void test_ltc2309_read_single_ended_unsigned_fails_when_writing_the_command_byte_fails(
	void)
{
	ltc2309_t* ltc = create_ltc();

	fake_i2c_write_op.retval = -1;

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_read_single_ended_unsigned_fails_when_reading_the_conversion_fails(
	void)
{
	ltc2309_t* ltc = create_ltc();

	fake_i2c_read_op.retval = -1;

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_read_single_ended_unsigned_fails_with_erange_for_a_bad_low_nibble(
	void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF1 }; // low nibble != 0
	fake_i2c_read_answers(reading, 2);

	errno = 0;
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	destroy_ltc(ltc);
}

void test_ltc2309_read_single_ended_unsigned_fails_with_efault_for_null_read_value(
	void)
{
	ltc2309_t* ltc = create_ltc();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      ltc2309_read_single_ended_unsigned(
				      TEST_I2C, ltc, LTC2309_CH0, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ltc(ltc);
}

/* ------------------- ltc2309_read_single_ended_signed --------------------- */

void test_ltc2309_read_single_ended_signed_writes_sgl_with_bip(void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x80, 0x00 };
	fake_i2c_read_answers(reading, 2);

	uint32_t writes_before = fake_i2c_write_op.calls;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_signed(
			TEST_I2C, ltc, LTC2309_CH3, &value, 1000));

	assert_wrote_cmd(
		writes_before,
		(uint8_t)(COMMAND_BYTE_SGL | COMMAND_BYTE_BIP |
			  (LTC2309_CH3 << COMMAND_BYTE_CHANNEL_SHIFT)));
	TEST_ASSERT_EQUAL_INT16(-2048, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_single_ended_signed_fails_with_efault_for_null_read_value(
	void)
{
	ltc2309_t* ltc = create_ltc();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      ltc2309_read_single_ended_signed(
				      TEST_I2C, ltc, LTC2309_CH0, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ltc(ltc);
}

/* ------------------ ltc2309_read_differential_unsigned -------------------- */

void test_ltc2309_read_differential_unsigned_writes_diff_with_uni(void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0xFF, 0xF0 };
	fake_i2c_read_answers(reading, 2);

	uint32_t writes_before = fake_i2c_write_op.calls;
	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_unsigned(
			TEST_I2C, ltc, LTC2309_P5_N4, &value, 1000));

	assert_wrote_cmd(
		writes_before,
		(uint8_t)(COMMAND_BYTE_DIFF | COMMAND_BYTE_UNI |
			  (LTC2309_P5_N4 << COMMAND_BYTE_CHANNEL_SHIFT)));
	TEST_ASSERT_EQUAL_UINT16(4095, value);

	destroy_ltc(ltc);
}

void test_ltc2309_read_differential_unsigned_fails_with_efault_for_null_read_value(
	void)
{
	ltc2309_t* ltc = create_ltc();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_unsigned(
			TEST_I2C, ltc, LTC2309_P0_N1, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ltc(ltc);
}

/* ---------------------------- range switching ----------------------------- */

void test_ltc2309_switching_the_range_on_the_same_channel_rewrites_the_command(
	void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	fake_i2c_read_answers(reading, 2);
	uint16_t unsigned_value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_unsigned(
			TEST_I2C, ltc, LTC2309_P0_N1, &unsigned_value, 1000));

	fake_i2c_read_answers(reading, 2);
	uint32_t writes_before = fake_i2c_write_op.calls;
	int16_t signed_value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &signed_value, 1000));

	assert_wrote_cmd(writes_before, COMMAND_BYTE_DIFF | COMMAND_BYTE_BIP);

	destroy_ltc(ltc);
}

/* ----------------------- ltc2309_protect / unprotect ---------------------- */

void test_ltc2309_protect_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_protect(TEST_I2C, NULL, TEST_SCOPE));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ltc2309_protect_fails_with_einval_for_another_bus(void)
{
	ltc2309_t* ltc = create_ltc();

	fake_i2c_bus = TEST_BUS + 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_protect(TEST_I2C, ltc, TEST_SCOPE));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	fake_i2c_bus = TEST_BUS;
	destroy_ltc(ltc);
}

void test_ltc2309_protect_creates_the_mutex_with_the_given_scope(void)
{
	ltc2309_t* ltc = create_ltc();

	plc_mutex_static_create_ExpectAndReturn(
		NULL, PLC_MUTEX_SCOPE_SHARED, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(
		0, ltc2309_protect(TEST_I2C, ltc, PLC_MUTEX_SCOPE_SHARED));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_protect_returns_1_if_already_protected(void)
{
	ltc2309_t* ltc = create_protected_ltc();

	// No second plc_mutex_static_create is expected.
	TEST_ASSERT_EQUAL_INT(1, ltc2309_protect(TEST_I2C, ltc, TEST_SCOPE));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_protect_leaves_the_handle_unprotected_when_it_fails(void)
{
	ltc2309_t* ltc = create_ltc();

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, -1);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_protect(TEST_I2C, ltc, TEST_SCOPE));

	// No plc_mutex_acquire/release is expected: the handle isn't protected.
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_unprotect_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_unprotect(NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ltc2309_unprotect_returns_1_if_not_protected(void)
{
	ltc2309_t* ltc = create_ltc();

	TEST_ASSERT_EQUAL_INT(1, ltc2309_unprotect(ltc));

	destroy_ltc(ltc);
}

void test_ltc2309_unprotect_destroys_the_mutex(void)
{
	ltc2309_t* ltc = create_protected_ltc();

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, ltc2309_unprotect(ltc));

	// No plc_mutex_acquire/release is expected anymore.
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));

	destroy_ltc(ltc);
}

void test_ltc2309_unprotect_keeps_the_handle_protected_when_it_fails(void)
{
	ltc2309_t* ltc = create_protected_ltc();

	expect_mutex_destroyed(-1);
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_unprotect(ltc));

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_mutex_released();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	fake_i2c_read_answers(reading, 2);
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_deinit_destroys_the_mutex_when_protected(void)
{
	ltc2309_t* ltc = create_protected_ltc();

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, ltc2309_deinit(TEST_I2C, ltc, false));
}

void test_ltc2309_deinit_keeps_the_handle_when_the_mutex_cant_be_destroyed(void)
{
	ltc2309_t* ltc = create_protected_ltc();

	uint32_t writes_before = fake_i2c_write_op.calls;
	expect_mutex_destroyed(-1);
	TEST_ASSERT_EQUAL_INT(-1, ltc2309_deinit(TEST_I2C, ltc, true));
	assert_skipped_write(writes_before);

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_fails_when_the_mutex_cant_be_taken(void)
{
	ltc2309_t* ltc = create_protected_ltc();

	// No I2C transfer and no release are expected.
	plc_mutex_acquire_Stub(acquire_times_out);
	uint32_t writes_before = fake_i2c_write_op.calls;

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P2_N3, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	assert_skipped_write(writes_before);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_read_op.calls);

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_sends_the_command_after_an_owner_died(void)
{
	ltc2309_t* ltc = create_protected_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	plc_mutex_acquire_Stub(acquire_after_owner_died);
	fake_i2c_read_answers(reading, 2);
	expect_mutex_released();

	// P0_N1 bipolar is the byte init wrote, and it is sent anyway.
	uint32_t writes_before = fake_i2c_write_op.calls;
	int16_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);
	assert_wrote_cmd(writes_before, INITIAL_STATE);
	TEST_ASSERT_EQUAL_INT16(255, value);

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_sends_the_command_again_after_a_failed_recovery(void)
{
	ltc2309_t* ltc = create_protected_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	fake_i2c_write_op.retval = -1;
	expect_mutex_released();

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_read_op.calls);

	// The next read sends the command too.
	fake_i2c_write_op.retval = 1;
	fake_i2c_read_answers(reading, 2);
	expect_mutex_released();

	uint32_t writes_before = fake_i2c_write_op.calls;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));
	assert_wrote_cmd(writes_before, INITIAL_STATE);

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_differential_signed_when_protected_locks_and_unlocks(void)
{
	ltc2309_t* ltc = create_protected_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	fake_i2c_read_answers(reading, 2);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P0_N1, &value, 1000));

	destroy_protected_ltc(ltc);
}

void test_ltc2309_read_single_ended_unsigned_when_protected_locks_and_unlocks(
	void)
{
	ltc2309_t* ltc = create_protected_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	fake_i2c_read_answers(reading, 2);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ltc2309_read_single_ended_unsigned(
			TEST_I2C, ltc, LTC2309_CH0, &value, 1000));

	destroy_protected_ltc(ltc);
}

/* ------------------------- fresh conversions ---------------------------- */

/*
 * A model of when the LTC2309 converts (datasheet, "Continuous Read"). A
 * conversion only starts at a STOP: the one ending a command write, on the
 * input that command selects, and the one ending a read, on the same input
 * again. A read returns the last conversion, not one of the input right now.
 *
 * chip_input_code is what the selected input is at, as a 12-bit code, and a
 * test changes it between reads.
 */
static uint8_t chip_cmd;
static uint16_t chip_input_code;
static uint16_t chip_conversion;

static void chip_converts(void)
{
	// Left-justified 12-bit result, the 4 low bits always 0
	chip_conversion = (uint16_t)(chip_input_code << 4);
}

static ssize_t chip_write(const i2c_interface_t* i2c,
			  plc_i2c_addr_t addr,
			  const uint8_t* to_write,
			  uint16_t to_write_len,
			  int num_calls)
{
	(void)i2c;
	(void)num_calls;

	TEST_ASSERT_EQUAL_HEX16(TEST_ADDR, addr);
	TEST_ASSERT_EQUAL_UINT16(1, to_write_len);

	chip_cmd = to_write[0];
	chip_converts();
	return 1;
}

static ssize_t chip_read(const i2c_interface_t* i2c,
			 plc_i2c_addr_t addr,
			 uint8_t* to_read,
			 uint16_t to_read_len,
			 int num_calls)
{
	(void)i2c;
	(void)num_calls;

	TEST_ASSERT_EQUAL_HEX16(TEST_ADDR, addr);
	TEST_ASSERT_EQUAL_UINT16(2, to_read_len);

	to_read[0] = (uint8_t)(chip_conversion >> 8);
	to_read[1] = (uint8_t)(chip_conversion & 0xFF);

	// The STOP ending the read starts the next conversion
	chip_converts();
	return 2;
}

void test_ltc2309_read_again_of_the_same_input_is_a_fresh_conversion(void)
{
	/*
	 * regressions-3x.md D27, and examples/LTC2309_StaleRead on hardware:
	 * a read of the same input as the previous one must not return the
	 * conversion that read started, but one of the input as it is now.
	 */
	ltc2309_t* ltc = create_ltc();
	const uint8_t ch4_cmd =
		(uint8_t)(LTC2309_CH4 << COMMAND_BYTE_CHANNEL_SHIFT) |
		COMMAND_BYTE_SGL | COMMAND_BYTE_UNI;
	uint16_t value;

	i2c_write_Stub(chip_write);
	i2c_read_Stub(chip_read);

	chip_input_code = 3300;
	TEST_ASSERT_EQUAL_INT(0,
			      ltc2309_read_single_ended_unsigned(
				      TEST_I2C, ltc, LTC2309_CH4, &value, 0));
	TEST_ASSERT_EQUAL_HEX8(ch4_cmd, chip_cmd);
	TEST_ASSERT_EQUAL_UINT16(3300, value);

	// The input changes after the read, as when a wire is moved
	chip_input_code = 0;
	TEST_ASSERT_EQUAL_INT(0,
			      ltc2309_read_single_ended_unsigned(
				      TEST_I2C, ltc, LTC2309_CH4, &value, 0));
	TEST_ASSERT_EQUAL_UINT16(0, value);

	chip_input_code = 3300;
	TEST_ASSERT_EQUAL_INT(0,
			      ltc2309_read_single_ended_unsigned(
				      TEST_I2C, ltc, LTC2309_CH4, &value, 0));
	TEST_ASSERT_EQUAL_UINT16(3300, value);

	destroy_ltc(ltc);
}

/* ---------------------------- plc_delay_us -------------------------------- */

void test_ltc2309_read_waits_the_conversion_after_every_command(void)
{
	ltc2309_t* ltc = create_ltc();
	static const uint8_t reading[2] = { 0x0F, 0xF0 };
	int16_t value;

	// Twice the same input, then another one: each read waits.
	for (int i = 0; i < 3; i++) {
		fake_i2c_read_answers(reading, 2);
		fake_delay_reset();
		TEST_ASSERT_EQUAL_INT(
			0,
			ltc2309_read_differential_signed(TEST_I2C,
							 ltc,
							 i < 2 ? LTC2309_P0_N1 :
								 LTC2309_P2_N3,
							 &value,
							 1000));
		TEST_ASSERT_EQUAL_size_t(1, fake_delay.len);
		TEST_ASSERT_EQUAL_UINT32(CMD_DELAY_US, fake_delay.us[0]);
	}

	destroy_ltc(ltc);
}

void test_ltc2309_init_fails_when_waiting_trefwake_fails(void)
{
	fake_i2c_write_op.retval = 1;
	fake_delay.fail_at = 0;

	errno = 0;
	TEST_ASSERT_NULL(ltc2309_init(TEST_I2C, TEST_ADDR));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_ltc2309_read_fails_without_reading_when_the_delay_fails(void)
{
	ltc2309_t* ltc = create_ltc();

	fake_delay_reset();
	fake_delay.fail_at = 0;
	uint32_t reads_before = fake_i2c_read_op.calls;

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ltc2309_read_differential_signed(
			TEST_I2C, ltc, LTC2309_P2_N3, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(reads_before, fake_i2c_read_op.calls);

	destroy_ltc(ltc);
}
