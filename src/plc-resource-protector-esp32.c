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

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF

#include <errno.h>

typedef struct {
	SemaphoreHandle_t m;
} error_checker_mutex_t;
#define ECM(m) ((error_checker_mutex_t*)m)

plc_mutex_t* plc_mutex_create(void)
{
	error_checker_mutex_t* mutex_struct =
		(error_checker_mutex_t*)malloc(sizeof(error_checker_mutex_t));
	ECM(mutex_struct)->m = xSemaphoreCreateMutex();
	return (plc_mutex_t*)mutex_struct;
}

int plc_mutex_destroy(plc_mutex_t* mutex_struct)
{
	if (mutex_struct == NULL || ECM(mutex_struct)->m == NULL) {
		errno = EINVAL;
		return -1;
	}

	vSemaphoreDelete(ECM(mutex_struct)->m);
	free(mutex_struct);
	return 0;
}

int plc_mutex_acquire(plc_mutex_t* mutex_struct, uint32_t timeout)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (mutex_struct == NULL || ECM(mutex_struct)->m == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	if (xSemaphoreTake(ECM(mutex_struct)->m, pdMS_TO_TICKS(timeout)) ==
	    pdTRUE) {
		return 0;
	}

	errno = EBUSY;
	return -1;
}

int plc_mutex_release(plc_mutex_t* mutex_struct)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (mutex_struct == NULL || ECM(mutex_struct)->m == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	if (xSemaphoreGive(ECM(mutex_struct)->m) == pdTRUE) {
		return 0;
	}

	errno = EALREADY;
	return -1;
}

#endif // PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
