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
 * The Linux backend on a simulated kernel that can't sleep on
 * CLOCK_MONOTONIC.
 *
 * It's on a separate test suite because plc-delay-linux.c picks its clock the
 * first time it's used, and keeps it.
 */

#include "unity.h"

#include "plc-delay.h"

// No header of its own maps to plc-delay-linux.c
TEST_SOURCE_FILE("plc-delay-linux.c")

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <linux/filter.h>
#include <linux/seccomp.h>

void setUp(void)
{
}

void tearDown(void)
{
}

static long elapsed_us(struct timespec start, struct timespec end)
{
	return (end.tv_sec - start.tv_sec) * 1000000L +
	       (end.tv_nsec - start.tv_nsec) / 1000L;
}

static volatile sig_atomic_t signals_handled;

static void count_signal(int signum)
{
	(void)signum;
	signals_handled++;
}

// clock_nanosleep fails with EINVAL on CLOCK_MONOTONIC, as if unsupported.
static int hide_monotonic_sleeps(void)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
			 offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_clock_nanosleep, 0, 2),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
			 offsetof(struct seccomp_data, args[0])),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, CLOCK_MONOTONIC, 1, 0),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EINVAL),
	};
	struct sock_fprog program = { .len = sizeof(filter) / sizeof(filter[0]),
				      .filter = filter };

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
		return -1;
	}
	return prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program);
}

void test_plc_delay_us_falls_back_to_the_realtime_clock(void)
{
	fflush(NULL);
	pid_t pid = fork();
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, pid);

	if (pid == 0) {
		struct sigaction action;
		struct itimerval timer;
		struct timespec start, end;

		if (hide_monotonic_sleeps() != 0) {
			_exit(1);
		}

		clock_gettime(CLOCK_MONOTONIC, &start);
		if (clock_nanosleep(
			    CLOCK_MONOTONIC, TIMER_ABSTIME, &start, NULL) !=
		    EINVAL) {
			_exit(1);
		}

		// No SA_RESTART, so the signal really interrupts clock_nanosleep.
		memset(&action, 0, sizeof(action));
		action.sa_handler = count_signal;
		sigemptyset(&action.sa_mask);
		if (sigaction(SIGALRM, &action, NULL) != 0) {
			_exit(2);
		}

		// A signal every 5 ms, through a 50 ms delay.
		memset(&timer, 0, sizeof(timer));
		timer.it_value.tv_usec = 5000;
		timer.it_interval.tv_usec = 5000;
		if (setitimer(ITIMER_REAL, &timer, NULL) != 0) {
			_exit(2);
		}

		clock_gettime(CLOCK_MONOTONIC, &start);
		errno = 0;
		if (plc_delay_us(50000) != 0 || errno != 0) {
			_exit(3);
		}
		clock_gettime(CLOCK_MONOTONIC, &end);

		if (signals_handled == 0) {
			_exit(4);
		}
		if (elapsed_us(start, end) < 50000) {
			_exit(5);
		}
		_exit(0);
	}

	int status = 0;
	TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
	TEST_ASSERT_TRUE_MESSAGE(WIFEXITED(status), "Child didn't exit");
	TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(
		1, WEXITSTATUS(status), "Couldn't hide the monotonic sleeps");
	TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(
		2, WEXITSTATUS(status), "Couldn't set up the signals");
	TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(
		3, WEXITSTATUS(status), "plc_delay_us failed");
	TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(
		4, WEXITSTATUS(status), "No signal interrupted the delay");
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0, WEXITSTATUS(status), "The delay ended early");
}
