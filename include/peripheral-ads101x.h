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

#include "plc-peripherals-i2c.h"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * WARNING: Never copy a live ads101x_t. Assigning an ads101x_t, embedding one in a
 * struct that is assigned or passed by value, memcpying it, or reallocating an
 * array of them all do it. The backend does not necessarily support it!
 */
struct _ads101x_t;
typedef struct _ads101x_t ads101x_t;

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
 * Initialize an ADS101X peripheral with address "addr". This function currently
 * supports ADS1015 only. You must only have one handle per device.
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
 *     - EFAULT   : Passed cfg is NULL.
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
 *     - EFAULT   : Passed ads101x_t is NULL.
 *     - EINVAL   : i2c is not on the bus the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 */
int ads101x_deinit(const i2c_interface_t* i2c, ads101x_t* ads, bool shutdown);

/**
 * ads101x_protect
 *
 * Protect the ADS101X with a mutex.
 *
 * Parameters:
 *   i2c (const i2c_interface_t*) - The I2C interface the ADS101X is on.
 *   ads (ads101x_t*)             - The ADS101X to protect.
 * Returns:
 *   int - 0 if successful, 1 if already protected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - ENOMEM : Out of memory during allocation.
 *     - EINVAL : Passed ads101x_t is NULL, or address is invalid.
 *     - EEXIST : The resource was already added.
 *     - EBUSY  : Hutex couldn't be taken.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int ads101x_protect(const i2c_interface_t* i2c, ads101x_t* ads);

/**
 * ads101x_unprotect
 *
 * Remove the mutex associated with the ADS101X.
 *
 * Parameters:
 *   ads (ads101x_t)         - The ADS101X to unprotect.
 * Returns:
 *   int - 0 if successful, 1 if already unprotected, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL : Passed ads101x_t is NULL, or address is invalid.
 *     - ENODEV : The resource is not present.
 *     - EBUSY  : Hash mutex couldn't be taken.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
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
 *     - EFAULT   : Passed ads101x_t or return_value is NULL.
 *     - EINVAL   : The ADS101X was initialized in continuous mode, or i2c is
 *                  not on the bus the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
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
 *     - EFAULT   : Passed ads101x_t or return_value is NULL.
 *     - EINVAL   : The ADS101X was initialized in continuous mode, or i2c is
 *                  not on the bus the ADS101X was initialized on.
 *     - ERANGE   : Reading value is less than -8.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
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
 *     - EFAULT   : Passed ads101x_t or return_value is NULL.
 *     - EINVAL   : The ADS101X was initialized in single-shot mode, or i2c is
 *                  not on the bus the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
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
 *     - EFAULT   : Passed ads101x_t or return_value is NULL.
 *     - EINVAL   : The ADS101X was initialized in single-shot mode, or i2c is
 *                  not on the bus the ADS101X was initialized on.
 *     - ERANGE   : Reading value is less than -8.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
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
 *     - EFAULT : Passed ads101x_t or dr is NULL.
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
 *     - EFAULT   : Passed ads101x_t is NULL.
 *     - EINVAL   : i2c is not on the bus the ADS101X was initialized on.
 *     - (others) : Whatever the I2C layer reports, for the bus lookup or the
 *                  transfer (see plc-peripherals-i2c.h and
 *                  plc-peripherals-i2c-hal.h).
 */
int ads101x_set_fs(const i2c_interface_t* i2c,
		   ads101x_t* ads,
		   ADS101X_DATA_RATE dr,
		   uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_ADS101X_I2C_H_
