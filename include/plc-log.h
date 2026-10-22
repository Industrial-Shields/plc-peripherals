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

#ifndef PLC_LOG_H_
#define PLC_LOG_H_

/*
 * The logging of the library, with the same macros on every platform. A source
 * file that logs defines PLC_LOG_TAG as a string literal naming it, then calls
 * PLC_LOGx(format, ...) with a printf format:
 *   - Arduino-ESP32 : Arduino's log_x, filtered by its Core Debug Level. At
 *                     level None, they compile to nothing.
 *   - ESP-IDF       : ESP_LOGx with PLC_LOG_TAG as the tag, filtered by the
 *                     log levels of menuconfig and esp_log_level_set.
 *   - Linux         : Not implemented, they compile to nothing.
 */

#if !defined(PLC_LOG_TAG)
#error "PLC_LOG_TAG is not defined!"
#endif

#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32
#include <esp32-hal-log.h>

// clang-format off
#define PLC_LOGE(format, ...) log_e("[" PLC_LOG_TAG "] " format, ##__VA_ARGS__)
#define PLC_LOGW(format, ...) log_w("[" PLC_LOG_TAG "] " format, ##__VA_ARGS__)
#define PLC_LOGI(format, ...) log_i("[" PLC_LOG_TAG "] " format, ##__VA_ARGS__)
#define PLC_LOGD(format, ...) log_d("[" PLC_LOG_TAG "] " format, ##__VA_ARGS__)
#define PLC_LOGV(format, ...) log_v("[" PLC_LOG_TAG "] " format, ##__VA_ARGS__)
// clang-format on

#elif PLC_ENVIRONMENT == PLC_ESP_IDF
#include <esp_log.h>

#define PLC_LOGE(format, ...) ESP_LOGE(PLC_LOG_TAG, format, ##__VA_ARGS__)
#define PLC_LOGW(format, ...) ESP_LOGW(PLC_LOG_TAG, format, ##__VA_ARGS__)
#define PLC_LOGI(format, ...) ESP_LOGI(PLC_LOG_TAG, format, ##__VA_ARGS__)
#define PLC_LOGD(format, ...) ESP_LOGD(PLC_LOG_TAG, format, ##__VA_ARGS__)
#define PLC_LOGV(format, ...) ESP_LOGV(PLC_LOG_TAG, format, ##__VA_ARGS__)

#else
#define PLC_LOGE(format, ...) ((void)0)
#define PLC_LOGW(format, ...) ((void)0)
#define PLC_LOGI(format, ...) ((void)0)
#define PLC_LOGD(format, ...) ((void)0)
#define PLC_LOGV(format, ...) ((void)0)

#endif

#endif // PLC_LOG_H_
