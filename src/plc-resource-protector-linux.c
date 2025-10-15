/*
 * Copyright (c) 2025 Industrial Shields. All rights reserved
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

#include <plc-resource-protector.h>
#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_LINUX

#include <stddef.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>

plc_mutex_t* plc_mutex_create(void)
{
	pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));

	if (mutex != NULL) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
		pthread_mutex_init(mutex, &attr);
	}

	return (plc_mutex_t*)mutex;
}

int plc_mutex_destroy(plc_mutex_t* mutex)
{
	if (mutex == NULL) {
		errno = EINVAL;
		return -1;
	}

	int result = pthread_mutex_destroy((pthread_mutex_t*)mutex);
	if (result == 0) {
		return 0;
	}

	errno = EBUSY;
	return -1;
}

int plc_mutex_acquire(plc_mutex_t* mutex, uint32_t timeout)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (mutex == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	struct timespec start, now, deadline;

	int local_errno;
	local_errno = pthread_mutex_trylock(mutex);
	if (local_errno == 0) {
		return 0;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
		errno = EINVAL;
		return -1;
	}

	deadline.tv_sec = start.tv_sec + timeout / 1000;
	deadline.tv_nsec = start.tv_nsec + (timeout % 1000) * 1000000L;
	if (deadline.tv_nsec >= 1000000000L) {
		deadline.tv_sec++;
		deadline.tv_nsec -= 1000000000L;
	}

	do {
		local_errno = pthread_mutex_trylock(mutex);
		if (local_errno == 0) {
			return 0;
		} else if (local_errno == EBUSY) {
			if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
				local_errno = EINVAL;
				break;
			}
			if (now.tv_sec > deadline.tv_sec ||
			    (now.tv_sec == deadline.tv_sec &&
			     now.tv_nsec >= deadline.tv_nsec)) {
				local_errno = EBUSY;
				break;
			}
		} else {
			break;
		}
	} while (true);

	errno = local_errno;
	return -1;
}

int plc_mutex_release(plc_mutex_t* mutex)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (mutex == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	int local_errno = pthread_mutex_unlock(mutex);
	if (local_errno == 0) {
		return 0;
	}

	if (local_errno == EPERM) {
		errno = EBUSY;
	}
	else {
		errno = local_errno;
	}
	return -1;
}

#endif // PLC_ENVIRONMENT == PLC_LINUX
