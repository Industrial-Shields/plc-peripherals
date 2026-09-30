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

#ifndef PLC_PERIPHERAL_ADS101X_I2C_H_
#define PLC_PERIPHERAL_ADS101X_I2C_H_

#include "plc-mutex.h"
#include "plc-peripherals-i2c.h"
#include "plc-peripherals-platform.h"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sized for the private handle in peripheral-ads101x.c: its mutex, the I2C
 * address, two copies of the CONFIG register, the bus number and two flags.
 */
#define ADS101X_INTERNAL_ALIGN PLC_MUTEX_ALIGN
#define ADS101X_INTERNAL_SIZE                                                 \
	PLC_PERIPHERAL_INTERNAL_PAD(PLC_MUTEX_SIZE + sizeof(plc_i2c_addr_t) + \
					    2 * sizeof(uint16_t) +            \
					    sizeof(uint8_t) +                 \
					    2 * sizeof(bool),                 \
				    ADS101X_INTERNAL_ALIGN)

/*
 * Storage for one ADS101X handle. Useful to statically allocate, without
 * malloc.
 *
 * A region handed to ads101x_static_init must be at least ADS101X_SIZE bytes
 * and at least ADS101X_ALIGN aligned.
 *
 * WARNING: Never copy a live ads101x_t. Assigning an ads101x_t, embedding one in a
 * struct that is assigned or passed by value, memcpying it, or reallocating an
 * array of them all do it. The backend does not necessarily support it!
 */
#define ADS101X_SIZE ADS101X_INTERNAL_SIZE
#define ADS101X_ALIGN ADS101X_INTERNAL_ALIGN

typedef struct {
	PLC_PERIPHERAL_INTERNAL_ALIGNAS(plc_mutex_t)
	unsigned char opaque[ADS101X_SIZE];
} ads101x_t;

typedef enum {
	// clang-format off
	ADS101X_FSR_6_144V   = 0b000,
	ADS101X_FSR_4_096V   = 0b001,
	ADS101X_FSR_2_048V   = 0b010,
	ADS101X_FSR_1_024V   = 0b011,
	ADS101X_FSR_0_512V   = 0b100,
	ADS101X_FSR_0_256V   = 0b101,
	// clang-format on
} ADS101X_GAIN_AMPLIFIER;

typedef enum {
	// clang-format off
	ADS101X_128SPS    = 0b000,
	ADS101X_250SPS    = 0b001,
	ADS101X_490SPS    = 0b010,
	ADS101X_920SPS    = 0b011,
	ADS101X_1600SPS   = 0b100,
	ADS101X_2400SPS   = 0b101,
	ADS101X_3300SPS   = 0b110,
	// clang-format on
} ADS101X_DATA_RATE;

typedef enum {
	// clang-format off
	ADS101X_P0_N1  = 0b000,
	ADS101X_P1_N3  = 0b001,
	ADS101X_P2_N3  = 0b010,
	ADS101X_P3_N3  = 0b011,
	ADS101X_P0_GND = 0b100,
	ADS101X_P1_GND = 0b101,
	ADS101X_P2_GND = 0b110,
	ADS101X_P3_GND = 0b111,
	// clang-format on
} ADS101X_INPUT;

/**
 * ads101x_config_t
 *
 * The configuration ads101x_init applies to an ADS101X. Every field must be set.
 *
 *   continuous_mode (bool)       - true to put the ADS101X in continuous
 *                                  conversion mode, false to put it in
 *                                  single-shot mode. This setting will apply
 *                                  regardless of whether the restart is true or
 *                                  false.
 *   fsr (ADS101X_GAIN_AMPLIFIER) - The programmable gain amplifier
 *                                  configuration. This setting will apply
 *                                  regardless of whether the restart is true or
 *                                  false.
 *   dr (ADS101X_DATA_RATE)       - The number of samples per second that will
 *                                  be picked up. This setting will apply
 *                                  regardless of whether the restart is true or
 *                                  false.
 */
typedef struct {
	bool continuous_mode;
	ADS101X_GAIN_AMPLIFIER fsr;
	ADS101X_DATA_RATE dr;
} ads101x_config_t;

/**
 * ads101x_init
 *
 * Allocate and initialize an ADS101X peripheral with address "addr". This
 * function currently supports ADS1015 only. You must only have one handle per
 * device.
 *
 * If a conversion was going on, it can block until that conversion is over. One
 * conversion time at the rate read back in single-shot mode, or at the slowest
 * rate (128 SPS) in continuous mode. In continuous mode, it then also blocks for
 * the first conversion with cfg. At worst, it will block up to ~9ms in single
 * mode, and up to ~18ms in continuous mode.
 *
 * Use ads101x_static_init instead to initialize a handle in storage you
 * provide, without malloc.
 *
 * WARNING: Tear down with ads101x_deinit, never with ads101x_static_deinit.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - The I2C interface to access the
 *                                   peripheral.
 *   addr (plc_i2c_addr_t)         - The I2C address of the peripheral.
 *   restart (bool)                - true if you want to reset the peripheral
 *                                   (that is, set the registers to their
 *                                   default values) before applying cfg.
 *   cfg (const ads101x_config_t*) - The configuration to apply. It is only
 *                                   read during the call.
 *
 * Returns:
 *   ads101x_t* - Pointer to the initialized peripheral struct on success.
 *                NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed cfg or i2c is NULL.
 *     - EINVAL   : cfg->fsr is not an ADS101X_GAIN_AMPLIFIER value, or cfg->dr
 *                  is not an ADS101X_DATA_RATE value.
 *     - ENOMEM   : Out of memory during allocation.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 */
ads101x_t* ads101x_init(const i2c_interface_t* i2c,
			plc_i2c_addr_t addr,
			bool restart,
			const ads101x_config_t* cfg);

/**
 * ads101x_deinit
 *
 * Deinitialize an ADS101X peripheral "ads". This function currently supports ADS1015
 * only.
 *
 * WARNINGS:
 *   - Never use this on an ads101x_static_init handle.
 *   - If the handle is protected, its mutex is destroyed first. If the
 *     shutdown write fails after that, the handle is not freed, but it is no
 *     longer protected.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to interact with.
 *   shutdown (bool)              - true if you want to leave the peripheral in
 *                                  a powered-down state.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_static_destroy reports while protected
 *                  (see plc-mutex.h). The handle is then left as it was.
 */
int ads101x_deinit(const i2c_interface_t* i2c, ads101x_t* ads, bool shutdown);

/**
 * ads101x_static_init
 *
 * Initialize an ADS101X peripheral with address "addr" in the storage passed by
 * argument, like ads101x_init but without malloc. You must ensure that this
 * region is at least ADS101X_SIZE bytes, and at least ADS101X_ALIGN aligned.
 * This function currently supports ADS1015 only. You must only have one handle
 * per device.
 *
 * If a conversion was going on, it can block until that conversion is over. One
 * conversion time at the rate read back in single-shot mode, or at the slowest
 * rate (128 SPS) in continuous mode. In continuous mode, it then also blocks for
 * the first conversion with cfg. At worst, it will block up to ~9ms in single
 * mode, and up to ~18ms in continuous mode.
 *
 * The storage must not already hold a live handle.
 *
 * WARNING: Tear down with ads101x_static_deinit, never with ads101x_deinit.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*)  - The I2C interface to access the
 *                                   peripheral.
 *   ads (ads101x_t*)              - Storage to make a handle out of.
 *   addr (plc_i2c_addr_t)         - The I2C address of the peripheral.
 *   restart (bool)                - true if you want to reset the peripheral
 *                                   (that is, set the registers to their
 *                                   default values) before applying cfg.
 *   cfg (const ads101x_config_t*) - The configuration to apply. It is only
 *                                   read during the call.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed storage is NULL or not ADS101X_ALIGN aligned, or cfg
 *                  or i2c is NULL.
 *     - EINVAL   : cfg->fsr is not an ADS101X_GAIN_AMPLIFIER value, or cfg->dr
 *                  is not an ADS101X_DATA_RATE value.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 */
int ads101x_static_init(const i2c_interface_t* i2c,
			ads101x_t* ads,
			plc_i2c_addr_t addr,
			bool restart,
			const ads101x_config_t* cfg);

/**
 * ads101x_static_deinit
 *
 * Deinitialize an ADS101X peripheral "ads" made by ads101x_static_init. The
 * storage is never freed. This function currently supports ADS1015 only.
 *
 * WARNINGS:
 *   - Never use this on an ads101x_init handle.
 *   - If the handle is protected, its mutex is destroyed first. If the
 *     shutdown write fails after that, the handle is still initialized, but it
 *     is no longer protected.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to interact with.
 *   shutdown (bool)              - true if you want to leave the peripheral in
 *                                  a powered-down state (single-shot mode).
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_static_destroy reports while protected
 *                  (see plc-mutex.h). The handle is then left as it was.
 */
int ads101x_static_deinit(const i2c_interface_t* i2c,
			  ads101x_t* ads,
			  bool shutdown);

/**
 * ads101x_protect
 *
 * Protect the ADS101X with a mutex embedded in its handle. Every other call on
 * the handle then holds it, waiting up to its timeout_ms for it.
 *
 * WARNING: Never call it while another thread or process uses the handle.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to protect.
 *   scope (plc_mutex_scope_t)    - Who the mutex has to exclude. Use
 *                                  PLC_MUTEX_SCOPE_SHARED if the handle is in
 *                                  memory shared with other processes (see
 *                                  plc_mutex_scope_t).
 * Returns:
 *   int - 0 if successful, 1 if already protected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t or i2c is NULL.
 *     - EINVAL   : i2c is not on the bus the ADS101X was initialized on.
 *     - (others) : Whatever plc_mutex_static_create reports (see
 *                  plc-mutex.h).
 */
int ads101x_protect(const i2c_interface_t* i2c,
		    ads101x_t* ads,
		    plc_mutex_scope_t scope);

/**
 * ads101x_unprotect
 *
 * Destroy the mutex embedded in the ADS101X handle.
 *
 * WARNING: Never call it while another thread or process uses the handle.
 *
 * Parameters:
 *   ads (ads101x_t*) - The ADS101X to unprotect.
 * Returns:
 *   int - 0 if successful, 1 if already unprotected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t is NULL.
 *     - (others) : Whatever plc_mutex_static_destroy reports (see
 *                  plc-mutex.h).
 */
int ads101x_unprotect(ads101x_t* ads);

/**
 * ads101x_single_read
 *
 * Trigger a single-shot conversion of an ADS101X channel, and retrieve its
 * reading. This function blocks for one conversion time at the current data
 * rate before reading the result. To use it, the ADS101X must be in single-shot
 * mode (ads101x_init must have been called with continuous_mode=false). It
 * returns the reading as a signed number.
 *
 * Per the ADS1015 datasheet (SBAS473F, section 7.4.2.1 and the OS bit's entry
 * in Table 8-4), the OS bit "can only be written when in power-down state and
 * has no effect when a conversion is ongoing". Single-shot mode returns the
 * device to power-down between conversions, so writing the OS bit starts a new
 * conversion. In continuous mode the device is always converting, so the OS
 * bit would be silently ignored, and the value read back could belong to a
 * different channel or be stale.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to interact with.
 *   index (ADS101X_INPUT)        - The input to read from the ADS101X.
 *   return_value (int16_t*)      - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t, return_value or i2c is NULL.
 *     - EINVAL   : index is not an ADS101X_INPUT value, the ADS101X was
 *                  initialized in continuous mode, or i2c is not on the bus
 *                  the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ads101x_single_read(const i2c_interface_t* i2c,
			ads101x_t* ads,
			ADS101X_INPUT index,
			int16_t* return_value,
			uint32_t timeout_ms);

/**
 * ads101x_unsigned_single_read
 *
 * Trigger a single-shot conversion of an ADS101X channel, and retrieve its
 * reading. This function blocks for one conversion time at the current data
 * rate before reading the result. To use it, the ADS101X must be in single-shot
 * mode (ads101x_init must have been called with continuous_mode=false). It
 * returns the reading as a signed number.
 *
 * Because of the device offset, a single-ended input close to 0V can still read
 * slightly negative (SBAS473F, section 7.5.4). Readings from -8 to -1 are
 * returned as 0. Readings below -8 are an error.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to interact with.
 *   index (ADS101X_INPUT)        - The input to read from the ADS101X.
 *   return_value (uint16_t*)     - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t, return_value or i2c is NULL.
 *     - EINVAL   : index is not an ADS101X_INPUT value, the ADS101X was
 *                  initialized in continuous mode, or i2c is not on the bus
 *                  the ADS101X was initialized on.
 *     - ERANGE   : Reading value is less than -8.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ads101x_unsigned_single_read(const i2c_interface_t* i2c,
				 ads101x_t* ads,
				 ADS101X_INPUT index,
				 uint16_t* return_value,
				 uint32_t timeout_ms);

/**
 * ads101x_continuous_read
 *
 * Retrieve the latest reading of an ADS101X channel. To use it, the ADS101X
 * must be in continuous mode (ads101x_init must have been called with
 * continuous_mode=true).
 *
 * This function never triggers a conversion: it relies on continuous mode's
 * free-running conversions. If the requested channel is the one being
 * converted, it reads the result right away. Otherwise it writes the new MUX
 * bits, and blocks until the conversion in flight (with the previous
 * settings) and one full conversion with the new ones have completed
 * (SBAS473F, section 7.4.2.2).
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to interact with.
 *   index (ADS101X_INPUT)        - The input to read from the ADS101X.
 *   return_value (int16_t*)      - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t, return_value or i2c is NULL.
 *     - EINVAL   : index is not an ADS101X_INPUT value, the ADS101X was
 *                  initialized in single-shot mode, or i2c is not on the bus
 *                  the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ads101x_continuous_read(const i2c_interface_t* i2c,
			    ads101x_t* ads,
			    ADS101X_INPUT index,
			    int16_t* return_value,
			    uint32_t timeout_ms);

/**
 * ads101x_unsigned_continuous_read
 *
 * Retrieve the latest reading of an ADS101X channel. To use it, the ADS101X
 * must be in continuous mode (ads101x_init must have been called with
 * continuous_mode=true).
 *
 * Because of the device offset, a single-ended input close to 0V can still read
 * slightly negative (SBAS473F, section 7.5.4). Readings from -8 to -1 are
 * stored as 0. Readings below -8 are an error.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to interact with.
 *   index (ADS101X_INPUT)        - The input to read from the ADS101X.
 *   return_value (uint16_t*)     - The value in which the reading will be
 *                                  stored.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t, return_value or i2c is NULL.
 *     - EINVAL   : index is not an ADS101X_INPUT value, the ADS101X was
 *                  initialized in single-shot mode, or i2c is not on the bus
 *                  the ADS101X was initialized on.
 *     - ERANGE   : Reading value is less than -8.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ads101x_unsigned_continuous_read(const i2c_interface_t* i2c,
				     ads101x_t* ads,
				     ADS101X_INPUT index,
				     uint16_t* return_value,
				     uint32_t timeout_ms);

/**
 * ads101x_get_fs
 *
 * Retrieve the sampling frequency set in the ADS101X.
 *
 * Parameters:
 *   ads (const ads101x_t*)  - The ADS101X to interact with.
 *   dr (ADS101X_DATA_RATE*) - Pointer to where the sampling frequency will be
 *                             saved.
 *   timeout_ms (uint32_t)   - The maximum time to wait for a reading. Only
 *                             applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t or dr is NULL.
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ads101x_get_fs(const ads101x_t* ads,
		   ADS101X_DATA_RATE* dr,
		   uint32_t timeout_ms);

/**
 * ads101x_set_fs
 *
 * Set a new sampling frequency for the ADS101X.
 *
 * In continuous mode, if the rate changed, it writes it to the device, and
 * blocks until the conversion in flight (at the previous rate) and one full
 * conversion at the new rate have completed (SBAS473F, section 7.4.2.2). In
 * single-shot mode, it doesn't access the I2C bus: the new rate is written with
 * the next ads101x_single_read.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to interact with.
 *   dr (ADS101X_DATA_RATE)       - Sampling frequency to set.
 *   timeout_ms (uint32_t)        - The maximum time to wait for a reading. Only
 *                                  applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT   : Passed ads101x_t or i2c is NULL.
 *     - EINVAL   : dr is not an ADS101X_DATA_RATE value, or i2c is not on the
 *                  bus the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 *     - (others) : Whatever plc_mutex_acquire reports while protected (see
 *                  plc-mutex.h). EOWNERDEAD is never reported, because the
 *                  driver recovers from it.
 */
int ads101x_set_fs(const i2c_interface_t* i2c,
		   ads101x_t* ads,
		   ADS101X_DATA_RATE dr,
		   uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_ADS101X_I2C_H_
