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
 * Real-thread stress test for the Linux mutex backend
 * (src/plc-mutex-linux.c).
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
#include <stdatomic.h>
#include <stdint.h>

#define NUM_MUTEXES 4
#define NUM_THREADS 8
#define ITERATIONS_PER_THREAD 500
#define LOCK_TIMEOUT_MS 1000

static plc_mutex_t* mutexes[NUM_MUTEXES];

// Per-mutex count of threads currently holding it.
static atomic_int concurrent_holders[NUM_MUTEXES];
static atomic_int max_concurrent_holders_seen[NUM_MUTEXES];

// Set by any thread that gets an unexpected error from lock/unlock. Unity
// assertions aren't safe to call from worker threads, so failures are
// recorded here and asserted on on the main thread only, after every
// thread has been joined.
static atomic_int unexpected_lock_failures;
static atomic_int unexpected_unlock_failures;

static void* hammer_thread(void* arg)
{
	uintptr_t thread_id = (uintptr_t)arg;

	for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
		int idx = (int)((thread_id + (uintptr_t)i) % NUM_MUTEXES);

		if (plc_mutex_acquire(mutexes[idx], LOCK_TIMEOUT_MS) != 0) {
			atomic_fetch_add(&unexpected_lock_failures, 1);
			continue;
		}

		int holders = atomic_fetch_add(&concurrent_holders[idx], 1) + 1;
		int prev_max = atomic_load(&max_concurrent_holders_seen[idx]);
		while (holders > prev_max &&
		       !atomic_compare_exchange_weak(
			       &max_concurrent_holders_seen[idx],
			       &prev_max,
			       holders)) {
			// retry: another thread updated the max concurrently
		}

		atomic_fetch_sub(&concurrent_holders[idx], 1);

		if (plc_mutex_release(mutexes[idx]) != 0) {
			atomic_fetch_add(&unexpected_unlock_failures, 1);
		}
	}

	return NULL;
}

void setUp(void)
{
	for (int i = 0; i < NUM_MUTEXES; i++) {
		mutexes[i] = plc_mutex_create();
		TEST_ASSERT_NOT_NULL(mutexes[i]);
		atomic_store(&concurrent_holders[i], 0);
		atomic_store(&max_concurrent_holders_seen[i], 0);
	}

	atomic_store(&unexpected_lock_failures, 0);
	atomic_store(&unexpected_unlock_failures, 0);
}

void tearDown(void)
{
	for (int i = 0; i < NUM_MUTEXES; i++) {
		plc_mutex_destroy(mutexes[i]);
	}
}

void test_concurrent_acquire_release_never_lets_two_threads_hold_one_mutex(void)
{
	pthread_t threads[NUM_THREADS];

	for (uintptr_t i = 0; i < NUM_THREADS; i++) {
		TEST_ASSERT_EQUAL_INT(
			0,
			pthread_create(
				&threads[i], NULL, hammer_thread, (void*)i));
	}

	for (int i = 0; i < NUM_THREADS; i++) {
		TEST_ASSERT_EQUAL_INT(0, pthread_join(threads[i], NULL));
	}

	TEST_ASSERT_EQUAL_INT(0, atomic_load(&unexpected_lock_failures));
	TEST_ASSERT_EQUAL_INT(0, atomic_load(&unexpected_unlock_failures));

	for (int i = 0; i < NUM_MUTEXES; i++) {
		TEST_ASSERT_LESS_OR_EQUAL_INT(
			1, atomic_load(&max_concurrent_holders_seen[i]));
	}
}
