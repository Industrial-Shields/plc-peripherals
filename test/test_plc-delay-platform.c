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
 * Tests for src/plc-delay-linux.c and src/plc-delay-esp32.c. They mock
 * nothing: they wait on the real clock. Only lower bounds are checked, since a
 * delay may always take longer (a loaded machine, valgrind, the kernel's timer
 * slack, other tasks), but never shorter.
 *
 * This file also runs as the Arduino ESP32 sketch at
 * test/Arduino/test_plc-delay-platform/ (symlinked in, alongside the platform
 * source and Unity itself). That .ino declares each test_* function with
 * extern "C" and calls it via RUN_TEST(); if you add, remove, or rename a
 * test_* function here, update the .ino to match.
 */

#include "unity.h"
#include "plc-peripherals-platform.h"
#include "plc-delay.h"

// No header of its own maps to plc-delay-linux.c
#if PLC_ENVIRONMENT == PLC_LINUX
TEST_SOURCE_FILE("plc-delay-linux.c")
#endif

#include <errno.h>
#include <string.h>
#include <time.h>

#if PLC_ENVIRONMENT == PLC_LINUX
#include <signal.h>
#include <sys/time.h>
#elif PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
/*
 * Because Ceedling things, who tries to copy every literal #include of a test
 * file into its runner, even a disabled one. And Linux doesn't have FreeRTOS,
 * of course.
 */
#define PLC_FREERTOS_H <freertos/FreeRTOS.h>
#define PLC_FREERTOS_TASK_H <freertos/task.h>
#include PLC_FREERTOS_H
#include PLC_FREERTOS_TASK_H
#endif

static long elapsed_us(struct timespec start, struct timespec end)
{
	return (end.tv_sec - start.tv_sec) * 1000000L +
	       (end.tv_nsec - start.tv_nsec) / 1000L;
}

#if PLC_ENVIRONMENT == PLC_LINUX
static volatile sig_atomic_t signals_handled;

static void count_signal(int signum)
{
	(void)signum;
	signals_handled++;
}

void setUp(void)
{
	signals_handled = 0;
}

void tearDown(void)
{
	struct itimerval off;

	memset(&off, 0, sizeof(off));
	setitimer(ITIMER_REAL, &off, NULL);
	signal(SIGALRM, SIG_DFL);
}
#else
void setUp(void)
{
}

void tearDown(void)
{
}
#endif // PLC_ENVIRONMENT == PLC_LINUX

void test_plc_delay_us_returns_at_once_for_0(void)
{
	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, plc_delay_us(0));
	TEST_ASSERT_EQUAL_INT(0, errno);
}

void test_plc_delay_us_waits_at_least_the_delay(void)
{
	struct timespec start, end;

	clock_gettime(CLOCK_MONOTONIC, &start);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, plc_delay_us(20000));
	clock_gettime(CLOCK_MONOTONIC, &end);

	TEST_ASSERT_EQUAL_INT(0, errno);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(20000, elapsed_us(start, end));
}

void test_plc_delay_us_waits_at_least_a_delay_shorter_than_a_tick(void)
{
	// On ESP32, it is spun as a whole, without vTaskDelay.
	struct timespec start, end;

	clock_gettime(CLOCK_MONOTONIC, &start);
	TEST_ASSERT_EQUAL_INT(0, plc_delay_us(500));
	clock_gettime(CLOCK_MONOTONIC, &end);

	TEST_ASSERT_GREATER_OR_EQUAL_INT(500, elapsed_us(start, end));
}

void test_plc_delay_us_waits_at_least_the_delay_across_a_second(void)
{
	// The deadline's nanoseconds carry into its seconds.
	struct timespec start, end;

	clock_gettime(CLOCK_MONOTONIC, &start);
	TEST_ASSERT_EQUAL_INT(0, plc_delay_us(999999));
	clock_gettime(CLOCK_MONOTONIC, &end);

	TEST_ASSERT_GREATER_OR_EQUAL_INT(999999, elapsed_us(start, end));
}

#if PLC_ENVIRONMENT == PLC_LINUX
void test_plc_delay_us_keeps_waiting_after_a_handled_signal(void)
{
	struct sigaction action;
	struct itimerval timer;
	struct timespec start, end;

	// No SA_RESTART, so the signal really interrupts clock_nanosleep.
	memset(&action, 0, sizeof(action));
	action.sa_handler = count_signal;
	sigemptyset(&action.sa_mask);
	TEST_ASSERT_EQUAL_INT(0, sigaction(SIGALRM, &action, NULL));

	// A signal every 5 ms, through a 50 ms delay.
	memset(&timer, 0, sizeof(timer));
	timer.it_value.tv_usec = 5000;
	timer.it_interval.tv_usec = 5000;
	TEST_ASSERT_EQUAL_INT(0, setitimer(ITIMER_REAL, &timer, NULL));

	clock_gettime(CLOCK_MONOTONIC, &start);
	errno = 0;
	TEST_ASSERT_EQUAL_INT(0, plc_delay_us(50000));
	clock_gettime(CLOCK_MONOTONIC, &end);

	TEST_ASSERT_EQUAL_INT(0, errno);
	TEST_ASSERT_GREATER_THAN_INT(0, signals_handled);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(50000, elapsed_us(start, end));
}
#else
void test_plc_delay_us_keeps_waiting_after_a_handled_signal(void)
{
	TEST_IGNORE_MESSAGE("Linux signals only");
}
#endif // PLC_ENVIRONMENT == PLC_LINUX

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
static volatile uint32_t lower_priority_runs;

static void count_runs(void* arg)
{
	(void)arg;

	for (;;) {
		lower_priority_runs++;
	}
}

void test_plc_delay_us_lets_a_lower_priority_task_run_for_whole_ticks(void)
{
	/*
	 * On the same core, a lower priority task only runs while this one
	 * blocks, so it can only count during vTaskDelay, never during a spin.
	 */
	TaskHandle_t counter;
	UBaseType_t priority = uxTaskPriorityGet(NULL);

	TEST_ASSERT_GREATER_THAN_UINT32(0, priority);
	lower_priority_runs = 0;
	TEST_ASSERT_EQUAL_INT(pdPASS,
			      xTaskCreatePinnedToCore(count_runs,
						      "count_runs",
						      2048,
						      NULL,
						      priority - 1,
						      &counter,
						      xPortGetCoreID()));

	TEST_ASSERT_EQUAL_INT(0, plc_delay_us(20000));
	uint32_t runs = lower_priority_runs;
	vTaskDelete(counter);

	TEST_ASSERT_GREATER_THAN_UINT32(0, runs);
}
#else
void test_plc_delay_us_lets_a_lower_priority_task_run_for_whole_ticks(void)
{
	TEST_IGNORE_MESSAGE("FreeRTOS only");
}
#endif // PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
