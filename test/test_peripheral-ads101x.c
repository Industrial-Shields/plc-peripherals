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
 * i2c_get_bus is mocked too: ads101x_protect calls it directly, one layer
 * below plc-peripherals-i2c.h.
 *
 * usleep() between a config write and the following conversion read is not
 * mocked; it's a real (short) sleep. Every test that exercises it picks a
 * fast data rate (ADS101X_2400SPS or faster) to keep the suite quick, except
 * the one test that deliberately measures elapsed wall-clock time to confirm
 * ads101x_continuous_read waits out both the old and the new conversion
 * period on a combined channel/rate change.
 */

#include "unity.h"

#include "mock_plc-mutex.h"

#include "mock_plc-peripherals-i2c-hal.h"
#include "mock_plc-peripherals-i2c.h"
#include "peripheral-ads101x.h"

// For the shared i2c_get_bus / i2c_check_bus stubs
#include "fake-i2c.h"

#include <errno.h>
#include <stdlib.h>
#include <time.h>

#define TEST_I2C ((i2c_interface_t*)0x1)
#define TEST_ADDR ((plc_i2c_addr_t)0x48)
#define TEST_BUS ((uint8_t)3)

#define CONVERSION_REG 0x00
#define CONFIG_REG 0x01
#define CONFIG_REG_OS 0x8000
#define CONFIG_REG_MUX 0x7000
#define CONFIG_REG_MUX_SHIFT 12
#define CONFIG_REG_PGA 0xE00
#define CONFIG_REG_PGA_SHIFT 9
#define CONFIG_REG_MODE 0x100
#define CONFIG_REG_DR 0xE0
#define CONFIG_REG_DR_SHIFT 5
#define CONFIG_REG_COMP 0x1F // COMP_MODE|COMP_POL|COMP_LAT|COMP_QUE
#define CONFIG_REG_RESET_VALUE 0x8583
#define LOW_THRESHOLD_REG 0x02
#define HIGH_THRESHOLD_REG 0x03
#define LOW_THRESHOLD_REG_RESET_VALUE 0x8000
#define HIGH_THRESHOLD_REG_RESET_VALUE 0x7FFF

/*
 * A data rate fast enough (~479us) that the real usleep() in
 * ads101x_delay_until_conversion doesn't slow the suite down.
 */
#define FAST_DR ADS101X_2400SPS
/*
 * The slowest available data rate (~8984us), used only by the one test that
 * deliberately measures elapsed time.
 */
#define SLOW_DR ADS101X_128SPS

/*
 * CONFIG_REG value ads101x_init(restart=true) writes for
 * fsr=ADS101X_FSR_4_096V (0b001), dr=FAST_DR (0b101):
 *   OS=1 (0x8000) | MUX=000 (unchanged from reset) | PGA=001<<9 (0x200) |
 *   MODE=1 (0x100, single-shot) | DR=101<<5 (0xA0) | COMP bits=000_11
 *   (unchanged from CONFIG_REG_RESET_VALUE's 0x3)
 */
#define INIT_RESTART_CFG_SINGLE 0x83A3 // 0x8000 | 0x200 | 0x100 | 0xA0 | 3

// ADS101x init methods write the single CFG as RESTART minus OS bit
#define INIT_NO_RESTART_CFG_SINGLE (INIT_RESTART_CFG_SINGLE & ~CONFIG_REG_OS)

/*
 * Continuous mode additionally clears OS (0x8000) on top of MODE (0x100),
 * unlike single mode's cfg above: 0x83A3 & ~0x100 & ~0x8000.
 */
#define INIT_RESTART_CFG_CONTINUOUS 0x02A3

/*
 * CMock copies a ReturnThruPtr value when the mocked call happens, not when it
 * is queued, so each register needs its own slot for a test to queue reads of
 * different registers.
 */
static uint16_t read_values[HIGH_THRESHOLD_REG + 1];

static void expect_i2c_read8_16b(uint8_t reg, uint16_t value, int retval)
{
	i2c_read8_16b_ExpectAndReturn(TEST_I2C, TEST_ADDR, reg, NULL, retval);
	i2c_read8_16b_IgnoreArg_to_read();
	if (retval == 0) {
		read_values[reg] = value;
		i2c_read8_16b_ReturnThruPtr_to_read(&read_values[reg]);
	}
}

static long elapsed_ms(struct timespec start, struct timespec end)
{
	return (end.tv_sec - start.tv_sec) * 1000L +
	       (end.tv_nsec - start.tv_nsec) / 1000000L;
}

static long elapsed_us(struct timespec start, struct timespec end)
{
	return (end.tv_sec - start.tv_sec) * 1000000L +
	       (end.tv_nsec - start.tv_nsec) / 1000L;
}

static void expect_init_config_read(void)
{
	expect_i2c_read8_16b(CONFIG_REG, CONFIG_REG_RESET_VALUE, 0);
}

static void expect_ads101x_restart(void)
{
	expect_init_config_read();
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

// Calls ads101x_init on the test bus and address, filling in the config.
static ads101x_t* init_ads(bool restart,
			   bool continuous_mode,
			   ADS101X_GAIN_AMPLIFIER fsr,
			   ADS101X_DATA_RATE dr)
{
	ads101x_config_t cfg;

	cfg.continuous_mode = continuous_mode;
	cfg.fsr = fsr;
	cfg.dr = dr;
	return ads101x_init(TEST_I2C, TEST_ADDR, restart, &cfg);
}

// Creates and initializes an ads101x_t via restart=true, fsr=4.096V, dr=fast.
static ads101x_t* create_ads(bool continuous)
{
	expect_ads101x_restart();
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       continuous ?
					       INIT_RESTART_CFG_CONTINUOUS :
					       INIT_NO_RESTART_CFG_SINGLE,
				       0);

	ads101x_t* ads =
		init_ads(true, continuous, ADS101X_FSR_4_096V, FAST_DR);
	TEST_ASSERT_NOT_NULL(ads);
	return ads;
}

static void destroy_ads(ads101x_t* ads)
{
	TEST_ASSERT_EQUAL_INT(0, ads101x_deinit(TEST_I2C, ads, false));
}

#define TEST_SCOPE PLC_MUTEX_SCOPE_PRIVATE

static ads101x_t* create_protected_ads(bool continuous)
{
	ads101x_t* ads = create_ads(continuous);

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(0, ads101x_protect(TEST_I2C, ads, TEST_SCOPE));
	return ads;
}

static void expect_mutex_destroyed(int retval)
{
	plc_mutex_static_destroy_ExpectAndReturn(NULL, retval);
	plc_mutex_static_destroy_IgnoreArg_mutex();
}

static void destroy_protected_ads(ads101x_t* ads)
{
	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, ads101x_deinit(TEST_I2C, ads, false));
}

static void expect_mutex_released(void)
{
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();
}

// Only the first acquire finds that the previous owner died.
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
	fake_i2c_bus = TEST_BUS;
	fake_i2c_bus_retval = 0;
	i2c_get_bus_Stub(fake_i2c_get_bus);
	i2c_check_bus_Stub(fake_i2c_check_bus);
}

void tearDown(void)
{
}

/* ---------------------------- ads101x_init -------------------------------- */

void test_ads101x_init_with_restart_packs_config_reg_in_single_mode(void)
{
	expect_ads101x_restart();
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_NO_RESTART_CFG_SINGLE, 0);

	ads101x_t* ads = init_ads(true, false, ADS101X_FSR_4_096V, FAST_DR);

	TEST_ASSERT_NOT_NULL(ads);
	destroy_ads(ads);
}

void test_ads101x_init_with_restart_packs_config_reg_in_continuous_mode(void)
{
	expect_ads101x_restart();
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_CONTINUOUS,
				       0);

	ads101x_t* ads = init_ads(true, true, ADS101X_FSR_4_096V, FAST_DR);

	TEST_ASSERT_NOT_NULL(ads);
	destroy_ads(ads);
}

void test_ads101x_init_without_restart_reads_then_patches_config_reg(void)
{
	// Device already has some COMP bits set; they must survive untouched.
	expect_i2c_read8_16b(CONFIG_REG, 0x0003, 0);
	// MODE(0x100) | PGA(3<<9=0x600) | DR(FAST_DR=5<<5=0xA0) | preserved
	// COMP(0x3).
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x07A3, 0);

	ads101x_t* ads = init_ads(false, false, ADS101X_FSR_1_024V, FAST_DR);

	TEST_ASSERT_NOT_NULL(ads);
	destroy_ads(ads);
}

void test_ads101x_init_without_restart_in_continuous_mode_clears_os_and_mode_bits(
	void)
{
	// Device was left configured for single mode (OS=1, MODE=1); an init
	// with continuous_mode=true must clear both, regardless of
	// whatever was on the wire before.
	expect_i2c_read8_16b(CONFIG_REG, 0x8103, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x0003, 0);

	ads101x_t* ads =
		init_ads(false, true, ADS101X_FSR_6_144V, ADS101X_128SPS);

	TEST_ASSERT_NOT_NULL(ads);
	destroy_ads(ads);
}

/*
 * The init functions don't write FSR and DR values that are not part of the
 * enum, even though it silently accepts them.
 */
static uint16_t written_pga(uint16_t fsr)
{
	return fsr >= 0b110 ? ADS101X_FSR_0_256V : fsr;
}

static uint16_t written_dr(uint16_t dr)
{
	return dr == 0b111 ? ADS101X_3300SPS : dr;
}

void test_ads101x_init_fuzzes_every_reachable_configuration_register_combination(
	void)
{
	/*
	 * Exhaustively covers every CONFIG_REG value ads101x_init can
	 * produce, across both entry paths (restart=true/false).
	 *
	 * When restart=false we don't vary continuous_mode/fsr/dr. Doing
	 * so would only add cost without coverage, since ads101x_init's OS/MODE/
	 * PGA/DR overwrite is proven for every value of those already by the
	 * restart=true sweep above.
	 */

	// restart = true
	for (int continuous = 0; continuous <= 1; continuous++) {
		for (uint16_t fsr = 0; fsr <= 0b111; fsr++) {
			for (uint16_t dr = 0; dr <= 0b111; dr++) {
				expect_ads101x_restart();

				uint16_t cfg = CONFIG_REG_RESET_VALUE;
				if (continuous) {
					cfg &= (uint16_t)~(CONFIG_REG_MODE |
							   CONFIG_REG_OS);
				} else {
					cfg |= CONFIG_REG_MODE | CONFIG_REG_OS;
				}
				cfg = (uint16_t)((cfg & ~CONFIG_REG_PGA) |
						 (written_pga(fsr)
						  << CONFIG_REG_PGA_SHIFT));
				cfg = (uint16_t)((cfg & ~CONFIG_REG_DR) |
						 (written_dr(dr)
						  << CONFIG_REG_DR_SHIFT));
				// Init never starts a single-shot conversion.
				if (!continuous) {
					cfg &= (uint16_t)~CONFIG_REG_OS;
				}

				i2c_write8_16b_ExpectAndReturn(TEST_I2C,
							       TEST_ADDR,
							       CONFIG_REG,
							       cfg,
							       0);

				ads101x_t* ads =
					init_ads(true,
						 continuous,
						 (ADS101X_GAIN_AMPLIFIER)fsr,
						 (ADS101X_DATA_RATE)dr);
				TEST_ASSERT_NOT_NULL(ads);
				destroy_ads(ads);
			}
		}
	}

	// restart = false
	for (uint16_t mux = 0; mux <= 0b111; mux++) {
		for (uint16_t comp = 0; comp <= CONFIG_REG_COMP; comp++) {
			uint16_t initial =
				(uint16_t)((mux << CONFIG_REG_MUX_SHIFT) |
					   comp | CONFIG_REG_OS |
					   CONFIG_REG_PGA | CONFIG_REG_MODE |
					   CONFIG_REG_DR);
			expect_i2c_read8_16b(CONFIG_REG, initial, 0);

			// Single mode, written without OS
			uint16_t cfg =
				(uint16_t)((mux << CONFIG_REG_MUX_SHIFT) |
					   comp | CONFIG_REG_MODE);
			cfg = (uint16_t)((cfg & ~CONFIG_REG_PGA) |
					 (ADS101X_FSR_4_096V
					  << CONFIG_REG_PGA_SHIFT));
			cfg = (uint16_t)((cfg & ~CONFIG_REG_DR) |
					 (FAST_DR << CONFIG_REG_DR_SHIFT));

			i2c_write8_16b_ExpectAndReturn(
				TEST_I2C, TEST_ADDR, CONFIG_REG, cfg, 0);

			ads101x_t* ads = init_ads(
				false, false, ADS101X_FSR_4_096V, FAST_DR);
			TEST_ASSERT_NOT_NULL(ads);
			destroy_ads(ads);
		}
	}
}

void test_ads101x_init_fails_with_efault_for_a_null_config(void)
{
	errno = 0;
	TEST_ASSERT_NULL(ads101x_init(TEST_I2C, TEST_ADDR, true, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_init_fails_when_the_high_threshold_reset_fails(void)
{
	expect_init_config_read();
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       HIGH_THRESHOLD_REG,
				       HIGH_THRESHOLD_REG_RESET_VALUE,
				       -1);

	TEST_ASSERT_NULL(init_ads(true, false, ADS101X_FSR_4_096V, FAST_DR));
}

void test_ads101x_init_fails_when_the_low_threshold_reset_fails(void)
{
	expect_init_config_read();
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

	TEST_ASSERT_NULL(init_ads(true, false, ADS101X_FSR_4_096V, FAST_DR));
}

void test_ads101x_init_fails_when_reading_the_config_reg_fails(void)
{
	expect_i2c_read8_16b(CONFIG_REG, 0, -1);

	TEST_ASSERT_NULL(init_ads(false, false, ADS101X_FSR_4_096V, FAST_DR));
}

void test_ads101x_init_fails_when_writing_the_config_reg_fails(void)
{
	expect_ads101x_restart();
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_NO_RESTART_CFG_SINGLE,
				       -1);

	TEST_ASSERT_NULL(init_ads(true, false, ADS101X_FSR_4_096V, FAST_DR));
}

void test_ads101x_init_fails_with_einval_for_an_invalid_fsr(void)
{
	// No I2C transfer is expected.
	errno = 0;
	TEST_ASSERT_NULL(
		init_ads(true, false, (ADS101X_GAIN_AMPLIFIER)0b1000, FAST_DR));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_ads101x_init_fails_with_einval_for_an_invalid_dr(void)
{
	// No I2C transfer is expected.
	errno = 0;
	TEST_ASSERT_NULL(init_ads(
		true, false, ADS101X_FSR_4_096V, (ADS101X_DATA_RATE)0b1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

/* --------------------------- ads101x_deinit ------------------------------- */

void test_ads101x_deinit_fails_with_efault_for_null_ads(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_deinit(TEST_I2C, NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_deinit_without_shutdown_just_frees(void)
{
	ads101x_t* ads = create_ads(false);
	TEST_ASSERT_EQUAL_INT(0, ads101x_deinit(TEST_I2C, ads, false));
}

void test_ads101x_deinit_with_shutdown_sets_the_mode_bit_and_writes_it_back(void)
{
	ads101x_t* ads = create_ads(true); // starts in continuous (MODE=0)

	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_CONTINUOUS | 0x100,
				       0);

	TEST_ASSERT_EQUAL_INT(0, ads101x_deinit(TEST_I2C, ads, true));
}

void test_ads101x_deinit_fuzzes_every_reachable_configuration_register_combination(
	void)
{
	/*
	 * ads101x_deinit(TEST_I2C, shutdown=true) only ORs MODE into expected_cfg_reg
	 * and writes it back, untouched otherwise.
	 *
	 * Single mode: Sweeps every (MUX, PGA, DR, COMP) to prove every other
	 * bit survives.
	 *
	 * Continuous mode: PGA/DR are fixed since that's already proven above;
	 * only MUX/COMP vary.
	 */

	// single mode
	for (uint16_t mux = 0; mux <= 0b111; mux++) {
		for (uint16_t pga = 0; pga <= 0b111; pga++) {
			for (uint16_t dr = 0; dr <= 0b111; dr++) {
				for (uint16_t comp = 0; comp <= CONFIG_REG_COMP;
				     comp++) {
					uint16_t cfg =
						(uint16_t)(CONFIG_REG_OS |
							   CONFIG_REG_MODE |
							   (mux
							    << CONFIG_REG_MUX_SHIFT) |
							   (pga
							    << CONFIG_REG_PGA_SHIFT) |
							   (dr
							    << CONFIG_REG_DR_SHIFT) |
							   comp);
					expect_i2c_read8_16b(
						CONFIG_REG, cfg, 0);
					cfg = (uint16_t)((cfg &
							  ~(CONFIG_REG_PGA |
							    CONFIG_REG_DR)) |
							 (written_pga(pga)
							  << CONFIG_REG_PGA_SHIFT) |
							 (written_dr(dr)
							  << CONFIG_REG_DR_SHIFT));

					// Init writes it without OS.
					i2c_write8_16b_ExpectAndReturn(
						TEST_I2C,
						TEST_ADDR,
						CONFIG_REG,
						cfg & (uint16_t)~CONFIG_REG_OS,
						0);

					ads101x_t* ads = init_ads(
						false,
						false,
						(ADS101X_GAIN_AMPLIFIER)pga,
						(ADS101X_DATA_RATE)dr);
					TEST_ASSERT_NOT_NULL(ads);

					i2c_write8_16b_ExpectAndReturn(
						TEST_I2C,
						TEST_ADDR,
						CONFIG_REG,
						cfg,
						0);
					TEST_ASSERT_EQUAL_INT(
						0,
						ads101x_deinit(
							TEST_I2C, ads, true));
				}
			}
		}
	}

	// continuous mode
	for (uint16_t mux = 0; mux <= 0b111; mux++) {
		for (uint16_t comp = 0; comp <= CONFIG_REG_COMP; comp++) {
			uint16_t cfg =
				(uint16_t)((mux << CONFIG_REG_MUX_SHIFT) |
					   (ADS101X_FSR_4_096V
					    << CONFIG_REG_PGA_SHIFT) |
					   (FAST_DR << CONFIG_REG_DR_SHIFT) |
					   comp);
			expect_i2c_read8_16b(CONFIG_REG, cfg, 0);

			i2c_write8_16b_ExpectAndReturn(
				TEST_I2C, TEST_ADDR, CONFIG_REG, cfg, 0);

			ads101x_t* ads = init_ads(
				false, true, ADS101X_FSR_4_096V, FAST_DR);
			TEST_ASSERT_NOT_NULL(ads);

			i2c_write8_16b_ExpectAndReturn(TEST_I2C,
						       TEST_ADDR,
						       CONFIG_REG,
						       cfg | CONFIG_REG_MODE,
						       0);
			TEST_ASSERT_EQUAL_INT(
				0, ads101x_deinit(TEST_I2C, ads, true));
		}
	}
}

void test_ads101x_deinit_fails_when_writing_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false); // MODE already set; OR is a no-op

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, -1);

	TEST_ASSERT_EQUAL_INT(-1, ads101x_deinit(TEST_I2C, ads, true));
	free(ads); // deinit bailed out before freeing it
}

/* ------------------------------ bus checks -------------------------------- */

/* ------------------ ads101x_static_init / static_deinit ------------------- */

// The same configuration create_ads uses, in single-shot mode.
static void fill_single_shot_cfg(ads101x_config_t* cfg)
{
	cfg->continuous_mode = false;
	cfg->fsr = ADS101X_FSR_4_096V;
	cfg->dr = FAST_DR;
}

// Rounds arena up to the next ADS101X_ALIGN boundary.
static unsigned char* align_up(unsigned char* arena)
{
	uintptr_t base = (uintptr_t)arena;

	return (unsigned char*)((base + ADS101X_ALIGN - 1) &
				~(uintptr_t)(ADS101X_ALIGN - 1));
}

void test_ads101x_static_init_and_static_deinit_use_the_callers_storage(void)
{
	// Static storage: if static_deinit tried to free it, glibc would abort.
	static ads101x_t storage;
	ads101x_config_t cfg;
	fill_single_shot_cfg(&cfg);

	expect_ads101x_restart();
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_NO_RESTART_CFG_SINGLE, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(&storage, &dr, 0));
	TEST_ASSERT_EQUAL_INT(FAST_DR, dr);

	// Single-shot mode already has MODE set, so shutdown rewrites the same.
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	TEST_ASSERT_EQUAL_INT(0,
			      ads101x_static_deinit(TEST_I2C, &storage, true));
}

void test_ads101x_static_init_can_reuse_the_storage_after_static_deinit(void)
{
	static ads101x_t storage;
	ads101x_config_t cfg;
	fill_single_shot_cfg(&cfg);

	for (int round = 0; round < 2; round++) {
		expect_ads101x_restart();
		i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					       TEST_ADDR,
					       CONFIG_REG,
					       INIT_NO_RESTART_CFG_SINGLE,
					       0);
		TEST_ASSERT_EQUAL_INT(
			0,
			ads101x_static_init(
				TEST_I2C, &storage, TEST_ADDR, true, &cfg));
		TEST_ASSERT_EQUAL_INT(
			0, ads101x_static_deinit(TEST_I2C, &storage, false));
	}
}

void test_ads101x_static_init_accepts_an_aligned_address_in_a_buffer(void)
{
	// The embedded case: a handle carved out of a region the caller owns.
	static unsigned char arena[sizeof(ads101x_t) + ADS101X_ALIGN];
	ads101x_t* aligned = (ads101x_t*)align_up(arena);
	ads101x_config_t cfg;
	fill_single_shot_cfg(&cfg);

	TEST_ASSERT_EQUAL_INT(0, (uintptr_t)aligned % ADS101X_ALIGN);

	expect_ads101x_restart();
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_NO_RESTART_CFG_SINGLE, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_static_init(TEST_I2C, aligned, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(0,
			      ads101x_static_deinit(TEST_I2C, aligned, false));
}

void test_ads101x_static_init_fails_with_efault_for_misaligned_storage(void)
{
	// One byte past an aligned address is never aligned (ADS101X_ALIGN > 1).
	static unsigned char arena[sizeof(ads101x_t) + 2 * ADS101X_ALIGN];
	ads101x_t* misaligned = (ads101x_t*)(align_up(arena) + 1);
	ads101x_config_t cfg;
	fill_single_shot_cfg(&cfg);

	TEST_ASSERT_NOT_EQUAL_INT(0, (uintptr_t)misaligned % ADS101X_ALIGN);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_static_init(
			TEST_I2C, misaligned, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_static_init_fails_with_efault_for_null_storage(void)
{
	ads101x_config_t cfg;
	fill_single_shot_cfg(&cfg);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_static_init(TEST_I2C, NULL, TEST_ADDR, true, &cfg));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_static_init_fails_with_efault_for_a_null_config(void)
{
	static ads101x_t storage;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_static_init(TEST_I2C, &storage, TEST_ADDR, true, NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_static_init_fails_when_the_i2c_transfer_fails(void)
{
	static ads101x_t storage;
	ads101x_config_t cfg;
	fill_single_shot_cfg(&cfg);

	expect_init_config_read();
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       HIGH_THRESHOLD_REG,
				       HIGH_THRESHOLD_REG_RESET_VALUE,
				       -1);

	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));
}

void test_ads101x_static_deinit_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_static_deinit(TEST_I2C, NULL, false));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_static_deinit_fails_with_einval_for_another_bus(void)
{
	static ads101x_t storage;
	ads101x_config_t cfg;
	fill_single_shot_cfg(&cfg);

	expect_ads101x_restart();
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_NO_RESTART_CFG_SINGLE, 0);
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_static_init(TEST_I2C, &storage, TEST_ADDR, true, &cfg));

	fake_i2c_bus = TEST_BUS + 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      ads101x_static_deinit(TEST_I2C, &storage, false));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	fake_i2c_bus = TEST_BUS;
	TEST_ASSERT_EQUAL_INT(0,
			      ads101x_static_deinit(TEST_I2C, &storage, false));
}

void test_ads101x_rejects_an_interface_for_another_bus(void)
{
	ads101x_t* ads = create_ads(false);

	fake_i2c_bus = TEST_BUS + 1;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(
			TEST_I2C, ads, ADS101X_P0_GND, &(int16_t){ 0 }, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_set_fs(TEST_I2C, ads, ADS101X_1600SPS, 0));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_deinit(TEST_I2C, ads, false));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	// Back on the right bus, the same handle still works.
	fake_i2c_bus = TEST_BUS;
	destroy_ads(ads);
}

/* ------------------------- ads101x_single_read ----------------------------- */

void test_ads101x_single_read_fails_with_einval_for_an_invalid_index(void)
{
	ads101x_t* ads = create_ads(false);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(
			TEST_I2C, ads, (ADS101X_INPUT)0b1000, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_ads(ads);
}

void test_ads101x_continuous_read_fails_with_einval_for_an_invalid_index(void)
{
	ads101x_t* ads = create_ads(true);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_continuous_read(
			TEST_I2C, ads, (ADS101X_INPUT)0b1000, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_ads(ads);
}

void test_ads101x_set_fs_fails_with_einval_for_an_invalid_dr(void)
{
	ads101x_t* ads = create_ads(true);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_set_fs(TEST_I2C, ads, (ADS101X_DATA_RATE)0b1000, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr, 1000));
	TEST_ASSERT_EQUAL_INT(FAST_DR, dr);

	destroy_ads(ads);
}

void test_ads101x_single_read_returns_a_positive_reading(void)
{
	ads101x_t* ads = create_ads(false);

	// CHANGE_CHANNEL(P0_N1=0) keeps MUX=0; every call writes CONFIG_REG
	// again (no CONFIG_REG read anymore -- the value is cached).
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT16(0x0FF, value);

	destroy_ads(ads);
}

void test_ads101x_single_read_selects_the_requested_channel(void)
{
	ads101x_t* ads = create_ads(false);

	// MUX bits (14-12) cleared then set to P1_N3's index (1): bit12 set.
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_SINGLE | 0x1000,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P1_N3, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_single_read_switches_correctly_between_two_nonzero_channels(
	void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_SINGLE | 0x1000,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P1_N3, &value, 1000));

	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_SINGLE | 0x2000,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P2_N3, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_single_read_fails_when_writing_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_single_read_fails_when_reading_the_conversion_reg_fails(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_single_read_handles_the_maximally_negative_reading(void)
{
	// Regression test for avoiding an implementation-defined right shift
	// on a negative int16_t: 0x8000 must map to -2048, the most negative
	// value the 12-bit conversion result can represent.
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x8000, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT16(-2048, value);

	destroy_ads(ads);
}

void test_ads101x_single_read_fails_with_efault_for_null_ads(void)
{
	int16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(
			TEST_I2C, NULL, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_single_read_fails_with_efault_for_null_return_value(void)
{
	ads101x_t* ads = create_ads(false);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ads(ads);
}

void test_ads101x_single_read_fails_with_einval_when_device_is_continuous_mode(
	void)
{
	ads101x_t* ads = create_ads(true);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_ads(ads);
}

/* --------------------- ads101x_unsigned_single_read ------------------------ */

void test_ads101x_unsigned_single_read_fails_with_efault_for_null_ads(void)
{
	uint16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_single_read(
			TEST_I2C, NULL, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_unsigned_single_read_fails_with_efault_for_null_return_value(
	void)
{
	ads101x_t* ads = create_ads(false);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_single_read(
			TEST_I2C, ads, ADS101X_P0_N1, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ads(ads);
}

void test_ads101x_unsigned_single_read_passes_through_a_positive_reading(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0); // -> +255

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_unsigned_single_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(255, value);

	destroy_ads(ads);
}

void test_ads101x_unsigned_single_read_clamps_a_small_negative_reading_to_0(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0xFFF0, 0); // -> -1

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_unsigned_single_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(0, value);

	destroy_ads(ads);
}

// Regression test for the -8 threshold itself...
void test_ads101x_unsigned_single_read_clamps_exactly_minus_8_to_0(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0xFF80, 0);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_unsigned_single_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(0, value);

	destroy_ads(ads);
}

// ... and the other side of the same boundary.
void test_ads101x_unsigned_single_read_fails_with_erange_at_minus_9(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0xFF70, 0); // -> -9

	uint16_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_single_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	destroy_ads(ads);
}

void test_ads101x_unsigned_single_read_fails_with_erange_below_minus_8(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0xFF00, 0); // -> -16

	uint16_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_single_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	destroy_ads(ads);
}

void test_ads101x_unsigned_single_read_propagates_a_single_read_failure(void)
{
	ads101x_t* ads = create_ads(false);

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, -1);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_single_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_ads(ads);
}

/* ----------------------- ads101x_continuous_read ---------------------------- */

void test_ads101x_continuous_read_skips_the_write_on_the_same_channel(void)
{
	ads101x_t* ads = create_ads(true); // starts on MUX=P0_N1 (0)

	// No CONFIG_REG write and no delay: the channel didn't change.
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT16(0x0FF, value);

	destroy_ads(ads);
}

void test_ads101x_continuous_read_writes_on_a_channel_change(void)
{
	ads101x_t* ads = create_ads(true);

	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_CONTINUOUS | 0x1000,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P1_N3, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_continuous_read_writes_again_after_returning_to_a_previous_channel(
	void)
{
	/*
	 * If continuous_read never updated last_cfg_reg after writing, it would
	 * stay stuck at whatever it was at init, so switching to a new channel
	 * and then switching BACK to the original one would wrongly look like
	 * no change, and no write would be issued
	 */
	ads101x_t* ads = create_ads(true);

	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_CONTINUOUS | 0x1000,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P1_N3, &value, 1000));

	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_CONTINUOUS,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_continuous_read_reads_directly_after_set_fs_already_settled_the_rate(
	void)
{
	/*
	 * ads101x_set_fs now writes CONFIG_REG and waits out the switch
	 * itself in continuous mode, syncing
	 * last_cfg_reg before returning.
	 */
	ads101x_t* ads = create_ads(true);

	// DR bits (7-5) replaced: 0xA0 (FAST_DR) -> 0x60 (920SPS).
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       (INIT_RESTART_CFG_CONTINUOUS & ~0xE0) |
					       0x60,
				       0);
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_set_fs(TEST_I2C, ads, ADS101X_920SPS, 1000));

	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_continuous_read_fails_when_writing_the_config_reg_fails(void)
{
	ads101x_t* ads = create_ads(true);

	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_CONTINUOUS | 0x1000,
				       -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P1_N3, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_continuous_read_fails_when_reading_the_conversion_reg_fails(
	void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONVERSION_REG, 0, -1);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_continuous_read_fails_with_efault_for_null_ads(void)
{
	int16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_continuous_read(
			TEST_I2C, NULL, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_continuous_read_fails_with_efault_for_null_return_value(void)
{
	ads101x_t* ads = create_ads(true);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ads(ads);
}

void test_ads101x_continuous_read_fails_with_einval_when_device_is_single_mode(
	void)
{
	ads101x_t* ads = create_ads(false);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	destroy_ads(ads);
}

/* -------------------- ads101x_unsigned_continuous_read ---------------------- */

void test_ads101x_unsigned_continuous_read_fails_with_efault_for_null_ads(void)
{
	uint16_t value;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_continuous_read(
			TEST_I2C, NULL, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_unsigned_continuous_read_fails_with_efault_for_null_return_value(
	void)
{
	ads101x_t* ads = create_ads(true);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ads(ads);
}

void test_ads101x_unsigned_continuous_read_passes_through_a_positive_reading(
	void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_unsigned_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_UINT16(255, value);

	destroy_ads(ads);
}

void test_ads101x_unsigned_continuous_read_fails_with_erange_below_minus_8(void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONVERSION_REG, 0xFF00, 0); // -> -16

	uint16_t value;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(ERANGE, errno);

	destroy_ads(ads);
}

void test_ads101x_unsigned_continuous_read_propagates_a_continuous_read_failure(
	void)
{
	ads101x_t* ads = create_ads(true);

	expect_i2c_read8_16b(CONVERSION_REG, 0, -1);

	uint16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_unsigned_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_ads(ads);
}

/* --------------------------- ads101x_get_fs -------------------------------- */

void test_ads101x_get_fs_fails_with_efault_for_null_ads(void)
{
	ADS101X_DATA_RATE dr;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_get_fs(NULL, &dr, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_get_fs_fails_with_efault_for_null_dr(void)
{
	ads101x_t* ads = create_ads(false);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_get_fs(ads, NULL, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);

	destroy_ads(ads);
}

void test_ads101x_get_fs_returns_the_current_data_rate(void)
{
	ads101x_t* ads = create_ads(false);

	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr, 1000));
	TEST_ASSERT_EQUAL_INT(FAST_DR, dr);

	destroy_ads(ads);
}

void test_ads101x_get_fs_maps_the_0b111_encoding_to_3300sps(void)
{
	ads101x_t* ads = create_ads(false);
	// Nothing in the public API can request DR=0b111 (it's a duplicate
	// 3300SPS encoding, not one of the named enumerators), but the field
	// is only 3 bits wide and set_fs doesn't validate its input.
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_set_fs(TEST_I2C, ads, (ADS101X_DATA_RATE)0b111, 1000));

	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr, 1000));
	TEST_ASSERT_EQUAL_INT(ADS101X_3300SPS, dr);

	destroy_ads(ads);
}

/* --------------------------- ads101x_set_fs -------------------------------- */

void test_ads101x_set_fs_fails_with_efault_for_null_ads(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      ads101x_set_fs(TEST_I2C, NULL, FAST_DR, 1000));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_set_fs_patches_only_the_dr_bits(void)
{
	ads101x_t* ads = create_ads(false);

	TEST_ASSERT_EQUAL_INT(
		0, ads101x_set_fs(TEST_I2C, ads, ADS101X_920SPS, 1000));

	/*
	 * Observe the effect through single_read's CONFIG_REG write: only the
	 * DR bits (0xA0 -> 0x60) should differ from INIT_RESTART_CFG_SINGLE;
	 * OS/MUX/PGA/MODE/COMP must survive untouched.
	 */
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       (INIT_RESTART_CFG_SINGLE & ~0xE0) | 0x60,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_ads(ads);
}

void test_ads101x_set_fs_writes_and_waits_when_the_rate_actually_changes_in_continuous_mode(
	void)
{
	ads101x_t* ads = create_ads(true); // continuous, starts at FAST_DR

	// DR bits (7-5) replaced: 0xA0 (FAST_DR) -> 0x60 (920SPS).
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       (INIT_RESTART_CFG_CONTINUOUS & ~0xE0) |
					       0x60,
				       0);

	TEST_ASSERT_EQUAL_INT(
		0, ads101x_set_fs(TEST_I2C, ads, ADS101X_920SPS, 1000));

	destroy_ads(ads);
}

void test_ads101x_set_fs_fails_when_writing_the_config_reg_fails_in_continuous_mode(
	void)
{
	ads101x_t* ads = create_ads(true); // continuous, starts at FAST_DR

	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       (INIT_RESTART_CFG_CONTINUOUS & ~0xE0) |
					       0x60,
				       -1);

	TEST_ASSERT_EQUAL_INT(
		-1, ads101x_set_fs(TEST_I2C, ads, ADS101X_920SPS, 1000));

	destroy_ads(ads);
}

void test_ads101x_set_fs_skips_the_write_when_the_rate_is_unchanged(void)
{
	ads101x_t* ads = create_ads(true); // continuous, starts at FAST_DR

	// No i2c_write8_16b expectation queued: any write here is a bug.
	TEST_ASSERT_EQUAL_INT(0, ads101x_set_fs(TEST_I2C, ads, FAST_DR, 1000));

	destroy_ads(ads);
}

void test_ads101x_set_fs_only_writes_in_single_mode_when_read_next(void)
{
	/*
	 * Single mode never writes from set_fs itself.
	 */
	ads101x_t* ads = create_ads(false);

	TEST_ASSERT_EQUAL_INT(
		0, ads101x_set_fs(TEST_I2C, ads, ADS101X_920SPS, 1000));

	destroy_ads(ads);
}

void test_ads101x_set_fs_waits_for_both_the_old_and_new_conversion_rate(void)
{
	// Ensure we wait up to two cycles when changing sampling frequency
	expect_ads101x_restart();

	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, 0x0203, 0);
	ads101x_t* ads = init_ads(true, true, ADS101X_FSR_4_096V, SLOW_DR);
	TEST_ASSERT_NOT_NULL(ads);

	// DR bits replaced: SLOW_DR's 0x00 -> FAST_DR's 0xA0.
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, (0x0203 & ~0xE0) | 0xA0, 0);

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	TEST_ASSERT_EQUAL_INT(0, ads101x_set_fs(TEST_I2C, ads, FAST_DR, 1000));

	clock_gettime(CLOCK_MONOTONIC, &end);
	long ms = elapsed_ms(start, end);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(8, ms);
	TEST_ASSERT_LESS_THAN_INT(14, ms);

	destroy_ads(ads);
}

/* ------------------ ads101x_get_conversion_time_us's table ------------------ */

void test_ads101x_single_read_exercises_every_data_rate_in_the_conversion_time_table(
	void)
{
	/*
	 * ads101x_get_conversion_time_us is a private lookup table with one
	 * case per ADS101X_DATA_RATE plus a default for the 0b111 duplicate of
	 * 3300SPS.  expected_us mirrors ads101x_get_conversion_time_us's own
	 * formula; the bound is [expected_us, 2 * expected_us) to tolerate
	 * scheduling jitter while still catching a grossly wrong table entry.
	 */
	ads101x_t* ads = create_ads(false);
	static const struct {
		ADS101X_DATA_RATE dr;
		uint16_t expected_cfg;
		long expected_us;
	} cases[] = {
		{ ADS101X_250SPS,
		  (INIT_RESTART_CFG_SINGLE & ~0xE0) | 0x20,
		  (1100000 + 50000) / 250 },
		{ ADS101X_490SPS,
		  (INIT_RESTART_CFG_SINGLE & ~0xE0) | 0x40,
		  (1100000 + 50000) / 490 },
		{ ADS101X_1600SPS,
		  (INIT_RESTART_CFG_SINGLE & ~0xE0) | 0x80,
		  (1100000 + 50000) / 1600 },
		{ ADS101X_3300SPS,
		  (INIT_RESTART_CFG_SINGLE & ~0xE0) | 0xC0,
		  (1100000 + 50000) / 3300 },
		{ (ADS101X_DATA_RATE)0b111,
		  (INIT_RESTART_CFG_SINGLE & ~0xE0) | 0xC0,
		  (1100000 + 50000) / 3300 },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		TEST_ASSERT_EQUAL_INT(
			0, ads101x_set_fs(TEST_I2C, ads, cases[i].dr, 1000));
		i2c_write8_16b_ExpectAndReturn(TEST_I2C,
					       TEST_ADDR,
					       CONFIG_REG,
					       cases[i].expected_cfg,
					       0);
		expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);

		struct timespec start, end;
		clock_gettime(CLOCK_MONOTONIC, &start);

		int16_t value;
		TEST_ASSERT_EQUAL_INT(
			0,
			ads101x_single_read(
				TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

		clock_gettime(CLOCK_MONOTONIC, &end);
		long us = elapsed_us(start, end);

		TEST_ASSERT_EQUAL_INT16(0x0FF, value);
		TEST_ASSERT_GREATER_OR_EQUAL_INT(cases[i].expected_us, us);
		TEST_ASSERT_LESS_THAN_INT(2 * cases[i].expected_us, us);
	}

	destroy_ads(ads);
}

/* ----------------------- ads101x_protect / unprotect ---------------------- */

void test_ads101x_protect_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_protect(TEST_I2C, NULL, TEST_SCOPE));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_protect_fails_with_einval_for_another_bus(void)
{
	ads101x_t* ads = create_ads(false);

	fake_i2c_bus = TEST_BUS + 1;
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_protect(TEST_I2C, ads, TEST_SCOPE));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);

	fake_i2c_bus = TEST_BUS;
	destroy_ads(ads);
}

void test_ads101x_protect_creates_the_mutex_with_the_given_scope(void)
{
	ads101x_t* ads = create_ads(false);

	plc_mutex_static_create_ExpectAndReturn(
		NULL, PLC_MUTEX_SCOPE_SHARED, 0);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(
		0, ads101x_protect(TEST_I2C, ads, PLC_MUTEX_SCOPE_SHARED));

	destroy_protected_ads(ads);
}

void test_ads101x_protect_returns_1_if_already_protected(void)
{
	ads101x_t* ads = create_protected_ads(false);

	// No second plc_mutex_static_create is expected.
	TEST_ASSERT_EQUAL_INT(1, ads101x_protect(TEST_I2C, ads, TEST_SCOPE));

	destroy_protected_ads(ads);
}

void test_ads101x_protect_leaves_the_handle_unprotected_when_it_fails(void)
{
	ads101x_t* ads = create_ads(false);

	plc_mutex_static_create_ExpectAndReturn(NULL, TEST_SCOPE, -1);
	plc_mutex_static_create_IgnoreArg_mutex();
	TEST_ASSERT_EQUAL_INT(-1, ads101x_protect(TEST_I2C, ads, TEST_SCOPE));

	// No plc_mutex_acquire/release is expected: the handle isn't protected.
	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr, 1000));

	destroy_ads(ads);
}

void test_ads101x_unprotect_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, ads101x_unprotect(NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_ads101x_unprotect_returns_1_if_not_protected(void)
{
	ads101x_t* ads = create_ads(false);

	TEST_ASSERT_EQUAL_INT(1, ads101x_unprotect(ads));

	destroy_ads(ads);
}

void test_ads101x_unprotect_destroys_the_mutex(void)
{
	ads101x_t* ads = create_protected_ads(false);

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, ads101x_unprotect(ads));

	// No plc_mutex_acquire/release is expected anymore.
	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr, 1000));

	destroy_ads(ads);
}

void test_ads101x_unprotect_keeps_the_handle_protected_when_it_fails(void)
{
	ads101x_t* ads = create_protected_ads(false);

	expect_mutex_destroyed(-1);
	TEST_ASSERT_EQUAL_INT(-1, ads101x_unprotect(ads));

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_mutex_released();
	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr, 1000));

	destroy_protected_ads(ads);
}

void test_ads101x_deinit_destroys_the_mutex_when_protected(void)
{
	ads101x_t* ads = create_protected_ads(false);

	expect_mutex_destroyed(0);
	TEST_ASSERT_EQUAL_INT(0, ads101x_deinit(TEST_I2C, ads, false));
}

void test_ads101x_deinit_keeps_the_handle_when_the_mutex_cant_be_destroyed(void)
{
	ads101x_t* ads = create_protected_ads(false);

	// No shutdown write is expected either.
	expect_mutex_destroyed(-1);
	TEST_ASSERT_EQUAL_INT(-1, ads101x_deinit(TEST_I2C, ads, true));

	destroy_protected_ads(ads);
}

void test_ads101x_single_read_fails_when_the_mutex_cant_be_taken(void)
{
	ads101x_t* ads = create_protected_ads(false);

	// No I2C transfer and no release are expected.
	plc_mutex_acquire_Stub(acquire_times_out);

	errno = 0;
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	destroy_protected_ads(ads);
}

// FAST_DR: 1 / 2400 SPS + 15%, the driver's conversion time.
#define FAST_DR_CONVERSION_US ((1100000 + 50000) / 2400)

void test_ads101x_single_read_waits_out_a_dead_owners_conversion(void)
{
	ads101x_t* ads = create_protected_ads(false);

	// Read back, OS=0 means the dead owner's conversion is still running.
	plc_mutex_acquire_Stub(acquire_after_owner_died);
	expect_i2c_read8_16b(
		CONFIG_REG, INIT_RESTART_CFG_SINGLE & ~CONFIG_REG_OS, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	clock_gettime(CLOCK_MONOTONIC, &end);

	// The dead owner's conversion, then this read's own.
	TEST_ASSERT_GREATER_OR_EQUAL_INT(2 * FAST_DR_CONVERSION_US,
					 elapsed_us(start, end));
	TEST_ASSERT_EQUAL_INT16(255, value);

	destroy_protected_ads(ads);
}

void test_ads101x_single_read_starts_at_once_when_a_dead_owner_left_it_idle(void)
{
	ads101x_t* ads = create_protected_ads(false);

	// Read back, OS=1 means no conversion is running: nothing to wait for.
	plc_mutex_acquire_Stub(acquire_after_owner_died);
	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	clock_gettime(CLOCK_MONOTONIC, &end);

	TEST_ASSERT_LESS_THAN_INT(2 * FAST_DR_CONVERSION_US,
				  elapsed_us(start, end));

	destroy_protected_ads(ads);
}

void test_ads101x_single_read_resyncs_only_once_after_a_dead_owner(void)
{
	ads101x_t* ads = create_protected_ads(false);
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	// No CONFIG_REG read this time.
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

void test_ads101x_single_read_retries_the_resync_when_it_fails(void)
{
	ads101x_t* ads = create_protected_ads(false);
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	// The CONFIG_REG read fails: nothing else runs, and the lock is released.
	expect_i2c_read8_16b(CONFIG_REG, 0, -1);
	expect_mutex_released();
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	// The next read, with a healthy lock, still resyncs first.
	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

// INIT_RESTART_CFG_SINGLE, with SLOW_DR instead of FAST_DR.
#define RESTART_CFG_SINGLE_AT_SLOW_DR                 \
	((INIT_RESTART_CFG_SINGLE & ~CONFIG_REG_DR) | \
	 (SLOW_DR << CONFIG_REG_DR_SHIFT))

void test_ads101x_set_fs_keeps_its_rate_through_a_dead_owners_resync(void)
{
	ads101x_t* ads = create_protected_ads(false);
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	// Single-shot mode: set_fs only changes the rate in memory.
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(0, ads101x_set_fs(TEST_I2C, ads, SLOW_DR, 1000));

	// The chip still has FAST_DR, and the resync must not bring it back.
	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       RESTART_CFG_SINGLE_AT_SLOW_DR,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

void test_ads101x_get_fs_leaves_the_resync_to_a_call_with_the_bus(void)
{
	ads101x_t* ads = create_protected_ads(false);
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	// get_fs has no I2C interface: no transfer is expected.
	expect_mutex_released();
	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr, 1000));
	TEST_ASSERT_EQUAL_INT(FAST_DR, dr);

	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(0, ads101x_set_fs(TEST_I2C, ads, SLOW_DR, 1000));

	// The resync is still pending, and keeps the new rate.
	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       RESTART_CFG_SINGLE_AT_SLOW_DR,
				       0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

// SLOW_DR (128 SPS), the slowest rate: 1 / 128 SPS + 15%.
#define SLOW_DR_CONVERSION_US ((1100000 + 50000) / 128)

void test_ads101x_continuous_read_waits_out_a_dead_owners_conversions(void)
{
	ads101x_t* ads = create_protected_ads(true);
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	// P0_N1 is what the ADS101X already has, so no CONFIG_REG write.
	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_CONTINUOUS, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));
	clock_gettime(CLOCK_MONOTONIC, &end);

	// The in-flight conversion at any rate, then one at the rate read back.
	TEST_ASSERT_GREATER_OR_EQUAL_INT(SLOW_DR_CONVERSION_US +
						 FAST_DR_CONVERSION_US,
					 elapsed_us(start, end));
	TEST_ASSERT_EQUAL_INT16(255, value);

	destroy_protected_ads(ads);
}

void test_ads101x_continuous_read_resyncs_only_once_after_a_dead_owner(void)
{
	ads101x_t* ads = create_protected_ads(true);
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_CONTINUOUS, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	// No CONFIG_REG read this time.
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

void test_ads101x_continuous_read_retries_the_resync_when_it_fails(void)
{
	ads101x_t* ads = create_protected_ads(true);
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	// The CONFIG_REG read fails: nothing else runs, and the lock is released.
	expect_i2c_read8_16b(CONFIG_REG, 0, -1);
	expect_mutex_released();
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		-1,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	// The next read, with a healthy lock, still resyncs first.
	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_CONTINUOUS, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

// INIT_RESTART_CFG_CONTINUOUS, with SLOW_DR instead of FAST_DR.
#define RESTART_CFG_CONTINUOUS_AT_SLOW_DR                 \
	((INIT_RESTART_CFG_CONTINUOUS & ~CONFIG_REG_DR) | \
	 (SLOW_DR << CONFIG_REG_DR_SHIFT))

void test_ads101x_set_fs_resyncs_in_continuous_mode_after_a_dead_owner(void)
{
	ads101x_t* ads = create_protected_ads(true); // cached at FAST_DR
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	/*
	 * The dead owner already wrote SLOW_DR, but didn't cache it. Asking for
	 * FAST_DR again must still reach the ADS101X.
	 */
	expect_i2c_read8_16b(CONFIG_REG, RESTART_CFG_CONTINUOUS_AT_SLOW_DR, 0);
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       INIT_RESTART_CFG_CONTINUOUS,
				       0);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(0, ads101x_set_fs(TEST_I2C, ads, FAST_DR, 1000));

	// Resynced: the next read neither reads CONFIG_REG nor writes it.
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	expect_mutex_released();
	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

void test_ads101x_set_fs_retries_the_resync_when_it_fails(void)
{
	ads101x_t* ads = create_protected_ads(true);
	plc_mutex_acquire_Stub(acquire_after_owner_died);

	// The CONFIG_REG read fails: no write, and the lock is released.
	expect_i2c_read8_16b(CONFIG_REG, 0, -1);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(-1, ads101x_set_fs(TEST_I2C, ads, SLOW_DR, 1000));

	// The next call, with a healthy lock, still resyncs first.
	expect_i2c_read8_16b(CONFIG_REG, INIT_RESTART_CFG_CONTINUOUS, 0);
	i2c_write8_16b_ExpectAndReturn(TEST_I2C,
				       TEST_ADDR,
				       CONFIG_REG,
				       RESTART_CFG_CONTINUOUS_AT_SLOW_DR,
				       0);
	expect_mutex_released();
	TEST_ASSERT_EQUAL_INT(0, ads101x_set_fs(TEST_I2C, ads, SLOW_DR, 1000));

	destroy_protected_ads(ads);
}

void test_ads101x_single_read_when_protected_locks_and_unlocks(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	i2c_write8_16b_ExpectAndReturn(
		TEST_I2C, TEST_ADDR, CONFIG_REG, INIT_RESTART_CFG_SINGLE, 0);
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_single_read(TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

void test_ads101x_continuous_read_when_protected_locks_and_unlocks(void)
{
	ads101x_t* ads = create_protected_ads(true);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	expect_i2c_read8_16b(CONVERSION_REG, 0x0FF0, 0);
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	int16_t value;
	TEST_ASSERT_EQUAL_INT(
		0,
		ads101x_continuous_read(
			TEST_I2C, ads, ADS101X_P0_N1, &value, 1000));

	destroy_protected_ads(ads);
}

void test_ads101x_get_fs_when_protected_locks_and_unlocks(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	ADS101X_DATA_RATE dr;
	TEST_ASSERT_EQUAL_INT(0, ads101x_get_fs(ads, &dr, 1000));
	TEST_ASSERT_EQUAL_INT(FAST_DR, dr);

	destroy_protected_ads(ads);
}

void test_ads101x_set_fs_when_protected_locks_and_unlocks(void)
{
	ads101x_t* ads = create_protected_ads(false);

	plc_mutex_acquire_ExpectAndReturn(NULL, 1000, 0);
	plc_mutex_acquire_IgnoreArg_mutex();
	plc_mutex_release_ExpectAndReturn(NULL, 0);
	plc_mutex_release_IgnoreArg_mutex();

	TEST_ASSERT_EQUAL_INT(
		0, ads101x_set_fs(TEST_I2C, ads, ADS101X_920SPS, 1000));

	destroy_protected_ads(ads);
}
