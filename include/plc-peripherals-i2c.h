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

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>

/*
 * If uncommented, the I2C functions check that the arguments are valid (check
 * for NULLs, invalid addresses...)
 */
// #define PLC_PERIPHERALS_CHECK_ARGUMENTS

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure representing an I2C interface.
 */
struct _i2c_interface_t;
typedef struct _i2c_interface_t i2c_interface_t;

// Type alias for I2C addresses
typedef uint16_t plc_i2c_addr_t;

/**
 * i2c_init
 *
 * It's a platform-specific function.
 *
 * Initialize an I2C interface for the current target environment, and if
 * applicable, initialize the I2C bus.
 *
 * Parameters:
 *   bus (uint8_t) - I2C bus number.
 *   sda (int32_t) - SDA GPIO number (if needed).
 *   scl (int32_t) - SCL GPIO number (if needed).
 *
 * Returns:
 *   i2c_interface_t* - Pointer to the initialized interface on success.
 *                      NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - ENOMEM        : Out of memory during allocation.
 *     - ESP32 specific:
 *       - EIO         : i2cInit function reported some error.
 */
i2c_interface_t* i2c_init(uint8_t bus, int32_t sda, int32_t scl);

/**
 * i2c_get_bus
 *
 * It's a platform-specific function.
 *
 * Get the I2C bus number of the given interface.
 *
 * Parameters:
 *   i2c (i2c_interface_t*) - I2C interface to get the bus number from.
 *   bus (uint8_t*)         - Pointer to store the bus number in.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL : Passed i2c_interface is NULL or invalid.
 */
int i2c_get_bus(i2c_interface_t* i2c, uint8_t* bus);

/**
 * i2c_deinit
 *
 * It's a platform-specific function.
 *
 * De-initialize an I2C interface for the current target environment, and if
 * applicable, de-initialize the bus if deinit_i2c_bus is true.
 *
 * Parameters:
 *   i2c (i2c_interface_t*) - I2C interface to de-initialize.
 *   deinit_i2c_bus (bool)  - If true, also de-initialize the I2C bus.
 *
 * Returns:
 *   int - 0 if successful, 1 if the bus was de-initialized, or -1 if some error
 *         happens.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL        : Passed i2c_interface is NULL.
 *     - ESP32 specific:
 *       - EIO         : i2cDeinit function reported some error.
 */
int i2c_deinit(i2c_interface_t* interface, bool deinit_i2c_bus);

/**
 * i2c_write
 *
 * It's a platform-specific function.
 *
 * Write "to_write_len" bytes from the "to_write" buffer to the I2C device "addr".
 *
 * Parameters:
 *   i2c (i2c_interface_t*)     - I2C interface to write on.
 *   addr (plc_i2c_addr_t)      - I2C address to write to.
 *   to_write (const uint8_t*)  - Array of bytes to write to the passed I2C address.
 *   to_write_len (size_t)      - Length of the array.
 *
 * Returns:
 *   ssize_t - The number of bytes written (it can be 0), or -1 if an error happens.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled)  : Some of the arguments given is invalid (bad
 *                              i2c_interface, invalid address, bad write
 * 				array...
 *     - ESP32 specific       :
 *       - EIO                : i2cWrite function reported some error.
 */
ssize_t i2c_write(i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  const uint8_t* to_write,
		  size_t to_write_len);

/**
 * i2c_read
 *
 * It's a platform-specific function.
 *
 * Read "to_read_len" bytes from the I2C device "addr" and put them in the
 * "to_read" buffer.
 *
 * Parameters:
 *   i2c (i2c_interface_t*)     - I2C interface to read from.
 *   addr (plc_i2c_addr_t)      - I2C address to read from.
 *   to_read (const uint8_t*)   - Array of bytes to read from the passed I2C address.
 *   to_read_len (size_t)       - Number of bytes to read. It must be equal or
 *                                greater than the array length.
 *
 * Returns:
 *   ssize_t - The number of bytes read (it can be 0), or -1 if an error happens.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled)  : Some of the arguments given is invalid (bad
 *                              i2c_interface, invalid address, bad read
 * 				array...
 *     - ESP32 specific       :
 *       - EIO                : i2cRead function reported some error.
 */
ssize_t i2c_read(i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t* to_read,
		 size_t to_read_len);

/**
 * i2c_write_then_read
 *
 * It's a platform-specific function.
 *
 * First write "to_write_len" bytes from "to_write" to "addr". Then read
 * "to_read_bytes" bytes to "to_read" from the same transaction.
 *
 * Parameters:
 *   i2c (i2c_interface_t*)     - I2C interface to read from.
 *   addr (plc_i2c_addr_t)      - I2C address to read from.
 *   to_write (const uint8_t*)  - Array of bytes to write from the passed I2C address.
 *   to_write_len (size_t)      - Number of bytes to write.
 *   to_read (const uint8_t*)   - Array of bytes to read from the passed I2C address.
 *   to_read_len (size_t)       - Number of bytes to read. It must be equal or
 *                                greater than the array length.
 *   read_bytes(size_t*)        - Pointer to save the real number of read bytes.
 *
 * Returns:
 *   ssize_t - The number of bytes written (it can be 0), or -1 if an error happens.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled)  : Some of the arguments given is invalid (bad
 *                              i2c_interface, invalid address, bad write
 * 				or read array...
 *     - ESP32 specific       :
 *       - EIO                : i2cWriteReadNonStop function reported some
 *                              error.
 */
ssize_t i2c_write_then_read(i2c_interface_t* i2c,
			    plc_i2c_addr_t addr,
			    const uint8_t* to_write,
			    size_t to_write_len,
			    uint8_t* to_read,
			    size_t to_read_len,
			    size_t* read_bytes);

/**
 * i2c_write8_8b
 *
 * Write a byte to a register of the given I2C address.
 *
 * Parameters:
 *   i2c (i2c_interface_t*)     - I2C interface to write to.
 *   addr (plc_i2c_addr_t)      - I2C address to write to.
 *   reg (uint8_t)              - Register address.
 *   to_write (uint8_t)         - Value to write to the register
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
int i2c_write8_8b(i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  uint8_t reg,
		  uint8_t to_write);

/**
 * i2c_write8_16b
 *
 * Write a byte to a register of the given I2C address.
 *
 * Parameters:
 *   i2c (i2c_interface_t*)     - I2C interface to write to.
 *   addr (plc_i2c_addr_t)      - I2C address to write to.
 *   reg (uint8_t)              - Register address.
 *   to_write (uint16_t)        - Value to write to the register
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
int i2c_write8_16b(i2c_interface_t* i2c,
		   plc_i2c_addr_t addr,
		   uint8_t reg,
		   uint16_t to_write);

/**
 * i2c_read8_8b
 *
 * Read a one-byte register from the given I2C address.
 *
 * Parameters:
 *   i2c (i2c_interface_t*)     - I2C interface to read to.
 *   addr (plc_i2c_addr_t)      - I2C address to read to.
 *   reg (uint8_t)              - Register address.
 *   to_read (uint8_t*)         - Pointer to save the register value.
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
int i2c_read8_8b(i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t reg,
		 uint8_t* to_read);

/**
 * i2c_read8_16b
 *
 * Read a one-byte register from the given I2C address.
 *
 * Parameters:
 *   i2c (i2c_interface_t*)     - I2C interface to read to.
 *   addr (plc_i2c_addr_t)      - I2C address to read to.
 *   reg (uint8_t)              - Register address.
 *   to_read (uint16_t*)        - Pointer to save the register value.
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
int i2c_read8_16b(i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  uint8_t reg,
		  uint16_t* to_read);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERALS_I2C_H_
