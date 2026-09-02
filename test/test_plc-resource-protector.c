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
 * Tests for the portable hash-table/lock bookkeeping of
 * src/plc-resource-protector.c. The platform layer it sits on
 * (plc-resource-protector-mutex.h) is mocked: each fake mutex is
 * just a distinct address, and plc_mutex_t is opaque to the code
 * under test.
 */

#include "unity.h"

#include "mock_plc-resource-protector-mutex.h"
#include "plc-resource-protector.h"

#include <errno.h>

/*
 * The library only defines I2C_RESOURCE. There is no GPIO_RESOURCE macro, so
 * build one of the same shape (PLC_RESOURCE_GPIO in the top byte, pin number
 * in the low bits) to exercise plc_resource_type values other than
 * PLC_RESOURCE_I2C.
 */
#define GPIO_RESOURCE(pin) \
	((plc_resource_t)((unsigned int)PLC_RESOURCE_GPIO << 24) | (pin))

#define RES_A ((plc_resource_t)I2C_RESOURCE(0, 0x50))
#define RES_B ((plc_resource_t)I2C_RESOURCE(1, 0x50))
#define RES_GPIO ((plc_resource_t)GPIO_RESOURCE(0x50))

// Fake mutex handles: only their addresses matter, CMock compares pointers.
static int hash_mutex_token;
static int res_a_mutex_token;
static int res_b_mutex_token;
static int res_gpio_mutex_token;
#define HASH_MUTEX ((plc_mutex_t*)&hash_mutex_token)
#define RES_A_MUTEX ((plc_mutex_t*)&res_a_mutex_token)
#define RES_B_MUTEX ((plc_mutex_t*)&res_b_mutex_token)
#define RES_GPIO_MUTEX ((plc_mutex_t*)&res_gpio_mutex_token)

void setUp(void)
{
	plc_mutex_create_IgnoreAndReturn(HASH_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_init());
}

void tearDown(void)
{
	// Tolerate whatever locks/mutexes a test left behind.
	plc_mutex_destroy_IgnoreAndReturn(0);
	plc_resource_deinit();
}

/* --------------------------- plc_resource_init --------------------------- */

void test_plc_resource_init_returns_0_correctly(void)
{
	plc_mutex_destroy_IgnoreAndReturn(0);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_deinit());

	plc_mutex_create_IgnoreAndReturn(HASH_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_init());
}

void test_plc_resource_init_returns_1_if_already_initialized(void)
{
	// setUp already initialized it once.
	TEST_ASSERT_EQUAL_INT(1, plc_resource_init());
}

void test_plc_resource_init_fails_when_mutex_create_fails(void)
{
	plc_mutex_destroy_IgnoreAndReturn(0);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_deinit());

	plc_mutex_create_IgnoreAndReturn(NULL);
	TEST_ASSERT_EQUAL_INT(-1, plc_resource_init());
}

/* -------------------------- plc_resource_deinit -------------------------- */

void test_plc_resource_deinit_returns_0_correctly(void)
{
	plc_mutex_destroy_IgnoreAndReturn(0);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_deinit());
}

void test_plc_resource_deinit_returns_1_if_not_initialized(void)
{
	plc_mutex_destroy_IgnoreAndReturn(0);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_deinit());

	TEST_ASSERT_EQUAL_INT(1, plc_resource_deinit());
}

void test_plc_resource_deinit_destroys_remaining_locks(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_destroy_ExpectAndReturn(RES_A_MUTEX, 0);
	plc_mutex_destroy_ExpectAndReturn(HASH_MUTEX, 0);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_deinit());
}

void test_plc_resource_deinit_fails_if_a_locks_mutex_cant_be_destroyed(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_destroy_ExpectAndReturn(RES_A_MUTEX, -1);
	TEST_ASSERT_EQUAL_INT(-1, plc_resource_deinit());
}

void test_plc_resource_deinit_fails_if_the_hash_mutex_cant_be_destroyed(void)
{
	plc_mutex_destroy_ExpectAndReturn(HASH_MUTEX, -1);
	TEST_ASSERT_EQUAL_INT(-1, plc_resource_deinit());
}

/* --------------------------- plc_resource_add ----------------------------- */

void test_plc_resource_add_succeeds(void)
{
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 0, 0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);

	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));
}

void test_plc_resource_add_returns_1_if_already_added(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	errno = 0;
	TEST_ASSERT_EQUAL_INT(1, plc_resource_add(RES_A));
	TEST_ASSERT_EQUAL_INT(EEXIST, errno);
}

void test_plc_resource_add_fails_when_its_mutex_cant_be_created(void)
{
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 0, 0);
	plc_mutex_create_ExpectAndReturn(NULL);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);

	TEST_ASSERT_EQUAL_INT(-1, plc_resource_add(RES_A));
}

void test_plc_resource_add_fails_when_the_hash_mutex_cant_be_acquired(void)
{
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 0, -1);

	TEST_ASSERT_EQUAL_INT(-1, plc_resource_add(RES_A));
}

void test_plc_resource_add_keeps_resources_on_different_buses_independent(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);

	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_create_ExpectAndReturn(RES_B_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_B));
}

void test_plc_resource_add_keeps_resources_of_different_types_independent(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);

	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_create_ExpectAndReturn(RES_GPIO_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_GPIO));
}

/* ----------------------------- I2C_RESOURCE ------------------------------- */

void test_I2C_RESOURCE_packs_type_bus_and_address_into_the_documented_layout(
	void)
{
	// type (bits 31-28) | bus (bits 17-10) | address (bits 9-0)
	TEST_ASSERT_EQUAL_UINT32(0x10000050u, I2C_RESOURCE(0, 0x50));
	TEST_ASSERT_EQUAL_UINT32(0x10000450u, I2C_RESOURCE(1, 0x50));
}

void test_I2C_RESOURCE_masks_a_bus_that_overflows_its_8_bits(void)
{
	// 0x100 is the first bus value that doesn't fit in 8 bits; it must wrap
	// to the same encoding as bus 0 rather than bleed into neighboring bits.
	TEST_ASSERT_EQUAL_UINT32(
		I2C_RESOURCE(0, 0x50), I2C_RESOURCE(0x100, 0x50));
}

void test_I2C_RESOURCE_masks_an_address_that_overflows_its_10_bits(void)
{
	// 0x400 is the first address value that doesn't fit in 10 bits; it must
	// wrap to the same encoding as address 0 rather than bleed into the bus
	// field.
	TEST_ASSERT_EQUAL_UINT32(I2C_RESOURCE(0, 0), I2C_RESOURCE(0, 0x400));
}

/* -------------------------- plc_resource_remove --------------------------- */

void test_plc_resource_remove_succeeds(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_destroy_ExpectAndReturn(RES_A_MUTEX, 0);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_remove(RES_A));
}

void test_plc_resource_remove_returns_1_if_not_present(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(1, plc_resource_remove(RES_A));
	TEST_ASSERT_EQUAL_INT(ENODEV, errno);
}

void test_plc_resource_remove_fails_when_its_mutex_cant_be_destroyed(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_destroy_ExpectAndReturn(RES_A_MUTEX, -1);
	TEST_ASSERT_EQUAL_INT(-1, plc_resource_remove(RES_A));
}

void test_plc_resource_remove_fails_when_the_hash_mutex_cant_be_acquired(void)
{
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 0, -1);

	TEST_ASSERT_EQUAL_INT(-1, plc_resource_remove(RES_A));
}

/* --------------------------- plc_resource_lock ---------------------------- */

void test_plc_resource_lock_acquires_the_resources_mutex(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	// Lookup takes the hash mutex with timeout 1, then acquires the
	// resource's own mutex with the remaining timeout budget.
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, 0);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);
	plc_mutex_acquire_ExpectAndReturn(RES_A_MUTEX, 999, 0);

	TEST_ASSERT_EQUAL_INT(0, plc_resource_lock(RES_A, 1000));
}

void test_plc_resource_lock_clamps_the_timeout_budget_to_0(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, 0);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);
	// timeout_ms (0) is below the hash mutex's own timeout (1ms), so no
	// negative budget is passed down.
	plc_mutex_acquire_ExpectAndReturn(RES_A_MUTEX, 0, 0);

	TEST_ASSERT_EQUAL_INT(0, plc_resource_lock(RES_A, 0));
}

void test_plc_resource_lock_fails_when_not_present(void)
{
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, 0);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_resource_lock(RES_A, 1000));
	TEST_ASSERT_EQUAL_INT(ENODEV, errno);
}

void test_plc_resource_lock_fails_when_the_hash_mutex_cant_be_acquired(void)
{
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, -1);

	TEST_ASSERT_EQUAL_INT(-1, plc_resource_lock(RES_A, 1000));
}

void test_plc_resource_lock_fails_when_its_mutex_times_out(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, 0);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);
	plc_mutex_acquire_ExpectAndReturn(RES_A_MUTEX, 999, -1);

	TEST_ASSERT_EQUAL_INT(-1, plc_resource_lock(RES_A, 1000));
}

/* -------------------------- plc_resource_unlock --------------------------- */

void test_plc_resource_unlock_releases_the_resources_mutex(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, 0);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);
	plc_mutex_release_ExpectAndReturn(RES_A_MUTEX, 0);

	TEST_ASSERT_EQUAL_INT(0, plc_resource_unlock(RES_A));
}

void test_plc_resource_unlock_fails_when_not_present(void)
{
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, 0);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_resource_unlock(RES_A));
	TEST_ASSERT_EQUAL_INT(ENODEV, errno);
}

void test_plc_resource_unlock_fails_when_the_hash_mutex_cant_be_acquired(void)
{
	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, -1);

	TEST_ASSERT_EQUAL_INT(-1, plc_resource_unlock(RES_A));
}

void test_plc_resource_unlock_fails_when_already_unlocked(void)
{
	plc_mutex_acquire_IgnoreAndReturn(0);
	plc_mutex_release_IgnoreAndReturn(0);
	plc_mutex_create_ExpectAndReturn(RES_A_MUTEX);
	TEST_ASSERT_EQUAL_INT(0, plc_resource_add(RES_A));

	plc_mutex_acquire_ExpectAndReturn(HASH_MUTEX, 1, 0);
	plc_mutex_release_ExpectAndReturn(HASH_MUTEX, 0);
	plc_mutex_release_ExpectAndReturn(RES_A_MUTEX, -1);

	TEST_ASSERT_EQUAL_INT(-1, plc_resource_unlock(RES_A));
}
