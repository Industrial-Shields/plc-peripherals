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

#ifndef PLC_PERIPHERALS_PLATFORMS_H_
#define PLC_PERIPHERALS_PLATFORMS_H_

// Compatible platforms
#define PLC_LINUX 0
#define PLC_ARDUINO_ESP32 1
#define PLC_ESP_IDF 2

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#if defined(__linux__)
#define PLC_ENVIRONMENT LINUX

#elif defined(ESP_ARDUINO_VERSION)
#define PLC_ENVIRONMENT PLC_ARDUINO_ESP32

#elif defined(ESP_IDF) || defined(ESP_PLATFORM) ||       \
	defined(__ESP_PLATFORM__) || defined(IDF_VER) || \
	defined(CONFIG_IDF_TARGET)
#define PLC_ENVIRONMENT PLC_ESP_IDF

#endif

#ifndef PLC_ENVIRONMENT
#error "Unknown environment detected"
#endif

#endif // PLC_PERIPHERALS_PLATFORMS_H_
