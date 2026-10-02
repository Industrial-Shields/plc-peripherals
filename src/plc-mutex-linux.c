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

// Needed for pthread_mutex_clocklock, a GNU extension
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <plc-mutex.h>
#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_LINUX

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
 * GCC's ThreadSanitizer (up to GCC 14 at least) intercepts pthread_mutex_lock,
 * trylock, timedlock and unlock, but not clocklock. LLVM's does, but clang
 * doesn't define __SANITIZE_THREAD__. Hardcode helpful macros from
 * libstdc++-v3/include/bits/shared_ptr_atomic.h to manually patch it.
 */
#if defined(__SANITIZE_THREAD__)
#include <sanitizer/tsan_interface.h>
#define PLC_TSAN_MUTEX_DESTROY(X) \
	__tsan_mutex_destroy(X, __tsan_mutex_not_static)
#define PLC_TSAN_MUTEX_TRY_LOCK(X) \
	__tsan_mutex_pre_lock(X,   \
			      __tsan_mutex_not_static | __tsan_mutex_try_lock)
#define PLC_TSAN_MUTEX_TRY_LOCK_FAILED(X) \
	__tsan_mutex_post_lock(           \
		X, __tsan_mutex_not_static | __tsan_mutex_try_lock_failed, 0)
#define PLC_TSAN_MUTEX_LOCKED(X) \
	__tsan_mutex_post_lock(X, __tsan_mutex_not_static, 0)
#define PLC_TSAN_MUTEX_PRE_UNLOCK(X) __tsan_mutex_pre_unlock(X, 0)
#define PLC_TSAN_MUTEX_POST_UNLOCK(X) __tsan_mutex_post_unlock(X, 0)
#define PLC_TSAN_MUTEX_PRE_SIGNAL(X) __tsan_mutex_pre_signal(X, 0)
#define PLC_TSAN_MUTEX_POST_SIGNAL(X) __tsan_mutex_post_signal(X, 0)
#else
#define PLC_TSAN_MUTEX_DESTROY(X)
#define PLC_TSAN_MUTEX_TRY_LOCK(X)
#define PLC_TSAN_MUTEX_TRY_LOCK_FAILED(X)
#define PLC_TSAN_MUTEX_LOCKED(X)
#define PLC_TSAN_MUTEX_PRE_UNLOCK(X)
#define PLC_TSAN_MUTEX_POST_UNLOCK(X)
#define PLC_TSAN_MUTEX_PRE_SIGNAL(X)
#define PLC_TSAN_MUTEX_POST_SIGNAL(X)
#endif // #if defined(__SANITIZE_THREAD__)

_Static_assert(sizeof(plc_mutex_t) == sizeof(pthread_mutex_t),
	       "Not exactly a pthread_mutex_t");
_Static_assert(PLC_PERIPHERAL_INTERNAL_ALIGNOF(plc_mutex_t) ==
		       PLC_PERIPHERAL_INTERNAL_ALIGNOF(pthread_mutex_t),
	       "Not aligned exactly as a pthread_mutex_t");

#define PTHREAD(m) ((pthread_mutex_t*)(m))

static int create_pthread_mutex(plc_mutex_t* mutex, plc_mutex_scope_t scope)
{
	assert(mutex != NULL);

	/*
	 * glibc registers a robust list per thread at thread start, and still
	 * accepts PTHREAD_MUTEX_ROBUST if the kernel refused it (seccomp,
	 * qemu-user, no futex cmpxchg). The mutex is then robust in name only:
	 * a dead owner is never reported and the lock wedges for good.
	 */
	void* robust_head = NULL;
	size_t robust_len = 0;
	if (syscall(SYS_get_robust_list, 0, &robust_head, &robust_len) != 0 ||
	    robust_head == NULL) {
		errno = ENOTSUP;
		return -1;
	}

	pthread_mutexattr_t attr;
	int local_errno;

	local_errno = pthread_mutexattr_init(&attr);
	if (local_errno != 0) {
		errno = local_errno;
		return -1;
	}

	/*
	 * - Reports EDEADLK if the thread already locked this mutex.
	 * - Reports EPERM if you try to unlock a mutex the thread
	 *   doesn't own.
	 */
	local_errno =
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	if (local_errno != 0) {
		goto create_pthread_mutex_error;
	}

	/*
	 * - Reports EOWNERDEAD if the last owner dies without
	 *   unlocking it, or if the new owner after receiving
	 *   EOWNERDEAD is terminated.
	 * - Reports ENOTRECOVERABLE if you unlock the mutex before
	 *   making it consistent again.
	 * - Destroying a robust mutex is legal now. But it shouldn't.
	 */
	local_errno = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
	if (local_errno != 0) {
		goto create_pthread_mutex_error;
	}

	/*
	 * - The owner runs at the priority of its highest waiter, so a
	 *   lower priority thread can't starve it while it holds the lock.
	 */
	local_errno =
		pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
	if (local_errno != 0) {
		goto create_pthread_mutex_error;
	}

	// Mutexes can be shared across threads and processes
	switch (scope) {
	case PLC_MUTEX_SCOPE_PRIVATE:
		local_errno = pthread_mutexattr_setpshared(
			&attr, PTHREAD_PROCESS_PRIVATE);

		break;
	case PLC_MUTEX_SCOPE_SHARED:
		local_errno = pthread_mutexattr_setpshared(
			&attr, PTHREAD_PROCESS_SHARED);

		break;
	default:
		local_errno = EINVAL;
		break;
	}
	if (local_errno != 0) {
		goto create_pthread_mutex_error;
	}

	local_errno = pthread_mutex_init(PTHREAD(mutex), &attr);

	/*
	 * Without priority inheritance futexes in the kernel, go without them.
	 * glibc's pthread_mutex_init only reports ENOTSUP for:
	 * - Priority inheritance, when the kernel has no PI futexes.
	 * - Priority protection on a robust mutex (which is impossible here).
	 * - A SHARED robust mutex, when the kernel refused the robust list.
	 *   It can only happen in very concrete situations (seccomp,
	 *   qemu-user, etc...).
	 */
	if (local_errno == ENOTSUP) {
		local_errno =
			pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_NONE);
		if (local_errno == 0) {
			local_errno = pthread_mutex_init(
				(pthread_mutex_t*)mutex, &attr);
		}
	}

	if (local_errno != 0) {
		goto create_pthread_mutex_error;
	}

	// Destroy should always work, since we used valid attributes
	local_errno = pthread_mutexattr_destroy(&attr);
	assert(local_errno == 0);

	return 0;

create_pthread_mutex_error:;
	int destroy_result = pthread_mutexattr_destroy(&attr);
	(void)destroy_result;
	assert(destroy_result == 0);
	errno = local_errno;
	return -1;
}

static int destroy_pthread_mutex(plc_mutex_t* mutex)
{
	assert(mutex != NULL);

	/*
	 * Best-effort to check if the mutex is being used before destroying it.
	 * trylock, unlike a timed lock, never waits in the kernel, which would
	 * abort on a lock-order cycle.
	 */
	int local_errno = pthread_mutex_trylock(PTHREAD(mutex));
	if (local_errno == EOWNERDEAD) {
		/*
		 * Only fails with EINVAL if the mutex is not robust or not
		 * inconsistent. It has just answered EOWNERDEAD, so it must be
		 * both.
		 */
		int consistent_result =
			pthread_mutex_consistent(PTHREAD(mutex));
		(void)consistent_result;
		assert(consistent_result == 0);
	} else if (local_errno != 0) {
		errno = local_errno == EDEADLK ? EBUSY : local_errno;
		return -1;
	}
	int unlock_result = pthread_mutex_unlock(PTHREAD(mutex));
	(void)unlock_result;
	assert(unlock_result == 0);

	local_errno = pthread_mutex_destroy(PTHREAD(mutex));
	if (local_errno == 0) {
		return 0;
	}

	// EBUSY if still locked, EINVAL if it was never a live mutex.
	errno = local_errno;
	return -1;
}

plc_mutex_t* plc_mutex_create(void)
{
	plc_mutex_t* mutex = malloc(sizeof(plc_mutex_t));

	if (mutex == NULL) {
		PLC_SET_MALLOC_ERRNO();
	} else if (create_pthread_mutex(mutex, PLC_MUTEX_SCOPE_PRIVATE) != 0) {
		free(mutex);
		return NULL;
	}

	return mutex;
}

int plc_mutex_destroy(plc_mutex_t* mutex)
{
	if (mutex == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (destroy_pthread_mutex(mutex) != 0) {
		return -1;
	}

	free(mutex);
	return 0;
}

int plc_mutex_static_create(plc_mutex_t* mutex, plc_mutex_scope_t scope)
{
	// The kernel can reject a non-aligned address for a futex...
	if (mutex == NULL || ((uintptr_t)mutex % PLC_MUTEX_ALIGN) != 0) {
		errno = EFAULT;
		return -1;
	}

	// Before create_pthread_mutex can refuse it for another reason
	if (scope != PLC_MUTEX_SCOPE_PRIVATE &&
	    scope != PLC_MUTEX_SCOPE_SHARED) {
		errno = EINVAL;
		return -1;
	}

	return create_pthread_mutex(mutex, scope);
}

int plc_mutex_static_destroy(plc_mutex_t* mutex)
{
	if (mutex == NULL) {
		errno = EFAULT;
		return -1;
	}

	return destroy_pthread_mutex(mutex);
}

static pthread_once_t deadline_clock_once = PTHREAD_ONCE_INIT;
static clockid_t deadline_clock;

static void pick_deadline_clock(void)
{
	/*
	 * glibc waits against CLOCK_MONOTONIC with FUTEX_LOCK_PI2, which
	 * kernels before 5.14 (and Valgrind) don't have, so it reports EINVAL.
	 * Relocking a normal priority inheritance mutex from its owner takes
	 * that same kernel path, and times out at once where it works.
	 *
	 * The probe is not made with create_pthread_mutex on purpose, since
	 * glibc answers the relock of an ERRORCHECK mutex with EDEADLK by
	 * itself, without ever asking the kernel.
	 */
	int saved_errno = errno;
	pthread_mutexattr_t attr;
	pthread_mutex_t probe;
	struct timespec now;

	// Set the default value
	deadline_clock = CLOCK_REALTIME;

	if (pthread_mutexattr_init(&attr) != 0) {
		goto pick_deadline_clock_end;
	}

	int init_result =
		pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
	if (init_result == 0) {
		init_result = pthread_mutex_init(&probe, &attr);
	}

	int attr_destroy_result = pthread_mutexattr_destroy(&attr);
	(void)attr_destroy_result;
	assert(attr_destroy_result == 0);

	if (init_result == ENOTSUP) {
		/*
		 * No PI futexes, so no mutex will have priority inheritance,
		 * and those can wait against the monotonic clock on any kernel.
		 */
		deadline_clock = CLOCK_MONOTONIC;
	}
	if (init_result != 0) {
		goto pick_deadline_clock_end;
	}

	if (pthread_mutex_lock(&probe) == 0) {
		if (clock_gettime(CLOCK_MONOTONIC, &now) == 0 &&
		    pthread_mutex_clocklock(&probe, CLOCK_MONOTONIC, &now) ==
			    ETIMEDOUT) {
			deadline_clock = CLOCK_MONOTONIC;
		}

		int unlock_result = pthread_mutex_unlock(&probe);
		(void)unlock_result;
		assert(unlock_result == 0);
	}

	int destroy_result = pthread_mutex_destroy(&probe);
	(void)destroy_result;
	assert(destroy_result == 0);

pick_deadline_clock_end:
	errno = saved_errno;
}

int plc_mutex_acquire(plc_mutex_t* mutex, uint32_t timeout_ms)
{
	int local_errno, consistent_result;
	struct timespec deadline;
	clockid_t clock;

	if (mutex == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (timeout_ms == PLC_MUTEX_MAX_DELAY) {
		local_errno = pthread_mutex_lock(PTHREAD(mutex));
		if (local_errno == 0) {
			return 0;
		} else if (local_errno == EOWNERDEAD) {
			/*
			 * Only fails with EINVAL if the mutex is not robust or not
			 * inconsistent. It has just answered EOWNERDEAD, so it must be
			 * both. Cover it, so if any mad person tries to acquire a
			 * non-robust mutex with this library or something...
			 */
			consistent_result =
				pthread_mutex_consistent(PTHREAD(mutex));
			(void)consistent_result;
			assert(consistent_result == 0);
			errno = EOWNERDEAD;
			return -1;
		}

		errno = local_errno;
		return -1;
	}

	pthread_once(&deadline_clock_once, pick_deadline_clock);
	clock = deadline_clock;

	if (clock_gettime(clock, &deadline) != 0) {
		errno = ENOTSUP;
		return -1;
	}

	deadline.tv_sec += timeout_ms / 1000;
	deadline.tv_nsec += (timeout_ms % 1000) * 1000000L;
	if (deadline.tv_nsec >= 1000000000L) {
		deadline.tv_sec++;
		deadline.tv_nsec -= 1000000000L;
	}

	PLC_TSAN_MUTEX_TRY_LOCK(PTHREAD(mutex));

	local_errno = pthread_mutex_clocklock(PTHREAD(mutex), clock, &deadline);
	if (local_errno == 0) {
		PLC_TSAN_MUTEX_LOCKED(mutex);
		return 0;
	} else if (local_errno == EOWNERDEAD) {
		PLC_TSAN_MUTEX_LOCKED(mutex);
		/*
		 * Only fails with EINVAL if the mutex is not robust or not
		 * inconsistent. It has just answered EOWNERDEAD, so it must be
		 * both. Cover it, so if any mad person tries to acquire a
		 * non-robust mutex with this library or something...
		 */
		consistent_result = pthread_mutex_consistent(PTHREAD(mutex));
		(void)consistent_result;
		assert(consistent_result == 0);
		errno = EOWNERDEAD;
		return -1;
	}

	PLC_TSAN_MUTEX_TRY_LOCK_FAILED(PTHREAD(mutex));

	// The documented timeout error is EBUSY, as trylock reports it.
	if (local_errno == ETIMEDOUT) {
		local_errno = EBUSY;
	}

	errno = local_errno;
	return -1;
}

int plc_mutex_release(plc_mutex_t* mutex)
{
	if (mutex == NULL) {
		errno = EFAULT;
		return -1;
	}

	int local_errno = pthread_mutex_unlock(PTHREAD(mutex));
	if (local_errno == 0) {
		return 0;
	}

	if (local_errno == EPERM) {
		/*
		 * Best-effort to try and determine if we tried to unlock an
		 * already unlocked mutex, or if the owner died while
		 * holding it...
		 */
		local_errno = pthread_mutex_trylock(PTHREAD(mutex));
		if (local_errno == 0) {
			int unlock_result =
				pthread_mutex_unlock(PTHREAD(mutex));
			(void)unlock_result;
			assert(unlock_result == 0);
			errno = EALREADY;
		} else if (local_errno == EOWNERDEAD) {
			/*
			 * Only fails with EINVAL if the mutex is not robust or
			 * not inconsistent. It has just answered EOWNERDEAD, so
			 * it must be both. Cover it, so if any mad person tries
			 * to acquire a non-robust mutex with this library or
			 * something...
			 */
			int consistent_result =
				pthread_mutex_consistent(PTHREAD(mutex));
			(void)consistent_result;
			assert(consistent_result == 0);
			errno = EOWNERDEAD;
		} else {
			errno = EPERM;
		}
	} else {
		errno = local_errno;
	}
	return -1;
}

#endif // PLC_ENVIRONMENT == PLC_LINUX
