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
 * Tests for src/plc-mutex-linux.c, the real pthread-backed
 * plc_mutex_* implementation. Unlike every other suite in this project, this
 * one mocks nothing: it calls the genuine pthread_mutex_t machinery with real
 * threads, since that's exactly what needs verifying here (the timeout math
 * and the error-checking-mutex error codes).
 *
 * This file also runs as the Arduino ESP32 sketch at
 * test/Arduino/test_plc-mutex-platform/ (symlinked in, alongside
 * the platform source and Unity itself). That .ino declares each test_*
 * function with extern "C" and calls it via RUN_TEST(); if you add, remove,
 * or rename a test_* function here, update the .ino to match.
 */

#include "unity.h"
#include "plc-peripherals-platform.h"
#include "plc-mutex.h"

// No header of its own maps to plc-mutex-linux.c
#if PLC_ENVIRONMENT == PLC_LINUX
TEST_SOURCE_FILE("plc-mutex-linux.c")
#endif

#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>

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

typedef struct {
	plc_mutex_t* mutex;
	int hold_ms;
	atomic_int acquired;
} hold_lock_args_t;

static void* hold_lock_thread(void* arg)
{
	hold_lock_args_t* args = (hold_lock_args_t*)arg;

	if (plc_mutex_acquire(args->mutex, 1000) != 0) {
		return NULL;
	}
	atomic_store(&args->acquired, 1);

	usleep((useconds_t)args->hold_ms * 1000);

	plc_mutex_release(args->mutex);
	return NULL;
}

static void wait_until_acquired(atomic_int* acquired)
{
	int waited_ms = 0;
	while (!atomic_load(acquired) && waited_ms < 1000) {
		usleep(1000); // 1ms
		waited_ms++;
	}
	TEST_ASSERT_TRUE_MESSAGE(atomic_load(acquired),
				 "Worker thread never acquired the mutex");
}

/* -------------------------- plc_mutex_create ------------------------------ */

void test_plc_mutex_create_returns_a_valid_mutex(void)
{
	// setUp already created it; just confirm it can actually be used.
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(mutex));
}

/* -------------------------- plc_mutex_destroy ------------------------------ */

void test_plc_mutex_destroy_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_destroy(NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
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

void test_plc_mutex_destroy_fails_with_ebusy_when_another_thread_holds_it(void)
{
	static hold_lock_args_t args;
	args = (hold_lock_args_t){ .mutex = mutex,
				   .hold_ms = 300,
				   .acquired = 0 };
	pthread_t thread;
	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, hold_lock_thread, &args));
	wait_until_acquired(&args.acquired);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_destroy(mutex));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	pthread_join(thread, NULL);
}

typedef struct {
	plc_mutex_t* first;
	plc_mutex_t* second;
	atomic_int acquired;
} lock_order_args_t;

static void* take_first_then_second(void* arg)
{
	lock_order_args_t* args = (lock_order_args_t*)arg;

	if (plc_mutex_acquire(args->first, 1000) != 0) {
		return NULL;
	}
	atomic_store(&args->acquired, 1);

	if (plc_mutex_acquire(args->second, 1000) == 0) {
		plc_mutex_release(args->second);
	}
	plc_mutex_release(args->first);
	return NULL;
}

void test_plc_mutex_destroy_fails_with_ebusy_on_a_lock_order_cycle(void)
{
	plc_mutex_t* other = plc_mutex_create();
	TEST_ASSERT_NOT_NULL(other);
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(other, 0));

	// The worker holds mutex, and waits for other
	lock_order_args_t args = { .first = mutex,
				   .second = other,
				   .acquired = 0 };
	pthread_t thread;
	TEST_ASSERT_EQUAL_INT(
		0,
		pthread_create(&thread, NULL, take_first_then_second, &args));
	for (int waited_ms = 0;
	     !atomic_load(&args.acquired) && waited_ms < 1000;
	     waited_ms++) {
		usleep(1000);
	}
	usleep(100 * 1000); // Let it block on other

	// Closes the cycle: holding other, probing mutex
	errno = 0;
	int result = plc_mutex_destroy(mutex);
	int error = errno;

	plc_mutex_release(other);
	pthread_join(thread, NULL);
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_destroy(other));

	TEST_ASSERT_TRUE_MESSAGE(atomic_load(&args.acquired),
				 "Worker thread never acquired the mutex");
	TEST_ASSERT_EQUAL_INT(-1, result);
	TEST_ASSERT_EQUAL_INT(EBUSY, error);
}

/* ----------------------- plc_mutex_static_create --------------------------- */

void test_plc_mutex_static_create_makes_a_usable_mutex(void)
{
	static plc_mutex_t storage;

	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(&storage, PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(&storage, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(&storage));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&storage));
}

void test_plc_mutex_static_create_makes_a_usable_shared_mutex(void)
{
	static plc_mutex_t storage;

	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(&storage, PLC_MUTEX_SCOPE_SHARED));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(&storage, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(&storage));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&storage));
}

void test_plc_mutex_static_create_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, plc_mutex_static_create(NULL, PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_plc_mutex_static_create_fails_with_efault_for_misaligned_storage(void)
{
	// A byte buffer is 1-aligned; deliberately start one byte in.
	static unsigned char arena[sizeof(plc_mutex_t) + PLC_MUTEX_ALIGN];
	plc_mutex_t* misaligned = (plc_mutex_t*)&arena[1];

	TEST_ASSERT_NOT_EQUAL_INT(0, (uintptr_t)misaligned % PLC_MUTEX_ALIGN);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      plc_mutex_static_create(misaligned,
						      PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_plc_mutex_static_create_accepts_an_aligned_address_in_a_buffer(void)
{
	// The embedded case: a mutex carved out of a region the caller owns.
	static unsigned char arena[sizeof(plc_mutex_t) + PLC_MUTEX_ALIGN];
	uintptr_t base = (uintptr_t)arena;
	plc_mutex_t* aligned =
		(plc_mutex_t*)((base + PLC_MUTEX_ALIGN - 1) &
			       ~(uintptr_t)(PLC_MUTEX_ALIGN - 1));

	TEST_ASSERT_EQUAL_INT(0, (uintptr_t)aligned % PLC_MUTEX_ALIGN);

	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(aligned, PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(aligned, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(aligned));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(aligned));
}

void test_plc_mutex_static_create_fails_with_einval_for_a_bad_scope(void)
{
	static plc_mutex_t storage;

	errno = 0;
	TEST_ASSERT_EQUAL_INT(
		-1, plc_mutex_static_create(&storage, (plc_mutex_scope_t)0xFF));
	TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

/* ---------------------- plc_mutex_static_destroy --------------------------- */

void test_plc_mutex_static_destroy_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_static_destroy(NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_plc_mutex_static_destroy_succeeds_for_an_unlocked_mutex(void)
{
	static plc_mutex_t storage;

	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(&storage, PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&storage));
}

void test_plc_mutex_static_destroy_fails_with_ebusy_for_a_locked_mutex(void)
{
	static plc_mutex_t storage;

	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(&storage, PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(&storage, 0));

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_static_destroy(&storage));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(&storage));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&storage));
}

void test_plc_mutex_static_destroy_fails_with_ebusy_when_another_thread_holds_it(
	void)
{
	static plc_mutex_t storage;

	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(&storage, PLC_MUTEX_SCOPE_PRIVATE));

	static hold_lock_args_t args;
	args = (hold_lock_args_t){ .mutex = &storage,
				   .hold_ms = 300,
				   .acquired = 0 };
	pthread_t thread;
	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, hold_lock_thread, &args));
	wait_until_acquired(&args.acquired);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_static_destroy(&storage));
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);

	pthread_join(thread, NULL);
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&storage));
}

void test_plc_mutex_static_destroy_leaves_the_storage_reusable(void)
{
	static plc_mutex_t storage;

	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(&storage, PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&storage));

	// The same storage again: proof it was never handed to free().
	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(&storage, PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(&storage, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(&storage));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&storage));
}

/* -------------------------- plc_mutex_acquire ------------------------------ */

void test_plc_mutex_acquire_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_acquire(NULL, 0));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_plc_mutex_acquire_succeeds_immediately_when_free(void)
{
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 1000));

	clock_gettime(CLOCK_MONOTONIC, &end);
	TEST_ASSERT_LESS_THAN_INT(50, elapsed_ms(start, end));
}

void test_plc_mutex_acquire_fails_with_edeadlk_for_the_same_thread(void)
{
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_acquire(mutex, 100));

	clock_gettime(CLOCK_MONOTONIC, &end);
	TEST_ASSERT_EQUAL_INT(EDEADLK, errno);

	// Reported at once: it must not sit out the 100 ms timeout.
	TEST_ASSERT_LESS_THAN_INT(50, elapsed_ms(start, end));
}

void test_plc_mutex_acquire_times_out_when_held_by_another_thread(void)
{
	static hold_lock_args_t args;
	args = (hold_lock_args_t){ .mutex = mutex,
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

void test_plc_mutex_acquire_fails_at_once_with_ebusy_for_a_zero_timeout(void)
{
	static hold_lock_args_t args;
	args = (hold_lock_args_t){ .mutex = mutex,
				   .hold_ms = 300,
				   .acquired = 0 };
	pthread_t thread;
	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, hold_lock_thread, &args));
	wait_until_acquired(&args.acquired);

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_acquire(mutex, 0));

	clock_gettime(CLOCK_MONOTONIC, &end);
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	TEST_ASSERT_LESS_THAN_INT(50, elapsed_ms(start, end));

	pthread_join(thread, NULL);
}

void test_plc_mutex_acquire_waits_out_a_timeout_of_seconds_and_milliseconds(void)
{
	static hold_lock_args_t args;
	args = (hold_lock_args_t){ .mutex = mutex,
				   .hold_ms = 2500,
				   .acquired = 0 };
	pthread_t thread;
	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, hold_lock_thread, &args));
	wait_until_acquired(&args.acquired);

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	// 999 ms carries into the seconds unless the clock is within 1 ms of
	// a whole second.
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_acquire(mutex, 1999));

	clock_gettime(CLOCK_MONOTONIC, &end);
	TEST_ASSERT_EQUAL_INT(EBUSY, errno);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(1990, elapsed_ms(start, end));

	pthread_join(thread, NULL);
}

void test_plc_mutex_acquire_waits_for_the_release_with_the_max_delay(void)
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

	int result = plc_mutex_acquire(mutex, PLC_MUTEX_MAX_DELAY);

	clock_gettime(CLOCK_MONOTONIC, &end);
	pthread_join(thread, NULL);

	TEST_ASSERT_EQUAL_INT(0, result);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(250, elapsed_ms(start, end));
}

void test_plc_mutex_acquire_fails_with_edeadlk_with_the_max_delay(void)
{
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      plc_mutex_acquire(mutex, PLC_MUTEX_MAX_DELAY));
	TEST_ASSERT_EQUAL_INT(EDEADLK, errno);
}

void test_plc_mutex_acquire_boosts_the_holder_by_priority_inheritance(void)
{
	TEST_IGNORE_MESSAGE("TODO: Priority inheritance is not tested");
}

/* -------------------------- plc_mutex_release ------------------------------ */

void test_plc_mutex_release_fails_with_efault_for_null(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_release(NULL));
	TEST_ASSERT_EQUAL_INT(EFAULT, errno);
}

void test_plc_mutex_release_succeeds_when_held(void)
{
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(mutex));
}

void test_plc_mutex_release_fails_with_eperm_from_a_non_owner_thread(void)
{
	static hold_lock_args_t args;
	args = (hold_lock_args_t){ .mutex = mutex,
				   .hold_ms = 300,
				   .acquired = 0 };
	pthread_t thread;
	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, hold_lock_thread, &args));
	wait_until_acquired(&args.acquired);

	// This thread never acquired it, so it must not be able to release it.
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_release(mutex));
	TEST_ASSERT_EQUAL_INT(EPERM, errno);

	pthread_join(thread, NULL);
}

void test_plc_mutex_release_fails_with_ealready_when_not_held(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_release(mutex));
	TEST_ASSERT_EQUAL_INT(EALREADY, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(mutex));
	TEST_ASSERT_EQUAL_INT(0, errno);
}

#if PLC_ENVIRONMENT == PLC_LINUX
static void* die_holding_thread(void* arg)
{
	if (plc_mutex_acquire((plc_mutex_t*)arg, 1000) == 0) {
		pthread_exit(NULL); // Leave without unlocking
	}
	return NULL;
}

void test_plc_mutex_release_leaves_a_dead_owners_lock_usable(void)
{
	pthread_t thread;
	plc_mutex_t* orphan = plc_mutex_create();
	TEST_ASSERT_NOT_NULL(orphan);

	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, die_holding_thread, orphan));
	TEST_ASSERT_EQUAL_INT(0, pthread_join(thread, NULL));

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1, plc_mutex_release(orphan));
	TEST_ASSERT_EQUAL_INT(EOWNERDEAD, errno);

	errno = 0;
	TEST_ASSERT_EQUAL_INT_MESSAGE(-1,
				      plc_mutex_acquire(orphan, 0),
				      "The mutex wasn't left held.");
	TEST_ASSERT_EQUAL_INT(EDEADLK, errno);

	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(orphan));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_destroy(orphan));
}

void test_plc_mutex_destroy_succeeds_after_the_owner_died_holding_it(void)
{
	pthread_t thread;
	plc_mutex_t* doomed = plc_mutex_create();
	TEST_ASSERT_NOT_NULL(doomed);

	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, die_holding_thread, doomed));
	TEST_ASSERT_EQUAL_INT(0, pthread_join(thread, NULL));

	errno = 255;
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_destroy(doomed));
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		255,
		errno,
		"errno was modified by plc_mutex_destroy, and it shouldn't");
}

void test_plc_mutex_acquire_reports_eownerdead_with_the_max_delay(void)
{
	pthread_t thread;
	plc_mutex_t* orphan = plc_mutex_create();
	TEST_ASSERT_NOT_NULL(orphan);

	TEST_ASSERT_EQUAL_INT(
		0, pthread_create(&thread, NULL, die_holding_thread, orphan));
	TEST_ASSERT_EQUAL_INT(0, pthread_join(thread, NULL));

	errno = 0;
	TEST_ASSERT_EQUAL_INT(-1,
			      plc_mutex_acquire(orphan, PLC_MUTEX_MAX_DELAY));
	TEST_ASSERT_EQUAL_INT(EOWNERDEAD, errno);

	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(orphan));
	TEST_ASSERT_EQUAL_INT(0,
			      plc_mutex_acquire(orphan, PLC_MUTEX_MAX_DELAY));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(orphan));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_destroy(orphan));
}
#else
void test_plc_mutex_release_leaves_a_dead_owners_lock_usable(void)
{
	TEST_IGNORE_MESSAGE("Needs a robust mutex");
}

void test_plc_mutex_destroy_succeeds_after_the_owner_died_holding_it(void)
{
	TEST_IGNORE_MESSAGE("Needs a robust mutex");
}

void test_plc_mutex_acquire_reports_eownerdead_with_the_max_delay(void)
{
	TEST_IGNORE_MESSAGE("Needs a robust mutex");
}

#endif
