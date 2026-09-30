/*
 * Copyright (c) 2026 Industrial Shields. All rights reserved
 *
 * This file is part of plc-peripherals.
 *
 * plc-peripherals is free software: you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version
 * 3 of the License, or (at your option) any later version.
 *
 * plc-peripherals is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <plc-delay.h>
#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_LINUX

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define US_PER_S 1000000U
#define NS_PER_US 1000L
#define NS_PER_S 1000000000L

static pthread_once_t delay_clock_once = PTHREAD_ONCE_INIT;
static clockid_t delay_clock;

static void pick_delay_clock(void)
{
	/*
	 * A kernel may not have CLOCK_MONOTONIC, or not sleep on it. Sleeping
	 * until a deadline that has already passed returns at once where it
	 * works.
	 */
	int saved_errno = errno;
	struct timespec now;

	// Set the default value
	delay_clock = CLOCK_REALTIME;

	if (clock_gettime(CLOCK_MONOTONIC, &now) == 0 &&
	    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &now, NULL) == 0) {
		delay_clock = CLOCK_MONOTONIC;
	}

	errno = saved_errno;
}

static int delay_monotonic(uint32_t us)
{
	int saved_errno = errno;
	struct timespec deadline;
	int local_errno;

	// clock_gettime sets errno, instead of returning it.
	if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
		local_errno = errno;
		errno = saved_errno;
		return local_errno;
	}

	deadline.tv_sec += us / US_PER_S;
	deadline.tv_nsec += (long)(us % US_PER_S) * NS_PER_US;
	if (deadline.tv_nsec >= NS_PER_S) {
		deadline.tv_sec++;
		deadline.tv_nsec -= NS_PER_S;
	}

	do {
		local_errno = clock_nanosleep(
			CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
	} while (local_errno == EINTR);

	return local_errno;
}

static int delay_realtime(uint32_t us)
{
	struct timespec left = {
		.tv_sec = us / US_PER_S,
		.tv_nsec = (long)(us % US_PER_S) * NS_PER_US,
	};
	int local_errno;

	do {
		local_errno = clock_nanosleep(CLOCK_REALTIME, 0, &left, &left);
	} while (local_errno == EINTR);

	return local_errno;
}

int plc_delay_us(uint32_t us)
{
	int local_errno;

	if (us == 0) {
		return 0;
	}

	pthread_once(&delay_clock_once, pick_delay_clock);

	if (delay_clock == CLOCK_MONOTONIC) {
		local_errno = delay_monotonic(us);
	} else {
		local_errno = delay_realtime(us);
	}

	if (local_errno != 0) {
		errno = local_errno;
		return -1;
	}

	return 0;
}

#endif // PLC_ENVIRONMENT == PLC_LINUX
