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

#ifndef PLC_PERIPHERALS_I2C_H_
#define PLC_PERIPHERALS_I2C_H_

/*
 * The portable half of the I2C API: register-level helpers built on top of the
 * platform functions declared in plc-peripherals-i2c-hal.h.
 */
#include <plc-peripherals-i2c-hal.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * i2c_write8_8b
 *
 * Write a byte to a register of the given I2C address.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - I2C interface to write to.
 *   addr (plc_i2c_addr_t)         - I2C address to write to.
 *   reg (uint8_t)                 - Register address.
 *   to_write (uint8_t)            - Value to write to the register
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled)  : Some of the arguments given is invalid (bad
 *                              i2c_interface, invalid address, bad write
 * 				array...
 *     - ESP32 specific       :
 *       - EIO                : i2cWrite function reported some error.
 */
int i2c_write8_8b(const i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  uint8_t reg,
		  uint8_t to_write);

/**
 * i2c_write8_16b
 *
 * Write a byte to a register of the given I2C address.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - I2C interface to write to.
 *   addr (plc_i2c_addr_t)         - I2C address to write to.
 *   reg (uint8_t)                 - Register address.
 *   to_write (uint16_t)           - Value to write to the register
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled)  : Some of the arguments given is invalid (bad
 *                              i2c_interface, invalid address, bad write
 * 				array...
 *     - ESP32 specific       :
 *       - EIO                : i2cWrite function reported some error.
 */
int i2c_write8_16b(const i2c_interface_t* i2c,
		   plc_i2c_addr_t addr,
		   uint8_t reg,
		   uint16_t to_write);

/**
 * i2c_read8_8b
 *
 * Read a one-byte register from the given I2C address.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - I2C interface to read to.
 *   addr (plc_i2c_addr_t)         - I2C address to read to.
 *   reg (uint8_t)                 - Register address.
 *   to_read (uint8_t*)            - Pointer to save the register value.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled)  : Some of the arguments given is invalid (bad
 *                              i2c_interface, invalid address, bad read
 * 				array...
 *     - ESP32 specific       :
 *       - EIO                : i2cRead function reported some error.
 */
int i2c_read8_8b(const i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t reg,
		 uint8_t* to_read);

/**
 * i2c_read8_16b
 *
 * Read a one-byte register from the given I2C address.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - I2C interface to read to.
 *   addr (plc_i2c_addr_t)         - I2C address to read to.
 *   reg (uint8_t)                 - Register address.
 *   to_read (uint16_t*)           - Pointer to save the register value.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled)  : Some of the arguments given is invalid (bad
 *                              i2c_interface, invalid address, bad read
 * 				array...
 *     - ESP32 specific       :
 *       - EIO                : i2cRead function reported some error.
 */
int i2c_read8_16b(const i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  uint8_t reg,
		  uint16_t* to_read);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERALS_I2C_H_
