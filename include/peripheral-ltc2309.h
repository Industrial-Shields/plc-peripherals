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

#ifndef PLC_PERIPHERAL_LTC2309_I2C_H_
#define PLC_PERIPHERAL_LTC2309_I2C_H_

#include "plc-mutex.h"
#include "plc-peripherals-i2c.h"
#include "plc-peripherals-platform.h"
#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define LTC2309_NUM_INPUTS  8
// clang-format on

// Sized for the private handle in peripheral-ltc2309.c.
#define LTC2309_INTERNAL_ALIGN PLC_MUTEX_ALIGN
#define LTC2309_INTERNAL_SIZE                                                 \
	PLC_PERIPHERAL_INTERNAL_PAD(PLC_MUTEX_SIZE + sizeof(plc_i2c_addr_t) + \
					    sizeof(uint8_t) + sizeof(bool),   \
				    LTC2309_INTERNAL_ALIGN)

/*
 * Storage for one LTC2309 handle. Useful to statically allocate, without
 * malloc.
 *
 * A region handed to ltc2309_static_init must be at least LTC2309_SIZE bytes
 * and at least LTC2309_ALIGN aligned.
 *
 * WARNING: Never copy a live ltc2309_t. Assigning an ltc2309_t, embedding one in a
 * struct that is assigned or passed by value, memcpying it, or reallocating an
 * array of them all do it. The backend does not necessarily support it!
 */
#define LTC2309_SIZE LTC2309_INTERNAL_SIZE
#define LTC2309_ALIGN LTC2309_INTERNAL_ALIGN

typedef struct {
	PLC_PERIPHERAL_INTERNAL_ALIGNAS(plc_mutex_t)
	unsigned char opaque[LTC2309_SIZE];
} ltc2309_t;

typedef enum {
	// clang-format off
	LTC2309_CH0 = 0b000,
	LTC2309_CH1 = 0b100,
	LTC2309_CH2 = 0b001,
	LTC2309_CH3 = 0b101,
	LTC2309_CH4 = 0b010,
	LTC2309_CH5 = 0b110,
	LTC2309_CH6 = 0b011,
	LTC2309_CH7 = 0b111,
	// clang-format on
} LTC2309_INPUT;

/*
 * Differential pairs, per the LTC2309 datasheet's Table 1 (Channel
 * Configuration): the ODD/SIGN bit picks which of the two channels in a pair
 * is positive, and S1/S0 pick the pair. LTC2309_P<x>_N<y> means "channel x is
 * the positive input, channel y is the negative input".
 */
typedef enum {
	// clang-format off
	LTC2309_P0_N1 = 0b000,
	LTC2309_P2_N3 = 0b001,
	LTC2309_P4_N5 = 0b010,
	LTC2309_P6_N7 = 0b011,
	LTC2309_P1_N0 = 0b100,
	LTC2309_P3_N2 = 0b101,
	LTC2309_P5_N4 = 0b110,
	LTC2309_P7_N6 = 0b111,
	// clang-format on
} LTC2309_DIFF_INPUT;

/**
 * ltc2309_init
 *
 * Allocate and initialize an LTC2309 ADC with address "addr". This function
 * writes the command byte's initial state (all bits 0) to the ADC, then sleeps
 * the required tREFWAKE time (200ms) to allow the reference buffer to wake up.
 *
 * Use ltc2309_static_init instead to initialize a handle in storage you
 * provide, without malloc.
 *
 * WARNING: Tear down with ltc2309_deinit, never with ltc2309_static_deinit.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface to access the peripheral.
 *   addr (plc_i2c_addr_t)        - The I2C address of the peripheral.
 *
 * Returns:
 *   ltc2309_t* - Pointer to the initialized peripheral struct on success.
 *                NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed i2c_interface is NULL.
 *     - ENOMEM   : Out of memory during allocation.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_delay_us (see
 *                  plc-delay.h).
 */
ltc2309_t* ltc2309_init(const i2c_interface_t* i2c, plc_i2c_addr_t addr);

/**
 * ltc2309_deinit
 *
 * De-initialize an LTC2309 ADC. If shutdown is true, the LTC2309 is placed in
 * sleep mode before returning.
 *
 * WARNINGS:
 *   - Never use this on an ltc2309_static_init handle.
 *   - If the handle is protected, its mutex is destroyed first. If the
 *     shutdown write fails after that, the handle is not freed, but it is no
 *     longer protected.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the LTC2309 is on.
 *   ltc (ltc2309_t*)             - The LTC2309 to interact with.
 *   shutdown (bool)              - true if you want to place the LTC2309 in
 *                                  sleep mode.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ltc2309_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the LTC2309 was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_mutex_static_destroy
 *                  while protected (see plc-mutex.h). If the mutex can't be
 *                  destroyed, the handle is left as it was.
 */
int ltc2309_deinit(const i2c_interface_t* i2c, ltc2309_t* ltc, bool shutdown);

/**
 * ltc2309_static_init
 *
 * Initialize an LTC2309 ADC with address "addr" in the storage passed by
 * argument, like ltc2309_init but without malloc. You must ensure that this
 * region is at least LTC2309_SIZE bytes, and at least LTC2309_ALIGN aligned.
 * This function writes the command byte's initial state (all bits 0) to the
 * ADC, then sleeps the required tREFWAKE time (200ms) to allow the reference
 * buffer to wake up.
 *
 * The storage must not already hold a live handle.
 *
 * WARNING: Tear down with ltc2309_static_deinit, never with ltc2309_deinit.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface to access the peripheral.
 *   ltc (ltc2309_t*)             - Storage to make a handle out of.
 *   addr (plc_i2c_addr_t)        - The I2C address of the peripheral.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed storage is NULL or not LTC2309_ALIGN aligned, or
 *                  i2c_interface is NULL.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_delay_us (see
 *                  plc-delay.h).
 */
int ltc2309_static_init(const i2c_interface_t* i2c,
			ltc2309_t* ltc,
			plc_i2c_addr_t addr);

/**
 * ltc2309_static_deinit
 *
 * De-initialize an LTC2309 ADC made by ltc2309_static_init. The storage is
 * never freed. If shutdown is true, the LTC2309 is placed in sleep mode before
 * returning.
 *
 * WARNINGS:
 *   - Never use this on an ltc2309_init handle.
 *   - If the handle is protected, its mutex is destroyed first. If the
 *     shutdown write fails after that, the handle is still initialized, but it
 *     is no longer protected.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the LTC2309 is on.
 *   ltc (ltc2309_t*)             - The LTC2309 to interact with.
 *   shutdown (bool)              - true if you want to place the LTC2309 in
 *                                  sleep mode.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ltc2309_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the LTC2309 was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), or plc_mutex_static_destroy
 *                  while protected (see plc-mutex.h). If the mutex can't be
 *                  destroyed, the handle is left as it was.
 */
int ltc2309_static_deinit(const i2c_interface_t* i2c,
			  ltc2309_t* ltc,
			  bool shutdown);

/**
 * ltc2309_protect
 *
 * Protect the LTC2309 with a mutex embedded in its handle. Every read on the
 * handle then holds it, waiting up to its timeout_ms for it.
 *
 * WARNING: Never call it while another thread or process uses the handle.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the LTC2309 is on.
 *   ltc (ltc2309_t*)             - The LTC2309 to protect.
 *   scope (plc_mutex_scope_t)    - Who the mutex has to exclude. Use
 *                                  PLC_MUTEX_SCOPE_SHARED if the handle is in
 *                                  memory shared with other processes (see
 *                                  plc_mutex_scope_t).
 * Returns:
 *   int - 0 if successful, 1 if already protected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ltc2309_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the LTC2309 was initialized on.
 *     - (others) : Whatever plc_mutex_static_create reports (see
 *                  plc-mutex.h).
 */
int ltc2309_protect(const i2c_interface_t* i2c,
		    ltc2309_t* ltc,
		    plc_mutex_scope_t scope);

/**
 * ltc2309_unprotect
 *
 * Destroy the mutex embedded in the LTC2309 handle.
 *
 * WARNING: Never call it while another thread or process uses the handle.
 *
 * Parameters:
 *   ltc (ltc2309_t*) - The LTC2309 to unprotect.
 * Returns:
 *   int - 0 if successful, 1 if already unprotected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ltc2309_t is NULL.
 *     - (others) : Whatever plc_mutex_static_destroy reports (see
 *                  plc-mutex.h).
 */
int ltc2309_unprotect(ltc2309_t* ltc);

/**
 * ltc2309_read_single_ended_unsigned
 *
 * Perform a single-ended unipolar conversion, and store the 12-bit result as
 * straight binary (0 to 4095).
 *
 * Single-ended inputs are measured against COM, which should be tied to ground
 * for unipolar conversions.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the LTC2309 is on.
 *   ltc (ltc2309_t*)             - The LTC2309 to interact with.
 *   index (LTC2309_INPUT)        - The channel to read.
 *   read_value (uint16_t*)       - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the LTC2309 is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ltc2309_t, read_value or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the LTC2309 was initialized on, or
 *                  index is not a valid LTC2309_INPUT value.
 *     - ERANGE   : The conversion result is invalid.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), plc_delay_us (see
 *                  plc-delay.h), or plc_mutex_acquire while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ltc2309_read_single_ended_unsigned(const i2c_interface_t* i2c,
				       ltc2309_t* ltc,
				       LTC2309_INPUT index,
				       uint16_t* read_value,
				       uint32_t timeout_ms);

/**
 * ltc2309_read_single_ended_signed
 *
 * Perform a single-ended bipolar conversion, and store the 12-bit result as
 * 2's complement (-2048 to 2047), sign-extended to int16_t.
 *
 * Single-ended inputs are measured against COM, which should be tied midway
 * between GND and REFCOMP for bipolar conversions.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the LTC2309 is on.
 *   ltc (ltc2309_t*)             - The LTC2309 to interact with.
 *   index (LTC2309_INPUT)        - The channel to read.
 *   read_value (int16_t*)        - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the LTC2309 is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ltc2309_t, read_value or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the LTC2309 was initialized on, or
 *                  index is not a valid LTC2309_INPUT value.
 *     - ERANGE   : The conversion result is invalid.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), plc_delay_us (see
 *                  plc-delay.h), or plc_mutex_acquire while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ltc2309_read_single_ended_signed(const i2c_interface_t* i2c,
				     ltc2309_t* ltc,
				     LTC2309_INPUT index,
				     int16_t* read_value,
				     uint32_t timeout_ms);

/**
 * ltc2309_read_differential_unsigned
 *
 * Perform a differential unipolar conversion, and store the 12-bit result as
 * straight binary (0 to 4095).
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the LTC2309 is on.
 *   ltc (ltc2309_t*)             - The LTC2309 to interact with.
 *   index (LTC2309_DIFF_INPUT)   - Differential input pair to read.
 *                                  LTC2309_P<x>_N<y> reads channel x as
 *                                  positive and y as negative.
 *   read_value (uint16_t*)       - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the LTC2309 is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ltc2309_t, read_value or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the LTC2309 was initialized on, or
 *                  index is not a valid LTC2309_DIFF_INPUT value.
 *     - ERANGE   : The conversion result is invalid.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), plc_delay_us (see
 *                  plc-delay.h), or plc_mutex_acquire while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ltc2309_read_differential_unsigned(const i2c_interface_t* i2c,
				       ltc2309_t* ltc,
				       LTC2309_DIFF_INPUT index,
				       uint16_t* read_value,
				       uint32_t timeout_ms);

/**
 * ltc2309_read_differential_signed
 *
 * Perform a differential bipolar conversion, and store the 12-bit result as
 * 2's complement (-2048 to 2047), sign-extended to int16_t.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the LTC2309 is on.
 *   ltc (ltc2309_t*)             - The LTC2309 to interact with.
 *   index (LTC2309_DIFF_INPUT)   - Differential input pair to read.
 *                                  LTC2309_P<x>_N<y> reads channel x as
 *                                  positive and y as negative.
 *   read_value (int16_t*)        - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the LTC2309 is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ltc2309_t, read_value or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the LTC2309 was initialized on, or
 *                  index is not a valid LTC2309_DIFF_INPUT value.
 *     - ERANGE   : The conversion result is invalid.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h), plc_delay_us (see
 *                  plc-delay.h), or plc_mutex_acquire while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ltc2309_read_differential_signed(const i2c_interface_t* i2c,
				     ltc2309_t* ltc,
				     LTC2309_DIFF_INPUT index,
				     int16_t* read_value,
				     uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_LTC2309_I2C_H_
