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
 * The Linux backend on a kernel, simulated without priority inheritance
 * futexes.
 *
 * It's on a separate test suite because glibc checks the availability of PI
 * futexes the first time a priority inheritance mutex is initialised, and
 * caches the answer.
 */

#include "unity.h"

#include "plc-peripherals-platform.h"
#include "plc-mutex.h"

// No header of its own maps to plc-mutex-linux.c
#if PLC_ENVIRONMENT == PLC_LINUX
TEST_SOURCE_FILE("plc-mutex-linux.c")
#endif

#include <errno.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/filter.h>
#include <linux/futex.h>
#include <linux/seccomp.h>

void setUp(void)
{
}

void tearDown(void)
{
}

// glibc asks FUTEX_UNLOCK_PI, and takes ENOSYS as "no PI futexes"
static int hide_priority_inheritance_futexes(void)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
			 offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_futex, 0, 3),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
			 offsetof(struct seccomp_data, args[1])),
		BPF_STMT(BPF_ALU | BPF_AND | BPF_K, FUTEX_CMD_MASK),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, FUTEX_UNLOCK_PI, 1, 0),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ENOSYS),
	};
	struct sock_fprog program = { .len = sizeof(filter) / sizeof(filter[0]),
				      .filter = filter };

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
		return -1;
	}
	return prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program);
}

void test_plc_mutex_works_without_priority_inheritance_futexes(void)
{
	fflush(NULL);
	pid_t pid = fork();
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, pid);

	if (pid == 0) {
		if (hide_priority_inheritance_futexes() != 0) {
			_exit(1);
		}

		plc_mutex_t* created = plc_mutex_create();
		if (created == NULL) {
			_exit(2);
		}
		if (plc_mutex_acquire(created, 100) != 0 ||
		    plc_mutex_release(created) != 0 ||
		    plc_mutex_destroy(created) != 0) {
			_exit(3);
		}

		plc_mutex_t storage;
		if (plc_mutex_static_create(&storage, PLC_MUTEX_SCOPE_SHARED) !=
		    0) {
			_exit(4);
		}
		if (plc_mutex_acquire(&storage, 100) != 0 ||
		    plc_mutex_release(&storage) != 0 ||
		    plc_mutex_static_destroy(&storage) != 0) {
			_exit(5);
		}
		_exit(0);
	}

	int status = 0;
	TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
	TEST_ASSERT_TRUE_MESSAGE(WIFEXITED(status), "Child didn't exit");
	TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(
		1, WEXITSTATUS(status), "Couldn't hide the PI futexes");
	TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(
		2, WEXITSTATUS(status), "plc_mutex_create failed");
	TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(
		4, WEXITSTATUS(status), "plc_mutex_static_create failed");
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0, WEXITSTATUS(status), "The mutex didn't work");
}

void test_plc_mutex_timeout_follows_the_monotonic_clock_without_pi_futexes(void)
{
	TEST_IGNORE_MESSAGE(
		"TODO: The monotonic clock without PI futexes is not tested");
}
