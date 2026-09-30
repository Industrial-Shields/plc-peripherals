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

#include "plc-mutex.h"
#include "plc-peripherals-i2c.h"
#include "plc-peripherals-platform.h"
#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define MCP23008_MAX_GPIOS     8
#define MCP23017_MAX_GPIOS     16
#define MCP230XX_HIGH          1
#define MCP230XX_LOW           0
// clang-format on

/*
 * Sized for the private handle in peripheral-mcp230xx.c: its mutex, the I2C
 * address, the bus number, the chip type and a flag.
 */
#define MCP230XX_INTERNAL_ALIGN PLC_MUTEX_ALIGN
#define MCP230XX_INTERNAL_SIZE                                                \
	PLC_PERIPHERAL_INTERNAL_PAD(PLC_MUTEX_SIZE + sizeof(plc_i2c_addr_t) + \
					    2 * sizeof(uint8_t) +             \
					    sizeof(bool),                     \
				    MCP230XX_INTERNAL_ALIGN)

/*
 * Storage for one MCP230XX handle. Useful to statically allocate, without
 * malloc.
 *
 * A region handed to mcp230xx_static_init must be at least MCP230XX_SIZE bytes
 * and at least MCP230XX_ALIGN aligned.
 *
 * WARNING: Never copy a live mcp230xx_t. Assigning a mcp230xx_t, embedding one in a
 * struct that is assigned or passed by value, memcpying it, or reallocating an
 * array of them all do it. The backend does not necessarily support it!
 */
#define MCP230XX_SIZE MCP230XX_INTERNAL_SIZE
#define MCP230XX_ALIGN MCP230XX_INTERNAL_ALIGN

typedef struct {
	PLC_PERIPHERAL_INTERNAL_ALIGNAS(plc_mutex_t)
	unsigned char opaque[MCP230XX_SIZE];
} mcp230xx_t;

typedef enum {
	MCP230XX_008, // MCP23008
	MCP230XX_017, // MCP23017
} MCP230XX_TYPE;

typedef enum {
	MCP230XX_OPEN_DRAIN_INT = 1, // Requires MCP230XX_INT_POLARITY_NONE
	MCP230XX_ACTIVE_DRIVER_INT = 0,
} MCP230XX_INT_TYPE;

typedef enum {
	MCP230XX_INT_POLARITY_NONE = 2,
	MCP230XX_INT_ACTIVE_HIGH = 1,
	MCP230XX_INT_ACTIVE_LOW = 0,
} MCP230XX_INT_POLARITY;

typedef enum {
	MCP230XX_MIRRORED_INT = 1,
	MCP230XX_NO_MIRRORED_INT = 0,
} MCP230XX_MIRROR_INT;

typedef enum {
	MCP230XX_NO_PULLUP = 0,
	MCP230XX_PULLUP = 1,
} MCP230XX_INPUT_CONFIG;

/**
 * mcp230xx_config_t
 *
 * The configuration mcp230xx_init applies to an MCP230XX. Every field must be
 * set.
 *
 *   type (MCP230XX_TYPE)            - The chip's specific type.
 *   disable_slew_rate (bool)        - Set it to true if you want to disable the
 *                                     slew rate control of the SDA output.
 *   int_type (MCP230XX_INT_TYPE)    - Configure the interrupt pin of the
 *                                     MCP230XX either as an open-drain output,
 *                                     or an active driver output.
 *   int_pol (MCP230XX_INT_POLARITY) - The polarity of the interrupt output pin.
 *                                     With an active driver output, it must be
 *                                     MCP230XX_INT_ACTIVE_HIGH or
 *                                     MCP230XX_INT_ACTIVE_LOW. With an
 *                                     open-drain output, it must be
 *                                     MCP230XX_INT_POLARITY_NONE.
 *   mirror (MCP230XX_MIRROR_INT)    - Configure the behaviour of the two
 *                                     interrupt pins. If mirrored, INTA and
 *                                     INTB pins will be logically OR'ed
 *                                     together (both will activate when an
 *                                     interrupt occurs). Only useful on
 *                                     MCP23017. It must be
 *                                     MCP230XX_NO_MIRRORED_INT in other chips.
 */
typedef struct {
	MCP230XX_TYPE type;
	bool disable_slew_rate;
	MCP230XX_INT_TYPE int_type;
	MCP230XX_INT_POLARITY int_pol;
	MCP230XX_MIRROR_INT mirror;
} mcp230xx_config_t;

/**
 * mcp230xx_init
 *
 * Allocate and initialize an MCP230XX peripheral with address "addr". This
 * function currently supports MCP23008 and MCP23017. You must only have one
 * handle per device.
 *
 * Use mcp230xx_static_init instead to initialize a handle in storage you
 * provide, without malloc.
 *
 * WARNING: Tear down with mcp230xx_deinit, never with mcp230xx_static_deinit.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)   - The I2C interface to access the
 *                                    peripheral.
 *   addr (plc_i2c_addr_t)          - The I2C address of the peripheral.
 *   restart (bool)                 - true if you want to reset the peripheral
 *                                    (that is, set the registers to their
 *                                    default values) before applying cfg. If
 *                                    false, pin directions, pull-ups, input
 *                                    polarity, interrupt settings and output
 *                                    latches stay as they are on the
 *                                    MCP230XX.
 *   cfg (const mcp230xx_config_t*) - The configuration to apply. It is only
 *                                    read during the call.
 *
 * Returns:
 *   mcp230xx_t* - Pointer to the initialized peripheral struct on success.
 *                 NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed cfg or i2c is NULL.
 *     - EINVAL   : type is not an MCP230XX_TYPE value, int_type is not an
 *                  MCP230XX_INT_TYPE value, int_pol doesn't match int_type
 *                  (see mcp230xx_config_t), or mirror is not an
 *                  MCP230XX_MIRROR_INT value, or is MCP230XX_MIRRORED_INT on
 *                  a chip other than the MCP23017.
 *     - ENOMEM   : Out of memory during allocation.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 */
mcp230xx_t* mcp230xx_init(const i2c_interface_t* i2c,
			  plc_i2c_addr_t addr,
			  bool restart,
			  const mcp230xx_config_t* cfg);

/**
 * mcp230xx_deinit
 *
 * Deinitialize an MCP230XX peripheral. This function currently supports
 * MCP23008 and MCP23017.
 *
 * WARNINGS:
 *   - Never use this on an mcp230xx_static_init handle.
 *   - If the handle is protected, its mutex is destroyed first. If the reset
 *     write fails after that, the handle is not freed, but it is no longer
 *     protected.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the MCP230XX is on.
 *   mcp (mcp230xx_t*)            - The MCP230XX to interact with.
 *   restart (bool)               - true if you want to reset the peripheral
 *                                  (that is, set the registers to its default
 *                                  values) before releasing it.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed mcp230xx_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the MCP230XX was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_static_destroy reports while protected
 *                  (see plc-mutex.h). The handle is then left as it was.
 */
int mcp230xx_deinit(const i2c_interface_t* i2c, mcp230xx_t* mcp, bool restart);

/**
 * mcp230xx_static_init
 *
 * Initialize an MCP230XX peripheral with address "addr" in the storage passed
 * by argument, like mcp230xx_init but without malloc. You must ensure that this
 * region is at least MCP230XX_SIZE bytes, and at least MCP230XX_ALIGN aligned.
 * This function currently supports MCP23008 and MCP23017. You must only have
 * one handle per device.
 *
 * The storage must not already hold a live handle.
 *
 * WARNING: Tear down with mcp230xx_static_deinit, never with mcp230xx_deinit.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)   - The I2C interface to access the
 *                                    peripheral.
 *   mcp (mcp230xx_t*)              - Storage to make a handle out of.
 *   addr (plc_i2c_addr_t)          - The I2C address of the peripheral.
 *   restart (bool)                 - true if you want to reset the peripheral
 *                                    (that is, set the registers to their
 *                                    default values) before applying cfg. If
 *                                    false, pin directions, pull-ups, input
 *                                    polarity, interrupt settings and output
 *                                    latches stay as they are on the
 *                                    MCP230XX.
 *   cfg (const mcp230xx_config_t*) - The configuration to apply. It is only
 *                                    read during the call.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed storage is NULL or not MCP230XX_ALIGN aligned, or
 *                  cfg or i2c is NULL.
 *     - EINVAL   : type is not an MCP230XX_TYPE value, int_type is not an
 *                  MCP230XX_INT_TYPE value, int_pol doesn't match int_type
 *                  (see mcp230xx_config_t), or mirror is not an
 *                  MCP230XX_MIRROR_INT value, or is MCP230XX_MIRRORED_INT on
 *                  a chip other than the MCP23017.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 */
int mcp230xx_static_init(const i2c_interface_t* i2c,
			 mcp230xx_t* mcp,
			 plc_i2c_addr_t addr,
			 bool restart,
			 const mcp230xx_config_t* cfg);

/**
 * mcp230xx_static_deinit
 *
 * Deinitialize an MCP230XX peripheral made by mcp230xx_static_init. The storage
 * is never freed. This function currently supports MCP23008 and MCP23017.
 *
 * WARNINGS:
 *   - Never use this on an mcp230xx_init handle.
 *   - If the handle is protected, its mutex is destroyed first. If the reset
 *     write fails after that, the handle is still initialized, but it is no
 *     longer protected.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the MCP230XX is on.
 *   mcp (mcp230xx_t*)            - The MCP230XX to interact with.
 *   restart (bool)               - true if you want to reset the peripheral
 *                                  (that is, set the registers to its default
 *                                  values) before releasing it.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed mcp230xx_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the MCP230XX was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_static_destroy reports while protected
 *                  (see plc-mutex.h). The handle is then left as it was.
 */
int mcp230xx_static_deinit(const i2c_interface_t* i2c,
			   mcp230xx_t* mcp,
			   bool restart);

/**
 * mcp230xx_protect
 *
 * Protect the MCP230XX with a mutex embedded in its handle. Every GPIO call on
 * the handle then holds it, waiting up to its timeout_ms for it.
 *
 * WARNING: Never call it while another thread or process uses the handle.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the MCP230XX is on.
 *   mcp (mcp230xx_t*)            - The MCP230XX to protect.
 *   scope (plc_mutex_scope_t)    - Who the mutex has to exclude. Use
 *                                  PLC_MUTEX_SCOPE_SHARED if the handle is in
 *                                  memory shared with other processes (see
 *                                  plc_mutex_scope_t).
 * Returns:
 *   int - 0 if successful, 1 if already protected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed mcp230xx_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the MCP230XX was initialized on.
 *     - (others) : Whatever plc_mutex_static_create reports (see
 *                  plc-mutex.h).
 */
int mcp230xx_protect(const i2c_interface_t* i2c,
		     mcp230xx_t* mcp,
		     plc_mutex_scope_t scope);

/**
 * mcp230xx_unprotect
 *
 * Destroy the mutex embedded in the MCP230XX handle.
 *
 * WARNING: Never call it while another thread or process uses the handle.
 *
 * Parameters:
 *   mcp (mcp230xx_t*) - The MCP230XX to unprotect.
 * Returns:
 *   int - 0 if successful, 1 if already unprotected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed mcp230xx_t is NULL.
 *     - (others) : Whatever plc_mutex_static_destroy reports (see
 *                  plc-mutex.h).
 */
int mcp230xx_unprotect(mcp230xx_t* mcp);

/**
 * mcp230xx_set_input
 *
 * Set a GPIO of the MCP230XX "mcp" as an input.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)   - The I2C interface the MCP230XX is on.
 *   mcp (mcp230xx_t*)              - The MCP230XX to interact with.
 *   index (uint8_t)                - The GPIO you want to set as input.
 *   config (MCP230XX_INPUT_CONFIG) - Used to enable/disable the pull-up of the
 *                                    input.
 *   timeout_ms (uint32_t)          - The maximum time to wait to configure
 *                                    the pin. Only applicable when the
 *                                    MCP230XX is protected.
 * Returns:
 *   int - 0 if successful, 1 if it was already an input and the pull-up was
 *         correctly configured, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed mcp230xx_t or i2c is NULL.
 *     - EINVAL   : index is invalid for the chip, config is not an
 *                  MCP230XX_INPUT_CONFIG value, or i2c is not on the bus the
 *                  MCP230XX was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int mcp230xx_set_input(const i2c_interface_t* i2c,
		       mcp230xx_t* mcp,
		       uint8_t index,
		       MCP230XX_INPUT_CONFIG config,
		       uint32_t timeout_ms);

/**
 * mcp230xx_read_gpio
 *
 * Read MCP230XX_HIGH/LOW from a GPIO of MCP230XX "mcp". It should be declared as
 * input before calling this function.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the MCP230XX is on.
 *   mcp (mcp230xx_t*)            - The MCP230XX to interact with.
 *   index (uint8_t)              - The GPIO you want to read from.
 *   return_value (uint8_t*)      - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait to read. Only
 *                                  applicable when the MCP230XX is protected.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed mcp230xx_t, return_value or i2c is NULL.
 *     - EINVAL   : index is invalid for the chip, or i2c is not on the bus the
 *                  MCP230XX was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int mcp230xx_read_gpio(const i2c_interface_t* i2c,
		       mcp230xx_t* mcp,
		       uint8_t index,
		       uint8_t* return_value,
		       uint32_t timeout_ms);

/**
 * mcp230xx_set_output
 *
 * Set a GPIO of the MCP230XX "mcp" as an output.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the MCP230XX is on.
 *   mcp (mcp230xx_t*)            - The MCP230XX to interact with.
 *   index (uint8_t)              - The GPIO you want to set as output.
 *   timeout_ms (uint32_t)        - The maximum time to wait to configure the
 *                                  pin. Only applicable when the MCP230XX is
 *                                  protected.
 * Returns:
 *   int - 0 if successful, 1 if it was already an output, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed mcp230xx_t or i2c is NULL.
 *     - EINVAL   : index is invalid for the chip, or i2c is not on the bus the
 *                  MCP230XX was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int mcp230xx_set_output(const i2c_interface_t* i2c,
			mcp230xx_t* mcp,
			uint8_t index,
			uint32_t timeout_ms);

/**
 * mcp230xx_write_gpio
 *
 * Set MCP230XX_HIGH/LOW to a GPIO of MCP230XX "mcp". It should be declared as
 * output before calling this function.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the MCP230XX is on.
 *   mcp (mcp230xx_t*)            - The MCP230XX to interact with.
 *   index (uint8_t)              - The GPIO you want to modify.
 *   to_write (uint8_t)           - The value to write. MCP230XX_LOW sets the
 *                                  output low, any other value sets it high.
 *   timeout_ms (uint32_t)        - The maximum time to wait to write. Only
 *                                  applicable when the MCP230XX is protected.
 * Returns:
 *   int - 0 if successful, 1 if it was already set/cleared, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed mcp230xx_t or i2c is NULL.
 *     - EINVAL   : index is invalid for the chip, or i2c is not on the bus the
 *                  MCP230XX was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int mcp230xx_write_gpio(const i2c_interface_t* i2c,
			mcp230xx_t* mcp,
			uint8_t index,
			uint8_t to_write,
			uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_MCP230XX_I2C_H_
