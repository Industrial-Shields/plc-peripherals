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
 * A waiter in another PID namespace than the owner of a SHARED mutex is aborted
 * by glibc, instead of timing out.
 *
 * Waiting on a priority inheritance mutex makes the kernel look up the
 * owner's TID in the waiter's PID namespace. Where that TID doesn't exist,
 * it answers ESRCH, and glibc asserts that a robust mutex never gets it:
 *
 *   Fatal glibc error: pthread_mutex_timedlock.c:375
 *   (__pthread_mutex_clocklock_common): assertion failed: e != ESRCH || !robust
 *
 * The waiter is the first process in the new PID namespace, so its init,
 * which ignores SIGABRT. glibc's abort then ends in SIGSEGV, so expect exit
 * status 139 instead of 134.
 *
 * It can be built and run by hand, from the project root:
 *
 *   gcc -Iinclude -Isubmodules/Unity/src -DUNITY_EXCLUDE_FLOAT \
 *       test/known-issues/plc-mutex-pid-namespace-linux.c \
 *       src/plc-mutex-linux.c submodules/Unity/src/unity.c \
 *       -lpthread -o build/plc-mutex-pid-namespace && \
 *       build/plc-mutex-pid-namespace
 */

#define _GNU_SOURCE // unshare, CLONE_NEWPID

#include "unity.h"
#include "plc-mutex.h"

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define NO_PID_NAMESPACE 77

void setUp(void)
{
}

void tearDown(void)
{
}

// Exit status of the waiter: 0 for the promised EBUSY, 128+signal if killed
static int wait_from_another_pid_namespace(plc_mutex_t* mutex)
{
	if (unshare(CLONE_NEWPID) != 0) {
		return NO_PID_NAMESPACE;
	}
	// Only children created from now on are in the new namespace

	pid_t waiter = fork();
	if (waiter < 0) {
		return 1;
	}
	if (waiter == 0) {
		errno = 0;
		int result = plc_mutex_acquire(mutex, 500);
		_exit(result == -1 && errno == EBUSY ? 0 : 1);
	}

	int status = 0;
	if (waitpid(waiter, &status, 0) != waiter) {
		return 1;
	}
	return WIFSIGNALED(status) ? 128 + WTERMSIG(status) :
				     WEXITSTATUS(status);
}

void test_plc_mutex_acquire_times_out_from_another_pid_namespace(void)
{
	plc_mutex_t* mutex = mmap(NULL,
				  sizeof(plc_mutex_t),
				  PROT_READ | PROT_WRITE,
				  MAP_SHARED | MAP_ANONYMOUS,
				  -1,
				  0);
	TEST_ASSERT_NOT_EQUAL(MAP_FAILED, mutex);
	TEST_ASSERT_EQUAL_INT(
		0, plc_mutex_static_create(mutex, PLC_MUTEX_SCOPE_SHARED));
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_acquire(mutex, 0));

	// A helper enters the new namespace, so this process never does
	fflush(NULL);
	pid_t helper = fork();
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, helper);
	if (helper == 0) {
		_exit(wait_from_another_pid_namespace(mutex));
	}

	int status = 0;
	TEST_ASSERT_EQUAL_INT(helper, waitpid(helper, &status, 0));
	int waiter_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

	plc_mutex_release(mutex);
	plc_mutex_static_destroy(mutex);
	munmap(mutex, sizeof(plc_mutex_t));

	if (waiter_status == NO_PID_NAMESPACE) {
		TEST_IGNORE_MESSAGE(
			"Needs CAP_SYS_ADMIN to create a PID namespace");
	}

	char message[64];
	snprintf(message,
		 sizeof(message),
		 "Waiter exit status %d (128+signal if killed)",
		 waiter_status);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, waiter_status, message);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_plc_mutex_acquire_times_out_from_another_pid_namespace);
	return UNITY_END();
}
