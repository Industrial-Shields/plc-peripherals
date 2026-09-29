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
 * Tests for src/peripheral-mcp230xx.c. Its dependencies are mocked:
 * plc-peripherals-i2c.h (the register-level i2c_write8_8b/i2c_read8_8b calls)
 * plc-peripherals-i2c-hal.h is mocked too, one layer below
 * plc-peripherals-i2c.h: mcp230xx_protect calls i2c_get_bus directly, and the
 * reset writes the whole register block in a single i2c_write.
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
#define IOCON_008                                                          0x05
#define GPPU_008                                                           0x06
#define GPIO_008                                                           0x09
#define OLAT_008                                                           0x0A

#define IODIR_A_017                                                        0x00
#define IODIR_B_017                                                        0x01
#define IOCON_A_017                                                        0x0A
#define GPPU_A_017                                                         0x0C
#define GPPU_B_017                                                         0x0D
#define GPIO_A_017                                                         0x12
#define GPIO_B_017                                                         0x13
#define OLAT_A_017                                                         0x14
#define OLAT_B_017                                                         0x15

#define IOCON_REG_MIRROR                                              (1 << 6)
#define IOCON_REG_DISSLW                                              (1 << 4)
#define IOCON_REG_ODR                                                 (1 << 2)
#define IOCON_REG_INTPOL                                              (1 << 1)
// Bits the driver must never touch: BANK, SEQOP and HAEN.
#define IOCON_REG_UNTOUCHED                                               0xA9
// clang-format on

#define RESET_LEN_008 12
#define RESET_LEN_017 23

/*
 * IOCON value mcp230xx_init(restart=true) writes for the fixtures below: slew
 * rate control left enabled (DISSLW=0), active-driver INT (ODR=0), active-low
 * INT (INTPOL=0), no mirroring (MIRROR=0).
 */
#define INIT_RESTART_IOCON 0x00

/*
 * mcp230xx_set_input reads IODIR and GPPU back to back
 * before acting on either, so those two values must stay alive at the same
 * time. Every other read in this driver is consumed before the next one is
 * queued, so they can all share the third slot.
 */
// clang-format off
#define READ_SLOT_IODIR                                                       0
#define READ_SLOT_GPPU                                                        1
#define READ_SLOT_OTHER                                                       2
#define READ_VALUE_SLOTS                                                      3

#define IS_IODIR_REG(reg) \
	((reg) == IODIR_008 || (reg) == IODIR_A_017 || (reg) == IODIR_B_017)
#define IS_GPPU_REG(reg) \
	((reg) == GPPU_008 || (reg) == GPPU_A_017 || (reg) == GPPU_B_017)

#define READ_SLOT_OF(reg)                            \
	(IS_IODIR_REG(reg) ? READ_SLOT_IODIR :       \
	 IS_GPPU_REG(reg)  ? READ_SLOT_GPPU  :       \
			     READ_SLOT_OTHER)
// clang-format on

static uint8_t read_values[READ_VALUE_SLOTS];

static void expect_i2c_read8_8b(uint8_t reg, uint8_t value, int retval)
{
	i2c_read8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, reg, NULL, retval);
	i2c_read8_8b_IgnoreArg_to_read();
	if (retval == 0) {
		uint8_t* slot = &read_values[READ_SLOT_OF(reg)];

		*slot = value;
		i2c_read8_8b_ReturnThruPtr_to_read(slot);
	}
}

static uint8_t iocon_addr_of(MCP230XX_TYPE type)
{
	return type == MCP230XX_017 ? IOCON_A_017 : IOCON_008;
}

static size_t reset_len_of(MCP230XX_TYPE type)
{
	return type == MCP230XX_017 ? RESET_LEN_017 : RESET_LEN_008;
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
	fake_i2c_write_op.retval = (ssize_t)reset_len_of(type);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C,
				      TEST_ADDR,
				      iocon_addr_of(type),
				      INIT_RESTART_IOCON,
				      0);

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

static mcp230xx_t* create_protected_mcp(MCP230XX_TYPE type)
{
	mcp230xx_t* mcp = create_mcp(type);

	TEST_ASSERT_EQUAL_INT(0, mcp230xx_protect(TEST_I2C, mcp));
	return mcp;
}

static void destroy_protected_mcp(mcp230xx_t* mcp)
{
	TEST_ASSERT_EQUAL_INT(0, mcp230xx_deinit(TEST_I2C, mcp, false));
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
	fake_i2c_write_op.retval = RESET_LEN_008;
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_008, INIT_RESTART_IOCON, 0);

	mcp230xx_t* mcp = init_mcp(true,
				   MCP230XX_008,
				   false,
				   MCP230XX_ACTIVE_DRIVER_INT,
				   MCP230XX_INT_ACTIVE_LOW,
				   MCP230XX_NO_MIRRORED_INT);

	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
	assert_last_write_was_a_reset_block(MCP230XX_008);
	destroy_mcp(mcp);
}

void test_mcp230xx_init_with_restart_resets_an_mcp23017_then_writes_iocon(void)
{
	fake_i2c_write_op.retval = RESET_LEN_017;
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_A_017, INIT_RESTART_IOCON, 0);

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
	expect_i2c_read8_8b(IOCON_008, 0xFF, 0);
	// DISSLW cleared, ODR set (open drain), INTPOL cleared (no polarity
	// with open drain), MIRROR cleared: 0xFF & ~0x10 & ~0x02 & ~0x40.
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IOCON_008, 0xAD, 0);

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
	fake_i2c_write_op.retval = RESET_LEN_017;
	// DISSLW(0x10) | MIRROR(0x40) | INTPOL(0x02); ODR stays clear because
	// an active-driver INT is what makes a polarity meaningful at all.
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_A_017, 0x52, 0);

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

void test_mcp230xx_init_fails_when_the_reset_is_short_on_the_wire(void)
{
	// The device acknowledged one byte fewer than the block that was sent.
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
	fake_i2c_write_op.retval = RESET_LEN_008;
	i2c_write8_8b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, IOCON_008, INIT_RESTART_IOCON, -1);

	TEST_ASSERT_NULL(init_mcp(true,
				  MCP230XX_008,
				  false,
				  MCP230XX_ACTIVE_DRIVER_INT,
				  MCP230XX_INT_ACTIVE_LOW,
				  MCP230XX_NO_MIRRORED_INT));
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

							const uint8_t base =
								restart ? 0x00 :
									  0xFF;
							if (restart) {
								fake_i2c_write_op
									.retval =
									(ssize_t)reset_len_of(
										type);
							} else {
								expect_i2c_read8_8b(
									iocon_addr_of(
										type),
									base,
									0);
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
									mirror),
								0);

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

	// Every combination of the bits a restart=false init must preserve.
	for (uint8_t untouched = 0; untouched <= IOCON_REG_UNTOUCHED;
	     untouched++) {
		if ((untouched & IOCON_REG_UNTOUCHED) != untouched) {
			continue; // Not one of the preserved bits.
		}

		expect_i2c_read8_8b(IOCON_008, untouched, 0);
		i2c_write8_8b_ExpectAndReturn(TEST_I2C,
					      TEST_ADDR,
					      IOCON_008,
					      untouched | IOCON_REG_DISSLW,
					      0);

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

	TEST_ASSERT_EQUAL_INT(0, mcp230xx_deinit(TEST_I2C, mcp, true));

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

/* -------------------------- mcp230xx_protect ------------------------------- */

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
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_deinit(TEST_I2C, mcp, false));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	// Back on the right bus, the same handle still works.
	fake_i2c_bus = TEST_BUS;
	destroy_mcp(mcp);
}

/* ------------------------- mcp230xx_unprotect ------------------------------ */

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

	// Anything but NO_PULLUP/PULLUP is a mistake, not a request for a pull-up.
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(
			TEST_I2C, mcp, 0, (MCP230XX_INPUT_CONFIG)2, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_writes_gppu_before_iodir(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Force distinct contents in IODIR and GPPU registers
	expect_i2c_read8_8b(IODIR_008, 0x10, 0);
	expect_i2c_read8_8b(GPPU_008, 0x40, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x48, 0);
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, IODIR_008, 0x18, 0);

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_set_input(TEST_I2C, mcp, 3, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_leaves_the_other_pins_alone(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	// Every pin but 1 is already an input; pins 0, 2 and 3 already have
	// pull-ups. Only pin 1's bit may change in either register.
	expect_i2c_read8_8b(IODIR_008, 0xFD, 0);
	expect_i2c_read8_8b(GPPU_008, 0x0D, 0);
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
	i2c_write8_8b_ExpectAndReturn(TEST_I2C, TEST_ADDR, GPPU_008, 0x21, -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		mcp230xx_set_input(TEST_I2C, mcp, 0, MCP230XX_PULLUP, 1000));

	destroy_mcp(mcp);
}

void test_mcp230xx_set_input_fails_when_writing_the_iodir_reg_fails(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	expect_i2c_read8_8b(IODIR_008, 0x10, 0);
	expect_i2c_read8_8b(GPPU_008, 0x20, 0);
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
	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_read_gpio(TEST_I2C, mcp, 4, &value, 1000));
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

	TEST_ASSERT_EQUAL_INT(0, mcp230xx_set_output(TEST_I2C, mcp, 5, 1000));

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

	TEST_ASSERT_EQUAL_INT(
		0, mcp230xx_write_gpio(TEST_I2C, mcp, 2, MCP230XX_HIGH, 1000));

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
			const uint8_t iodir_mark =
				(uint8_t)(1 << ((index + 1) % 8));
			const uint8_t gppu_mark =
				(uint8_t)(1 << ((index + 2) % 8));
			expect_i2c_read8_8b(iodir, iodir_mark, 0);
			expect_i2c_read8_8b(gppu, gppu_mark, 0);
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

void test_mcp230xx_deinit_also_unprotects_when_protected(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(0, mcp230xx_deinit(TEST_I2C, mcp, false));
}

void test_mcp230xx_deinit_fails_when_the_unprotect_fails(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_deinit(TEST_I2C, mcp, false));

	free(mcp); // deinit bailed out before freeing it
}

void test_mcp230xx_protect_fails_with_einval_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_protect(TEST_I2C, NULL));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_mcp230xx_protect_adds_the_resource_for_its_bus_and_address(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(0, mcp230xx_protect(TEST_I2C, mcp));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_protect_fails_when_the_bus_cant_be_read(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_protect(TEST_I2C, mcp));

	destroy_mcp(mcp);
}

void test_mcp230xx_protect_fails_when_the_resource_cant_be_added(void)
{
	mcp230xx_t* mcp = create_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_protect(TEST_I2C, mcp));

	destroy_mcp(mcp);
}

void test_mcp230xx_protect_returns_1_if_already_protected(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(1, mcp230xx_protect(TEST_I2C, mcp));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_unprotect_fails_with_einval_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_unprotect(NULL));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_mcp230xx_unprotect_removes_the_resource(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(0, mcp230xx_unprotect(mcp));

	destroy_mcp(mcp);
}

void test_mcp230xx_unprotect_returns_1_if_already_unprotected(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(1, mcp230xx_unprotect(mcp));

	destroy_mcp(mcp);
}

void test_mcp230xx_unprotect_fails_when_the_resource_cant_be_removed(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	TEST_ASSERT_EQUAL_INT(-1, mcp230xx_unprotect(mcp));

	destroy_protected_mcp(mcp);
}

void test_mcp230xx_set_input_when_protected_locks_and_unlocks(void)
{
	mcp230xx_t* mcp = create_protected_mcp(MCP230XX_008);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_i2c_read8_8b(IODIR_008, 0xFF, 0);
	expect_i2c_read8_8b(GPPU_008, 0x81, 0);
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
