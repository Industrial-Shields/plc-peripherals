/*
 * Copyright (c) 2024 Industrial Shields. All rights reserved
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

#ifndef __DETECT_PLATFORM_H__
#define __DETECT_PLATFORM_H__

// Compatible platforms
#define Linux 0
#define Arduino_ESP32 1

#if defined(ARDUINO_ESP32) || defined(ARDUINO_ARCH_ESP32)
#define PLC_ENVIRONMENT Arduino_ESP32

#elif defined(__linux__)
#define PLC_ENVIRONMENT Linux

#else
    #error "Unknown environment detected"
#endif

#endif // __DETECT_PLATFORM_H__
