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

#ifndef PLC_RESOURCE_PROTECTOR_H_
#define PLC_RESOURCE_PROTECTOR_H_

/*
 * The portable half of the resource protector API: hash-table/lock
 * bookkeeping built on top of the platform mutex functions declared in
 * plc-resource-protector-mutex.h.
 */
#include <plc-resource-protector-mutex.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	PLC_RESOURCE_GPIO,
	PLC_RESOURCE_I2C,
} plc_resource_type;

typedef uint64_t plc_resource_t;

#define I2C_RESOURCE(bus, address)                             \
	(((plc_resource_t)((uint64_t)PLC_RESOURCE_I2C << 56) | \
	  ((plc_resource_t)((uint64_t)(bus)) << 48) | (address)))

/**
 * plc_resource_init
 *
 * Initialize the PLC resource protector. It must be called before any other
 * function. This function is NOT multi-thread safe.
 *
 * Returns:
 *   int - 0 if successful, 1 if already initialized, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - ENOMEM : Out of memory during allocation.
 */
int plc_resource_init(void);

/**
 * plc_resource_deinit
 *
 * De-initialize the PLC resource protector. Removes and frees all locks.
 * This function is NOT multi-thread safe.
 *
 * Returns:
 *   int - 0 if successful, 1 if already de-initialized, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EBUSY : The PLC resource protector is in use.
 */
int plc_resource_deinit(void);

/**
 * plc_resource_add
 *
 * Add a new shared resource. This function can fail if other thread is using
 * the resource protector.
 *
 * Parameters:
 *   resource (plc_resource_t) - The resource to lock.
 *
 * Returns:
 *   int - 0 if successful, 1 if it was already added, and -1 if there was an
 *         error.
 *
 * Errors:
 *   errno set to:
 *     - ENOMEM : Out of memory during allocation.
 *     - EEXIST : The resource was already added.
 *     - EBUSY  : Hash mutex couldn't be taken.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int plc_resource_add(plc_resource_t resource);

/**
 * plc_resource_remove
 *
 * Remove a new shared resource.
 *
 * Parameters:
 *   resource (plc_resource_t) - The resource to lock.
 *
 * Returns:
 *   int - 0 if successful, 1 if it wasn't already present, and -1 if there was
 *         an error.
 *
 * Errors:
 *   errno set to:
 *     - ENODEV : The resource is not present.
 *     - EBUSY  : Hash mutex couldn't be taken.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int plc_resource_remove(plc_resource_t resource);

/**
 * plc_resource_lock
 *
 * Ask to lock a shared resource. You must ensure that you aren't adding or
 * removing resources while locking.
 *
 * Parameters:
 *   resource (plc_resource_t) - The resource to lock.
 *   timeout_ms (uint32_t)     - The maximum time to wait for the unlock
 *                               (in ms).
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
int plc_resource_lock(plc_resource_t resource, uint32_t timeout_ms);

/**
 * plc_resource_unlock
 *
 * Ask to unlock a shared resource. You must ensure that you aren't adding or
 * removing resources while unlocking.
 *
 * Parameters:
 *   resource (plc_resource_t) - The resource to unlock.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise (it probably wasn't taken!).
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : The passed mutex is invalid.
 *     - EALREADY            : Mutex is already free!
 */
int plc_resource_unlock(plc_resource_t resource);

#ifdef __cplusplus
}
#endif

#endif // PLC_RESOURCE_PROTECTOR_H_
