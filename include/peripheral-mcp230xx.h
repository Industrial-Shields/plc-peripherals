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

struct _mcp230xx_t;
typedef struct _mcp230xx_t mcp230xx_t;

typedef enum {
	MCP230XX_008, // MCP23008
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
 * supports MCP23008. You must only have one interface per device.
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
 * only.
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

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_MCP230XX_I2C_H_
