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

#include <plc-peripherals-version.h>

#define xstr(a) str(a)
#define str(a) #a



const int LIB_PLC_PERIPHERALS_VERSION_MAJOR_NUM = __LIB_PLC_PERIPHERALS_VERSION_MAJOR_NUM;
const int LIB_PLC_PERIPHERALS_VERSION_MINOR_NUM = __LIB_PLC_PERIPHERALS_VERSION_MINOR_NUM;
const int LIB_PLC_PERIPHERALS_VERSION_PATCH_NUM = __LIB_PLC_PERIPHERALS_VERSION_PATCH_NUM;

const char* LIB_PLC_PERIPHERALS_VERSION_MAJOR = xstr(__LIB_PLC_PERIPHERALS_VERSION_MAJOR_NUM);
const char* LIB_PLC_PERIPHERALS_VERSION_MINOR = xstr(__LIB_PLC_PERIPHERALS_VERSION_MINOR_NUM);
const char* LIB_PLC_PERIPHERALS_VERSION_PATCH = xstr(__LIB_PLC_PERIPHERALS_VERSION_PATCH_NUM);
const char* LIB_PLC_PERIPHERALS_VERSION = \
	xstr(__LIB_PLC_PERIPHERALS_VERSION_MAJOR_NUM) "." \
	xstr(__LIB_PLC_PERIPHERALS_VERSION_MINOR_NUM) "." \
	xstr(__LIB_PLC_PERIPHERALS_VERSION_PATCH_NUM);
