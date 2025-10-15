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

#define I2C_RESOURCE(address) \
	(((plc_resource_t)((uint64_t)PLC_RESOURCE_I2C << 56) | address))

/**
 * plc_resource_init
 *
 * Initialize the PLC resource protector. It must be called before any other
 * function.
 *
 * Returns:
 *   int - 0 if successful, 1 if already initialized, -1 otherwise.
 */
int plc_resource_init(void);

/**
 * plc_resource_deinit
 *
 * De-initialize the PLC resource protector. Removes and frees all locks.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 */
int plc_resource_deinit(void);

/**
 * plc_resource_add
 *
 * Add a new shared resource.
 *
 * Parameters:
 *   resource (plc_resource_t) - The resource to lock.
 *
 * Returns:
 *   int - 0 if successful, 1 if it was already added, and -1 if there was an
 *         error.
 *
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
 */
int plc_resource_remove(plc_resource_t resource);

/**
 * plc_resource_lock
 *
 * Ask to lock a shared resource.
 *
 * Parameters:
 *   resource (plc_resource_t) - The resource to lock.
 *   timeout_ms (uint32_t)     - The maximum time to wait for the unlock
 *                               (in ms).
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 */
int plc_resource_lock(plc_resource_t resource, uint32_t timeout_ms);

/**
 * plc_resource_unlock
 *
 * Ask to unlock a shared resource.
 *
 * Parameters:
 *   resource (plc_resource_t) - The resource to unlock.
 *   timeout_ms (uint32_t)     - The maximum time to wait for the unlock
 *                               (in ms).
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 */
int plc_resource_unlock(plc_resource_t resource, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLC_RESOURCE_PROTECTOR_H_
