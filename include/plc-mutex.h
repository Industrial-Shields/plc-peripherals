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

#ifndef PLC_MUTEX_H_
#define PLC_MUTEX_H_

/*
 * The resource protector: one implementation per target environment
 * (see plc-mutex-linux.c, plc-mutex-esp32.c).
 */
#include <stdint.h>

#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_LINUX
#include <pthread.h>
#elif PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#else
#error "Static mutex storage not defined for this platform."
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus)
#define PLC_MUTEX_INTERNAL_ALIGNOF(t) alignof(t)
#define PLC_MUTEX_INTERNAL_ALIGNAS(t) alignas(t)
#else
#define PLC_MUTEX_INTERNAL_ALIGNOF(t) _Alignof(t)
#define PLC_MUTEX_INTERNAL_ALIGNAS(t) _Alignas(t)
#endif

#define PLC_MUTEX_INTERNAL_PAD(n, a) ((((n) + (a) - 1) / (a)) * (a))

#if PLC_ENVIRONMENT == PLC_LINUX
#define PLC_MUTEX_INTERNAL_NATIVE pthread_mutex_t
#define PLC_MUTEX_INTERNAL_SIZE (sizeof(pthread_mutex_t))
#elif PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
#define PLC_MUTEX_INTERNAL_NATIVE StaticSemaphore_t
#define PLC_MUTEX_INTERNAL_SIZE                               \
	PLC_MUTEX_INTERNAL_PAD(sizeof(StaticSemaphore_t) + 1, \
			       PLC_MUTEX_INTERNAL_ALIGNOF(StaticSemaphore_t))
#endif

/*
 * Storage for one mutex. Useful to statically allocate, without malloc.
 *
 * A region handed to plc_mutex_static_create must be at least PLC_MUTEX_SIZE
 * bytes and at least PLC_MUTEX_ALIGN aligned.
 *
 * WARNING: Never copy a live mutex. Assigning a plc_mutex_t, embedding one in a
 * struct that is assigned or passed by value, memcpying it, or reallocating an
 * array of them all do it. The backend does not necessarily support it!
 */
#define PLC_MUTEX_SIZE PLC_MUTEX_INTERNAL_SIZE
#define PLC_MUTEX_ALIGN PLC_MUTEX_INTERNAL_ALIGNOF(PLC_MUTEX_INTERNAL_NATIVE)

typedef struct {
	PLC_MUTEX_INTERNAL_ALIGNAS(PLC_MUTEX_INTERNAL_NATIVE)
	unsigned char opaque[PLC_MUTEX_SIZE];
} plc_mutex_t;

/**
 * plc_mutex_scope_t
 *
 * The scopes a plc_mutex_t can use.
 *
 *   PLC_MUTEX_SCOPE_PRIVATE - Only for threads of the calling process. Don't
 *                             use it from a forked child, even in shared
 *                             memory.
 *   PLC_MUTEX_SCOPE_SHARED  - Any process mapping it, forked children
 *                             included.
 *                             Linux specific:
 *                               - All the processes that use a SHARED mutex
 *                                 should be in the same PID namespace.
 *                                 Otherwise, a waiter may get a false EDEADLK
 *                                 instead of waiting (assuming trustable
 *                                 process).
 *                               - All the processes that use a SHARED mutex
 *                                 must be built for the same architecture
 *                                 and word size. A 32-bit and a 64-bit
 *                                 process disagree on its layout, and leave
 *                                 it locked forever.
 *                               - Never unmap the storage while holding the
 *                                 mutex. If the process then dies, it stays
 *                                 locked forever.
 */
typedef enum {
	PLC_MUTEX_SCOPE_PRIVATE,
	PLC_MUTEX_SCOPE_SHARED,
} plc_mutex_scope_t;

/**
 * PLC_MUTEX_MAX_DELAY
 *
 * Wait until the mutex is acquired, without any time limit.
 */
#define PLC_MUTEX_MAX_DELAY UINT32_MAX

/**
 * plc_mutex_create
 *
 * Allocate a single mutex, and return a pointer to it.
 *
 * The scope is the narrowest the platform offers, which is
 * PLC_MUTEX_SCOPE_PRIVATE wherever that exists.
 *
 * Use plc_mutex_static_create, with SHARED scope and storage you placed in a
 * shared mapping, if another process (child or independent) must see it.
 *
 * WARNING: Tear down with plc_mutex_destroy, never with
 *          plc_mutex_static_destroy.
 *
 * Linux specific: not async-signal-safe. Called from a signal handler it can
 * deadlock inside malloc, with no timeout to break it.
 *
 * Returns:
 *   plc_mutex_t* - Pointer to the ready mutex on success.
 *                  NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - ENOMEM : Out of memory during allocation.
 *     - EINVAL : Arguments to create the mutex were invalid (internal
 *                failure).
 *     - Linux specific:
 *       - ENOTSUP: The kernel refused this thread's robust list, so a dead
 *                  owner could never be recovered. See
 *                  https://gitlab.com/qemu-project/qemu/-/work_items/2424
 */
plc_mutex_t* plc_mutex_create(void);

/**
 * plc_mutex_destroy
 *
 * Destroy a single mutex, and free the storage plc_mutex_create allocated
 * for it.
 *
 * WARNING: Never use this on a plc_mutex_static_create mutex. And ensure that
 *          the mutex is NOT being used by other threads.
 *
 * Linux specific: not async-signal-safe. Called from a signal handler it can
 * deadlock inside free, with no timeout to break it.
 *
 * Parameters:
 *   mutex (plc_mutex_t*) - The mutex to destroy.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT: Passed mutex is NULL.
 *     - EINVAL: May fail if the passed mutex is invalid.
 *     - EBUSY : Mutex can't be destroyed while in use. "In use" only means
 *               "locked" right now, not about to be locked...
 *               A mutex whose owner died holding it isn't in use, so
 *               it's destroyed.
 */
int plc_mutex_destroy(plc_mutex_t* mutex);

/**
 * plc_mutex_static_create
 *
 * Create a mutex on the memory region passed by argument. You must ensure that
 * this region is at least of PLC_MUTEX_SIZE, and at least PLC_MUTEX_ALIGN
 * aligned.
 *
 * The storage must not already hold a live mutex, otherwise it's UB.
 *
 * WARNING: Tear down with plc_mutex_static_destroy, never with
 *          plc_mutex_destroy.
 *
 * Linux specific: not async-signal-safe. Do not call it from a signal
 * handler.
 *
 * Parameters:
 *   mutex (plc_mutex_t*)      - Storage to make a mutex out of.
 *   scope (plc_mutex_scope_t) - Who the mutex has to exclude.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT : Passed mutex storage is NULL, or not PLC_MUTEX_ALIGN
 *                aligned.
 *     - EINVAL : The scope is not a plc_mutex_scope_t value, or arguments to
 *                create the mutex were invalid (internal failure).
 *     - ENOMEM : Out of memory setting the mutex up.
 *     - ENOTSUP: The scope is unsupported on this platform. For example, ESP32
 *                rejects _PRIVATE, since all mutexes are shared.
 *     - Linux specific:
 *       - ENOTSUP: The kernel refused this thread's robust list, so a dead
 *                  owner could never be recovered. See
 *                  https://gitlab.com/qemu-project/qemu/-/work_items/2424
 *
 */
int plc_mutex_static_create(plc_mutex_t* mutex, plc_mutex_scope_t scope);

/**
 * plc_mutex_static_destroy
 *
 * Tear down a mutex made by plc_mutex_static_create. The storage is never
 * freed.
 *
 * WARNING: Never use this on a plc_mutex_create mutex. And ensure that the
 *          mutex is NOT being used by other threads or processes.
 *
 * Linux specific: not async-signal-safe. Do not call it from a signal
 * handler.
 *
 * Parameters:
 *   mutex (plc_mutex_t*) - The mutex to tear down.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT: Passed mutex is NULL.
 *     - EINVAL: May fail if the passed mutex is invalid.
 *     - EBUSY : Mutex can't be torn down while in use. "In use" only means
 *               "locked" right now, not about to be locked...
 *               A mutex whose owner died holding it isn't in use, so
 *               it's torn down.
 */
int plc_mutex_static_destroy(plc_mutex_t* mutex);

/**
 * plc_mutex_acquire
 *
 * Try to acquire a single mutex.
 *
 * Linux specific:
 *   - Not async-signal-safe. A handler runs on top of the thread it
 *     interrupted and inherits its ownership, so this is refused with
 *     EDEADLK and the handler's work is skipped, not delayed.
 *   - On kernels before 5.14 (and under Valgrind), the timeout follows the
 *     wall clock, so setting the clock forward or back shortens or stretches
 *     it. They can't wait on a priority inheritance mutex against the
 *     monotonic clock.
 *
 * Parameters:
 *   mutex (plc_mutex_t*)  - The mutex to lock.
 *   timeout_ms (uint32_t) - The maximum time to wait for the unlock (in ms).
 *                           0 doesn't wait. It fails with EBUSY at once if
 *                           another owner holds the mutex.
 *                           PLC_MUTEX_MAX_DELAY waits without a time limit.
 *
 * Returns:
 *   int - 0 if acquired, -1 otherwise (unless errno == EOWNERDEAD, which
 *         means acquired, but with possible half-write leftovers).
 *
 * Errors:
 *   errno set to:
 *     - EFAULT          : Passed mutex is NULL.
 *     - EINVAL          : May fail if the passed mutex is invalid.
 *     - EBUSY           : Mutex couldn't be taken within the timeout given.
 *                         Never with PLC_MUTEX_MAX_DELAY.
 *     - EDEADLK         : The calling thread already holds the mutex.
 *     - EOWNERDEAD      : A previous owner died holding the mutex. You must
 *                         recover from half-writes, and release the lock after
 *                         you are done.
 *     - Linux specific:
 *       - ENOTSUP: The clock the timeout is measured on isn't available.
 */
int plc_mutex_acquire(plc_mutex_t* mutex, uint32_t timeout_ms);

/**
 * plc_mutex_release
 *
 * Try to release an acquired mutex.
 *
 * WARNING: A mutex whose acquire reported EOWNERDEAD was acquired, and is
 * released exactly like any other.
 *
 * Linux specific: not async-signal-safe. A handler runs on top of the thread
 * it interrupted and inherits its ownership, so this succeeds and strips the
 * lock from a thread still inside its critical section.
 *
 * Parameters:
 *   mutex (plc_mutex_t*)  - The mutex to unlock.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise (it probably wasn't taken!).
 *
 * Errors:
 *   errno set to:
 *     - EFAULT     : Passed mutex is NULL.
 *     - EINVAL     : May fail if the passed mutex is invalid.
 *     - EALREADY   : Mutex is already free!
 *     - EPERM      : Another thread holds the mutex.
 *     - EOWNERDEAD : A previous owner died holding the mutex. You can only
 *                    find this error if you tried to release a mutex that
 *                    isn't yours, and whose owner died before releasing
 *                    it (which is very strange). The lock is now held by you.
 */
int plc_mutex_release(plc_mutex_t* mutex);

#ifdef __cplusplus
}
#endif

#endif // PLC_MUTEX_H_
