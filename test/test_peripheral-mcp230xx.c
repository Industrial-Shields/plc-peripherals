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
 * Tests for src/peripheral-mcp230xx.c. All of its dependencies are mocked:
 * plc-peripherals-i2c.h (the register-level i2c_write8_8b/i2c_read8_8b calls)
 * and plc-peripherals-i2c-hal.h (the bus lookups, and the reset, which writes
 * the whole register block in a single i2c_write, are stubbed by fake-i2c),
 * and plc-mutex.h (the mutex mcp230xx_protect embeds in the handle).
 */

#include "unity.h"

#include "mock_plc-mutex.h"

#include "fake-i2c.h"
#include "mock_plc-peripherals-i2c-hal.h"
#include "mock_plc-peripherals-i2c.h"
#include "peripheral-mcp230xx.h"

#include <errno.h>
#include <stdlib.h>

#define TEST_I2C FAKE_I2C_IFACE
#define TEST_ADDR ((plc_i2c_addr_t)0x20)
#define TEST_BUS ((uint8_t)3)

// clang-format off
#define IODIR_008                                                          0x00
#define IPOL_008                                                           0x01
#define IOCON_008                                                          0x05
#define GPINTEN_008                                                        0x02
#define DEFVAL_008                                                         0x03
#define INTCON_008                                                         0x04
#define INTF_008                                                           0x07
#define INTCAP_008                                                         0x08
#define GPPU_008                                                           0x06
#define GPIO_008                                                           0x09
#define OLAT_008                                                           0x0A

#define IODIR_A_017                                                        0x00
#define IODIR_B_017                                                        0x01
#define IPOL_A_017                                                         0x02
#define IPOL_B_017                                                         0x03
#define IOCON_A_017                                                        0x0A
#define IOCON_B_017                                                        0x0B
// IOCON's address with IOCON.BANK = 1.
#define IOCON_BANK_1_017                                                   0x05
#define GPINTEN_B_017                                                      0x05
#define DEFVAL_B_017                                                       0x07
#define INTCON_B_017                                                       0x09
#define INTF_A_017                                                         0x0E
#define INTF_B_017                                                         0x0F
#define INTCAP_B_017                                                       0x11
#define GPPU_A_017                                                         0x0C
#define GPPU_B_017                                                         0x0D
#define GPIO_A_017                                                         0x12
#define GPIO_B_017                                                         0x13
#define OLAT_A_017                                                         0x14
#define OLAT_B_017                                                         0x15

#define IOCON_REG_BANK                                                (1 << 7)
#define IOCON_REG_MIRROR                                              (1 << 6)
#define IOCON_REG_SEQOP                                               (1 << 5)
#define IOCON_REG_DISSLW                                              (1 << 4)
#define IOCON_REG_HAEN                                                (1 << 3)
#define IOCON_REG_ODR                                                 (1 << 2)
#define IOCON_REG_INTPOL                                              (1 << 1)
// IOCON bits no cfg field sets: bit 7, SEQOP, HAEN and bit 0.
#define IOCON_REG_NOT_IN_CFG                                              0xA9
// clang-format on

#define RESET_LEN_008 12
#define RESET_LEN_017 23

/*
 * CMock copies a ReturnThruPtr value when the mocked call happens, not when it
 * is queued, so each register needs its own slot for a test to queue reads of
 * different registers.
 */
static uint8_t read_values[OLAT_B_017 + 1];

static void expect_i2c_read8_8b(uint8_t reg, uint8_t value, int retval)
{
	i2c_read8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, reg, NULL, retval);
	i2c_read8_8b_IgnoreArg_to_read();
	if (retval == 0) {
		read_values[reg] = value;
		i2c_read8_8b_ReturnThruPtr_to_read(&read_values[reg]);
	}
}

static void expect_i2c_read8_16b(uint8_t reg, uint16_t value, int retval)
{
	static uint16_t read_value;

	i2c_read8_16b_ExpectAndReturn(TEST_I2C, TEST_ADDR, reg, NULL, retval);
	i2c_read8_16b_IgnoreArg_to_read();
	if (retval == 0) {
		read_value = value;
		i2c_read8_16b_ReturnThruPtr_to_read(&read_value);
	}
}

// Expects the GPIO read every init ends with. Bit i is GPIO i.
static void
expect_init_gpio_read(MCP230XX_TYPE type, uint16_t gpios, int retval)
{
	if (type == MCP230XX_017) {
		// GPIOA is read first, into the high byte
		expect_i2c_read8_16b(GPIO_A_017,
				     (uint16_t)((gpios << 8) | (gpios >> 8)),
				     retval);
	} else {
		expect_i2c_read8_8b(GPIO_008, (uint8_t)gpios, retval);
	}
}

static uint8_t iocon_addr_of(MCP230XX_TYPE type)
{
	return type == MCP230XX_017 ? IOCON_A_017 : IOCON_008;
}

/*
 * IOCON value mcp230xx_init(restart=true) writes for the fixtures below: slew
 * rate control left enabled (DISSLW=0), active-driver INT (ODR=0), active-low
 * INT (INTPOL=0), no mirroring (MIRROR=0). The MCP23017 also keeps HAEN set.
 */
static uint8_t init_restart_iocon_of(MCP230XX_TYPE type)
{
	return type == MCP230XX_017 ? IOCON_REG_HAEN : 0x00;
}

/*
 * Expects the IOCON reads every init starts with. On the MCP23017, IOCON is
 * read through 0x0B first, so iocon must be non-zero there: IOCON.BANK is then
 * already 0, and no canary is written.
 */
static void expect_init_iocon_reads(MCP230XX_TYPE type, uint8_t iocon)
{
	if (type == MCP230XX_017) {
		expect_i2c_read8_8b(IOCON_B_017, iocon, 0);
	}
	expect_i2c_read8_8b(iocon_addr_of(type), iocon, 0);
}

static size_t reset_len_of(MCP230XX_TYPE type)
{
	return type == MCP230XX_017 ? RESET_LEN_017 : RESET_LEN_008;
}

// Expects a restart=true init up to the reset: the IOCON reads, SEQOP cleared.
static void expect_init_restart(MCP230XX_TYPE type, uint8_t iocon)
{
	expect_init_iocon_reads(type, iocon);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C,
				      TEST_ADDR,
				      iocon_addr_of(type),
				      iocon & ~IOCON_REG_SEQOP,
				      0);
	fake_i2c_write_op.retval = (ssize_t)reset_len_of(type);
}

// Asserts the bytes the fake last saw are a full reset block.
static void assert_last_write_was_a_reset_block(MCP230XX_TYPE type)
{
	static const uint8_t expected_008[RESET_LEN_008] = {
		IODIR_008, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};
	static const uint8_t expected_017[RESET_LEN_017] = {
		IODIR_A_017, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,	     0,	   0,	 0, 0, 0, 0, 0, 0, 0, 0,
	};

	TEST_ASSERT_EQUAL_UINT(reset_len_of(type), fake_i2c_write_op.len);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(type == MCP230XX_017 ? expected_017 :
							     expected_008,
				      fake_i2c_write_op.bytes,
				      reset_len_of(type));
}

// Calls mcp230xx_init on the test bus and address, filling in the config.
static mcp230xx_t* init_mcp(bool restart,
			    MCP230XX_TYPE type,
			    bool disable_slew_rate,
			    MCP230XX_INT_TYPE int_type,
			    MCP230XX_INT_POLARITY int_pol,
			    MCP230XX_MIRROR_INT mirror)
{
	mcp230xx_config_t cfg;

	cfg.type = type;
	cfg.disable_slew_rate = disable_slew_rate;
	cfg.int_type = int_type;
	cfg.int_pol = int_pol;
	cfg.mirror = mirror;
	return mcp230xx_init(TEST_I2C, TEST_ADDR, restart, &cfg);
}

// Creates and initializes an mcp230xx_t via restart=true, IOCON bits all clear.
static mcp230xx_t* create_mcp(MCP230XX_TYPE type)
{
	expect_init_restart(type, init_restart_iocon_of(type));
	i2c_write8_8b_ExpectAndReturn(TEST_I2C,
				      TEST_ADDR,
				      iocon_addr_of(type),
				      init_restart_iocon_of(type),
				      0);
	expect_init_gpio_read(type, 0, 0);

	mcp230xx_t* mcp = init_mcp(true,
				   type,
				   false,
				   MCP230XX_ACTIVE_DRIVER_INT,
				   MCP230XX_INT_ACTIVE_LOW,
				   MCP230XX_NO_MIRRORED_INT);
	TEST_ASSERT_NOT_NULL(mcp);
	return mcp;
}

static void destroy_mcp(mcp230xx_t* mcp)
{
	TEST_ASSERT_EQUAL_INT(0, mcp230xx_deinit(TEST_I2C, mcp, false));
}

#define TEST_SCOPE PLC_MUTEX_SCOPE_PRIVATE

static mcp230xx_t* create_protected_mcp(MCP230XX_TYPE type)
{
	mcp230xx_t* mcp = create_mcp(type);

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(0, mcp230xx_protect(TEST_I2C, mcp, TEST_SCOPE));
	return mcp;
}

static void expect_mutex_destroyed(int retval)
{
	plc_mutex_static_destroy_ExpectAndReturn(NULL, retval);
	plc_mutex_static_destroy_IgnoreArg_mutex();
}

static void destroy_protected_mcp(mcp230xx_t* mcp)
{
	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, mcp230xx_deinit(TEST_I2C, mcp, false));
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

void setUp(void)
{
	fake_i2c_reset();
	fake_i2c_expected_addr = TEST_ADDR;
	i2c_write_Stub(fake_i2c_write);

	fake_i2c_bus = TEST_BUS;
	fake_i2c_bus_retval = 0;
	i2c_get_bus_Stub(fake_i2c_get_bus);
	i2c_check_bus_Stub(fake_i2c_check_bus);
}

void tearDown(void)
{
}

/* --------------------------- mcp230xx_init --------------------------------- */

void test_mcp230xx_init_with_restart_resets_an_mcp23008_then_writes_iocon(void)
{
	expect_init_restart(MCP230XX_008, 0x00);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IOCON_008, 0x00, 0);
	expect_init_gpio_read(MCP230XX_008, 0, 0);

	errno = 0;
	mcp230xx_t* mcp = init_mcp(true,
				   MCP230XX_008,
				   false,
				   MCP230XX_ACTIVE_DRIVER_INT,
				   MCP230XX_INT_ACTIVE_LOW,
				   MCP230XX_NO_MIRRORED_INT);
	TEST_ASSERT_EQUAL_INT(0, errno);

	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
	assert_last_write_was_a_reset_block(MCP230XX_008);
	destroy_mcp(mcp);
}

void test_mcp230xx_init_fails_when_reading_gpio_fails(void)
{
	const MCP230XX_TYPE types[] = { MCP230XX_008, MCP230XX_017 };

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		expect_init_restart(types[i], init_restart_iocon_of(types[i]));
		i2c_write8_8b_ExpectAndReturn(TEST_I2C,
					      TEST_ADDR,
					      iocon_addr_of(types[i]),
					      init_restart_iocon_of(types[i]),
					      0);
		expect_init_gpio_read(types[i], 0, -1);

		TEST_ASSERT_NULL(init_mcp(true,
					  types[i],
					  false,
					  MCP230XX_ACTIVE_DRIVER_INT,
					  MCP230XX_INT_ACTIVE_LOW,
					  MCP230XX_NO_MIRRORED_INT));
	}
}

void test_mcp230xx_init_with_restart_resets_an_mcp23017_then_writes_iocon(void)
{
	expect_init_restart(MCP230XX_017, IOCON_REG_HAEN);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_A_017, IOCON_REG_HAEN, 0);
	expect_init_gpio_read(MCP230XX_017, 0, 0);

	mcp230xx_t* mcp = init_mcp(true,
				   MCP230XX_017,
				   false,
				   MCP230XX_ACTIVE_DRIVER_INT,
				   MCP230XX_INT_ACTIVE_LOW,
				   MCP230XX_NO_MIRRORED_INT);

	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
	assert_last_write_was_a_reset_block(MCP230XX_017);
	destroy_mcp(mcp);
}

void test_mcp230xx_init_without_restart_reads_then_patches_iocon(void)
{
	expect_init_iocon_reads(MCP230XX_008, 0xFF);
	// SEQOP cleared, DISSLW cleared, ODR set (open drain), INTPOL cleared
	// (no polarity with open drain), MIRROR cleared:
	// 0xFF & ~0x20 & ~0x10 & ~0x02 & ~0x40.
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IOCON_008, 0x8D, 0);
	expect_init_gpio_read(MCP230XX_008, 0, 0);

	mcp230xx_t* mcp = init_mcp(false,
				   MCP230XX_008,
				   false,
				   MCP230XX_OPEN_DRAIN_INT,
				   MCP230XX_INT_POLARITY_NONE,
				   MCP230XX_NO_MIRRORED_INT);

	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
	destroy_mcp(mcp);
}

void test_mcp230xx_init_packs_every_configurable_iocon_bit(void)
{
	expect_init_restart(MCP230XX_017, IOCON_REG_HAEN);
	// DISSLW(0x10) | MIRROR(0x40) | INTPOL(0x02) | HAEN(0x08); ODR stays
	// clear because an active-driver INT is what makes a polarity
	// meaningful at all.
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_A_017, 0x5A, 0);
	expect_init_gpio_read(MCP230XX_017, 0, 0);

	mcp230xx_t* mcp = init_mcp(true,
				   MCP230XX_017,
				   true,
				   MCP230XX_ACTIVE_DRIVER_INT,
				   MCP230XX_INT_ACTIVE_HIGH,
				   MCP230XX_MIRRORED_INT);

	TEST_ASSERT_NOT_NULL(mcp);
	destroy_mcp(mcp);
}

void test_mcp230xx_init_fails_with_einval_for_open_drain_with_a_polarity(void)
{
	// An open-drain INT pin has no polarity to configure, so asking for
	// one is a contradiction rather than something to silently ignore.
	errno = 0;
	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_008,
				  false,
				  MCP230XX_OPEN_DRAIN_INT,
				  MCP230XX_INT_ACTIVE_HIGH,
				  MCP230XX_NO_MIRRORED_INT));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_init_fails_with_efault_for_a_null_config(void)
{
	errno = 0;
	TEST_ASSERT_NULL(mcp230xx_init(TEST_I2C, TEST_ADDR, true, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_init_fails_with_einval_for_an_active_driver_without_polarity(
	void)
{
	// An active-driver INT pin always drives some level: pick it.
	errno = 0;
	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_008,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_POLARITY_NONE,
				  MCP230XX_NO_MIRRORED_INT));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_init_fails_with_einval_for_an_invalid_int_type(void)
{
	errno = 0;
	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_008,
				  false,
				  (MCP230XX_INT_TYPE)2,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_NO_MIRRORED_INT));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_init_fails_with_einval_for_a_mirrored_int_on_an_mcp23008(void)
{
	// Only the 23017 has the second INT pin there is to mirror.
	errno = 0;
	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_008,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_MIRRORED_INT));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_init_fails_with_einval_for_an_invalid_type(void)
{
	// It would pass every index check of the GPIO calls.
	errno = 0;
	TEST_ASSERT_NULL(init_mcp(true,
				  (MCP230XX_TYPE)2,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_NO_MIRRORED_INT));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_init_fails_with_einval_for_an_invalid_mirror(void)
{
	// 2 << IOCON_REG_MIRROR_SHIFT would spill into another IOCON bit.
	errno = 0;
	TEST_ASSERT_NULL(init_mcp(false,
				  MCP230XX_017,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  (MCP230XX_MIRROR_INT)2));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_init_fails_when_the_reset_is_short_on_the_wire(void)
{
	// The device acknowledged one byte fewer than the block that was sent.
	expect_init_restart(MCP230XX_008, 0x00);
	fake_i2c_write_op.retval = RESET_LEN_008 - 1;

	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_008,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_NO_MIRRORED_INT));
}

void test_mcp230xx_init_fails_when_the_reset_write_fails(void)
{
	expect_init_restart(MCP230XX_017, IOCON_REG_HAEN);
	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_017,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_NO_MIRRORED_INT));
}

void test_mcp230xx_init_fails_when_reading_the_iocon_reg_fails(void)
{
	expect_i2c_read8_8b(IOCON_008, 0, -1);

	TEST_ASSERT_NULL(init_mcp(false,
				  MCP230XX_008,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_NO_MIRRORED_INT));
}

void test_mcp230xx_init_fails_when_writing_the_iocon_reg_fails(void)
{
	expect_init_restart(MCP230XX_008, 0x00);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IOCON_008, 0x00, -1);

	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_008,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_NO_MIRRORED_INT));
}

/* ------------------- mcp230xx_init: IOCON.BANK and SEQOP ------------------ */

// Calls a restart=false init of an MCP23017 with the default cfg.
static mcp230xx_t* init_mcp23017_without_restart(void)
{
	return init_mcp(false,
			MCP230XX_017,
			false,
			MCP230XX_ACTIVE_DRIVER_INT,
			MCP230XX_INT_ACTIVE_LOW,
			MCP230XX_NO_MIRRORED_INT);
}

void test_mcp230xx_init_skips_the_canary_when_0x0b_reads_non_zero(void)
{
	// No write to 0x0B and no access to 0x05 are expected.
	expect_init_iocon_reads(MCP230XX_017, IOCON_REG_MIRROR);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_A_017, IOCON_REG_HAEN, 0);
	expect_init_gpio_read(MCP230XX_017, 0, 0);

	mcp230xx_t* mcp = init_mcp23017_without_restart();
	TEST_ASSERT_NOT_NULL(mcp);
	destroy_mcp(mcp);
}

struct read_step {
	uint8_t reg;
	uint8_t value;
};

/*
 * The canary test reads 0x0B twice with different values, which one slot per
 * register can't give, so its reads are played back from this list.
 */
static const struct read_step* read_steps;
static size_t read_steps_len;

static int play_read_steps(const i2c_interface_t* i2c,
			   plc_i2c_addr_t addr,
			   uint8_t reg,
			   uint8_t* to_read,
			   int cmock_num_calls)
{
	TEST_ASSERT_EQUAL_PTR(TEST_I2C, i2c);
	TEST_ASSERT_EQUAL_UINT8(TEST_ADDR, addr);
	TEST_ASSERT_LESS_THAN_size_t(read_steps_len, (size_t)cmock_num_calls);
	TEST_ASSERT_EQUAL_HEX8(read_steps[cmock_num_calls].reg, reg);
	*to_read = read_steps[cmock_num_calls].value;
	return 0;
}

void test_mcp230xx_init_keeps_bank_0_when_the_canary_reads_back(void)
{
	static const struct read_step steps[] = {
		{ IOCON_B_017, 0x00 },
		{ IOCON_B_017, IOCON_REG_HAEN },
		{ IOCON_A_017, IOCON_REG_HAEN },
	};
	read_steps = steps;
	read_steps_len = sizeof(steps) / sizeof(steps[0]);
	i2c_read8_8b_Stub(play_read_steps);

	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_B_017, IOCON_REG_HAEN, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_A_017, IOCON_REG_HAEN, 0);
	expect_init_gpio_read(MCP230XX_017, 0, 0);

	mcp230xx_t* mcp = init_mcp23017_without_restart();
	TEST_ASSERT_NOT_NULL(mcp);
	destroy_mcp(mcp);
}

void test_mcp230xx_init_clears_bank_through_0x05_when_the_canary_is_lost(void)
{
	// With IOCON.BANK = 1, 0x0B maps to no register: it reads 0x00.
	expect_i2c_read8_8b(IOCON_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_B_017, IOCON_REG_HAEN, 0);
	expect_i2c_read8_8b(IOCON_B_017, 0x00, 0);
	// Only IOCON.BANK is cleared through 0x05, SEQOP comes after.
	expect_i2c_read8_8b(
		IOCON_BANK_1_017, IOCON_REG_BANK | IOCON_REG_SEQOP, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_BANK_1_017, IOCON_REG_SEQOP, 0);
	expect_i2c_read8_8b(IOCON_A_017, IOCON_REG_SEQOP, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_A_017, IOCON_REG_HAEN, 0);
	expect_init_gpio_read(MCP230XX_017, 0, 0);

	mcp230xx_t* mcp = init_mcp23017_without_restart();
	TEST_ASSERT_NOT_NULL(mcp);
	destroy_mcp(mcp);
}

void test_mcp230xx_init_fails_when_reading_0x0b_fails(void)
{
	expect_i2c_read8_8b(IOCON_B_017, 0, -1);

	TEST_ASSERT_NULL(init_mcp23017_without_restart());
}

void test_mcp230xx_init_fails_when_writing_the_canary_fails(void)
{
	expect_i2c_read8_8b(IOCON_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_B_017, IOCON_REG_HAEN, -1);

	TEST_ASSERT_NULL(init_mcp23017_without_restart());
}

void test_mcp230xx_init_fails_when_reading_the_canary_back_fails(void)
{
	expect_i2c_read8_8b(IOCON_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_B_017, IOCON_REG_HAEN, 0);
	expect_i2c_read8_8b(IOCON_B_017, 0, -1);

	TEST_ASSERT_NULL(init_mcp23017_without_restart());
}

void test_mcp230xx_init_fails_when_reading_iocon_through_0x05_fails(void)
{
	expect_i2c_read8_8b(IOCON_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_B_017, IOCON_REG_HAEN, 0);
	expect_i2c_read8_8b(IOCON_B_017, 0x00, 0);
	expect_i2c_read8_8b(IOCON_BANK_1_017, 0, -1);

	TEST_ASSERT_NULL(init_mcp23017_without_restart());
}

void test_mcp230xx_init_fails_when_clearing_bank_through_0x05_fails(void)
{
	expect_i2c_read8_8b(IOCON_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_B_017, IOCON_REG_HAEN, 0);
	expect_i2c_read8_8b(IOCON_B_017, 0x00, 0);
	expect_i2c_read8_8b(IOCON_BANK_1_017, IOCON_REG_BANK, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_BANK_1_017, 0x00, -1);

	TEST_ASSERT_NULL(init_mcp23017_without_restart());
}

// How many reset writes each i2c_write8_8b call came after.
static uint32_t resets_before_write[2];

static int record_resets_before_write(const i2c_interface_t* i2c,
				      plc_i2c_addr_t addr,
				      uint8_t reg,
				      uint8_t to_write,
				      int cmock_num_calls)
{
	(void)i2c;
	(void)addr;
	(void)reg;
	(void)to_write;

	TEST_ASSERT_LESS_THAN_INT(2, cmock_num_calls);
	resets_before_write[cmock_num_calls] = fake_i2c_write_op.calls;
	return 0;
}

void test_mcp230xx_init_clears_seqop_before_the_reset(void)
{
	expect_init_restart(MCP230XX_017, IOCON_REG_SEQOP | IOCON_REG_HAEN);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_A_017, IOCON_REG_HAEN, 0);
	i2c_write8_8b_AddCallback(record_resets_before_write);
	expect_init_gpio_read(MCP230XX_017, 0, 0);

	mcp230xx_t* mcp = init_mcp(true,
				   MCP230XX_017,
				   false,
				   MCP230XX_ACTIVE_DRIVER_INT,
				   MCP230XX_INT_ACTIVE_LOW,
				   MCP230XX_NO_MIRRORED_INT);
	TEST_ASSERT_NOT_NULL(mcp);
	assert_last_write_was_a_reset_block(MCP230XX_017);
	// The SEQOP write comes before the reset, the final IOCON write after.
	TEST_ASSERT_EQUAL_UINT32(0, resets_before_write[0]);
	TEST_ASSERT_EQUAL_UINT32(1, resets_before_write[1]);
	destroy_mcp(mcp);
}

void test_mcp230xx_init_fails_when_clearing_seqop_before_the_reset_fails(void)
{
	expect_init_iocon_reads(MCP230XX_008, IOCON_REG_SEQOP);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IOCON_008, 0x00, -1);

	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_008,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_NO_MIRRORED_INT));
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

static uint8_t expected_iocon(uint8_t base,
			      bool disable_slew_rate,
			      MCP230XX_INT_TYPE int_type,
			      MCP230XX_INT_POLARITY int_pol,
			      MCP230XX_MIRROR_INT mirror)
{
	uint8_t cfg = base;

	cfg &= (uint8_t)~IOCON_REG_DISSLW;
	if (disable_slew_rate) {
		cfg |= IOCON_REG_DISSLW;
	}

	cfg &= (uint8_t)~IOCON_REG_ODR;
	if (int_type == MCP230XX_OPEN_DRAIN_INT) {
		cfg |= IOCON_REG_ODR;
	}

	// Always cleared first; only an explicit polarity sets it again.
	cfg &= (uint8_t)~IOCON_REG_INTPOL;
	if (int_pol == MCP230XX_INT_ACTIVE_HIGH) {
		cfg |= IOCON_REG_INTPOL;
	}

	cfg &= (uint8_t)~IOCON_REG_MIRROR;
	if (mirror == MCP230XX_MIRRORED_INT) {
		cfg |= IOCON_REG_MIRROR;
	}

	return cfg;
}

static bool config_is_rejected(MCP230XX_TYPE type,
			       MCP230XX_INT_TYPE int_type,
			       MCP230XX_INT_POLARITY int_pol,
			       MCP230XX_MIRROR_INT mirror)
{
	// Open drain takes no polarity, and an active driver needs one.
	return (int_type == MCP230XX_OPEN_DRAIN_INT) !=
		       (int_pol == MCP230XX_INT_POLARITY_NONE) ||
	       (type != MCP230XX_017 && mirror == MCP230XX_MIRRORED_INT);
}

void test_mcp230xx_init_fuzzes_every_configuration_combination(void)
{
	/*
	 * Exhaustively covers every argument combination mcp230xx_init
	 * accepts, across both entry paths (restart=true/false):
	 *
	 * Sweep is (type, restart, disable_slew_rate, int_type, int_pol,
	 * mirror) = 2*2*2*2*3*2 = 96 combinations, each either rejected with
	 * EINVAL before any I/O happens, or packed into one IOCON write. The
	 * bits that must survive a restart=false init get their own sweep
	 * below, since they're independent of what the arguments ask for.
	 */
	for (int type_i = 0; type_i <= 1; type_i++) {
		const MCP230XX_TYPE type = type_i == 0 ? MCP230XX_008 :
							 MCP230XX_017;
		for (int restart = 0; restart <= 1; restart++) {
			for (int slew = 0; slew <= 1; slew++) {
				for (int int_type_i = 0; int_type_i <= 1;
				     int_type_i++) {
					const MCP230XX_INT_TYPE int_type =
						int_type_i == 0 ?
							MCP230XX_ACTIVE_DRIVER_INT :
							MCP230XX_OPEN_DRAIN_INT;
					for (int pol = 0; pol <= 2; pol++) {
						const MCP230XX_INT_POLARITY int_pol =
							(MCP230XX_INT_POLARITY)
								pol;
						for (int mirror_i = 0;
						     mirror_i <= 1;
						     mirror_i++) {
							const MCP230XX_MIRROR_INT mirror =
								mirror_i == 0 ?
									MCP230XX_NO_MIRRORED_INT :
									MCP230XX_MIRRORED_INT;

							if (config_is_rejected(
								    type,
								    int_type,
								    int_pol,
								    mirror)) {
								errno = 0;
								TEST_ASSERT_NULL(init_mcp(
									restart,
									type,
									slew,
									int_type,
									int_pol,
									mirror));
								TEST_ASSERT_EQUAL_INT(
									EINVAL,
									errno);
								continue;
							}

							// IOCON.BANK is 0 before a restart=false init.
							const uint8_t before =
								restart ?
									init_restart_iocon_of(
										type) :
									0x7F;
							const uint8_t base =
								restart ?
									0x00 :
									before &
										~IOCON_REG_SEQOP;
							if (restart) {
								expect_init_restart(
									type,
									before);
							} else {
								expect_init_iocon_reads(
									type,
									before);
							}
							i2c_write8_8b_ExpectAndReturn(
								TEST_I2C,
								TEST_ADDR,
								iocon_addr_of(
									type),
								expected_iocon(
									base,
									slew,
									int_type,
									int_pol,
									mirror) |
									init_restart_iocon_of(
										type),
								0);
							expect_init_gpio_read(
								type, 0, 0);

							mcp230xx_t* mcp =
								init_mcp(
									restart,
									type,
									slew,
									int_type,
									int_pol,
									mirror);
							TEST_ASSERT_NOT_NULL(
								mcp);
							destroy_mcp(mcp);
						}
					}
				}
			}
		}
	}

	/*
	 * Every combination of the IOCON bits no cfg field sets. A
	 * restart=false init keeps them all but SEQOP, which it clears.
	 */
	for (uint8_t untouched = 0; untouched <= IOCON_REG_NOT_IN_CFG;
	     untouched++) {
		if ((untouched & IOCON_REG_NOT_IN_CFG) != untouched) {
			continue; // Not one of those bits.
		}

		expect_init_iocon_reads(MCP230XX_008, untouched);
		i2c_write8_8b_ExpectAndReturn(TEST_I2C,
					      TEST_ADDR,
					      IOCON_008,
					      (untouched & ~IOCON_REG_SEQOP) |
						      IOCON_REG_DISSLW,
					      0);
		expect_init_gpio_read(MCP230XX_008, 0, 0);

		mcp230xx_t* mcp = init_mcp(false,
					   MCP230XX_008,
					   true,
					   MCP230XX_ACTIVE_DRIVER_INT,
					   MCP230XX_INT_ACTIVE_LOW,
					   MCP230XX_NO_MIRRORED_INT);
		TEST_ASSERT_NOT_NULL(mcp);
		destroy_mcp(mcp);
	}
}

/* -------------------------- mcp230xx_deinit -------------------------------- */

void test_mcp230xx_deinit_fails_with_efault_for_null_mcp(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_deinit(TEST_I2C, NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_deinit_without_restart_just_frees(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(0, mcp230xx_deinit(TEST_I2C, mcp, false));

	// Only the one reset create_mcp() itself did.
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
}

void test_mcp230xx_deinit_with_restart_resets_the_registers(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, mcp230xx_deinit(TEST_I2C, mcp, true));
	TEST_ASSERT_EQUAL_INT(0, errno);

	TEST_ASSERT_EQUAL_UINT32(2, fake_i2c_write_op.calls);
	assert_last_write_was_a_reset_block(MCP230XX_017);
}

void test_mcp230xx_deinit_fails_when_the_reset_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_deinit(TEST_I2C, mcp, true));

	free(mcp); // deinit bailed out before freeing it
}

/* ------------------------------ bus checks -------------------------------- */

void test_mcp230xx_rejects_an_interface_for_another_bus(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Same interface pointer, but it now reports a different bus.
	fake_i2c_bus = TEST_BUS + 1;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_gpio(TEST_I2C, mcp, 0, &(uint8_t){ 0 }, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_PULLUP, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_set_output(TEST_I2C, mcp, 0, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_write_gpio(TEST_I2C, mcp, 0, MCP230XX_HIGH, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_write_all(TEST_I2C, mcp, MCP230XX_HIGH, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_pin_mode_all(TEST_I2C, mcp, MCP230XX_INPUT, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_read_all_gpios(TEST_I2C, mcp, &(uint16_t){ 0 }, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 0, MCP230XX_INT_ON_CHANGE, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_disable_interrupt(
			TEST_I2C, mcp, 0, &(mcp230xx_int_regs_t){ 0 }, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_read_interrupt(
			TEST_I2C, mcp, &(mcp230xx_int_regs_t){ 0 }, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_deinit(TEST_I2C, mcp, false));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	// Back on the right bus, the same handle still works.
	fake_i2c_bus = TEST_BUS;
	destroy_mcp(mcp);
}

/* ------------------ mcp230xx_static_init / static_deinit ------------------ */

// The same configuration create_mcp uses: IOCON bits all clear.
static void fill_default_cfg(mcp230xx_config_t* cfg, MCP230XX_TYPE type)
{
	cfg->type = type;
	cfg->disable_slew_rate = false;
	cfg->int_type = MCP230XX_ACTIVE_DRIVER_INT;
	cfg->int_pol = MCP230XX_INT_ACTIVE_LOW;
	cfg->mirror = MCP230XX_NO_MIRRORED_INT;
}

// Rounds arena up to the next MCP230XX_ALIGN boundary.
static unsigned char* align_up(unsigned char* arena)
{
	uintptr_t base = (uintptr_t)arena;

	return (unsigned char*)((base + MCP230XX_ALIGN - 1) &
				~(uintptr_t)(MCP230XX_ALIGN - 1));
}

// Expects the reset and IOCON write of a restart=true static_init.
static void expect_static_init_writes(MCP230XX_TYPE type)
{
	expect_init_restart(type, init_restart_iocon_of(type));
	i2c_write8_8b_ExpectAndReturn(TEST_I2C,
				      TEST_ADDR,
				      iocon_addr_of(type),
				      init_restart_iocon_of(type),
				      0);
	expect_init_gpio_read(type, 0, 0);
}

void test_mcp230xx_static_init_and_static_deinit_use_the_callers_storage(void)
{
	// Static storage: if static_deinit tried to free it, glibc would abort.
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);

	expect_static_init_writes(MCP230XX_008);
	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	expect_i2c_read8_8b(GPIO_008, 0x01, 0);
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, &storage, 0, &value, 0));
	TEST_ASSERT_EQUAL_UINT8(MCP230XX_HIGH, value);

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_static_deinit(TEST_I2C, &storage, false));
}

void test_mcp230xx_static_init_keeps_the_chip_type_of_an_mcp23017(void)
{
	// The type is stored in a single byte: B-side registers must still work.
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_017);

	expect_static_init_writes(MCP230XX_017);
	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	// Pin 12 is bit 4 of port B.
	expect_i2c_read8_8b(GPIO_B_017, 0x10, 0);
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, &storage, 12, &value, 0));
	TEST_ASSERT_EQUAL_UINT8(MCP230XX_HIGH, value);

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_static_deinit(TEST_I2C, &storage, false));
}

void test_mcp230xx_static_deinit_with_restart_resets_the_chip(void)
{
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);

	expect_static_init_writes(MCP230XX_008);
	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	uint32_t writes_before = fake_i2c_write_op.calls;
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_static_deinit(TEST_I2C, &storage, true));
	TEST_ASSERT_EQUAL_UINT32(writes_before + 1, fake_i2c_write_op.calls);
	assert_last_write_was_a_reset_block(MCP230XX_008);
}

void test_mcp230xx_static_init_accepts_an_aligned_address_in_a_buffer(void)
{
	// The embedded case: a handle carved out of a region the caller owns.
	static unsigned char arena[sizeof(mcp230xx_t) + MCP230XX_ALIGN];
	mcp230xx_t* aligned = (mcp230xx_t*)align_up(arena);
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(0, (uintptr_t)aligned % MCP230XX_ALIGN);

	expect_static_init_writes(MCP230XX_008);
	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_static_init(TEST_I2C, aligned, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_static_deinit(TEST_I2C, aligned, false));
}

void test_mcp230xx_static_init_fails_with_efault_for_misaligned_storage(void)
{
	// One byte past an aligned address is never aligned (MCP230XX_ALIGN > 1).
	static unsigned char arena[sizeof(mcp230xx_t) + 2 * MCP230XX_ALIGN];
	mcp230xx_t* misaligned = (mcp230xx_t*)(align_up(arena) + 1);
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);

	TEST_ASSERT_NOT_EQUAL_INT(0, (uintptr_t)misaligned % MCP230XX_ALIGN);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_static_init(
			TEST_I2C, misaligned, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_static_init_fails_with_efault_for_null_storage(void)
{
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_static_init(TEST_I2C, NULL, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_static_init_fails_with_efault_for_a_null_config(void)
{
	static mcp230xx_t storage;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_static_init_fails_with_einval_for_an_active_driver_without_polarity(
	void)
{
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);
	cfg.int_pol = MCP230XX_INT_POLARITY_NONE;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_static_init_fails_with_einval_for_an_invalid_type(void)
{
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);
	cfg.type = (MCP230XX_TYPE)2;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_static_init_fails_with_einval_for_an_invalid_mirror(void)
{
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_017);
	cfg.mirror = (MCP230XX_MIRROR_INT)2;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_static_init(
			TEST_I2C, &storage, TEST_ADDR, false, &cfg));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_write_op.calls);
}

void test_mcp230xx_static_init_fails_when_the_reset_write_fails(void)
{
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);

	expect_init_restart(MCP230XX_008, 0x00);
	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));
}

void test_mcp230xx_static_deinit_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_static_deinit(TEST_I2C, NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_static_deinit_fails_with_einval_for_another_bus(void)
{
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);

	expect_static_init_writes(MCP230XX_008);
	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	fake_i2c_bus = TEST_BUS + 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_static_deinit(TEST_I2C, &storage, false));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	fake_i2c_bus = TEST_BUS;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_static_deinit(TEST_I2C, &storage, false));
}

/* ------------------------- mcp230xx_set_input ------------------------------ */

void test_mcp230xx_set_input_fails_with_efault_for_null_mcp(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(TEST_I2C, NULL, 0, MCP230XX_PULLUP, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_set_input_fails_with_einval_for_out_of_range_index(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_set_input(TEST_I2C,
						 mcp,
						 MCP23008_MAX_GPIOS,
						 MCP230XX_PULLUP,
						 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_fails_with_einval_for_an_invalid_config(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Anything but an MCP230XX_INPUT_CONFIG value is a mistake
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(
			TEST_I2C, mcp, 0, (MCP230XX_INPUT_CONFIG)4, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_writes_gppu_before_iodir(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Force distinct contents in IODIR and GPPU registers
	expect_i2c_read8_8b(IODIR_008, 0x10, 0);
	expect_i2c_read8_8b(GPPU_008, 0x40, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x48, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x18, 0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_set_input(TEST_I2C, mcp, 3, MCP230XX_PULLUP, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_leaves_the_other_pins_alone(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Every pin but 1 is already an input; pins 0, 2 and 3 already have
	// pull-ups. Only pin 1's bit may change in either register.
	expect_i2c_read8_8b(IODIR_008, 0xFD, 0);
	expect_i2c_read8_8b(GPPU_008, 0x0D, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x0F, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0xFF, 0);

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_set_input(TEST_I2C, mcp, 1, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_disables_the_pullup_when_asked(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Pin 2 is already an input, so only its pull-up goes away.
	expect_i2c_read8_8b(IODIR_008, 0xFF, 0);
	expect_i2c_read8_8b(GPPU_008, 0x3C, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x38, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_set_input(TEST_I2C, mcp, 2, MCP230XX_NO_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_only_writes_gppu_when_it_is_already_an_input(void)
{
	/*
	 * Pin 5 is already an input but has no pull-up yet, so only GPPU may
	 * change. This is one of the two cases where the pin's own bit differs
	 * between the two registers.
	 */
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x20, 0);
	expect_i2c_read8_8b(GPPU_008, 0x02, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x22, 0);

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_set_input(TEST_I2C, mcp, 5, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_drops_a_stale_pullup_from_an_output(void)
{
	/*
	 * The mirror image: pin 6 is an output that somehow still has a
	 * pull-up latched, and is asked to become an input without one. Both
	 * registers change, and again the pin's bit disagrees between them.
	 */
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x01, 0);
	expect_i2c_read8_8b(GPPU_008, 0x40, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x41, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_set_input(TEST_I2C, mcp, 6, MCP230XX_NO_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_only_writes_iodir_when_the_pullup_already_matches(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Pin 0 is an output and already has no pull-up, so GPPU is left alone.
	expect_i2c_read8_8b(IODIR_008, 0x80, 0);
	expect_i2c_read8_8b(GPPU_008, 0x40, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x81, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_NO_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_returns_1_when_already_configured(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Pin 7 is already an input and already pulled up: nothing to write.
	expect_i2c_read8_8b(IODIR_008, 0x80, 0);
	expect_i2c_read8_8b(GPPU_008, 0xA0, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);

	TEST_ASSERT_EQUAL_INT(
		1, mcp230xx_set_input(TEST_I2C, mcp, 7, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_uses_the_b_side_registers_for_high_indices(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	// Pin 11 is bit 3 of port B.
	expect_i2c_read8_8b(IODIR_B_017, 0x01, 0);
	expect_i2c_read8_8b(GPPU_B_017, 0x02, 0);
	expect_i2c_read8_8b(IPOL_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_B_017, 0x0A, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IODIR_B_017, 0x09, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_set_input(TEST_I2C, mcp, 11, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_fails_when_reading_the_iodir_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_fails_when_reading_the_gppu_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x10, 0);
	expect_i2c_read8_8b(GPPU_008, 0, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_fails_when_writing_the_gppu_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x10, 0);
	expect_i2c_read8_8b(GPPU_008, 0x20, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x21, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_writes_ipol_before_iodir_when_inverted(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x00, 0);
	expect_i2c_read8_8b(GPPU_008, 0x00, 0);
	expect_i2c_read8_8b(IPOL_008, 0x10, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IPOL_008, 0x18, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x08, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_set_input(
			TEST_I2C, mcp, 3, MCP230XX_NO_PULLUP_INVERTED, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_writes_gppu_then_ipol_then_iodir(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x00, 0);
	expect_i2c_read8_8b(GPPU_008, 0x00, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IPOL_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x08, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_set_input(
			TEST_I2C, mcp, 3, MCP230XX_PULLUP_INVERTED, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_drops_the_inversion_when_not_asked(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Already an input without pull-up: only IPOL changes
	expect_i2c_read8_8b(IODIR_008, 0x08, 0);
	expect_i2c_read8_8b(GPPU_008, 0x00, 0);
	expect_i2c_read8_8b(IPOL_008, 0x88, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IPOL_008, 0x80, 0);

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_set_input(TEST_I2C, mcp, 3, MCP230XX_NO_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_returns_1_when_already_inverted(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x08, 0);
	expect_i2c_read8_8b(GPPU_008, 0x08, 0);
	expect_i2c_read8_8b(IPOL_008, 0x08, 0);

	TEST_ASSERT_EQUAL_INT(
		1,
		mcp230xx_set_input(
			TEST_I2C, mcp, 3, MCP230XX_PULLUP_INVERTED, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_uses_the_b_side_ipol_for_high_indices(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	// GPIO 9 is bit 1 of port B
	expect_i2c_read8_8b(IODIR_B_017, 0x02, 0);
	expect_i2c_read8_8b(GPPU_B_017, 0x00, 0);
	expect_i2c_read8_8b(IPOL_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IPOL_B_017, 0x02, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_set_input(
			TEST_I2C, mcp, 9, MCP230XX_NO_PULLUP_INVERTED, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_fails_when_reading_the_ipol_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Nothing is written
	expect_i2c_read8_8b(IODIR_008, 0x00, 0);
	expect_i2c_read8_8b(GPPU_008, 0x00, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(
			TEST_I2C, mcp, 3, MCP230XX_NO_PULLUP_INVERTED, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_fails_when_writing_the_ipol_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// IODIR isn't written, so it doesn't become an input
	expect_i2c_read8_8b(IODIR_008, 0x00, 0);
	expect_i2c_read8_8b(GPPU_008, 0x00, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IPOL_008, 0x08, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(
			TEST_I2C, mcp, 3, MCP230XX_NO_PULLUP_INVERTED, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_fails_when_writing_the_iodir_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x10, 0);
	expect_i2c_read8_8b(GPPU_008, 0x20, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x21, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x11, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

/* ------------------------- mcp230xx_read_gpio ------------------------------ */

void test_mcp230xx_read_gpio_fails_with_efault_for_null_mcp(void)
{
	errno = 0;
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_gpio(TEST_I2C, NULL, 0, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_read_gpio_fails_with_efault_for_null_return_value(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_read_gpio(TEST_I2C, mcp, 0, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_gpio_fails_with_einval_for_out_of_range_index(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_read_gpio(
			TEST_I2C, mcp, MCP23008_MAX_GPIOS, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_gpio_returns_high_when_the_pin_bit_is_set(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(GPIO_008, 0x10, 0); // only pin 4 high

	uint8_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 4, &value, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);
	TEST_ASSERT_EQUAL_UINT8(MCP230XX_HIGH, value);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_gpio_returns_low_when_the_pin_bit_is_clear(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(GPIO_008, 0xEF, 0); // everything but pin 4 high

	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 4, &value, 1000));
	TEST_ASSERT_EQUAL_UINT8(MCP230XX_LOW, value);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_gpio_uses_the_b_side_register_for_high_indices(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	// Pin 12 is bit 4 of port B.
	expect_i2c_read8_8b(GPIO_B_017, 0x10, 0);

	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 12, &value, 1000));
	TEST_ASSERT_EQUAL_UINT8(MCP230XX_HIGH, value);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_gpio_fails_when_reading_the_gpio_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(GPIO_008, 0, -1);

	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_gpio(TEST_I2C, mcp, 0, &value, 1000));

	destroy_mcp(mcp);
}

/* ----------------------- mcp230xx_read_all_gpios -------------------------- */

void test_mcp230xx_read_all_gpios_fails_with_efault_for_null_mcp(void)
{
	uint16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_all_gpios(TEST_I2C, NULL, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_read_all_gpios_fails_with_efault_for_null_return_value(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_all_gpios(TEST_I2C, mcp, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_all_gpios_reads_the_gpio_reg_of_an_mcp23008(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(GPIO_008, 0xA5, 0);

	uint16_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_all_gpios(TEST_I2C, mcp, &value, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);
	TEST_ASSERT_EQUAL_HEX16(0x00A5, value);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_all_gpios_reads_both_ports_of_an_mcp23017_at_once(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	// GPIOA (0x12) first, so in the high byte, then GPIOB (0x34)
	expect_i2c_read8_16b(GPIO_A_017, 0x1234, 0);

	uint16_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_all_gpios(TEST_I2C, mcp, &value, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);
	// GPIOs 0 to 7 are port A
	TEST_ASSERT_EQUAL_HEX16(0x3412, value);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_all_gpios_bits_match_mcp230xx_read_gpio_indices(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	// Only GPIO 12 (bit 4 of port B) high
	expect_i2c_read8_16b(GPIO_A_017, 0x0010, 0);
	expect_i2c_read8_8b(GPIO_B_017, 0x10, 0);

	uint16_t all;
	uint8_t one;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_all_gpios(TEST_I2C, mcp, &all, 1000));
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 12, &one, 1000));
	TEST_ASSERT_EQUAL_HEX16(1 << 12, all);
	TEST_ASSERT_EQUAL_UINT8(MCP230XX_HIGH, one);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_all_gpios_fails_without_writing_when_the_read_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	expect_i2c_read8_16b(GPIO_A_017, 0, -1);

	uint16_t value = 0xBEEF;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_all_gpios(TEST_I2C, mcp, &value, 1000));
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, value);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_all_gpios_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_i2c_read8_8b(GPIO_008, 0x01, 0);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_all_gpios(TEST_I2C, mcp, &value, 1000));
	TEST_ASSERT_EQUAL_HEX16(0x0001, value);

	destroy_protected_mcp(mcp);
}

/* ------------------------- mcp230xx_set_output ----------------------------- */

void test_mcp230xx_set_output_fails_with_efault_for_null_mcp(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_set_output(TEST_I2C, NULL, 0, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_set_output_fails_with_einval_for_out_of_range_index(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_output(TEST_I2C, mcp, MCP23008_MAX_GPIOS, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_set_output_clears_the_iodir_bit(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0xFF, 0); // every pin an input
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0xDF, 0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, mcp230xx_set_output(TEST_I2C, mcp, 5, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_set_output_returns_1_when_already_an_output(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x00, 0); // every pin an output

	TEST_ASSERT_EQUAL_INT(1, mcp230xx_set_output(TEST_I2C, mcp, 5, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_output_uses_the_b_side_register_for_high_indices(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	// Pin 9 is bit 1 of port B.
	expect_i2c_read8_8b(IODIR_B_017, 0xFF, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IODIR_B_017, 0xFD, 0);

	TEST_ASSERT_EQUAL_INT(0, mcp230xx_set_output(TEST_I2C, mcp, 9, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_output_fails_when_reading_the_iodir_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0, -1);

	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_set_output(TEST_I2C, mcp, 0, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_output_fails_when_writing_the_iodir_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0xFF, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0xFE, -1);

	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_set_output(TEST_I2C, mcp, 0, 1000));

	destroy_mcp(mcp);
}

/* ---------------------- mcp230xx_set_pin_mode_all -------------------------- */

void test_mcp230xx_set_pin_mode_all_fails_with_efault_for_null_mcp(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_set_pin_mode_all(
				      TEST_I2C, NULL, MCP230XX_INPUT, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_set_pin_mode_all_fails_with_einval_for_gpios_over_7_on_an_mcp23008(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// GPIO 8 as input; nothing is written
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_pin_mode_all(
			TEST_I2C, mcp, MCP230XX_INPUT << 8, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_set_pin_mode_all_writes_the_iodir_reg_of_an_mcp23008(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// GPIOs 0 and 7 as inputs, the others as outputs
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x81, 0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_set_pin_mode_all(
				      TEST_I2C,
				      mcp,
				      MCP230XX_INPUT << 7 | MCP230XX_INPUT << 0,
				      1000));
	TEST_ASSERT_EQUAL_INT(0, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_set_pin_mode_all_writes_both_ports_of_an_mcp23017_at_once(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	/*
	 * GPIOs 0 (port A) and 15 (port B) as inputs: IODIRA (0x01) from the
	 * high byte, then IODIRB (0x80)
	 */
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IODIR_A_017, 0x0180, 0);

	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_set_pin_mode_all(TEST_I2C,
							mcp,
							MCP230XX_INPUT << 15 |
								MCP230XX_INPUT
									<< 0,
							1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_pin_mode_all_fails_when_the_write_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IODIR_A_017, 0x0100, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_pin_mode_all(TEST_I2C, mcp, MCP230XX_INPUT, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_pin_mode_all_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	expect_mutex_acquired();
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x01, 0);
	expect_mutex_released();

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_set_pin_mode_all(TEST_I2C, mcp, MCP230XX_INPUT, 1000));

	destroy_protected_mcp(mcp);
}

/* ------------------------- mcp230xx_write_gpio ----------------------------- */

void test_mcp230xx_write_gpio_fails_with_efault_for_null_mcp(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_write_gpio(TEST_I2C, NULL, 0, MCP230XX_HIGH, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_write_gpio_fails_with_einval_for_out_of_range_index(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_write_gpio(TEST_I2C,
						  mcp,
						  MCP23008_MAX_GPIOS,
						  MCP230XX_HIGH,
						  1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_write_gpio_sets_the_olat_bit(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(OLAT_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, OLAT_008, 0x04, 0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_write_gpio(TEST_I2C, mcp, 2, MCP230XX_HIGH, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_write_gpio_clears_the_olat_bit(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(OLAT_008, 0xFF, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, OLAT_008, 0xFB, 0);

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_write_gpio(TEST_I2C, mcp, 2, MCP230XX_LOW, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_write_gpio_returns_1_when_already_at_that_level(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(OLAT_008, 0x04, 0); // pin 2 already high

	TEST_ASSERT_EQUAL_INT(
		1, mcp230xx_write_gpio(TEST_I2C, mcp, 2, MCP230XX_HIGH, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_write_gpio_treats_any_nonzero_as_high(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(OLAT_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, OLAT_008, 0x01, 0);

	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_write_gpio(TEST_I2C, mcp, 0, 42, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_write_gpio_uses_the_b_side_register_for_high_indices(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	// Pin 15 is bit 7 of port B.
	expect_i2c_read8_8b(OLAT_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, OLAT_B_017, 0x80, 0);

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_write_gpio(TEST_I2C, mcp, 15, MCP230XX_HIGH, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_write_gpio_fails_when_reading_the_olat_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(OLAT_008, 0, -1);

	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_write_gpio(TEST_I2C, mcp, 0, MCP230XX_HIGH, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_write_gpio_fails_when_writing_the_olat_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(OLAT_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, OLAT_008, 0x01, -1);

	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_write_gpio(TEST_I2C, mcp, 0, MCP230XX_HIGH, 1000));

	destroy_mcp(mcp);
}

/* -------------------------- mcp230xx_write_all ----------------------------- */

void test_mcp230xx_write_all_fails_with_efault_for_null_mcp(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_write_all(TEST_I2C, NULL, MCP230XX_HIGH, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_write_all_fails_with_einval_for_gpios_over_7_on_an_mcp23008(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// GPIO 8 high; nothing is written
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_write_all(TEST_I2C, mcp, MCP230XX_HIGH << 8, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_write_all_writes_the_olat_reg_of_an_mcp23008(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// GPIOs 0 and 7 high, the others low
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, OLAT_008, 0x81, 0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_write_all(TEST_I2C,
						 mcp,
						 MCP230XX_HIGH << 7 |
							 MCP230XX_HIGH << 0,
						 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_write_all_writes_both_ports_of_an_mcp23017_at_once(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	/*
	 * GPIOs 0 (port A) and 15 (port B) high: OLATA (0x01) from the high
	 * byte, then OLATB (0x80)
	 */
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, OLAT_A_017, 0x0180, 0);

	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_write_all(TEST_I2C,
						 mcp,
						 MCP230XX_HIGH << 15 |
							 MCP230XX_HIGH << 0,
						 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_write_all_fails_when_the_write_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, OLAT_A_017, 0x0100, -1);

	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_write_all(TEST_I2C, mcp, MCP230XX_HIGH, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_write_all_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	expect_mutex_acquired();
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, OLAT_008, 0x01, 0);
	expect_mutex_released();

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_write_all(TEST_I2C, mcp, MCP230XX_HIGH, 1000));

	destroy_protected_mcp(mcp);
}

/* --------------------- GPIO index to register mapping ---------------------- */

static uint8_t expected_addr(MCP230XX_TYPE type,
			     uint8_t index,
			     uint8_t reg_008,
			     uint8_t reg_a_017,
			     uint8_t reg_b_017)
{
	if (type == MCP230XX_008) {
		return reg_008;
	}
	return index < 8 ? reg_a_017 : reg_b_017;
}

void test_mcp230xx_fuzzes_every_gpio_index_to_its_register_and_bit(void)
{
	for (int type_i = 0; type_i <= 1; type_i++) {
		const MCP230XX_TYPE type = type_i == 0 ? MCP230XX_008 :
							 MCP230XX_017;
		const uint8_t pins = type == MCP230XX_017 ? 16 : 8;

		for (uint8_t index = 0; index < pins; index++) {
			const uint8_t bit = (uint8_t)(1 << (index % 8));
			mcp230xx_t* mcp = create_mcp(type);

			const uint8_t iodir = expected_addr(type,
							    index,
							    IODIR_008,
							    IODIR_A_017,
							    IODIR_B_017);
			const uint8_t gppu = expected_addr(
				type, index, GPPU_008, GPPU_A_017, GPPU_B_017);
			const uint8_t ipol = expected_addr(
				type, index, IPOL_008, IPOL_A_017, IPOL_B_017);
			const uint8_t iodir_mark =
				(uint8_t)(1 << ((index + 1) % 8));
			const uint8_t gppu_mark =
				(uint8_t)(1 << ((index + 2) % 8));
			expect_i2c_read8_8b(iodir, iodir_mark, 0);
			expect_i2c_read8_8b(gppu, gppu_mark, 0);
			expect_i2c_read8_8b(ipol, 0x00, 0);
			i2c_write8_8b_ExpectAndReturn(TEST_I2C,
						      TEST_ADDR,
						      gppu,
						      (uint8_t)(gppu_mark |
								bit),
						      0);
			i2c_write8_8b_ExpectAndReturn(TEST_I2C,
						      TEST_ADDR,
						      iodir,
						      (uint8_t)(iodir_mark |
								bit),
						      0);
			TEST_ASSERT_EQUAL_INT(
				0,
				mcp230xx_set_input(TEST_I2C,
						   mcp,
						   index,
						   MCP230XX_PULLUP,
						   1000));

			// read_gpio: only this pin's bit is high.
			const uint8_t gpio = expected_addr(
				type, index, GPIO_008, GPIO_A_017, GPIO_B_017);
			uint8_t value;
			expect_i2c_read8_8b(gpio, bit, 0);
			TEST_ASSERT_EQUAL_INT(
				0,
				mcp230xx_read_gpio(
					TEST_I2C, mcp, index, &value, 1000));
			TEST_ASSERT_EQUAL_UINT8(MCP230XX_HIGH, value);
			expect_i2c_read8_8b(gpio, (uint8_t)~bit, 0);
			TEST_ASSERT_EQUAL_INT(
				0,
				mcp230xx_read_gpio(
					TEST_I2C, mcp, index, &value, 1000));
			TEST_ASSERT_EQUAL_UINT8(MCP230XX_LOW, value);

			// set_output: clears just this pin's IODIR bit.
			expect_i2c_read8_8b(iodir, 0xFF, 0);
			i2c_write8_8b_ExpectAndReturn(
				TEST_I2C, TEST_ADDR, iodir, (uint8_t)~bit, 0);
			TEST_ASSERT_EQUAL_INT(
				0,
				mcp230xx_set_output(TEST_I2C, mcp, index, 1000));

			// write_gpio: sets just this pin's OLAT bit.
			const uint8_t olat = expected_addr(
				type, index, OLAT_008, OLAT_A_017, OLAT_B_017);
			expect_i2c_read8_8b(olat, 0x00, 0);
			i2c_write8_8b_ExpectAndReturn(
				TEST_I2C, TEST_ADDR, olat, bit, 0);
			TEST_ASSERT_EQUAL_INT(0,
					      mcp230xx_write_gpio(TEST_I2C,
								  mcp,
								  index,
								  MCP230XX_HIGH,
								  1000));

			destroy_mcp(mcp);
		}
	}
}

/* ---------------------------- mcp230xx_get_type --------------------------- */

void test_mcp230xx_get_type_returns_the_type_of_the_init(void)
{
	const MCP230XX_TYPE types[] = { MCP230XX_008, MCP230XX_017 };

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		mcp230xx_t* mcp = create_mcp(types[i]);
		MCP230XX_TYPE type;

		TEST_ASSERT_EQUAL_INT(0, mcp230xx_get_type(mcp, &type));
		TEST_ASSERT_EQUAL_INT(types[i], type);

		destroy_mcp(mcp);
	}
}

void test_mcp230xx_get_type_fails_with_efault_for_null(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	MCP230XX_TYPE type;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_get_type(NULL, &type));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_get_type(mcp, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_mcp(mcp);
}

/* ----------------------- mcp230xx_protect / unprotect --------------------- */

void test_mcp230xx_protect_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_protect(TEST_I2C, NULL, TEST_SCOPE));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_protect_fails_with_einval_for_another_bus(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	fake_i2c_bus = TEST_BUS + 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_protect(TEST_I2C, mcp, TEST_SCOPE));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	fake_i2c_bus = TEST_BUS;
	destroy_mcp(mcp);
}

void test_mcp230xx_protect_fails_when_the_bus_cant_be_read(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	fake_i2c_bus_retval = -1;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_protect(TEST_I2C, mcp, TEST_SCOPE));

	fake_i2c_bus_retval = 0;
	destroy_mcp(mcp);
}

void test_mcp230xx_protect_creates_the_mutex_with_the_given_scope(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	plc_mutex_static_create_ExpectAndReturn(
		NULL, PLC_MUTEX_SCOPE_SHARED, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_protect(TEST_I2C, mcp, PLC_MUTEX_SCOPE_SHARED));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_protect_returns_1_if_already_protected(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	// No second plc_mutex_static_create is expected.
	TEST_ASSERT_EQUAL_INT(1, mcp230xx_protect(TEST_I2C, mcp, TEST_SCOPE));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_protect_leaves_the_handle_unprotected_when_it_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, -1);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_protect(TEST_I2C, mcp, TEST_SCOPE));

	// No plc_mutex_acquire/release is expected: the handle isn't protected.
	expect_i2c_read8_8b(GPIO_008, 0x01, 0);
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 0, &value, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_unprotect_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_unprotect(NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_unprotect_returns_1_if_not_protected(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(1, mcp230xx_unprotect(mcp));

	destroy_mcp(mcp);
}

void test_mcp230xx_unprotect_destroys_the_mutex(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, mcp230xx_unprotect(mcp));

	// No plc_mutex_acquire/release is expected anymore.
	expect_i2c_read8_8b(GPIO_008, 0x01, 0);
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 0, &value, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_unprotect_keeps_the_handle_protected_when_it_fails(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	expect_mutex_destroyed(-1);
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_unprotect(mcp));

	expect_mutex_acquired();
	expect_i2c_read8_8b(GPIO_008, 0x01, 0);
	expect_mutex_released();
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 0, &value, 1000));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_deinit_destroys_the_mutex_when_protected(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, mcp230xx_deinit(TEST_I2C, mcp, false));
}

void test_mcp230xx_deinit_keeps_the_handle_when_the_mutex_cant_be_destroyed(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	uint32_t writes_before = fake_i2c_write_op.calls;
	expect_mutex_destroyed(-1);
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_deinit(TEST_I2C, mcp, true));
	TEST_ASSERT_EQUAL_UINT32(writes_before, fake_i2c_write_op.calls);

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_static_deinit_destroys_the_mutex_when_protected(void)
{
	static mcp230xx_t storage;
	mcp230xx_config_t cfg;
	fill_default_cfg(&cfg, MCP230XX_008);

	expect_static_init_writes(MCP230XX_008);
	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_protect(TEST_I2C, &storage, TEST_SCOPE));

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_static_deinit(TEST_I2C, &storage, false));
}

void test_mcp230xx_fails_when_the_mutex_cant_be_taken(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_017);

	// No I2C transfer and no release are expected.
	plc_mutex_acquire_Stub(acquire_times_out);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(TEST_I2C, mcp, 9, MCP230XX_PULLUP, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_gpio(TEST_I2C, mcp, 9, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_set_output(TEST_I2C, mcp, 9, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_write_gpio(TEST_I2C, mcp, 9, MCP230XX_HIGH, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_write_all(TEST_I2C, mcp, MCP230XX_HIGH << 9, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_pin_mode_all(
			TEST_I2C, mcp, MCP230XX_INPUT << 9, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	uint16_t all;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_all_gpios(TEST_I2C, mcp, &all, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 9, MCP230XX_INT_ON_CHANGE, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_disable_interrupt(
			TEST_I2C, mcp, 9, &(mcp230xx_int_regs_t){ 0 }, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	errno = 0;
	mcp230xx_int_regs_t regs;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_interrupt(TEST_I2C, mcp, &regs, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_goes_on_after_an_owner_died(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	plc_mutex_acquire_Stub(acquire_after_owner_died);
	expect_i2c_read8_8b(OLAT_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, OLAT_008, 0x04, 0);
	expect_mutex_released();

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_write_gpio(TEST_I2C, mcp, 2, MCP230XX_HIGH, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_releases_the_mutex_when_the_transfer_fails(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	expect_mutex_acquired();
	expect_i2c_read8_8b(IODIR_008, 0, -1);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_PULLUP, 1000));

	expect_mutex_acquired();
	expect_i2c_read8_8b(GPIO_008, 0, -1);
	expect_mutex_released();
	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_read_gpio(TEST_I2C, mcp, 0, &value, 1000));

	expect_mutex_acquired();
	expect_i2c_read8_8b(IODIR_008, 0xFF, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0xFE, -1);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_set_output(TEST_I2C, mcp, 0, 1000));

	expect_mutex_acquired();
	expect_i2c_read8_8b(OLAT_008, 0, -1);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		-1, mcp230xx_write_gpio(TEST_I2C, mcp, 0, MCP230XX_HIGH, 1000));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_rejects_bad_arguments_without_taking_the_mutex(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	// No plc_mutex_acquire/release is expected.
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_set_input(TEST_I2C,
						 mcp,
						 MCP23008_MAX_GPIOS,
						 MCP230XX_PULLUP,
						 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	fake_i2c_bus = TEST_BUS + 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_set_output(TEST_I2C, mcp, 0, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	fake_i2c_bus = TEST_BUS;
	destroy_protected_mcp(mcp);
}

void test_mcp230xx_set_input_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_i2c_read8_8b(IODIR_008, 0xFF, 0);
	expect_i2c_read8_8b(GPPU_008, 0x81, 0);
	expect_i2c_read8_8b(IPOL_008, 0x00, 0);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	TEST_ASSERT_EQUAL_INT(
		1, mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_PULLUP, 1000));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_read_gpio_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_i2c_read8_8b(GPIO_008, 0x01, 0);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	uint8_t value;
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 0, &value, 1000));
	TEST_ASSERT_EQUAL_UINT8(MCP230XX_HIGH, value);

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_set_output_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_i2c_read8_8b(IODIR_008, 0x00, 0);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	TEST_ASSERT_EQUAL_INT(1, mcp230xx_set_output(TEST_I2C, mcp, 0, 1000));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_write_gpio_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_i2c_read8_8b(OLAT_008, 0x01, 0);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	TEST_ASSERT_EQUAL_INT(
		1, mcp230xx_write_gpio(TEST_I2C, mcp, 0, MCP230XX_HIGH, 1000));

	destroy_protected_mcp(mcp);
}

/* ---------------------- mcp230xx_enable_interrupt ------------------------- */

void test_mcp230xx_enable_interrupt_fails_with_efault_for_null_mcp(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_enable_interrupt(
			TEST_I2C, NULL, 0, MCP230XX_INT_ON_CHANGE, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_enable_interrupt_fails_with_einval_for_out_of_range_index(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_enable_interrupt(TEST_I2C,
							mcp,
							MCP23008_MAX_GPIOS,
							MCP230XX_INT_ON_CHANGE,
							1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_fails_with_einval_for_an_invalid_mode(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 0, (MCP230XX_INT_MODE)3, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_on_change_compares_with_the_previous_value(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// GPIO 2 was set to compare with DEFVAL: INTCON's bit is cleared
	expect_i2c_read8_8b(GPINTEN_008, 0x00, 0);
	expect_i2c_read8_8b(INTCON_008, 0x04, 0);
	expect_i2c_read8_8b(DEFVAL_008, 0x04, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, DEFVAL_008, 0x04, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, INTCON_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0x04, 0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 2, MCP230XX_INT_ON_CHANGE, 1000));
	TEST_ASSERT_EQUAL_INT(0, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_while_low_compares_with_a_high_defval(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// DEFVAL, then INTCON, and GPINTEN last
	expect_i2c_read8_8b(GPINTEN_008, 0x00, 0);
	expect_i2c_read8_8b(INTCON_008, 0x00, 0);
	expect_i2c_read8_8b(DEFVAL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, DEFVAL_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, INTCON_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0x08, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 3, MCP230XX_INT_WHILE_LOW, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_while_high_compares_with_a_low_defval(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(GPINTEN_008, 0x00, 0);
	expect_i2c_read8_8b(INTCON_008, 0x00, 0);
	expect_i2c_read8_8b(DEFVAL_008, 0xFF, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, DEFVAL_008, 0xF7, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, INTCON_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0x08, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 3, MCP230XX_INT_WHILE_HIGH, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_leaves_the_other_pins_alone(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Every other pin is set the opposite way
	expect_i2c_read8_8b(GPINTEN_008, 0xFE, 0);
	expect_i2c_read8_8b(INTCON_008, 0xFE, 0);
	expect_i2c_read8_8b(DEFVAL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, DEFVAL_008, 0x01, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, INTCON_008, 0xFF, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0xFF, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 0, MCP230XX_INT_WHILE_LOW, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_returns_1_when_already_enabled_like_that(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(GPINTEN_008, 0x08, 0);
	expect_i2c_read8_8b(INTCON_008, 0x08, 0);
	expect_i2c_read8_8b(DEFVAL_008, 0x08, 0);

	TEST_ASSERT_EQUAL_INT(
		1,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 3, MCP230XX_INT_WHILE_LOW, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_changes_the_mode_of_an_enabled_pin(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Already enabled while low: going to while high flips DEFVAL
	expect_i2c_read8_8b(GPINTEN_008, 0x08, 0);
	expect_i2c_read8_8b(INTCON_008, 0x08, 0);
	expect_i2c_read8_8b(DEFVAL_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, DEFVAL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, INTCON_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0x08, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 3, MCP230XX_INT_WHILE_HIGH, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_uses_the_b_side_registers_for_high_indices(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);

	expect_i2c_read8_8b(GPINTEN_B_017, 0x00, 0);
	expect_i2c_read8_8b(INTCON_B_017, 0x00, 0);
	expect_i2c_read8_8b(DEFVAL_B_017, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, DEFVAL_B_017, 0x80, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, INTCON_B_017, 0x80, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_B_017, 0x80, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 15, MCP230XX_INT_WHILE_LOW, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_fails_when_reading_a_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(GPINTEN_008, 0x00, 0);
	expect_i2c_read8_8b(INTCON_008, 0x00, 0);
	expect_i2c_read8_8b(DEFVAL_008, 0x00, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 3, MCP230XX_INT_WHILE_LOW, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_fails_without_enabling_when_a_write_fails(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// INTCON fails, so GPINTEN is never written
	expect_i2c_read8_8b(GPINTEN_008, 0x00, 0);
	expect_i2c_read8_8b(INTCON_008, 0x00, 0);
	expect_i2c_read8_8b(DEFVAL_008, 0x00, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, DEFVAL_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, INTCON_008, 0x08, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 3, MCP230XX_INT_WHILE_LOW, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_enable_interrupt_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_i2c_read8_8b(GPINTEN_008, 0x00, -1);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_enable_interrupt(
			TEST_I2C, mcp, 0, MCP230XX_INT_ON_CHANGE, 1000));

	destroy_protected_mcp(mcp);
}

/* ----------------------- mcp230xx_read_interrupt -------------------------- */

// INTF, INTCAP and GPIO go out in one i2c_write_then_read, answered by the fake.
static void answer_interrupt_read(const uint8_t* regs, size_t len)
{
	i2c_write_then_read_Stub(fake_i2c_write_then_read);
	fake_i2c_answers(regs, len);
}

static int read_interrupt(mcp230xx_t* mcp, mcp230xx_int_regs_t* regs)
{
	return mcp230xx_read_interrupt(TEST_I2C, mcp, regs, 1000);
}

// Like create_mcp, with "gpios" as the levels the init reads from GPIO.
static mcp230xx_t* create_mcp_reading_gpio(MCP230XX_TYPE type, uint16_t gpios)
{
	expect_init_restart(type, init_restart_iocon_of(type));
	i2c_write8_8b_ExpectAndReturn(TEST_I2C,
				      TEST_ADDR,
				      iocon_addr_of(type),
				      init_restart_iocon_of(type),
				      0);
	expect_init_gpio_read(type, gpios, 0);

	mcp230xx_t* mcp = init_mcp(true,
				   type,
				   false,
				   MCP230XX_ACTIVE_DRIVER_INT,
				   MCP230XX_INT_ACTIVE_LOW,
				   MCP230XX_NO_MIRRORED_INT);
	TEST_ASSERT_NOT_NULL(mcp);
	return mcp;
}

void test_mcp230xx_read_interrupt_fails_with_efault_for_null_arguments(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, read_interrupt(NULL, &regs));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, read_interrupt(mcp, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_reads_intf_intcap_and_gpio_of_an_mcp23008(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	// GPIO 3 interrupted
	answer_interrupt_read((const uint8_t[]){ 0x08, 0x5A, 0x0C }, 3);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_INT(0, errno);
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
	TEST_ASSERT_EQUAL_HEX8(INTF_008, fake_i2c_transfer_op.bytes[0]);
	TEST_ASSERT_EQUAL_size_t(3, fake_i2c_transfer_op.requested_read_len);
	TEST_ASSERT_EQUAL_HEX16(0x0008, regs.intf);
	TEST_ASSERT_EQUAL_HEX16(0x005A, regs.intcap);
	TEST_ASSERT_EQUAL_HEX16(0x000C, regs.gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_reads_both_ports_of_an_mcp23017_at_once(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);
	mcp230xx_int_regs_t regs;

	// INTFA, INTFB, INTCAPA, INTCAPB, GPIOA, GPIOB: GPIOs 0 and 15
	answer_interrupt_read(
		(const uint8_t[]){ 0x01, 0x80, 0x11, 0x22, 0x33, 0x44 }, 6);

	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX8(INTF_A_017, fake_i2c_transfer_op.bytes[0]);
	TEST_ASSERT_EQUAL_size_t(6, fake_i2c_transfer_op.requested_read_len);
	// GPIOs 0 to 7 are port A
	TEST_ASSERT_EQUAL_HEX16(0x8001, regs.intf);
	TEST_ASSERT_EQUAL_HEX16(0x2211, regs.intcap);
	TEST_ASSERT_EQUAL_HEX16(0x4433, regs.gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_first_gives_the_gpio_the_init_read(void)
{
	mcp230xx_t* mcp = create_mcp_reading_gpio(MCP230XX_017, 0x0400);
	mcp230xx_int_regs_t regs;

	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x08, 0x00, 0x08, 0x00, 0x0C }, 6);

	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0C00, regs.gpio);
	TEST_ASSERT_EQUAL_HEX16(0x0400, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_saves_gpio_for_the_next_call(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	answer_interrupt_read((const uint8_t[]){ 0x04, 0x04, 0x04 }, 3);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0000, regs.prev_gpio);

	answer_interrupt_read((const uint8_t[]){ 0x04, 0x00, 0x08 }, 3);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0008, regs.gpio);
	TEST_ASSERT_EQUAL_HEX16(0x0004, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_gives_the_port_read_gpio_saved(void)
{
	mcp230xx_t* mcp = create_mcp_reading_gpio(MCP230XX_017, 0xAB00);
	mcp230xx_int_regs_t regs;
	uint8_t value;

	// Port A is read, so port B keeps what the init read
	expect_i2c_read8_8b(GPIO_A_017, 0x08, 0);
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_read_gpio(TEST_I2C, mcp, 3, &value, 0));

	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x08, 0xAB }, 6);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0xAB08, regs.prev_gpio);

	// Port B is read, so port A keeps what read_interrupt read
	expect_i2c_read8_8b(GPIO_B_017, 0x10, 0);
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_read_gpio(TEST_I2C, mcp, 12, &value, 0));

	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x08, 0x10 }, 6);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x1008, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_gives_what_read_all_gpios_saved(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);
	mcp230xx_int_regs_t regs;
	uint16_t all;

	// GPIOA (0x12), then GPIOB (0x34)
	expect_i2c_read8_16b(GPIO_A_017, 0x1234, 0);
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_read_all_gpios(TEST_I2C, mcp, &all, 0));

	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x12, 0x34 }, 6);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x3412, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_gives_what_read_gpio_saved_on_an_mcp23008(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;
	uint8_t value;

	expect_i2c_read8_8b(GPIO_008, 0x08, 0);
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_read_gpio(TEST_I2C, mcp, 3, &value, 0));

	answer_interrupt_read((const uint8_t[]){ 0x00, 0x00, 0x08 }, 3);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0008, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_gives_what_read_all_gpios_saved_on_an_mcp23008(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;
	uint16_t all;

	expect_i2c_read8_8b(GPIO_008, 0xA5, 0);
	TEST_ASSERT_EQUAL_INT(0,
			      mcp230xx_read_all_gpios(TEST_I2C, mcp, &all, 0));

	answer_interrupt_read((const uint8_t[]){ 0x00, 0x00, 0xA5 }, 3);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x00A5, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_keeps_the_save_when_read_gpio_fails(void)
{
	mcp230xx_t* mcp = create_mcp_reading_gpio(MCP230XX_017, 0xAB05);
	mcp230xx_int_regs_t regs;
	uint8_t value;

	expect_i2c_read8_8b(GPIO_A_017, 0, -1);
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_read_gpio(TEST_I2C, mcp, 3, &value, 0));

	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x05, 0xAB }, 6);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0xAB05, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_keeps_the_save_when_read_all_gpios_fails(
	void)
{
	mcp230xx_t* mcp = create_mcp_reading_gpio(MCP230XX_017, 0xAB05);
	mcp230xx_int_regs_t regs;
	uint16_t all;

	expect_i2c_read8_16b(GPIO_A_017, 0, -1);
	TEST_ASSERT_EQUAL_INT(-1,
			      mcp230xx_read_all_gpios(TEST_I2C, mcp, &all, 0));

	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x05, 0xAB }, 6);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0xAB05, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_reports_an_empty_intf_when_none_is_pending(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs = { .intf = 0xBEEF };

	answer_interrupt_read((const uint8_t[]){ 0x00, 0xFF, 0x00 }, 3);

	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0000, regs.intf);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_fails_without_writing_or_saving(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);
	mcp230xx_int_regs_t regs = { 0xBEEF, 0xBEEF, 0xBEEF, 0xBEEF };

	answer_interrupt_read(
		(const uint8_t[]){ 0x01, 0x00, 0x01, 0x00, 0xFF, 0xFF }, 6);
	fake_i2c_transfer_op.retval = -1;

	TEST_ASSERT_EQUAL_INT(-1, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, regs.intf);
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, regs.intcap);
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, regs.gpio);
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, regs.prev_gpio);

	// The GPIO saved by the init is still there
	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 }, 6);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0000, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_fails_with_eio_when_the_read_is_short(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);
	mcp230xx_int_regs_t regs = { 0xBEEF, 0xBEEF, 0xBEEF, 0xBEEF };

	answer_interrupt_read(
		(const uint8_t[]){ 0x01, 0x00, 0x01, 0x00, 0xFF, 0xFF }, 6);
	fake_i2c_transfer_op.reported_read_len = 4;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_INT(EIO, errno);
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, regs.intf);
	TEST_ASSERT_EQUAL_HEX16(0xBEEF, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_read_interrupt_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	answer_interrupt_read((const uint8_t[]){ 0x01, 0x01, 0x01 }, 3);
	expect_mutex_acquired();
	expect_mutex_released();

	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0001, regs.intf);

	destroy_protected_mcp(mcp);
}

/* ---------------------- mcp230xx_disable_interrupt ------------------------ */

static int disable_interrupt(mcp230xx_t* mcp,
			     uint8_t index,
			     mcp230xx_int_regs_t* regs)
{
	return mcp230xx_disable_interrupt(TEST_I2C, mcp, index, regs, 1000);
}

void test_mcp230xx_disable_interrupt_fails_with_efault_for_null_mcp(void)
{
	mcp230xx_int_regs_t regs;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, disable_interrupt(NULL, 0, &regs));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_mcp230xx_disable_interrupt_fails_with_efault_for_null_regs(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, disable_interrupt(mcp, 0, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_fails_with_einval_for_out_of_range_index(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      disable_interrupt(mcp, MCP23008_MAX_GPIOS, &regs));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_clears_only_the_gpinten_bit_then_reads(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	// INTCON and DEFVAL are neither read nor written
	expect_i2c_read8_8b(GPINTEN_008, 0xFF, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0xF7, 0);
	answer_interrupt_read((const uint8_t[]){ 0x00, 0x00, 0x00 }, 3);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, disable_interrupt(mcp, 3, &regs));
	TEST_ASSERT_EQUAL_INT(0, errno);
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
	TEST_ASSERT_EQUAL_HEX8(INTF_008, fake_i2c_transfer_op.bytes[0]);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_returns_1_when_already_disabled(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	// Nothing of it can be latched, so nothing is read
	expect_i2c_read8_8b(GPINTEN_008, 0xF7, 0);
	answer_interrupt_read((const uint8_t[]){ 0x00, 0x00, 0x00 }, 3);

	TEST_ASSERT_EQUAL_INT(1, disable_interrupt(mcp, 3, &regs));
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_transfer_op.calls);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_hands_over_what_it_read(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_017);
	mcp230xx_int_regs_t regs;

	// GPIO 11 is masked when read, so INTF hides it; GPIO 10 changed too
	expect_i2c_read8_8b(GPINTEN_B_017, 0x0C, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_B_017, 0x04, 0);
	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x00, 0x00, 0x08, 0x00, 0x0C }, 6);

	TEST_ASSERT_EQUAL_INT(0, disable_interrupt(mcp, 11, &regs));
	TEST_ASSERT_EQUAL_HEX8(INTF_A_017, fake_i2c_transfer_op.bytes[0]);
	TEST_ASSERT_EQUAL_HEX16(0x0000, regs.intf);
	TEST_ASSERT_EQUAL_HEX16(0x0800, regs.intcap);
	TEST_ASSERT_EQUAL_HEX16(0x0C00, regs.gpio);
	TEST_ASSERT_EQUAL_HEX16(0x0000, regs.prev_gpio);

	// Its read of GPIO was saved
	answer_interrupt_read(
		(const uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C }, 6);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0C00, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_fails_when_reading_the_gpinten_reg_fails(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	// Nothing is cleared
	expect_i2c_read8_8b(GPINTEN_008, 0x00, -1);
	answer_interrupt_read((const uint8_t[]){ 0x00, 0x00, 0x00 }, 3);

	TEST_ASSERT_EQUAL_INT(-1, disable_interrupt(mcp, 0, &regs));
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_transfer_op.calls);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_fails_when_writing_the_gpinten_reg_fails(
	void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	// Nothing is cleared
	expect_i2c_read8_8b(GPINTEN_008, 0x01, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0x00, -1);
	answer_interrupt_read((const uint8_t[]){ 0x00, 0x00, 0x00 }, 3);

	TEST_ASSERT_EQUAL_INT(-1, disable_interrupt(mcp, 0, &regs));
	TEST_ASSERT_EQUAL_UINT32(0, fake_i2c_transfer_op.calls);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_fails_when_the_read_fails(void)
{
	mcp230xx_t* mcp = create_mcp_reading_gpio(MCP230XX_008, 0x00);
	mcp230xx_int_regs_t regs;

	// The GPIO is left disabled, and GPIO isn't saved
	expect_i2c_read8_8b(GPINTEN_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0x00, 0);
	answer_interrupt_read((const uint8_t[]){ 0x00, 0x08, 0x08 }, 3);
	fake_i2c_transfer_op.retval = -1;

	TEST_ASSERT_EQUAL_INT(-1, disable_interrupt(mcp, 3, &regs));

	answer_interrupt_read((const uint8_t[]){ 0x00, 0x08, 0x08 }, 3);
	TEST_ASSERT_EQUAL_INT(0, read_interrupt(mcp, &regs));
	TEST_ASSERT_EQUAL_HEX16(0x0000, regs.prev_gpio);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_fails_with_eio_on_a_short_read(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	expect_i2c_read8_8b(GPINTEN_008, 0x08, 0);
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, GPINTEN_008, 0x00, 0);
	answer_interrupt_read((const uint8_t[]){ 0x00, 0x08, 0x08 }, 3);
	fake_i2c_transfer_op.reported_read_len = 2;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, disable_interrupt(mcp, 3, &regs));
	TEST_ASSERT_EQUAL_INT(EIO, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_disable_interrupt_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);
	mcp230xx_int_regs_t regs;

	expect_mutex_acquired();
	expect_i2c_read8_8b(GPINTEN_008, 0x00, 0);
	expect_mutex_released();

	TEST_ASSERT_EQUAL_INT(1, disable_interrupt(mcp, 0, &regs));

	destroy_protected_mcp(mcp);
}
