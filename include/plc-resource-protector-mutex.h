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

#ifndef PLC_MUTEX_H_
#define PLC_MUTEX_H_

/*
 * The platform-specific half of the resource protector: one implementation
 * per target environment (see plc-resource-protector-linux.c,
 * plc-resource-protector-esp32.c). The portable hash-table/lock bookkeeping is
 * built on top of these functions and lives in plc-resource-protector.h,
 * which includes this header. Include that one instead unless you are
 * implementing or replacing the platform layer.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void plc_mutex_t;

/**
 * plc_mutex_create
 *
 * Create a single mutex, and return it as argument.
 *
 * Returns:
 *   plc_mutex_t* - Pointer to the initialized interface on success.
 *                  NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - ENOMEM : Out of memory during allocation.
 */
plc_mutex_t* plc_mutex_create(void);

/**
 * plc_mutex_destroy
 *
 * Destroy a single mutex.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL: The passed mutex is invalid.
 *     - EBUSY : Mutex can't be destroyed while in use.
 */
int plc_mutex_destroy(plc_mutex_t* mutex);

/**
 * plc_mutex_acquire
 *
 * Try to acquire a single mutex.
 *
 * Parameters:
 *   mutex (plc_mutex_t)   - The mutex to acquire to lock.
 *   timeout_ms (uint32_t) - The maximum time to wait for the unlock
 *                           (in ms).
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled): The passed mutex is invalid
 *     - EBUSY              : Mutex couldn't be taken within the timeout given.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int plc_mutex_acquire(plc_mutex_t* mutex, uint32_t timeout_ms);

/**
 * plc_mutex_release
 *
 * Try to release an acquired mutex.
 *
 * Parameters:
 *   mutex (plc_mutex_t)   - The mutex to acquire to lock.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise (it probably wasn't taken!).
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : The passed mutex is invalid.
 *     - EALREADY            : Mutex is already free!
 */
int plc_mutex_release(plc_mutex_t* mutex);

#ifdef __cplusplus
}
#endif

#endif // PLC_MUTEX_H_
