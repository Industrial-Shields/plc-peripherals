/*
 * Copyright (c) 2026 Industrial Shields. All rights reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef I2C_PROBE_H_
#define I2C_PROBE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Probe every 7-bit address from 0x08 to 0x77 on the bus, and print the ones
 * that answer to stdout. Returns the number of devices found, or -1 if the bus
 * can't be opened or probed.
 */
int i2c_probe_scan(uint8_t bus, int32_t sda, int32_t scl);

#ifdef __cplusplus
}
#endif

#endif // I2C_PROBE_H_
