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

#ifndef PLC_PERIPHERAL_MCP230XX_I2C_H_
#define PLC_PERIPHERAL_MCP230XX_I2C_H_

#include "plc-peripherals-i2c.h"
#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define MCP23008_MAX_GPIOS     8
#define MCP23017_MAX_GPIOS     16
#define MCP230XX_HIGH          1
#define MCP230XX_LOW           0
// clang-format on

struct _mcp230xx_t;
typedef struct _mcp230xx_t mcp230xx_t;

typedef enum {
	MCP230XX_008, // MCP23008
	MCP230XX_017, // MCP23017
} MCP230XX_TYPE;

typedef enum {
	MCP230XX_OPEN_DRAIN_INT = 1, // This overrides the interrupt polarity
	MCP230XX_ACTIVE_DRIVER_INT = 0,
} MCP230XX_INT_TYPE;

typedef enum {
	MCP230XX_INT_POLARITY_NONE = 2,
	MCP230XX_INT_ACTIVE_HIGH = 1,
	MCP230XX_INT_ACTIVE_LOW = 0,
} MCP230XX_INT_POLARITY;

/**
 * mcp230xx_init
 *
 * Initialize an MCP230XX peripheral with address "addr". This function currently
 * supports MCP23008 and MCP23017. You must only have one interface per device.
 *
 * Parameters:
 *   i2c (i2c_interface_t*)          - The I2C interface to access the peripheral.
 *   addr (plc_i2c_addr_t)           - The I2C address of the peripheral.
 *   restart (bool)                  - true if you want to reset the peripheral
 *                                     (that is, set the registers to it's default
 *                                     values).
 *   type (MCP230XX_TYPE)            - The chip's specific type.
 *   disable_slew_rate (bool)        - Set it to true if you want to disable the
 *                                     slew rate control of the SDA output.
 *   int_type (MCP230XX_INT_TYPE)    - Configure the interrupt pin of the MCP230XX
 *                                    either as an open-drain output, or an active
 *                                    driver output.
 *   int_pol (MCP230XX_INT_POLARITY) - The polarity of the interrupt output pin. It's
 *                                     only valid when the pin is configured as an
 *                                     active ouptut. If you want to configure it as
 *                                     an open-drain output, you must set this
 *                                     argument to MCP230XX_INT_POLARITY_NONE.
 *
 * Returns:
 *   mcp230xx_t* - Pointer to the initialized peripheral struct on success.
 *                 NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL : Passed i2c_interface is NULL, or address, or type, or
 *                int_type, or int_pol are invalid.
 *     - ENOMEM : Out of memory during allocation.
 *     - EIO    : Communication with the MCP230XX couldn't be established.
 */
mcp230xx_t* mcp230xx_init(i2c_interface_t* i2c,
			  plc_i2c_addr_t addr,
			  bool restart,
			  MCP230XX_TYPE type,
			  bool disable_slew_rate,
			  MCP230XX_INT_TYPE int_type,
			  MCP230XX_INT_POLARITY int_pol);

/**
 * mcp230xx_deinit
 *
 * Initialize an MCP230XX peripheral. This function currently supports MCP23008
 * and MCP23017.
 *
 * Parameters:
 *   ads (mcp230xx_t)        - The MCP230XX to interact with.
 *   restart (bool)          - true if you want to leave the peripheral in it's
 *                             initial state.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL : Passed mcp230xx_t is NULL, or address is invalid.
 *     - EIO    : Communication with the MCP230XX couldn't be established.
 */
int mcp230xx_deinit(mcp230xx_t* mcp, bool restart);

/**
 * mcp230xx_protect
 *
 * Protect the MCP230XX with a mutex.
 *
 * Parameters:
 *   mcp (mcp230xx_t)         - The MCP230XX to protect.
 * Returns:
 *   int - 0 if successful, 1 if already protected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - ENOMEM : Out of memory during allocation.
 *     - EINVAL : Passed mcp230xx_t is NULL, or address is invalid.
 *     - EEXIST : The resource was already added.
 *     - EBUSY  : Hutex couldn't be taken.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int mcp230xx_protect(mcp230xx_t* mcp);

/**
 * mcp230xx_unprotect
 *
 * Remove the mutex associated with the MCP230XX.
 *
 * Parameters:
 *   mcp (mcp230xx_t)         - The MCP230XX to unprotect.
 * Returns:
 *   int - 0 if successful, 1 if already unprotected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL : Passed mcp230xx_t is NULL, or address is invalid.
 *     - ENODEV : The resource is not present.
 *     - EBUSY  : Hash mutex couldn't be taken.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int mcp230xx_unprotect(mcp230xx_t* mcp);

/**
 * mcp230xx_set_input
 *
 * Set a GPIO of the MCP230XX "mcp" as an input.
 *
 * Parameters:
 *   mcp (mcp230xx_t)        - The MCP230XX to interact with.
 *   index (uint8_t)         - The input you want to set as input.
 *   timeout_ms (uint32_t)   - The maximum time to wait for a reading. Only
 *                             applicable when the MCP230XX is protected.
 * Returns:
 *   int - 0 if successful, 1 if it was already an input, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed mcp230xx_t is NULL, or address or
 *                             index are invalid.
 *     - EIO                 : Communication with the MCP230XX couldn't
 *                             be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout
 *                             given.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int mcp230xx_set_input(mcp230xx_t* mcp, uint8_t index, uint32_t timeout_ms);

/**
 * mcp230xx_read_gpio
 *
 * Read MCP230XX_HIGH/LOW from a GPIO of MCP230XX "mcp". It shoud be declared as
 * input before calling this function.
 *
 * Parameters:
 *   mcp (mcp230xx_t)        - The MCP230XX to interact with.
 *   index (uint8_t)         - The input you want to read from.
 *   return_value (uint8_t*) - The value in which the reading will be stored.
 *   timeout_ms (uint32_t)   - The maximum time to wait to read. Only
 *                             applicable when the MCP230XX is protected.
 * Returns:
 *   int - 0 if successful, 1 if it was already set/cleared, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed mcp230xx_t is NULL, or address or
 *                             index are invalid.
 *     - EIO                 : Communication with the MCP230XX couldn't
 *                             be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout
 *                             given.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int mcp230xx_read_gpio(mcp230xx_t* mcp,
		       uint8_t index,
		       uint8_t* return_value,
		       uint32_t timeout_ms);

/**
 * mcp230xx_set_output
 *
 * Set a GPIO of the MCP230XX "mcp" as an output.
 *
 * Parameters:
 *   mcp (mcp230xx_t)        - The MCP230XX to interact with.
 *   index (uint8_t)         - The output you want to set as output.
 *   timeout_ms (uint32_t)   - The maximum time to wait for a reading. Only
 *                             applicable when the MCP230XX is protected.
 * Returns:
 *   int - 0 if successful, 1 if it was already an output, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed mcp230xx_t is NULL, or address or
 *                             index are invalid.
 *     - EIO                 : Communication with the MCP230XX couldn't
 *                             be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout
 *                             given.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int mcp230xx_set_output(mcp230xx_t* mcp, uint8_t index, uint32_t timeout_ms);

/**
 * mcp230xx_write_gpio
 *
 * Set MCP230XX_HIGH/LOW to a GPIO of MCP230XX "mcp". It shoud be declared as
 * output before calling this function.
 *
 * Parameters:
 *   mcp (mcp230xx_t)        - The MCP230XX to interact with.
 *   index (uint8_t)         - The output you want to modify.
 *   to_write (uint8_t)      - The value to write.
 *   timeout_ms (uint32_t)   - The maximum time to wait to write. Only
 *                             applicable when the MCP230XX is protected.
 * Returns:
 *   int - 0 if successful, 1 if it was already set/cleared, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed mcp230xx_t is NULL, or address or
 *                             index are invalid.
 *     - EIO                 : Communication with the MCP230XX couldn't
 *                             be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout
 *                             given.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int mcp230xx_write_gpio(mcp230xx_t* mcp,
			uint8_t index,
			uint8_t to_write,
			uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_MCP230XX_I2C_H_
