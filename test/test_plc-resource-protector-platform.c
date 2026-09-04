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
 * Tests for src/plc-resource-protector-linux.c, the real pthread-backed
 * plc_mutex_* implementation. Unlike every other suite in this project, this
 * one mocks nothing: it calls the genuine pthread_mutex_t machinery with real
 * threads, since that's exactly what needs verifying here (the timeout math
 * and the error-checking-mutex error codes).
 *
 * The NULL-argument checks in plc_mutex_acquire/plc_mutex_release are gated
 * behind PLC_PERIPHERALS_CHECK_ARGUMENTS, which defaults off; this suite
 * doesn't enable it, since passing NULL with it disabled is undefined
 * behavior, not something to test. plc_mutex_destroy's NULL check is
 * unconditional and is tested below.
 *
 * This file also runs as the Arduino ESP32 sketch at
 * test/Arduino/test_plc-resource-protector-platform/ (symlinked in, alongside
 * the platform source and Unity itself). That .ino declares each test_*
 * function with extern "C" and calls it via RUN_TEST(); if you add, remove,
 * or rename a test_* function here, update the .ino to match.
 */

#include "unity.h"
#include "plc-peripherals-platform.h"
#include "plc-resource-protector-mutex.h"

// No header of its own maps to plc-resource-protector-linux.c
#if PLC_ENVIRONMENT == PLC_LINUX
TEST_SOURCE_FILE("plc-resource-protector-linux.c")
#endif

#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

static plc_mutex_t* mutex;

void setUp(void)
{
	mutex = plc_mutex_create();
	TEST_ASSERT_NOT_NULL(mutex);
}

void tearDown(void)
{
	// Tolerate whatever lock state a test left behind.
	plc_mutex_release(mutex);
	plc_mutex_destroy(mutex);
}

static long elapsed_ms(struct timespec start, struct timespec end)
{
	return (end.tv_sec - start.tv_sec) * 1000L +
	       (end.tv_nsec - start.tv_nsec) / 1000000L;
}

/* -------------------------- plc_mutex_create ------------------------------ */

void test_plc_mutex_create_returns_a_valid_mutex(void)
{
	// setUp already created it; just confirm it can actually be used.
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(mutex));
}

/* -------------------------- plc_mutex_destroy ------------------------------ */

void test_plc_mutex_destroy_fails_with_einval_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_destroy(NULL));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_plc_mutex_destroy_succeeds_for_an_unlocked_mutex(void)
{
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_destroy(mutex));

	// Already destroyed; skip tearDown's release/destroy on it.
	mutex = plc_mutex_create();
}

void test_plc_mutex_destroy_fails_with_ebusy_for_a_locked_mutex(void)
{
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_destroy(mutex));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
}

/* -------------------------- plc_mutex_acquire ------------------------------ */

void test_plc_mutex_acquire_succeeds_immediately_when_free(void)
{
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 1000));

	clock_gettime(CLOCK_MONOTONIC, &end);
	TEST_ASSERT_LESS_THAN_INT(50, elapsed_ms(start, end));
}

void test_plc_mutex_acquire_times_out_when_already_held_by_the_same_thread(void)
{
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_acquire(mutex, 100));

	clock_gettime(CLOCK_MONOTONIC, &end);
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	long ms = elapsed_ms(start, end);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(95, ms);
	TEST_ASSERT_LESS_THAN_INT(300, ms);
}

typedef struct {
	plc_mutex_t* mutex;
	int hold_ms;
	volatile int acquired;
} hold_lock_args_t;

static void* hold_lock_thread(void* arg)
{
	hold_lock_args_t* args = (hold_lock_args_t*)arg;

	if (plc_mutex_acquire(args->mutex, 1000) != 0) {
		return NULL;
	}
	args->acquired = 1;

	usleep((useconds_t)args->hold_ms * 1000);

	plc_mutex_release(args->mutex);
	return NULL;
}

static void wait_until_acquired(volatile int* acquired)
{
	int waited_ms = 0;
	while (!*acquired && waited_ms < 1000) {
		usleep(1000); // 1ms
		waited_ms++;
	}
	TEST_ASSERT_TRUE_MESSAGE(*acquired,
				 "worker thread never acquired the mutex");
}

void test_plc_mutex_acquire_times_out_when_held_by_another_thread(void)
{
	hold_lock_args_t args = { .mutex = mutex,
				  .hold_ms = 300,
				  .acquired = 0 };
	pthread_t thread;
	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, hold_lock_thread, &args));
	wait_until_acquired(&args.acquired);

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_acquire(mutex, 100));

	clock_gettime(CLOCK_MONOTONIC, &end);
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	long ms = elapsed_ms(start, end);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(95, ms);
	TEST_ASSERT_LESS_THAN_INT(280, ms);

	pthread_join(thread, NULL);
}

/* -------------------------- plc_mutex_release ------------------------------ */

void test_plc_mutex_release_succeeds_when_held(void)
{
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(mutex));
}

void test_plc_mutex_release_fails_with_ealready_when_not_held(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_release(mutex));
	TEST_ASSERT_EQUAL_INT(EALREADY, errno);
}
