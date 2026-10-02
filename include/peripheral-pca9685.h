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

#ifndef PLC_PERIPHERAL_PCA9685_I2C_H_
#define PLC_PERIPHERAL_PCA9685_I2C_H_

#include "plc-mutex.h"
#include "plc-peripherals-i2c.h"
#include "plc-peripherals-platform.h"
#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define PCA9685_NUM_OUTPUTS    16
#define PCA9685_OUTPUT_OFF     0
#define PCA9685_OUTPUT_ON      4096
#define PCA9685_MIN_FREQ_HZ    24
#define PCA9685_MAX_FREQ_HZ    1526
#define PCA9685_MIN_PRESCALER  3
#define PCA9685_MAX_PRESCALER  255
// clang-format on

/*
 * Sized for the private handle in peripheral-pca9685.c: its mutex, the I2C
 * address, the bus number and a flag.
 */
#define PCA9685_INTERNAL_ALIGN PLC_MUTEX_ALIGN
#define PCA9685_INTERNAL_SIZE                                                 \
	PLC_PERIPHERAL_INTERNAL_PAD(PLC_MUTEX_SIZE + sizeof(plc_i2c_addr_t) + \
					    sizeof(uint8_t) + sizeof(bool),   \
				    PCA9685_INTERNAL_ALIGN)

/*
 * Storage for one PCA9685 handle. Useful to statically allocate, without
 * malloc.
 *
 * A region handed to pca9685_static_init must be at least PCA9685_SIZE bytes
 * and at least PCA9685_ALIGN aligned.
 *
 * WARNING: Never copy a live pca9685_t. Assigning a pca9685_t, embedding one in
 * a struct that is assigned or passed by value, memcpying it, or reallocating
 * an array of them all do it. The backend does not necessarily support it!
 */
#define PCA9685_SIZE PCA9685_INTERNAL_SIZE
#define PCA9685_ALIGN PCA9685_INTERNAL_ALIGN

typedef struct {
	PLC_PERIPHERAL_INTERNAL_ALIGNAS(plc_mutex_t)
	unsigned char opaque[PCA9685_SIZE];
} pca9685_t;

typedef enum {
	PCA9685_OPEN_DRAIN = 0,
	PCA9685_TOTEM_POLE = 1,
} PCA9685_OUTPUT_DRIVE;

typedef enum {
	PCA9685_NOT_INVERTED = 0,
	PCA9685_INVERTED = 1,
} PCA9685_OUTPUT_LOGIC;

/**
 * pca9685_config_t
 *
 * The configuration pca9685_init applies to a PCA9685. Every field must be
 * set.
 *
 *   drive (PCA9685_OUTPUT_DRIVE) - Whether the 16 outputs are open-drain or
 *                                  totem pole.
 *   logic (PCA9685_OUTPUT_LOGIC) - Whether the 16 outputs are inverted. An
 *                                  inverted output is low while it is on.
 */
typedef struct {
	PCA9685_OUTPUT_DRIVE drive;
	PCA9685_OUTPUT_LOGIC logic;
} pca9685_config_t;

/**
 * pca9685_init
 *
 * Allocate and initialize a PCA9685 peripheral with address "addr". You must
 * only have one handle per device.
 *
 * Use pca9685_static_init instead to initialize a handle in storage you
 * provide, without malloc.
 *
 * It leaves the chip awake, with register auto-increment enabled. It also
 * leaves MODE2.OCH and MODE2.OUTNE cleared, whatever they were. This means
 * that outputs change on the I2C STOP, and are LOW while the OE pin is high
 * (not high-impedance).
 *
 * WARNINGS:
 *   - Tear down with pca9685_deinit, never with pca9685_static_deinit.
 *   - The driver never switches the chip to its external clock
 *     (MODE1.EXTCLK). Only a power cycle or a software reset clears it, and
 *     neither restart nor anything else in this driver can do either.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - The I2C interface to access the
 *                                   peripheral.
 *   addr (plc_i2c_addr_t)         - The I2C address of the peripheral.
 *   restart (bool)                - true if you want to reset the peripheral
 *                                   before applying cfg. If false, the outputs
 *                                   and the PWM frequency stay as they are on
 *                                   the PCA9685.
 *   cfg (const pca9685_config_t*) - The configuration to apply. It is only
 *                                   read during the call.
 *
 * Returns:
 *   pca9685_t* - Pointer to the initialized peripheral struct on success.
 *                NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed cfg or i2c is NULL.
 *     - EINVAL   : drive is not a PCA9685_OUTPUT_DRIVE value, or logic is not
 *                  a PCA9685_OUTPUT_LOGIC value.
 *     - EIO      : With restart, the chip did not take every byte written.
 *     - ENOMEM   : Out of memory during allocation.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_delay_us (see
 *                  plc-delay.h).
 */
pca9685_t* pca9685_init(const i2c_interface_t* i2c,
			plc_i2c_addr_t addr,
			bool restart,
			const pca9685_config_t* cfg);

/**
 * pca9685_deinit
 *
 * Deinitialize a PCA9685 peripheral.
 *
 * WARNINGS:
 *   - Never use this on a pca9685_static_init handle.
 *   - If the handle is protected, its mutex is destroyed first. If the shutdown
 *     fails after that, the handle is not freed, but it is no longer
 *     protected.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to interact with.
 *   shutdown (bool)              - true if you want to turn every output
 *                                  fully off and put the PCA9685 to sleep
 *                                  before releasing it. If false, the
 *                                  outputs stay as they are.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the PCA9685 was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_mutex_static_destroy
 *                  while protected (see plc-mutex.h). If the mutex can't be
 *                  destroyed, the handle is left as it was.
 */
int pca9685_deinit(const i2c_interface_t* i2c, pca9685_t* pca, bool shutdown);

/**
 * pca9685_static_init
 *
 * Initialize a PCA9685 peripheral with address "addr" in the storage passed by
 * argument, like pca9685_init but without malloc. You must ensure that this
 * region is at least PCA9685_SIZE bytes, and at least PCA9685_ALIGN aligned.
 * You must only have one handle per device.
 *
 * The storage must not already hold a live handle.
 *
 * It leaves the chip awake, with register auto-increment enabled. It also
 * leaves MODE2.OCH and MODE2.OUTNE cleared, whatever they were. This means
 * that outputs change on the I2C STOP, and are LOW while the OE pin is high
 * (not high-impedance).
 *
 * WARNINGS:
 *   - Tear down with pca9685_static_deinit, never with pca9685_deinit.
 *   - The driver never switches the chip to its external clock
 *     (MODE1.EXTCLK). Only a power cycle or a software reset clears it, and
 *     neither restart nor anything else in this driver can do either.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - The I2C interface to access the
 *                                   peripheral.
 *   pca (pca9685_t*)              - Storage to make a handle out of.
 *   addr (plc_i2c_addr_t)         - The I2C address of the peripheral.
 *   restart (bool)                - true if you want to reset the peripheral
 *                                   before applying cfg. If false, the outputs
 *                                   and the PWM frequency stay as they are on
 *                                   the PCA9685.
 *   cfg (const pca9685_config_t*) - The configuration to apply. It is only
 *                                   read during the call.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed storage is NULL or not PCA9685_ALIGN aligned, or
 *                  cfg or i2c is NULL.
 *     - EINVAL   : drive is not a PCA9685_OUTPUT_DRIVE value, or logic is not
 *                  a PCA9685_OUTPUT_LOGIC value.
 *     - EIO      : With restart, the chip did not take every byte written.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_delay_us (see
 *                  plc-delay.h).
 */
int pca9685_static_init(const i2c_interface_t* i2c,
			pca9685_t* pca,
			plc_i2c_addr_t addr,
			bool restart,
			const pca9685_config_t* cfg);

/**
 * pca9685_static_deinit
 *
 * Deinitialize a PCA9685 peripheral made by pca9685_static_init. The storage
 * is never freed.
 *
 * WARNINGS:
 *   - Never use this on a pca9685_init handle.
 *   - If the handle is protected, its mutex is destroyed first. If the shutdown
 *     fails after that, the handle is still initialized, but it is no
 *     longer protected.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to interact with.
 *   shutdown (bool)              - true if you want to turn every output
 *                                  fully off and put the PCA9685 to sleep
 *                                  before releasing it. If false, the
 *                                  outputs stay as they are.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the PCA9685 was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_mutex_static_destroy
 *                  while protected (see plc-mutex.h). If the mutex can't be
 *                  destroyed, the handle is left as it was.
 */
int pca9685_static_deinit(const i2c_interface_t* i2c,
			  pca9685_t* pca,
			  bool shutdown);

/**
 * pca9685_protect
 *
 * Protect the PCA9685 with a mutex embedded in its handle. Every output and
 * frequency call on the handle then holds it, waiting up to its timeout_ms for
 * it.
 *
 * WARNING: Never call it while another thread or process uses the handle.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to protect.
 *   scope (plc_mutex_scope_t)    - Who the mutex has to exclude. Use
 *                                  PLC_MUTEX_SCOPE_SHARED if the handle is in
 *                                  memory shared with other processes (see
 *                                  plc_mutex_scope_t).
 * Returns:
 *   int - 0 if successful, 1 if already protected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the PCA9685 was initialized on.
 *     - (others) : Whatever plc_mutex_static_create reports (see
 *                  plc-mutex.h).
 */
int pca9685_protect(const i2c_interface_t* i2c,
		    pca9685_t* pca,
		    plc_mutex_scope_t scope);

/**
 * pca9685_unprotect
 *
 * Destroy the mutex embedded in the PCA9685 handle.
 *
 * WARNING: Never call it while another thread or process uses the handle.
 *
 * Parameters:
 *   pca (pca9685_t*) - The PCA9685 to unprotect.
 * Returns:
 *   int - 0 if successful, 1 if already unprotected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t is NULL.
 *     - (others) : Whatever plc_mutex_static_destroy reports (see
 *                  plc-mutex.h).
 */
int pca9685_unprotect(pca9685_t* pca);

/**
 * pca9685_set_frequency
 *
 * Set the PWM frequency of every output of the PCA9685 "pca". The chip can only
 * reach some of the frequencies (see datasheet Rev. 4, Equation 1 for the
 * formula).
 *
 * To change the frequency, the chip goes to sleep for a moment, which stops the
 * outputs for at least 500 us. They automatically resume with the values they
 * had.
 *
 * WARNING: It assumes the chip runs on its internal 25 MHz oscillator, as this
 * driver never switches it to the external clock. If something else did
 * (MODE1.EXTCLK set), the frequency set will be wrong.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to interact with.
 *   freq_hz (uint16_t)           - The PWM frequency, in Hz. From
 *                                  PCA9685_MIN_FREQ_HZ to PCA9685_MAX_FREQ_HZ.
 *   timeout_ms (uint32_t)        - The maximum time to wait to set it. Only
 *                                  applicable when the PCA9685 is protected.
 * Returns:
 *   int - 0 if successful, 1 if the chip already had that frequency (so the
 *         outputs are not stopped), otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t or i2c is NULL.
 *     - EINVAL   : freq_hz is out of range, or i2c is not on the bus the
 *                  PCA9685 was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), plc_delay_us (see
 *                  plc-delay.h), or plc_mutex_acquire while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int pca9685_set_frequency(const i2c_interface_t* i2c,
			  pca9685_t* pca,
			  uint16_t freq_hz,
			  uint32_t timeout_ms);

/**
 * pca9685_set_prescaler
 *
 * Set the PWM frequency of every output of the PCA9685 "pca" by writing the
 * raw PRE_SCALE register. The frequency is then
 * oscillator / (4096 * (prescale + 1)) (datasheet Rev. 4, section 7.3.5).
 * Use pca9685_set_frequency instead to give it in Hz.
 *
 * To change the prescale, the chip goes to sleep for a moment, which stops the
 * outputs for at least 500 us. They automatically resume with the values they
 * had.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to interact with.
 *   prescale (uint8_t)           - The PRE_SCALE value. From
 *                                  PCA9685_MIN_PRESCALER, the lowest the chip
 *                                  accepts, to PCA9685_MAX_PRESCALER.
 *   timeout_ms (uint32_t)        - The maximum time to wait to set it. Only
 *                                  applicable when the PCA9685 is protected.
 * Returns:
 *   int - 0 if successful, 1 if the chip already had that prescale (so the
 *         outputs are not stopped), otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t or i2c is NULL.
 *     - EINVAL   : prescale is below PCA9685_MIN_PRESCALER, or i2c is not on
 *                  the bus the PCA9685 was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), plc_delay_us (see
 *                  plc-delay.h), or plc_mutex_acquire while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int pca9685_set_prescaler(const i2c_interface_t* i2c,
			  pca9685_t* pca,
			  uint8_t prescale,
			  uint32_t timeout_ms);

/**
 * pca9685_set_output
 *
 * Set the duty cycle of an output of the PCA9685 "pca", in 1/4096 steps of the
 * PWM period.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to interact with.
 *   index (uint8_t)              - The output you want to set.
 *   value (uint16_t)             - The duty cycle. PCA9685_OUTPUT_OFF (0)
 *                                  turns it fully off, PCA9685_OUTPUT_ON
 *                                  (4096) fully on, and anything in between
 *                                  keeps it on for value/4096 of each period.
 *   timeout_ms (uint32_t)        - The maximum time to wait to write. Only
 *                                  applicable when the PCA9685 is protected.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t or i2c is NULL.
 *     - EINVAL   : index is not below PCA9685_NUM_OUTPUTS, value is above
 *                  PCA9685_OUTPUT_ON, or i2c is not on the bus the PCA9685
 *                  was initialized on.
 *     - EIO      : The chip did not take every byte written.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_mutex_acquire while
 *                  protected (see plc-mutex.h). EOWNERDEAD is never
 *                  reported, because the driver recovers from it.
 */
int pca9685_set_output(const i2c_interface_t* i2c,
		       pca9685_t* pca,
		       uint8_t index,
		       uint16_t value,
		       uint32_t timeout_ms);

/**
 * pca9685_set_all_outputs
 *
 * Set the duty cycle of every output of the PCA9685 "pca" in a single I2C
 * write, so they all change at once. Each value is as in pca9685_set_output.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to interact with.
 *   values (const uint16_t*)     - PCA9685_NUM_OUTPUTS duty cycles, values[0]
 *                                  for output 0. Nothing is written unless
 *                                  all are valid.
 *   timeout_ms (uint32_t)        - The maximum time to wait to write. Only
 *                                  applicable when the PCA9685 is protected.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t, values or i2c is NULL.
 *     - EINVAL   : A value is above PCA9685_OUTPUT_ON, or i2c is not on the
 *                  bus the PCA9685 was initialized on.
 *     - EIO      : The chip did not take every byte written.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_mutex_acquire while
 *                  protected (see plc-mutex.h). EOWNERDEAD is never
 *                  reported, because the driver recovers from it.
 */
int pca9685_set_all_outputs(const i2c_interface_t* i2c,
			    pca9685_t* pca,
			    const uint16_t values[PCA9685_NUM_OUTPUTS],
			    uint32_t timeout_ms);

/**
 * pca9685_get_output
 *
 * Read the duty cycle of an output of the PCA9685 "pca" from the chip, in the
 * same scale as pca9685_set_output.
 *
 * It assumes the output has no phase offset (LEDn_ON count at 0), as this
 * driver never uses phasing. If something else gave it one, the value read is
 * the count at which the output turns off, not its duty cycle.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to interact with.
 *   index (uint8_t)              - The output you want to read.
 *   value (uint16_t*)            - Where the duty cycle is stored, from
 *                                  PCA9685_OUTPUT_OFF to PCA9685_OUTPUT_ON.
 *   timeout_ms (uint32_t)        - The maximum time to wait to read. Only
 *                                  applicable when the PCA9685 is protected.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t, value or i2c is NULL.
 *     - EINVAL   : index is not below PCA9685_NUM_OUTPUTS, or i2c is not on
 *                  the bus the PCA9685 was initialized on.
 *     - EIO      : The chip did not take the register address, or did not
 *                  return exactly 4 bytes.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_mutex_acquire while
 *                  protected (see plc-mutex.h). EOWNERDEAD is never
 *                  reported, because the driver recovers from it.
 */
int pca9685_get_output(const i2c_interface_t* i2c,
		       pca9685_t* pca,
		       uint8_t index,
		       uint16_t* value,
		       uint32_t timeout_ms);

/**
 * pca9685_get_all_outputs
 *
 * Read the duty cycle of every output of the PCA9685 "pca" from the chip in a
 * single I2C transfer. Each value is as in pca9685_get_output, and makes the
 * same assumption about phase offsets.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the PCA9685 is on.
 *   pca (pca9685_t*)             - The PCA9685 to interact with.
 *   values (uint16_t*)           - Where the PCA9685_NUM_OUTPUTS duty cycles
 *                                  are stored, values[0] for output 0. It is
 *                                  left as it was if the read fails.
 *   timeout_ms (uint32_t)        - The maximum time to wait to read. Only
 *                                  applicable when the PCA9685 is protected.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed pca9685_t, values or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the PCA9685 was initialized on.
 *     - EIO      : The chip did not take the register address, or did not
 *                  return exactly 64 bytes.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_mutex_acquire while
 *                  protected (see plc-mutex.h). EOWNERDEAD is never
 *                  reported, because the driver recovers from it.
 */
int pca9685_get_all_outputs(const i2c_interface_t* i2c,
			    pca9685_t* pca,
			    uint16_t values[PCA9685_NUM_OUTPUTS],
			    uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_PCA9685_I2C_H_
