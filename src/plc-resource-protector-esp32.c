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

plc_mutex_t* plc_mutex_create(void)
{
	SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
	return (plc_mutex_t*)mutex;
}

int plc_mutex_destroy(plc_mutex_t* mutex)
{
	if (mutex == NULL) {
		errno = EINVAL;
		return -1;
	}

	vSemaphoreDelete(mutex);
	return 0;
}

int plc_mutex_acquire(plc_mutex_t* mutex, uint32_t timeout)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (mutex == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	if (xSemaphoreTake(mutex, pdMS_TO_TICKS(timeout)) == pdTRUE) {
		return 0;
	}

	errno = EBUSY;
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

	if (xSemaphoreGive(mutex) == pdTRUE) {
		return 0;
	}

	errno = EALREADY;
	return -1;
}

#endif // PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
