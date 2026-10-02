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

#include <plc-delay.h>
#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define US_PER_S 1000000U

int plc_delay_us(uint32_t us)
{
	if (us == 0) {
		return 0;
	}

	// esp_timer counts microseconds since boot, and never goes back.
	const int64_t deadline = esp_timer_get_time() + us;

	/*
	 * We block the task until ticks - 1 to not make large busy waits. The
	 * spin below waits out the rest.
	 */
	const TickType_t ticks =
		(TickType_t)((uint64_t)us * configTICK_RATE_HZ / US_PER_S);
	if (ticks > 0) {
		vTaskDelay(ticks);
	}

	/*
	 * Spin until the deadline, yielding to any other ready task of the same
	 * priority or higher.
	 */
	while (esp_timer_get_time() < deadline) {
		taskYIELD();
	}

	return 0;
}

int plc_time_us(uint64_t* us)
{
	if (us == NULL) {
		errno = EFAULT;
		return -1;
	}
	*us = (uint64_t)esp_timer_get_time();
	return 0;
}

#endif // PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
