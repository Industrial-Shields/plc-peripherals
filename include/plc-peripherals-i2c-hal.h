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

#ifndef PLC_PERIPHERALS_I2C_HAL_H_
#define PLC_PERIPHERALS_I2C_HAL_H_

/*
 * The platform-specific half of the I2C API: one implementation per target
 * environment (see plc-peripherals-i2c-linux.c, plc-peripherals-i2c-arduino-esp32.c).
 * Everything portable is built on top of these functions and lives in
 * plc-peripherals-i2c.h, which includes this header. Include that one instead
 * unless you are implementing or replacing the platform layer.
 */
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>

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
 *     - EINVAL        : The bus number does not exist.
 *     - ENOMEM        : Out of memory during allocation.
 *     - ESP32 specific:
 *       - EIO         : i2cInit function reported some error.
 *     - Linux specific:
 *       - ENOTSUP      : SDA/SCL were given. Linux addresses buses through
 *                        /dev/i2c-<bus> only.
 *       - EACCES       : No permission to open /dev/i2c-<bus>.
 *       - ENFILE/EMFILE: Out of file descriptors, system-wide or per-process.
 *       - ENOTTY       : /dev/i2c-<bus> is not an i2c-dev device. The adapter
 *                        capabilities are queried here, which doubles as a
 *                        check that the node is the real thing.
 *       - (others)     : Any other errno that open(2) can report for O_RDWR on
 *                        a character device, or that ioctl(I2C_FUNCS) can
 *                        report on the adapter driver.
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
 *   i2c (const i2c_interface_t*) - I2C interface to get the bus number from.
 *   bus (uint8_t*)               - Pointer to store the bus number in.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT : Passed i2c_interface or bus is NULL.
 */
int i2c_get_bus(const i2c_interface_t* i2c, uint8_t* bus);

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
 *     - EFAULT        : Passed i2c_interface is NULL.
 *     - ESP32 specific:
 *       - EIO         : i2cDeinit function reported some error.
 *     - Linux specific:
 *       - ENOTSUP     : deinit_i2c_bus was true. This function only closes
 *                       the interface, it doesn't take down the I2C bus.
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
 *   i2c (const i2c_interface_t*)  - I2C interface to write on.
 *   addr (plc_i2c_addr_t)         - I2C address to write to.
 *   to_write (const uint8_t*)     - Array of bytes to write to the passed I2C address.
 *   to_write_len (uint16_t)       - Length of the array.
 *
 * Returns:
 *   ssize_t - The number of bytes written (it can be 0), or -1 if an error happens.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT               : A NULL pointer was given.
 *     - EINVAL               : The address, or the passed i2c_interface, is
 *                              invalid.
 *     - ENOTSUP              : The address asks for 10-bit addressing, which
 *                              this platform does not support.
 *     - ETIMEDOUT            : The transfer timed out.
 *     - ESP32 specific       :
 *       - EIO                : i2cWrite function reported some error.
 *     - Linux specific       :
 *       - EAGAIN             : The transfer completed zero messages.
 *       - EBADE              : The adapter driver returned an unexpected
 *                              message count.
 *       - EINTR              : Interrupted. It is not retried internally;
 *                              the caller must retry if desired.
 *       - EIO                : The transfer failed on the adapter.
 *       - ENXIO              : No device acknowledged the address.
 *       - EREMOTEIO          : The device did not acknowledge.
 *       - (others)           : Any other errno that ioctl(I2C_RDWR) reports
 *                              on the underlying adapter driver.
 */
ssize_t i2c_write(const i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  const uint8_t* to_write,
		  uint16_t to_write_len);

/**
 * i2c_read
 *
 * It's a platform-specific function.
 *
 * Read "to_read_len" bytes from the I2C device "addr" and put them in the
 * "to_read" buffer.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - I2C interface to read from.
 *   addr (plc_i2c_addr_t)         - I2C address to read from.
 *   to_read (const uint8_t*)      - Array of bytes to read from the passed I2C address.
 *   to_read_len (uint16_t)        - Number of bytes to read. It must be equal or
 *                                   greater than the array length.
 *
 * Returns:
 *   ssize_t - The number of bytes read (it can be 0), or -1 if an error happens.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT               : A NULL pointer was given.
 *     - EINVAL               : The address, or the passed i2c_interface, is
 *                              invalid.
 *     - ENOTSUP              : The address asks for 10-bit addressing, which
 *                              this platform does not support.
 *     - ETIMEDOUT            : The transfer timed out.
 *     - ESP32 specific       :
 *       - EIO                : i2cRead function reported some error.
 *     - Linux specific       :
 *       - EAGAIN             : The transfer completed zero messages.
 *       - EBADE              : The adapter driver returned an unexpected
 *                              message count.
 *       - EINTR              : Interrupted. It is not retried internally;
 *                              the caller must retry if desired.
 *       - EIO                : The transfer failed on the adapter.
 *       - ENXIO              : No device acknowledged the address.
 *       - EREMOTEIO          : The device did not acknowledge.
 *       - (others)           : Any other errno that ioctl(I2C_RDWR) reports
 *                              on the underlying adapter driver.
 */
ssize_t i2c_read(const i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t* to_read,
		 uint16_t to_read_len);

/**
 * i2c_write_then_read
 *
 * It's a platform-specific function.
 *
 * First write "to_write_len" bytes from "to_write" to "addr". Then read
 * "to_read_bytes" bytes to "to_read" from the same transaction.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - I2C interface to read from.
 *   addr (plc_i2c_addr_t)         - I2C address to read from.
 *   to_write (const uint8_t*)     - Array of bytes to write from the passed I2C address.
 *   to_write_len (uint16_t)       - Number of bytes to write.
 *   to_read (const uint8_t*)      - Array of bytes to read from the passed I2C address.
 *   to_read_len (uint16_t)        - Number of bytes to read. It must be equal or
 *                                   greater than the array length.
 *   read_bytes (uint16_t*)        - Pointer to save the real number of read bytes.
 *
 * Returns:
 *   ssize_t - The number of bytes written (it can be 0), or -1 if an error happens.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT               : A NULL pointer was given.
 *     - EINVAL               : The address, or the passed i2c_interface, is
 *                              invalid.
 *     - ENOTSUP              : The address asks for 10-bit addressing, which
 *                              this platform does not support.
 *     - ETIMEDOUT            : The transfer timed out.
 *     - ESP32 specific       :
 *       - EIO                : i2cWriteReadNonStop function reported some
 *                              error.
 *     - Linux specific       :
 *       - EAGAIN             : The transfer completed zero messages.
 *       - EBADE              : The adapter driver returned an unexpected
 *                              message count.
 *       - EINTR              : Interrupted. It is not retried internally;
 *                              the caller must retry if desired.
 *       - EIO                : The transfer failed on the adapter.
 *       - ENXIO              : No device acknowledged the address.
 *       - EREMOTEIO          : The device did not acknowledge.
 *       - (others)           : Any other errno that ioctl(I2C_RDWR) reports
 *                              on the underlying adapter driver.
 */
ssize_t i2c_write_then_read(const i2c_interface_t* i2c,
			    plc_i2c_addr_t addr,
			    const uint8_t* to_write,
			    uint16_t to_write_len,
			    uint8_t* to_read,
			    uint16_t to_read_len,
			    uint16_t* read_bytes);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERALS_I2C_HAL_H_
