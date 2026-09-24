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
 * A lock-order deadlock aborts the process on glibc, instead of timing out as
 * plc_mutex_acquire promises.
 *
 * If A holds X and waits for Y while B holds Y and waits for X, the kernel
 * detects the cycle on priority inheritance mutexes and reports EDEADLK.
 * glibc asserts that an ERRORCHECK mutex never gets that answer, and aborts:
 *
 *   Fatal glibc error: pthread_mutex_timedlock.c:370
 *   (__pthread_mutex_clocklock_common): assertion failed: e != EDEADLK ||
 *   (kind != PTHREAD_MUTEX_ERRORCHECK_NP && kind != PTHREAD_MUTEX_RECURSIVE_NP)
 *
 * It can be built and run by hand, from the project root:
 *
 *   gcc -Iinclude -Isubmodules/Unity/src -DUNITY_EXCLUDE_FLOAT \
 *       test/known-issues/plc-mutex-lock-order-linux.c \
 *       src/plc-mutex-linux.c submodules/Unity/src/unity.c \
 *       -lpthread -o build/plc-mutex-lock-order && build/plc-mutex-lock-order
 */

#include "unity.h"
#include "plc-mutex.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

static plc_mutex_t* mutex;

void setUp(void)
{
	mutex = plc_mutex_create();
	TEST_ASSERT_NOT_NULL(mutex);
}

void tearDown(void)
{
	plc_mutex_release(mutex);
	plc_mutex_destroy(mutex);
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

	if (plc_mutex_acquire(args->second, 2000) == 0) {
		plc_mutex_release(args->second);
	}
	plc_mutex_release(args->first);
	return NULL;
}

void test_plc_mutex_acquire_times_out_on_a_lock_order_deadlock(void)
{
	plc_mutex_t* other = plc_mutex_create();
	TEST_ASSERT_NOT_NULL(other);
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));

	// The worker holds other, and waits for mutex
	lock_order_args_t args = { .first = other,
				   .second = mutex,
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
	TEST_ASSERT_TRUE_MESSAGE(atomic_load(&args.acquired),
				 "Worker thread never acquired the mutex");
	usleep(100 * 1000); // Let it block on mutex

	// Closes the cycle: holding mutex, waiting for other
	errno = 0;
	int result = plc_mutex_acquire(other, 300);
	int error = errno;

	plc_mutex_release(mutex);
	pthread_join(thread, NULL);
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_destroy(other));

	TEST_ASSERT_EQUAL_INT(-1, result);
	TEST_ASSERT_EQUAL_INT(EBUSY, error);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_plc_mutex_acquire_times_out_on_a_lock_order_deadlock);
	return UNITY_END();
}
