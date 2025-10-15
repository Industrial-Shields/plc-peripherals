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
#include <stdatomic.h>
#include <uthash.h>

#include <errno.h>

typedef struct {
	plc_resource_t resource;
	UT_hash_handle hh;
} lock_hash_table_t;
static lock_hash_table_t* locks = NULL;
static atomic_bool initialized = false;

int plc_resource_init(void)
{
	if (initialized) {
		return 1;
	}
	initialized = true;
	return 0;
}

int plc_resource_deinit(void)
{
	if (!initialized) {
		return 1;
	}

	lock_hash_table_t* current_lock;
	lock_hash_table_t* tmp;

	HASH_ITER(hh, locks, current_lock, tmp)
	{
		HASH_DEL(locks, current_lock);
		free(current_lock);
	}

	locks = NULL;
	initialized = false;
	return 0;
}

int plc_resource_add(plc_resource_t resource)
{
	lock_hash_table_t* tmp = NULL;

	HASH_FIND_INT(locks, &resource, tmp);
	if (tmp == NULL) {
		tmp = (lock_hash_table_t*)malloc(sizeof(lock_hash_table_t));
		if (tmp == NULL) {
			return -1;
		}
		tmp->resource = resource;
		HASH_ADD_INT(locks, resource, tmp);
		return 0;
	} else {
		errno = EEXIST;
		return 1; // Already added
	}
}

int plc_resource_remove(plc_resource_t resource)
{
	lock_hash_table_t* tmp = NULL;

	HASH_FIND_INT(locks, &resource, tmp);
	if (tmp != NULL) {
		HASH_DEL(locks, tmp);
		free(tmp);
		return 0;
	} else {
		errno = ENODEV;
		return 1; // Not added
	}
}

/*
int plc_resource_lock(plc_resource_t resource, uint32_t timeout_ms)
{
	return -1;
}

int plc_resource_unlock(plc_resource_t resource, uint32_t timeout_ms)
{
	return -1;
}
*/
