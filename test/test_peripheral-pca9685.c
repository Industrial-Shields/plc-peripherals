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
 * Tests for src/peripheral-pca9685.c. All of its dependencies are mocked:
 * plc-peripherals-i2c.h (the single-register i2c_write8_8b/i2c_read8_8b
 * calls), plc-peripherals-i2c-hal.h (the bus lookups, and the multi-byte
 * writes and reads of the reset and the outputs, are stubbed by fake-i2c),
 * plc-mutex.h (the mutex pca9685_protect embeds in the handle), and
 * plc-delay.h (fake-delay records the oscillator start-up waits).
 */

#include "unity.h"

#include "mock_plc-mutex.h"

#include "fake-i2c.h"
#include "mock_plc-peripherals-i2c-hal.h"
#include "mock_plc-peripherals-i2c.h"
#include "peripheral-pca9685.h"

#include "mock_plc-delay.h"

#include "fake-delay.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define TEST_I2C FAKE_I2C_IFACE
#define TEST_ADDR ((plc_i2c_addr_t)0x40)
#define TEST_BUS ((uint8_t)3)
#define TEST_SCOPE PLC_MUTEX_SCOPE_PRIVATE

// clang-format off
#define MODE1                                                              0x00
#define MODE2                                                              0x01
#define LED0_ON_L                                                          0x06
#define ALL_LED_ON_L                                                       0xFA
#define ALL_LED_OFF_H                                                      0xFD
#define PRE_SCALE                                                          0xFE

#define MODE1_RESTART                                                  (1 << 7)
#define MODE1_EXTCLK                                                   (1 << 6)
#define MODE1_AI                                                       (1 << 5)
#define MODE1_SLEEP                                                    (1 << 4)
#define MODE1_SUB1                                                     (1 << 3)
#define MODE1_SUB2                                                     (1 << 2)
#define MODE1_SUB3                                                     (1 << 1)
#define MODE1_ALLCALL                                                  (1 << 0)
#define MODE1_POR                                (MODE1_SLEEP | MODE1_ALLCALL)

#define MODE2_INVRT                                                    (1 << 4)
#define MODE2_OUTDRV                                                   (1 << 2)

#define PRE_SCALE_POR                                                      0x1E
#define OSC_STARTUP_US                                                      500
// clang-format on

#define RESET_MODE_LEN 7
#define RESET_ALL_LED_LEN 6
#define OUTPUT_WRITE_LEN 5
#define ALL_OUTPUTS_WRITE_LEN (1 + 4 * PCA9685_NUM_OUTPUTS)
#define ALL_OUTPUTS_READ_LEN (4 * PCA9685_NUM_OUTPUTS)

/*
 * CMock copies a ReturnThruPtr value when the mocked call happens, not when it
 * is queued, so each register needs its own slot for a test to queue reads of
 * different registers.
 */
static uint8_t read_values[256];

static void expect_i2c_read8_8b(uint8_t reg, uint8_t value, int retval)
{
	i2c_read8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, reg, NULL, retval);
	i2c_read8_8b_IgnoreArg_to_read();
	if (retval == 0) {
		read_values[reg] = value;
		i2c_read8_8b_ReturnThruPtr_to_read(&read_values[reg]);
	}
}

static void expect_i2c_write8_8b(uint8_t reg, uint8_t value, int retval)
{
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, reg, value, retval);
}

static uint8_t led_on_l_of(uint8_t index)
{
	return (uint8_t)(LED0_ON_L + 4 * index);
}

// The four LEDn registers the driver writes for a duty cycle.
static void expected_output_regs(uint16_t value, uint8_t regs[4])
{
	if (value == 0) {
		// Full OFF
		regs[0] = 0x00;
		regs[1] = 0x00;
		regs[2] = 0x00;
		regs[3] = 0x10;
	} else if (value == 4096) {
		// Full ON
		regs[0] = 0x00;
		regs[1] = 0x10;
		regs[2] = 0x00;
		regs[3] = 0x00;
	} else {
		// ON at count 0, OFF at count value
		regs[0] = 0x00;
		regs[1] = 0x00;
		regs[2] = (uint8_t)(value & 0xFF);
		regs[3] = (uint8_t)(value >> 8);
	}
}

/*
 * fake-i2c only keeps the last i2c_write, and the reset makes two. This log
 * keeps them all. While writes_are_honest, every write is acknowledged in
 * full, except the one at short_write_at; otherwise the fake's retval is
 * returned.
 */
#define WRITE_LOG_MAX 4

static struct {
	uint8_t bytes[FAKE_I2C_MAX_WIRE_BYTES];
	size_t len;
} write_log[WRITE_LOG_MAX];
static size_t write_log_len;
static bool writes_are_honest;
static int short_write_at;

static void reset_write_log(void)
{
	memset(write_log, 0, sizeof(write_log));
	write_log_len = 0;
	writes_are_honest = false;
	short_write_at = -1;
}

static ssize_t logged_write(const i2c_interface_t* i2c,
			    plc_i2c_addr_t addr,
			    const uint8_t* to_write,
			    uint16_t to_write_len,
			    int num_calls)
{
	const ssize_t retval =
		fake_i2c_write(i2c, addr, to_write, to_write_len, num_calls);
	const size_t index = write_log_len++;

	if (index < WRITE_LOG_MAX) {
		memcpy(write_log[index].bytes, to_write, to_write_len);
		write_log[index].len = to_write_len;
	}

	if (!writes_are_honest) {
		return retval;
	}

	if ((int)index == short_write_at) {
		return (ssize_t)to_write_len - 1;
	}

	return (ssize_t)to_write_len;
}

static void expect_reset(void)
{
	writes_are_honest = true;
	expect_i2c_write8_8b(MODE1, MODE1_POR, 0);
}

static void assert_the_log_starts_with_the_reset_blocks(void)
{
	const uint8_t expected_mode[RESET_MODE_LEN] = {
		MODE1, MODE1_POR | MODE1_AI, MODE2_OUTDRV, 0xE2, 0xE4, 0xE8,
		0xE0,
	};
	// The ALL_LED power-on values (Table 8), then PRE_SCALE
	const uint8_t expected_all_led[RESET_ALL_LED_LEN] = {
		ALL_LED_ON_L, 0x00, 0x10, 0x00, 0x10, PRE_SCALE_POR,
	};

	TEST_ASSERT_TRUE(write_log_len >= 2);
	TEST_ASSERT_EQUAL_UINT(RESET_MODE_LEN, write_log[0].len);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		expected_mode, write_log[0].bytes, RESET_MODE_LEN);
	TEST_ASSERT_EQUAL_UINT(RESET_ALL_LED_LEN, write_log[1].len);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		expected_all_led, write_log[1].bytes, RESET_ALL_LED_LEN);
}

static void expect_shutdown(uint8_t mode1)
{
	expect_i2c_read8_8b(MODE1, mode1, 0);
	expect_i2c_write8_8b(ALL_LED_OFF_H, 0x10, 0);
	expect_i2c_write8_8b(MODE1,
			     (mode1 | MODE1_SLEEP) &
				     ~(MODE1_RESTART | MODE1_EXTCLK),
			     0);
}

static void expect_wake_from_sleep(uint8_t awake_mode1)
{
	expect_i2c_write8_8b(MODE1, awake_mode1, 0);
	expect_i2c_write8_8b(MODE1, awake_mode1 | MODE1_RESTART, 0);
}

static pca9685_t*
init_pca(bool restart, PCA9685_OUTPUT_DRIVE drive, PCA9685_OUTPUT_LOGIC logic)
{
	pca9685_config_t cfg;

	cfg.drive = drive;
	cfg.logic = logic;
	return pca9685_init(TEST_I2C, TEST_ADDR, restart, &cfg);
}

static void expect_init_restart(void)
{
	expect_reset();
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_wake_from_sleep(MODE1_AI | MODE1_ALLCALL);
}

static pca9685_t* create_pca(void)
{
	expect_init_restart();

	pca9685_t* pca =
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED);
	TEST_ASSERT_NOT_NULL(pca);

	fake_i2c_reset();
	fake_i2c_expected_addr = TEST_ADDR;
	fake_i2c_bus = TEST_BUS;
	fake_delay_reset();
	reset_write_log();
	return pca;
}

static void destroy_pca(pca9685_t* pca)
{
	TEST_ASSERT_EQUAL_INT(0, pca9685_deinit(TEST_I2C, pca, false));
}

static pca9685_t* create_protected_pca(void)
{
	pca9685_t* pca = create_pca();

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(0, pca9685_protect(TEST_I2C, pca, TEST_SCOPE));
	return pca;
}

static void expect_mutex_destroyed(int retval)
{
	plc_mutex_static_destroy_ExpectAndReturn(NULL, retval);
	plc_mutex_static_destroy_IgnoreArg_mutex();
}

static void destroy_protected_pca(pca9685_t* pca)
{
	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, pca9685_deinit(TEST_I2C, pca, false));
}

static void expect_mutex_acquired(void)
{
	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
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

// Prescale the frequency fuzz test makes the chip report.
static uint8_t chip_prescale;

static int read_chip_prescale(const i2c_interface_t* i2c,
			      plc_i2c_addr_t addr,
			      uint8_t reg,
			      uint8_t* to_read,
			      int cmock_num_calls)
{
	(void)i2c;
	(void)addr;
	(void)cmock_num_calls;

	TEST_ASSERT_EQUAL_HEX8(PRE_SCALE, reg);
	*to_read = chip_prescale;
	return 0;
}

static uint8_t prescale_of(uint16_t freq_hz)
{
	return (uint8_t)((uint32_t)(25000000.0 / (4096.0 * freq_hz) + 0.5) - 1);
}

/*
 * Expects a frequency change from PRE_SCALE_POR on an awake chip whose MODE1
 * reads as mode1.
 */
static void expect_frequency_change(uint8_t mode1, uint8_t prescale)
{
	const uint8_t asleep = (mode1 | MODE1_SLEEP) &
			       ~(MODE1_RESTART | MODE1_EXTCLK);

	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);
	expect_i2c_read8_8b(MODE1, mode1, 0);
	expect_i2c_write8_8b(MODE1, asleep, 0);
	expect_i2c_write8_8b(PRE_SCALE, prescale, 0);
	expect_wake_from_sleep(asleep & ~MODE1_SLEEP);
}

void setUp(void)
{
	fake_i2c_reset();
	fake_i2c_expected_addr = TEST_ADDR;
	reset_write_log();
	i2c_write_Stub(logged_write);
	i2c_write_then_read_Stub(fake_i2c_write_then_read);

	fake_i2c_bus = TEST_BUS;
	fake_i2c_bus_retval = 0;
	i2c_get_bus_Stub(fake_i2c_get_bus);
	i2c_check_bus_Stub(fake_i2c_check_bus);

	fake_delay_reset();
	plc_delay_us_Stub(fake_plc_delay_us);
}

void tearDown(void)
{
}

/* ---------------------------- pca9685_init -------------------------------- */

void test_pca9685_init_with_restart_resets_then_configures_and_wakes(void)
{
	expect_init_restart();

	errno = 0;
	pca9685_t* pca =
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED);
	TEST_ASSERT_EQUAL_INT(0, errno);

	TEST_ASSERT_NOT_NULL(pca);
	TEST_ASSERT_EQUAL_size_t(2, write_log_len);
	assert_the_log_starts_with_the_reset_blocks();
	TEST_ASSERT_EQUAL_size_t(1, fake_delay.len);
	TEST_ASSERT_EQUAL_UINT32(OSC_STARTUP_US, fake_delay.us[0]);
	destroy_pca(pca);
}

void test_pca9685_init_without_restart_on_an_awake_chip_only_sets_ai(void)
{
	expect_i2c_read8_8b(MODE1, MODE1_ALLCALL, 0);
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);

	pca9685_t* pca =
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED);

	TEST_ASSERT_NOT_NULL(pca);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
	TEST_ASSERT_EQUAL_size_t(0, fake_delay.len);
	destroy_pca(pca);
}

void test_pca9685_init_without_restart_wakes_and_restarts_a_sleeping_chip(void)
{
	// Asleep with outputs held, as after a pca9685_set_frequency cut short
	expect_i2c_read8_8b(MODE1, MODE1_RESTART | MODE1_POR, 0);
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_wake_from_sleep(MODE1_AI | MODE1_ALLCALL);

	pca9685_t* pca =
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED);

	TEST_ASSERT_NOT_NULL(pca);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
	TEST_ASSERT_EQUAL_size_t(1, fake_delay.len);
	TEST_ASSERT_EQUAL_UINT32(OSC_STARTUP_US, fake_delay.us[0]);
	destroy_pca(pca);
}

void test_pca9685_init_without_restart_keeps_the_other_mode1_bits(void)
{
	const uint8_t others = MODE1_SUB1 | MODE1_SUB2 | MODE1_SUB3 |
			       MODE1_ALLCALL;

	expect_i2c_read8_8b(MODE1, others | MODE1_EXTCLK, 0);
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_i2c_write8_8b(MODE1, others | MODE1_AI, 0);

	pca9685_t* pca =
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED);

	TEST_ASSERT_NOT_NULL(pca);
	destroy_pca(pca);
}

void test_pca9685_init_without_restart_on_an_initialized_chip_does_not_wait(void)
{
	// As a previous init leaves it
	expect_i2c_read8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);

	pca9685_t* pca =
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED);

	TEST_ASSERT_NOT_NULL(pca);
	TEST_ASSERT_EQUAL_size_t(0, fake_delay.len);
	destroy_pca(pca);
}

void test_pca9685_init_without_restart_wakes_without_writing_back_extclk(void)
{
	expect_i2c_read8_8b(MODE1, MODE1_EXTCLK | MODE1_POR, 0);
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_wake_from_sleep(MODE1_AI | MODE1_ALLCALL);

	pca9685_t* pca =
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED);

	TEST_ASSERT_NOT_NULL(pca);
	destroy_pca(pca);
}

void test_pca9685_init_without_restart_never_writes_back_a_restart_read(void)
{
	// Awake, but with the outputs still held from a sleep
	expect_i2c_read8_8b(MODE1, MODE1_RESTART | MODE1_ALLCALL, 0);
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);

	pca9685_t* pca =
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED);

	TEST_ASSERT_NOT_NULL(pca);
	destroy_pca(pca);
}

void test_pca9685_init_without_restart_fails_when_setting_ai_fails(void)
{
	expect_i2c_read8_8b(MODE1, MODE1_ALLCALL, 0);
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, -1);

	TEST_ASSERT_NULL(
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
}

void test_pca9685_init_packs_every_output_config_into_mode2(void)
{
	static const struct {
		PCA9685_OUTPUT_DRIVE drive;
		PCA9685_OUTPUT_LOGIC logic;
		uint8_t mode2;
	} cases[] = {
		{ PCA9685_OPEN_DRAIN, PCA9685_NOT_INVERTED, 0x00 },
		{ PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED, MODE2_OUTDRV },
		{ PCA9685_OPEN_DRAIN, PCA9685_INVERTED, MODE2_INVRT },
		{ PCA9685_TOTEM_POLE,
		  PCA9685_INVERTED,
		  MODE2_INVRT | MODE2_OUTDRV },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		// MODE2 is written whole, so OCH and OUTNE are always 0
		expect_i2c_read8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);
		expect_i2c_write8_8b(MODE2, cases[i].mode2, 0);
		expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);

		pca9685_t* pca =
			init_pca(false, cases[i].drive, cases[i].logic);
		TEST_ASSERT_NOT_NULL(pca);
		destroy_pca(pca);
	}
}

void test_pca9685_init_fails_with_efault_for_a_null_config(void)
{
	errno = 0;
	TEST_ASSERT_NULL(pca9685_init(TEST_I2C, TEST_ADDR, true, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_pca9685_init_fails_with_einval_for_an_invalid_drive(void)
{
	errno = 0;
	TEST_ASSERT_NULL(
		init_pca(true, (PCA9685_OUTPUT_DRIVE)2, PCA9685_NOT_INVERTED));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_pca9685_init_fails_with_einval_for_an_invalid_logic(void)
{
	errno = 0;
	TEST_ASSERT_NULL(
		init_pca(true, PCA9685_TOTEM_POLE, (PCA9685_OUTPUT_LOGIC)2));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_pca9685_init_passes_the_bus_lookup_error_through(void)
{
	fake_i2c_bus_retval = -1;
	fake_i2c_bus_errno = ENODEV;

	errno = 0;
	TEST_ASSERT_NULL(
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
	TEST_ASSERT_EQUAL_INT(ENODEV, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_pca9685_init_fails_when_the_reset_mode_block_is_short(void)
{
	writes_are_honest = true;
	short_write_at = 0;

	errno = 0;
	TEST_ASSERT_NULL(
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	TEST_ASSERT_EQUAL_size_t(1, write_log_len);
}

void test_pca9685_init_fails_when_the_reset_all_led_block_is_short(void)
{
	writes_are_honest = true;
	short_write_at = 1;

	errno = 0;
	TEST_ASSERT_NULL(
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	TEST_ASSERT_EQUAL_size_t(2, write_log_len);
}

void test_pca9685_init_fails_when_writing_the_reset_mode1_fails(void)
{
	writes_are_honest = true;
	expect_i2c_write8_8b(MODE1, MODE1_POR, -1);

	TEST_ASSERT_NULL(
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
}

void test_pca9685_init_fails_when_reading_mode1_fails(void)
{
	expect_i2c_read8_8b(MODE1, 0, -1);

	TEST_ASSERT_NULL(
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
}

void test_pca9685_init_fails_when_writing_mode2_fails(void)
{
	expect_i2c_read8_8b(MODE1, MODE1_ALLCALL, 0);
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, -1);

	TEST_ASSERT_NULL(
		init_pca(false, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
}

void test_pca9685_init_fails_when_leaving_sleep_fails(void)
{
	expect_reset();
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, -1);

	TEST_ASSERT_NULL(
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
	TEST_ASSERT_EQUAL_size_t(0, fake_delay.len);
}

void test_pca9685_init_fails_when_the_oscillator_wait_fails(void)
{
	expect_reset();
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);
	fake_delay.fail_at = 0;

	errno = 0;
	TEST_ASSERT_NULL(
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_pca9685_init_fails_when_writing_restart_fails(void)
{
	expect_reset();
	expect_i2c_write8_8b(MODE2, MODE2_OUTDRV, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);
	expect_i2c_write8_8b(
		MODE1, MODE1_RESTART | MODE1_AI | MODE1_ALLCALL, -1);

	TEST_ASSERT_NULL(
		init_pca(true, PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED));
}

/* --------------------------- pca9685_deinit ------------------------------- */

void test_pca9685_deinit_fails_with_efault_for_null_pca(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_deinit(TEST_I2C, NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_deinit_without_restart_just_frees(void)
{
	pca9685_t* pca = create_pca();

	TEST_ASSERT_EQUAL_INT(0, pca9685_deinit(TEST_I2C, pca, false));
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_pca9685_deinit_with_shutdown_turns_the_outputs_off_and_sleeps(void)
{
	pca9685_t* pca = create_pca();

	expect_shutdown(MODE1_AI | MODE1_ALLCALL);
	TEST_ASSERT_EQUAL_INT(0, pca9685_deinit(TEST_I2C, pca, true));
	TEST_ASSERT_EQUAL_size_t(0, write_log_len);
	TEST_ASSERT_EQUAL_size_t(0, fake_delay.len);
}

void test_pca9685_deinit_with_shutdown_keeps_the_other_mode1_bits(void)
{
	pca9685_t* pca = create_pca();

	// Neither a pending RESTART nor EXTCLK is written back
	expect_shutdown(MODE1_RESTART | MODE1_EXTCLK | MODE1_AI | MODE1_SUB2 |
			MODE1_ALLCALL);
	TEST_ASSERT_EQUAL_INT(0, pca9685_deinit(TEST_I2C, pca, true));
}

void test_pca9685_deinit_fails_when_reading_mode1_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(MODE1, 0, -1);
	TEST_ASSERT_EQUAL_INT(-1, pca9685_deinit(TEST_I2C, pca, true));

	destroy_pca(pca);
}

void test_pca9685_deinit_fails_when_turning_the_outputs_off_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);
	expect_i2c_write8_8b(ALL_LED_OFF_H, 0x10, -1);
	TEST_ASSERT_EQUAL_INT(-1, pca9685_deinit(TEST_I2C, pca, true));

	destroy_pca(pca);
}

void test_pca9685_deinit_fails_when_going_to_sleep_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(MODE1, MODE1_AI | MODE1_ALLCALL, 0);
	expect_i2c_write8_8b(ALL_LED_OFF_H, 0x10, 0);
	expect_i2c_write8_8b(MODE1, MODE1_SLEEP | MODE1_AI | MODE1_ALLCALL, -1);
	TEST_ASSERT_EQUAL_INT(-1, pca9685_deinit(TEST_I2C, pca, true));

	destroy_pca(pca);
}

void test_pca9685_rejects_an_interface_for_another_bus(void)
{
	pca9685_t* pca = create_pca();
	const uint16_t values[PCA9685_NUM_OUTPUTS] = { 0 };
	uint16_t all_values[PCA9685_NUM_OUTPUTS];
	uint16_t value;

	fake_i2c_bus = TEST_BUS + 1;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_deinit(TEST_I2C, pca, false));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_frequency(TEST_I2C, pca, 200, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_prescaler(TEST_I2C, pca, 0x1E, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_output(TEST_I2C, pca, 0, 0, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_set_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_output(TEST_I2C, pca, 0, &value, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_get_all_outputs(TEST_I2C, pca, all_values, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_protect(TEST_I2C, pca, TEST_SCOPE));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_transfer_op.calls);

	fake_i2c_bus = TEST_BUS;
	destroy_pca(pca);
}

/* ------------------------ pca9685_static_(de)init ------------------------- */

void test_pca9685_static_init_and_static_deinit_use_the_callers_storage(void)
{
	pca9685_t storage;
	pca9685_config_t cfg = { PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED };

	expect_init_restart();
	TEST_ASSERT_EQUAL_INT(
		0,
		pca9685_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_set_output(TEST_I2C, &storage, 3, 4096, 0));

	TEST_ASSERT_EQUAL_INT(0,
			      pca9685_static_deinit(TEST_I2C, &storage, false));
}

void test_pca9685_static_deinit_with_shutdown_turns_off_and_sleeps(void)
{
	pca9685_t storage;
	pca9685_config_t cfg = { PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED };

	expect_init_restart();
	TEST_ASSERT_EQUAL_INT(
		0,
		pca9685_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	fake_i2c_reset();
	fake_i2c_expected_addr = TEST_ADDR;
	fake_i2c_bus = TEST_BUS;
	reset_write_log();
	expect_shutdown(MODE1_AI | MODE1_ALLCALL);
	TEST_ASSERT_EQUAL_INT(0,
			      pca9685_static_deinit(TEST_I2C, &storage, true));
	TEST_ASSERT_EQUAL_size_t(0, write_log_len);
}

void test_pca9685_static_init_accepts_an_aligned_address_in_a_buffer(void)
{
	static unsigned char buffer[2 * PCA9685_SIZE]
		__attribute__((aligned(PCA9685_ALIGN)));
	pca9685_t* pca = (pca9685_t*)(buffer + PCA9685_ALIGN);
	pca9685_config_t cfg = { PCA9685_OPEN_DRAIN, PCA9685_INVERTED };

	expect_reset();
	expect_i2c_write8_8b(MODE2, MODE2_INVRT, 0);
	expect_wake_from_sleep(MODE1_AI | MODE1_ALLCALL);

	TEST_ASSERT_EQUAL_INT(
		0, pca9685_static_init(TEST_I2C, pca, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(0, pca9685_static_deinit(TEST_I2C, pca, false));
}

void test_pca9685_static_init_fails_with_efault_for_misaligned_storage(void)
{
	static unsigned char buffer[2 * PCA9685_SIZE]
		__attribute__((aligned(PCA9685_ALIGN)));
	pca9685_config_t cfg = { PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED };

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_static_init(TEST_I2C,
						  (pca9685_t*)(buffer + 1),
						  TEST_ADDR,
						  true,
						  &cfg));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_pca9685_static_init_fails_with_efault_for_null_storage(void)
{
	pca9685_config_t cfg = { PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED };

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_static_init(TEST_I2C, NULL, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_static_init_fails_with_efault_for_a_null_config(void)
{
	pca9685_t storage;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		pca9685_static_init(TEST_I2C, &storage, TEST_ADDR, true, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_static_init_fails_with_einval_for_an_invalid_drive(void)
{
	pca9685_t storage;
	pca9685_config_t cfg = { (PCA9685_OUTPUT_DRIVE)2,
				 PCA9685_NOT_INVERTED };

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		pca9685_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_pca9685_static_deinit_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_static_deinit(TEST_I2C, NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

/* ------------------------ pca9685_set_frequency --------------------------- */

void test_pca9685_set_frequency_fails_with_efault_for_null_pca(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_frequency(TEST_I2C, NULL, 200, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_set_frequency_fails_with_einval_below_the_minimum(void)
{
	pca9685_t* pca = create_pca();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		pca9685_set_frequency(
			TEST_I2C, pca, PCA9685_MIN_FREQ_HZ - 1, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_frequency(TEST_I2C, pca, 0, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	destroy_pca(pca);
}

void test_pca9685_set_frequency_fails_with_einval_above_the_maximum(void)
{
	pca9685_t* pca = create_pca();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		pca9685_set_frequency(
			TEST_I2C, pca, PCA9685_MAX_FREQ_HZ + 1, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	destroy_pca(pca);
}

void test_pca9685_set_frequency_sleeps_writes_the_prescale_and_restarts(void)
{
	pca9685_t* pca = create_pca();

	// round(25 MHz / (4096 * 1000 Hz)) - 1 = 5
	expect_frequency_change(MODE1_AI | MODE1_ALLCALL, 5);

	TEST_ASSERT_EQUAL_INT(0, pca9685_set_frequency(TEST_I2C, pca, 1000, 0));
	TEST_ASSERT_EQUAL_size_t(1, fake_delay.len);
	TEST_ASSERT_EQUAL_UINT32(OSC_STARTUP_US, fake_delay.us[0]);
	destroy_pca(pca);
}

void test_pca9685_set_frequency_reaches_both_ends_of_the_range(void)
{
	pca9685_t* pca = create_pca();

	/*
	 * The datasheet's own limits, section 7.3.5, which it rounds: 0xFF
	 * gives 25 MHz / (4096 * 256) = 23.84 Hz ("24 Hz"), and 0x03 gives
	 * 25 MHz / (4096 * 4) = 1525.9 Hz ("1526 Hz").
	 *
	 * Equation 1 for exactly 24 Hz is round(25 MHz / (4096 * 24)) - 1 =
	 * round(254.3) - 1 = 253 (0xFF - 2), that is 24.03 Hz, closer to 24 Hz
	 * than 0xFF is. So no whole frequency in Hz reaches 254 or 255.
	 */
	expect_frequency_change(MODE1_AI | MODE1_ALLCALL, 0xFF - 2);
	TEST_ASSERT_EQUAL_INT(
		0,
		pca9685_set_frequency(TEST_I2C, pca, PCA9685_MIN_FREQ_HZ, 0));

	expect_frequency_change(MODE1_AI | MODE1_ALLCALL, 0x03);
	TEST_ASSERT_EQUAL_INT(
		0,
		pca9685_set_frequency(TEST_I2C, pca, PCA9685_MAX_FREQ_HZ, 0));
	destroy_pca(pca);
}

void test_pca9685_set_frequency_returns_1_when_already_at_that_frequency(void)
{
	pca9685_t* pca = create_pca();

	// 200 Hz is the power-on PRE_SCALE, 0x1E
	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);

	TEST_ASSERT_EQUAL_INT(1, pca9685_set_frequency(TEST_I2C, pca, 200, 0));
	TEST_ASSERT_EQUAL_size_t(0, fake_delay.len);
	destroy_pca(pca);
}

void test_pca9685_set_frequency_keeps_the_other_mode1_bits(void)
{
	pca9685_t* pca = create_pca();

	// Neither a pending RESTART nor EXTCLK is written back
	expect_frequency_change(MODE1_RESTART | MODE1_EXTCLK | MODE1_AI |
					MODE1_SUB2 | MODE1_ALLCALL,
				5);

	TEST_ASSERT_EQUAL_INT(0, pca9685_set_frequency(TEST_I2C, pca, 1000, 0));
	destroy_pca(pca);
}

void test_pca9685_set_frequency_fuzzes_every_frequency_to_its_prescale(void)
{
	pca9685_t* pca = create_pca();

	// The chip already reports the expected prescale, nothing to change.
	i2c_read8_8b_Stub(read_chip_prescale);

	for (uint16_t freq = PCA9685_MIN_FREQ_HZ; freq <= PCA9685_MAX_FREQ_HZ;
	     freq++) {
		chip_prescale = prescale_of(freq);
		TEST_ASSERT_TRUE(chip_prescale >= 3);
		TEST_ASSERT_EQUAL_INT_MESSAGE(
			1,
			pca9685_set_frequency(TEST_I2C, pca, freq, 0),
			"wrong prescale for a frequency");
	}

	destroy_pca(pca);
}

void test_pca9685_set_frequency_fails_when_reading_the_prescale_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(PRE_SCALE, 0, -1);
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_frequency(TEST_I2C, pca, 1000, 0));
	destroy_pca(pca);
}

void test_pca9685_set_frequency_fails_when_reading_mode1_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);
	expect_i2c_read8_8b(MODE1, 0, -1);
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_frequency(TEST_I2C, pca, 1000, 0));
	destroy_pca(pca);
}

void test_pca9685_set_frequency_fails_when_going_to_sleep_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);
	expect_i2c_read8_8b(MODE1, MODE1_AI, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_SLEEP, -1);
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_frequency(TEST_I2C, pca, 1000, 0));
	destroy_pca(pca);
}

void test_pca9685_set_frequency_fails_when_writing_the_prescale_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);
	expect_i2c_read8_8b(MODE1, MODE1_AI, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_SLEEP, 0);
	expect_i2c_write8_8b(PRE_SCALE, 5, -1);
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_frequency(TEST_I2C, pca, 1000, 0));
	destroy_pca(pca);
}

void test_pca9685_set_frequency_fails_when_waking_up_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);
	expect_i2c_read8_8b(MODE1, MODE1_AI, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_SLEEP, 0);
	expect_i2c_write8_8b(PRE_SCALE, 5, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI, -1);
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_frequency(TEST_I2C, pca, 1000, 0));
	destroy_pca(pca);
}

/* ------------------------ pca9685_set_prescaler -------------------------- */

void test_pca9685_set_prescaler_fails_with_efault_for_null_pca(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_prescaler(TEST_I2C, NULL, 0x1E, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_set_prescaler_fails_with_einval_below_the_minimum(void)
{
	pca9685_t* pca = create_pca();

	for (uint8_t prescale = 0; prescale < PCA9685_MIN_PRESCALER;
	     prescale++) {
		errno = 0;
		TEST_ASSERT_EQUAL_INT(
			-1, pca9685_set_prescaler(TEST_I2C, pca, prescale, 0));
		TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	}
	destroy_pca(pca);
}

void test_pca9685_set_prescaler_sleeps_writes_it_as_given_and_restarts(void)
{
	pca9685_t* pca = create_pca();

	expect_frequency_change(MODE1_AI | MODE1_ALLCALL, 0x79);

	TEST_ASSERT_EQUAL_INT(0, pca9685_set_prescaler(TEST_I2C, pca, 0x79, 0));
	TEST_ASSERT_EQUAL_size_t(1, fake_delay.len);
	TEST_ASSERT_EQUAL_UINT32(OSC_STARTUP_US, fake_delay.us[0]);
	destroy_pca(pca);
}

void test_pca9685_set_prescaler_reaches_both_ends_of_the_range(void)
{
	pca9685_t* pca = create_pca();

	expect_frequency_change(MODE1_AI | MODE1_ALLCALL,
				PCA9685_MIN_PRESCALER);
	TEST_ASSERT_EQUAL_INT(
		0,
		pca9685_set_prescaler(TEST_I2C, pca, PCA9685_MIN_PRESCALER, 0));

	expect_frequency_change(MODE1_AI | MODE1_ALLCALL, 0xFF);
	TEST_ASSERT_EQUAL_INT(0, pca9685_set_prescaler(TEST_I2C, pca, 0xFF, 0));
	destroy_pca(pca);
}

void test_pca9685_set_prescaler_returns_1_when_already_at_that_prescale(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);

	TEST_ASSERT_EQUAL_INT(
		1, pca9685_set_prescaler(TEST_I2C, pca, PRE_SCALE_POR, 0));
	TEST_ASSERT_EQUAL_size_t(0, fake_delay.len);
	destroy_pca(pca);
}

void test_pca9685_set_prescaler_fails_when_writing_the_prescale_fails(void)
{
	pca9685_t* pca = create_pca();

	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);
	expect_i2c_read8_8b(MODE1, MODE1_AI, 0);
	expect_i2c_write8_8b(MODE1, MODE1_AI | MODE1_SLEEP, 0);
	expect_i2c_write8_8b(PRE_SCALE, 0x79, -1);
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_prescaler(TEST_I2C, pca, 0x79, 0));
	destroy_pca(pca);
}

/* -------------------------- pca9685_set_output ---------------------------- */

void test_pca9685_set_output_fails_with_efault_for_null_pca(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_output(TEST_I2C, NULL, 0, 0, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_set_output_fails_with_einval_for_out_of_range_index(void)
{
	pca9685_t* pca = create_pca();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		pca9685_set_output(TEST_I2C, pca, PCA9685_NUM_OUTPUTS, 0, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
	destroy_pca(pca);
}

void test_pca9685_set_output_fails_with_einval_above_full_on(void)
{
	pca9685_t* pca = create_pca();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		pca9685_set_output(TEST_I2C, pca, 0, PCA9685_OUTPUT_ON + 1, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
	destroy_pca(pca);
}

void test_pca9685_set_output_turns_it_fully_off_at_0(void)
{
	pca9685_t* pca = create_pca();
	const uint8_t expected[] = { 0x06, 0x00, 0x00, 0x00, 0x10 };

	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_set_output(TEST_I2C, pca, 0, PCA9685_OUTPUT_OFF, 0));
	TEST_ASSERT_EQUAL_UINT(OUTPUT_WRITE_LEN, fake_i2c_write_op.len);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		expected, fake_i2c_write_op.bytes, OUTPUT_WRITE_LEN);
	destroy_pca(pca);
}

void test_pca9685_set_output_turns_it_fully_on_at_4096(void)
{
	pca9685_t* pca = create_pca();
	const uint8_t expected[] = { 0x06, 0x00, 0x10, 0x00, 0x00 };

	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_set_output(TEST_I2C, pca, 0, PCA9685_OUTPUT_ON, 0));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		expected, fake_i2c_write_op.bytes, OUTPUT_WRITE_LEN);
	destroy_pca(pca);
}

void test_pca9685_set_output_turns_it_off_at_the_count_in_between(void)
{
	pca9685_t* pca = create_pca();
	const uint8_t expected_1[] = { 0x06, 0x00, 0x00, 0x01, 0x00 };
	const uint8_t expected_2048[] = { 0x06, 0x00, 0x00, 0x00, 0x08 };
	const uint8_t expected_4095[] = { 0x06, 0x00, 0x00, 0xFF, 0x0F };

	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(0, pca9685_set_output(TEST_I2C, pca, 0, 1, 0));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		expected_1, fake_i2c_write_op.bytes, OUTPUT_WRITE_LEN);

	TEST_ASSERT_EQUAL_INT(0, pca9685_set_output(TEST_I2C, pca, 0, 2048, 0));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		expected_2048, fake_i2c_write_op.bytes, OUTPUT_WRITE_LEN);

	TEST_ASSERT_EQUAL_INT(0, pca9685_set_output(TEST_I2C, pca, 0, 4095, 0));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		expected_4095, fake_i2c_write_op.bytes, OUTPUT_WRITE_LEN);
	destroy_pca(pca);
}

void test_pca9685_set_output_never_writes_equal_on_and_off_counts(void)
{
	pca9685_t* pca = create_pca();

	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	for (uint32_t value = 0; value <= PCA9685_OUTPUT_ON; value++) {
		uint8_t expected[OUTPUT_WRITE_LEN] = { 0x06 };

		TEST_ASSERT_EQUAL_INT(
			0,
			pca9685_set_output(
				TEST_I2C, pca, 0, (uint16_t)value, 0));

		const uint8_t* regs = &fake_i2c_write_op.bytes[1];
		const uint16_t on = (uint16_t)(regs[0] | (regs[1] << 8));
		const uint16_t off = (uint16_t)(regs[2] | (regs[3] << 8));
		TEST_ASSERT_NOT_EQUAL_UINT16(on, off);

		expected_output_regs((uint16_t)value, &expected[1]);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(
			expected, fake_i2c_write_op.bytes, OUTPUT_WRITE_LEN);
	}
	destroy_pca(pca);
}

void test_pca9685_set_output_writes_each_index_to_its_registers(void)
{
	pca9685_t* pca = create_pca();

	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		TEST_ASSERT_EQUAL_INT(
			0, pca9685_set_output(TEST_I2C, pca, i, 100, 0));
		TEST_ASSERT_EQUAL_HEX8(led_on_l_of(i),
				       fake_i2c_write_op.bytes[0]);
	}
	TEST_ASSERT_EQUAL_HEX8(0x42, fake_i2c_write_op.bytes[0]);
	destroy_pca(pca);
}

void test_pca9685_set_output_fails_when_the_write_is_short(void)
{
	pca9685_t* pca = create_pca();

	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN - 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_output(TEST_I2C, pca, 0, 1, 0));
	TEST_ASSERT_EQUAL_INT(EIO, errno);

	// The HAL can report nothing written as success
	fake_i2c_write_op.retval = 0;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_output(TEST_I2C, pca, 0, 1, 0));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	destroy_pca(pca);
}

void test_pca9685_set_output_passes_a_failed_write_through(void)
{
	pca9685_t* pca = create_pca();

	// errno as the HAL would have set it
	fake_i2c_write_op.retval = -1;
	errno = ENXIO;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_output(TEST_I2C, pca, 0, 1, 0));
	TEST_ASSERT_EQUAL_INT(ENXIO, errno);
	destroy_pca(pca);
}

/* ----------------------- pca9685_set_all_outputs -------------------------- */

void test_pca9685_set_all_outputs_fails_with_efault_for_null_pca(void)
{
	const uint16_t values[PCA9685_NUM_OUTPUTS] = { 0 };

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_set_all_outputs(TEST_I2C, NULL, values, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_set_all_outputs_fails_with_efault_for_null_values(void)
{
	pca9685_t* pca = create_pca();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_all_outputs(TEST_I2C, pca, NULL, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	destroy_pca(pca);
}

void test_pca9685_set_all_outputs_writes_nothing_if_any_value_is_invalid(void)
{
	pca9685_t* pca = create_pca();
	uint16_t values[PCA9685_NUM_OUTPUTS] = { 0 };

	values[PCA9685_NUM_OUTPUTS - 1] = PCA9685_OUTPUT_ON + 1;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_set_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
	destroy_pca(pca);
}

void test_pca9685_set_all_outputs_writes_every_output_in_one_write(void)
{
	pca9685_t* pca = create_pca();
	uint16_t values[PCA9685_NUM_OUTPUTS];
	uint8_t expected[ALL_OUTPUTS_WRITE_LEN] = { 0x06 };

	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		values[i] = (uint16_t)(i * 273);
		expected_output_regs(values[i], &expected[1 + 4 * i]);
	}
	values[PCA9685_NUM_OUTPUTS - 1] = PCA9685_OUTPUT_ON;
	expected_output_regs(PCA9685_OUTPUT_ON,
			     &expected[1 + 4 * (PCA9685_NUM_OUTPUTS - 1)]);

	fake_i2c_write_op.retval = ALL_OUTPUTS_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_set_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
	TEST_ASSERT_EQUAL_UINT(ALL_OUTPUTS_WRITE_LEN, fake_i2c_write_op.len);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		expected, fake_i2c_write_op.bytes, ALL_OUTPUTS_WRITE_LEN);
	destroy_pca(pca);
}

void test_pca9685_set_all_outputs_fails_when_the_write_is_short(void)
{
	pca9685_t* pca = create_pca();
	const uint16_t values[PCA9685_NUM_OUTPUTS] = { 0 };

	fake_i2c_write_op.retval = ALL_OUTPUTS_WRITE_LEN - 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_set_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	destroy_pca(pca);
}

/* -------------------------- pca9685_get_output ---------------------------- */

// Arms the fake with the four LEDn registers, ON then OFF.
static void chip_has_counts(uint16_t on, uint16_t off)
{
	const uint8_t regs[4] = {
		(uint8_t)(on & 0xFF),
		(uint8_t)(on >> 8),
		(uint8_t)(off & 0xFF),
		(uint8_t)(off >> 8),
	};

	fake_i2c_answers(regs, sizeof(regs));
}

static uint16_t get_output(pca9685_t* pca, uint8_t index)
{
	uint16_t value = 0xBEEF;

	TEST_ASSERT_EQUAL_INT(
		0, pca9685_get_output(TEST_I2C, pca, index, &value, 0));
	return value;
}

void test_pca9685_get_output_fails_with_efault_for_null_pca(void)
{
	uint16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_output(TEST_I2C, NULL, 0, &value, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_get_output_fails_with_efault_for_null_value(void)
{
	pca9685_t* pca = create_pca();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_output(TEST_I2C, pca, 0, NULL, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	destroy_pca(pca);
}

void test_pca9685_get_output_fails_with_einval_for_out_of_range_index(void)
{
	pca9685_t* pca = create_pca();
	uint16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		pca9685_get_output(
			TEST_I2C, pca, PCA9685_NUM_OUTPUTS, &value, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_transfer_op.calls);
	destroy_pca(pca);
}

void test_pca9685_get_output_reads_the_four_registers_of_the_index(void)
{
	pca9685_t* pca = create_pca();

	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		chip_has_counts(0, 100);
		TEST_ASSERT_EQUAL_UINT16(100, get_output(pca, i));
		TEST_ASSERT_EQUAL_UINT(1, fake_i2c_transfer_op.len);
		TEST_ASSERT_EQUAL_HEX8(led_on_l_of(i),
				       fake_i2c_transfer_op.bytes[0]);
		TEST_ASSERT_EQUAL_UINT(4,
				       fake_i2c_transfer_op.requested_read_len);
	}
	destroy_pca(pca);
}

void test_pca9685_get_output_reads_full_off_as_0(void)
{
	pca9685_t* pca = create_pca();

	chip_has_counts(0x0000, 0x1000);
	TEST_ASSERT_EQUAL_UINT16(PCA9685_OUTPUT_OFF, get_output(pca, 0));
	destroy_pca(pca);
}

void test_pca9685_get_output_reads_full_on_as_4096(void)
{
	pca9685_t* pca = create_pca();

	chip_has_counts(0x1000, 0x0000);
	TEST_ASSERT_EQUAL_UINT16(PCA9685_OUTPUT_ON, get_output(pca, 0));
	destroy_pca(pca);
}

void test_pca9685_get_output_gives_full_off_precedence_over_full_on(void)
{
	pca9685_t* pca = create_pca();

	chip_has_counts(0x1000, 0x1000);
	TEST_ASSERT_EQUAL_UINT16(PCA9685_OUTPUT_OFF, get_output(pca, 0));
	destroy_pca(pca);
}

void test_pca9685_get_output_reads_back_every_value_set_output_writes(void)
{
	pca9685_t* pca = create_pca();

	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	for (uint32_t value = 0; value <= PCA9685_OUTPUT_ON; value++) {
		TEST_ASSERT_EQUAL_INT(
			0,
			pca9685_set_output(
				TEST_I2C, pca, 7, (uint16_t)value, 0));
		fake_i2c_answers(&fake_i2c_write_op.bytes[1], 4);
		TEST_ASSERT_EQUAL_UINT16((uint16_t)value, get_output(pca, 7));
	}
	destroy_pca(pca);
}

void test_pca9685_get_output_fails_when_the_transfer_fails(void)
{
	pca9685_t* pca = create_pca();
	uint16_t value = 0xBEEF;

	chip_has_counts(0, 100);
	fake_i2c_transfer_op.retval = -1;
	// errno as the HAL would have set it
	errno = ENXIO;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_output(TEST_I2C, pca, 0, &value, 0));
	TEST_ASSERT_EQUAL_INT(ENXIO, errno);
	TEST_ASSERT_EQUAL_UINT16(0xBEEF, value);
	destroy_pca(pca);
}

void test_pca9685_get_output_fails_with_eio_when_the_address_is_not_written(void)
{
	pca9685_t* pca = create_pca();
	uint16_t value = 0xBEEF;

	// The HAL can report nothing written as success
	chip_has_counts(0, 100);
	fake_i2c_transfer_op.retval = 0;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_output(TEST_I2C, pca, 0, &value, 0));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	TEST_ASSERT_EQUAL_UINT16(0xBEEF, value);
	destroy_pca(pca);
}

void test_pca9685_get_output_fails_with_eio_when_the_read_is_short(void)
{
	pca9685_t* pca = create_pca();
	uint16_t value = 0xBEEF;

	chip_has_counts(0, 100);
	fake_i2c_transfer_op.reported_read_len = 3;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_output(TEST_I2C, pca, 0, &value, 0));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	TEST_ASSERT_EQUAL_UINT16(0xBEEF, value);
	destroy_pca(pca);
}

/* ------------------------ pca9685_get_all_outputs ------------------------ */

// Arms the fake with all 64 LEDn registers, output 0 first.
static void chip_has_all_counts(const uint16_t on[PCA9685_NUM_OUTPUTS],
				const uint16_t off[PCA9685_NUM_OUTPUTS])
{
	uint8_t regs[ALL_OUTPUTS_READ_LEN];

	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		regs[4 * i] = (uint8_t)(on[i] & 0xFF);
		regs[4 * i + 1] = (uint8_t)(on[i] >> 8);
		regs[4 * i + 2] = (uint8_t)(off[i] & 0xFF);
		regs[4 * i + 3] = (uint8_t)(off[i] >> 8);
	}

	fake_i2c_answers(regs, sizeof(regs));
}

void test_pca9685_get_all_outputs_fails_with_efault_for_null_pca(void)
{
	uint16_t values[PCA9685_NUM_OUTPUTS];

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_get_all_outputs(TEST_I2C, NULL, values, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_get_all_outputs_fails_with_efault_for_null_values(void)
{
	pca9685_t* pca = create_pca();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_all_outputs(TEST_I2C, pca, NULL, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_transfer_op.calls);
	destroy_pca(pca);
}

void test_pca9685_get_all_outputs_reads_every_output_in_one_transfer(void)
{
	pca9685_t* pca = create_pca();
	uint16_t on[PCA9685_NUM_OUTPUTS], off[PCA9685_NUM_OUTPUTS];
	uint16_t expected[PCA9685_NUM_OUTPUTS];
	uint16_t values[PCA9685_NUM_OUTPUTS];

	// Every encoding, spread over the 16 outputs
	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		switch (i % 4) {
		case 0:
			on[i] = 0x0000;
			off[i] = 0x1000;
			expected[i] = PCA9685_OUTPUT_OFF;
			break;
		case 1:
			on[i] = 0x1000;
			off[i] = 0x0000;
			expected[i] = PCA9685_OUTPUT_ON;
			break;
		case 2:
			on[i] = 0x1000;
			off[i] = 0x1000;
			expected[i] = PCA9685_OUTPUT_OFF;
			break;
		default:
			on[i] = 0x0000;
			off[i] = (uint16_t)(i * 200);
			expected[i] = (uint16_t)(i * 200);
			break;
		}
	}
	chip_has_all_counts(on, off);

	TEST_ASSERT_EQUAL_INT(
		0, pca9685_get_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
	TEST_ASSERT_EQUAL_UINT(1, fake_i2c_transfer_op.len);
	TEST_ASSERT_EQUAL_HEX8(LED0_ON_L, fake_i2c_transfer_op.bytes[0]);
	TEST_ASSERT_EQUAL_UINT(ALL_OUTPUTS_READ_LEN,
			       fake_i2c_transfer_op.requested_read_len);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, values, PCA9685_NUM_OUTPUTS);
	destroy_pca(pca);
}

void test_pca9685_get_all_outputs_reads_back_what_set_all_outputs_writes(void)
{
	pca9685_t* pca = create_pca();
	uint16_t written[PCA9685_NUM_OUTPUTS];
	uint16_t values[PCA9685_NUM_OUTPUTS];

	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		written[i] = (uint16_t)(i * 273);
	}
	written[PCA9685_NUM_OUTPUTS - 1] = PCA9685_OUTPUT_ON;

	fake_i2c_write_op.retval = ALL_OUTPUTS_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_set_all_outputs(TEST_I2C, pca, written, 0));
	fake_i2c_answers(&fake_i2c_write_op.bytes[1], ALL_OUTPUTS_READ_LEN);

	TEST_ASSERT_EQUAL_INT(
		0, pca9685_get_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_UINT16_ARRAY(written, values, PCA9685_NUM_OUTPUTS);
	destroy_pca(pca);
}

void test_pca9685_get_all_outputs_fails_when_the_transfer_fails(void)
{
	pca9685_t* pca = create_pca();
	const uint8_t regs[ALL_OUTPUTS_READ_LEN] = { 0 };
	uint16_t values[PCA9685_NUM_OUTPUTS];
	uint16_t untouched[PCA9685_NUM_OUTPUTS];

	memset(values, 0xBE, sizeof(values));
	memcpy(untouched, values, sizeof(values));
	fake_i2c_answers(regs, sizeof(regs));
	fake_i2c_transfer_op.retval = -1;

	// errno as the HAL would have set it
	errno = ENXIO;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_get_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_INT(ENXIO, errno);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(untouched, values, PCA9685_NUM_OUTPUTS);
	destroy_pca(pca);
}

void test_pca9685_get_all_outputs_fails_with_eio_when_the_address_is_not_written(
	void)
{
	pca9685_t* pca = create_pca();
	const uint8_t regs[ALL_OUTPUTS_READ_LEN] = { 0 };
	uint16_t values[PCA9685_NUM_OUTPUTS];
	uint16_t untouched[PCA9685_NUM_OUTPUTS];

	memset(values, 0xBE, sizeof(values));
	memcpy(untouched, values, sizeof(values));
	// The HAL can report nothing written as success
	fake_i2c_answers(regs, sizeof(regs));
	fake_i2c_transfer_op.retval = 0;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_get_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(untouched, values, PCA9685_NUM_OUTPUTS);
	destroy_pca(pca);
}

void test_pca9685_get_all_outputs_fails_with_eio_when_the_read_is_short(void)
{
	pca9685_t* pca = create_pca();
	const uint8_t regs[ALL_OUTPUTS_READ_LEN] = { 0 };
	uint16_t values[PCA9685_NUM_OUTPUTS];
	uint16_t untouched[PCA9685_NUM_OUTPUTS];

	memset(values, 0xBE, sizeof(values));
	memcpy(untouched, values, sizeof(values));
	fake_i2c_answers(regs, sizeof(regs));
	fake_i2c_transfer_op.reported_read_len = ALL_OUTPUTS_READ_LEN - 1;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_get_all_outputs(TEST_I2C, pca, values, 0));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(untouched, values, PCA9685_NUM_OUTPUTS);
	destroy_pca(pca);
}

/* ----------------------- pca9685_(un)protect ------------------------------ */

void test_pca9685_protect_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_protect(TEST_I2C, NULL, TEST_SCOPE));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_protect_creates_the_mutex_with_the_given_scope(void)
{
	pca9685_t* pca = create_pca();

	plc_mutex_static_create_ExpectAndReturn(
		NULL, PLC_MUTEX_SCOPE_SHARED, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_protect(TEST_I2C, pca, PLC_MUTEX_SCOPE_SHARED));

	destroy_protected_pca(pca);
}

void test_pca9685_protect_returns_1_if_already_protected(void)
{
	pca9685_t* pca = create_protected_pca();

	TEST_ASSERT_EQUAL_INT(1, pca9685_protect(TEST_I2C, pca, TEST_SCOPE));

	destroy_protected_pca(pca);
}

void test_pca9685_protect_leaves_the_handle_unprotected_when_it_fails(void)
{
	pca9685_t* pca = create_pca();

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, -1);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(-1, pca9685_protect(TEST_I2C, pca, TEST_SCOPE));

	// No mutex is taken, and none is destroyed
	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(0, pca9685_set_output(TEST_I2C, pca, 0, 1, 1000));
	destroy_pca(pca);
}

void test_pca9685_unprotect_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_unprotect(NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_pca9685_unprotect_returns_1_if_not_protected(void)
{
	pca9685_t* pca = create_pca();

	TEST_ASSERT_EQUAL_INT(1, pca9685_unprotect(pca));
	destroy_pca(pca);
}

void test_pca9685_unprotect_destroys_the_mutex(void)
{
	pca9685_t* pca = create_protected_pca();

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, pca9685_unprotect(pca));

	destroy_pca(pca);
}

void test_pca9685_unprotect_keeps_the_handle_protected_when_it_fails(void)
{
	pca9685_t* pca = create_protected_pca();

	expect_mutex_destroyed(-1);
	TEST_ASSERT_EQUAL_INT(-1, pca9685_unprotect(pca));

	destroy_protected_pca(pca);
}

void test_pca9685_deinit_keeps_the_handle_when_the_mutex_cant_be_destroyed(void)
{
	pca9685_t* pca = create_protected_pca();

	expect_mutex_destroyed(-1);
	TEST_ASSERT_EQUAL_INT(-1, pca9685_deinit(TEST_I2C, pca, true));
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);

	destroy_protected_pca(pca);
}

void test_pca9685_static_deinit_destroys_the_mutex_when_protected(void)
{
	pca9685_t storage;
	pca9685_config_t cfg = { PCA9685_TOTEM_POLE, PCA9685_NOT_INVERTED };

	expect_init_restart();
	TEST_ASSERT_EQUAL_INT(
		0,
		pca9685_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));
	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(0,
			      pca9685_protect(TEST_I2C, &storage, TEST_SCOPE));

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0,
			      pca9685_static_deinit(TEST_I2C, &storage, false));
}

/* --------------------------- protected calls ------------------------------ */

void test_pca9685_get_all_outputs_when_protected_locks_and_unlocks(void)
{
	pca9685_t* pca = create_protected_pca();
	const uint8_t regs[ALL_OUTPUTS_READ_LEN] = { 0 };
	uint16_t values[PCA9685_NUM_OUTPUTS];

	expect_mutex_acquired();
	expect_mutex_released();
	fake_i2c_answers(regs, sizeof(regs));
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_get_all_outputs(TEST_I2C, pca, values, 1000));

	destroy_protected_pca(pca);
}

void test_pca9685_set_frequency_when_protected_locks_and_unlocks(void)
{
	pca9685_t* pca = create_protected_pca();

	expect_mutex_acquired();
	expect_frequency_change(MODE1_AI | MODE1_ALLCALL, 5);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(0,
			      pca9685_set_frequency(TEST_I2C, pca, 1000, 1000));

	expect_mutex_acquired();
	expect_i2c_read8_8b(PRE_SCALE, PRE_SCALE_POR, 0);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(1,
			      pca9685_set_frequency(TEST_I2C, pca, 200, 1000));

	expect_mutex_acquired();
	expect_frequency_change(MODE1_AI | MODE1_ALLCALL, 0x79);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(0,
			      pca9685_set_prescaler(TEST_I2C, pca, 0x79, 1000));

	destroy_protected_pca(pca);
}

void test_pca9685_set_output_when_protected_locks_and_unlocks(void)
{
	pca9685_t* pca = create_protected_pca();

	expect_mutex_acquired();
	expect_mutex_released();
	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(0, pca9685_set_output(TEST_I2C, pca, 0, 1, 1000));

	destroy_protected_pca(pca);
}

void test_pca9685_set_all_outputs_when_protected_locks_and_unlocks(void)
{
	pca9685_t* pca = create_protected_pca();
	const uint16_t values[PCA9685_NUM_OUTPUTS] = { 0 };

	expect_mutex_acquired();
	expect_mutex_released();
	fake_i2c_write_op.retval = ALL_OUTPUTS_WRITE_LEN;
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_set_all_outputs(TEST_I2C, pca, values, 1000));

	destroy_protected_pca(pca);
}

void test_pca9685_get_output_when_protected_locks_and_unlocks(void)
{
	pca9685_t* pca = create_protected_pca();
	uint16_t value;

	expect_mutex_acquired();
	expect_mutex_released();
	chip_has_counts(0, 100);
	TEST_ASSERT_EQUAL_INT(
		0, pca9685_get_output(TEST_I2C, pca, 0, &value, 1000));

	destroy_protected_pca(pca);
}

void test_pca9685_fails_when_the_mutex_cant_be_taken(void)
{
	pca9685_t* pca = create_protected_pca();
	const uint16_t values[PCA9685_NUM_OUTPUTS] = { 0 };
	uint16_t all_values[PCA9685_NUM_OUTPUTS];
	uint16_t value;

	plc_mutex_acquire_Stub(acquire_times_out);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_frequency(TEST_I2C, pca, 1000, 10));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_prescaler(TEST_I2C, pca, 0x79, 10));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_output(TEST_I2C, pca, 0, 1, 10));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_set_all_outputs(TEST_I2C, pca, values, 10));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_output(TEST_I2C, pca, 0, &value, 10));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_get_all_outputs(TEST_I2C, pca, all_values, 10));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_transfer_op.calls);
	destroy_protected_pca(pca);
}

void test_pca9685_goes_on_after_an_owner_died(void)
{
	pca9685_t* pca = create_protected_pca();

	plc_mutex_acquire_Stub(acquire_after_owner_died);
	expect_mutex_released();
	fake_i2c_write_op.retval = OUTPUT_WRITE_LEN;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, pca9685_set_output(TEST_I2C, pca, 0, 1, 10));
	TEST_ASSERT_EQUAL_INT(0, errno);
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);

	destroy_protected_pca(pca);
}

void test_pca9685_releases_the_mutex_when_the_transfer_fails(void)
{
	pca9685_t* pca = create_protected_pca();
	const uint16_t values[PCA9685_NUM_OUTPUTS] = { 0 };
	uint16_t all_values[PCA9685_NUM_OUTPUTS];
	uint16_t value;

	fake_i2c_write_op.retval = -1;
	fake_i2c_transfer_op.retval = -1;

	expect_mutex_acquired();
	expect_i2c_read8_8b(PRE_SCALE, 0, -1);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_frequency(TEST_I2C, pca, 1000, 1000));

	expect_mutex_acquired();
	expect_i2c_read8_8b(PRE_SCALE, 0, -1);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_prescaler(TEST_I2C, pca, 0x79, 1000));

	expect_mutex_acquired();
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_output(TEST_I2C, pca, 0, 1, 1000));

	expect_mutex_acquired();
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_set_all_outputs(TEST_I2C, pca, values, 1000));

	expect_mutex_acquired();
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_get_output(TEST_I2C, pca, 0, &value, 1000));

	expect_mutex_acquired();
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_get_all_outputs(TEST_I2C, pca, all_values, 1000));

	destroy_protected_pca(pca);
}

void test_pca9685_rejects_bad_arguments_without_taking_the_mutex(void)
{
	pca9685_t* pca = create_protected_pca();
	uint16_t values[PCA9685_NUM_OUTPUTS] = { 0 };

	values[0] = PCA9685_OUTPUT_ON + 1;

	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_frequency(TEST_I2C, pca, 0, 10));
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_prescaler(TEST_I2C, pca, 2, 10));
	TEST_ASSERT_EQUAL_INT(-1, pca9685_set_output(TEST_I2C, pca, 16, 1, 10));
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_set_output(TEST_I2C, pca, 0, 4097, 10));
	TEST_ASSERT_EQUAL_INT(
		-1, pca9685_set_all_outputs(TEST_I2C, pca, values, 10));
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_output(TEST_I2C, pca, 0, NULL, 10));
	TEST_ASSERT_EQUAL_INT(-1,
			      pca9685_get_all_outputs(TEST_I2C, pca, NULL, 10));

	destroy_protected_pca(pca);
}
