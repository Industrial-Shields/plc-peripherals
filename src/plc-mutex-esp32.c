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

#include <plc-mutex.h>
#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF

#include <assert.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <errno.h>

typedef struct {
	StaticSemaphore_t _buf;
	SemaphoreHandle_t handle;
	atomic_bool is_locked;
} error_checker_mutex_t;

_Static_assert(sizeof(plc_mutex_t) == sizeof(error_checker_mutex_t),
	       "Not exactly an error_checker_mutex_t");
_Static_assert(PLC_PERIPHERAL_INTERNAL_ALIGNOF(plc_mutex_t) ==
		       PLC_PERIPHERAL_INTERNAL_ALIGNOF(error_checker_mutex_t),
	       "Not aligned exactly as an error_checker_mutex_t");

#define ECM(m) ((error_checker_mutex_t*)m)

static int create_esp_mutex(error_checker_mutex_t* mutex)
{
	assert(mutex != NULL);

	mutex->handle = xSemaphoreCreateMutexStatic(&(mutex->_buf));
	if (mutex->handle == NULL) {
		errno = ENOMEM;
		return -1;
	}

	mutex->is_locked = false;
	return 0;
}

static int destroy_esp_mutex(error_checker_mutex_t* mutex)
{
	assert(mutex != NULL);
	assert(mutex->handle != NULL);

	if (mutex->is_locked) {
		errno = EBUSY;
		return -1;
	}

	vSemaphoreDelete(mutex->handle);
	mutex->handle = NULL;
	return 0;
}

plc_mutex_t* plc_mutex_create(void)
{
	error_checker_mutex_t* mutex_struct =
		ECM(malloc(sizeof(error_checker_mutex_t)));

	if (mutex_struct == NULL) {
		PLC_SET_MALLOC_ERRNO();
	}

	else if (create_esp_mutex(mutex_struct) != 0) {
		free(mutex_struct);
		mutex_struct = NULL;
	}

	return (plc_mutex_t*)mutex_struct;
}

int plc_mutex_destroy(plc_mutex_t* mutex)
{
	if (mutex == NULL || ECM(mutex)->handle == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (destroy_esp_mutex(ECM(mutex)) != 0) {
		return -1;
	}

	free(mutex);
	return 0;
}

int plc_mutex_static_create(plc_mutex_t* mutex, plc_mutex_scope_t scope)
{
	if (mutex == NULL || ((uintptr_t)mutex % PLC_MUTEX_ALIGN) != 0) {
		errno = EFAULT;
		return -1;
	}

	if (scope == PLC_MUTEX_SCOPE_PRIVATE) {
		errno = ENOTSUP;
		return -1;
	} else if (scope != PLC_MUTEX_SCOPE_SHARED) {
		errno = EINVAL;
		return -1;
	}

	return create_esp_mutex(ECM(mutex));
}

int plc_mutex_static_destroy(plc_mutex_t* mutex)
{
	if (mutex == NULL || ECM(mutex)->handle == NULL) {
		errno = EFAULT;
		return -1;
	}

	return destroy_esp_mutex(ECM(mutex));
}

_Static_assert(portMAX_DELAY == (TickType_t)0xffffffffUL,
	       "MAX_WAIT_MS assumes 32-bit ticks");
_Static_assert(INCLUDE_vTaskSuspend == 1,
	       "PLC_MUTEX_MAX_DELAY needs portMAX_DELAY to wait forever");

/*
 * portMAX_DELAY is 2³² - 1. That means that the last
 * non-infinite timeout is 2³² - 2.
 * (2³² - 2) * 1000 / configTICK_RATE_HZ, capped at uint32_t
 */
static const uint64_t MAX_WAIT_MS_64 =
	(((uint64_t)4294967296) - 2) * 1000 / configTICK_RATE_HZ;
static const uint32_t MAX_WAIT_MS = (uint32_t)(MAX_WAIT_MS_64 >= UINT32_MAX ?
						       UINT32_MAX - 1 :
						       MAX_WAIT_MS_64);

// Integer ceil(n / d), for n + d - 1 that does not overflow
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

int plc_mutex_acquire(plc_mutex_t* mutex, uint32_t timeout_ms)
{
	if (mutex == NULL || ECM(mutex)->handle == NULL) {
		errno = EFAULT;
		return -1;
	}

	/*
	 * pdMS_TO_TICKS multiplies in 32 bits.
	 *
	 * Rounded up, so the wait is never shorter than asked.
	 */
	TickType_t ticks =
		timeout_ms == PLC_MUTEX_MAX_DELAY ?
			portMAX_DELAY :
			(TickType_t)DIV_ROUND_UP((uint64_t)timeout_ms *
							 configTICK_RATE_HZ,
						 1000U);

	if (timeout_ms > MAX_WAIT_MS && timeout_ms != PLC_MUTEX_MAX_DELAY) {
		errno = EINVAL;
		return -1;
	}

	// Try to detect a deadlock
	if (xSemaphoreGetMutexHolder(ECM(mutex)->handle) ==
	    xTaskGetCurrentTaskHandle()) {
		errno = EDEADLK;
		return -1;
	}

	if (xSemaphoreTake(ECM(mutex)->handle, ticks) == pdTRUE) {
		ECM(mutex)->is_locked = true;
		return 0;
	}

	errno = EBUSY;
	return -1;
}

int plc_mutex_release(plc_mutex_t* mutex)
{
	int ret;

	if (mutex == NULL || ECM(mutex)->handle == NULL) {
		errno = EFAULT;
		return -1;
	}

	/*
	 * FreeRTOS aborts when a task tries to give a mutex it doesn't hold,
	 * so try to refuse before calling it.
	 */
	TaskHandle_t holder = xSemaphoreGetMutexHolder(ECM(mutex)->handle);
	if (holder == xTaskGetCurrentTaskHandle()) {
		ECM(mutex)->is_locked = false;
		xSemaphoreGive(ECM(mutex)->handle);
		ret = 0;
	} else if (holder == NULL) {
		errno = EALREADY;
		ret = -1;
	} else {
		errno = EPERM;
		ret = -1;
	}

	return ret;
}

#endif // PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
