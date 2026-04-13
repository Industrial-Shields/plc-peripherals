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
 * ads101x_init
 *
 * Initialize an ADS101X peripheral with address "addr". This function currently
 * supports ADS1015 only. You must only have one interface per device.
 *
 * Parameters:
 *   i2c (i2c_interface_t*)       - The I2C interface to access the peripheral.
 *   addr (plc_i2c_addr_t)        - The I2C address of the peripheral.
 *   restart (bool)               - true if you want to reset the peripheral
 *                                  (that is, set the registers to it's default
 *                                  values).
 *   fsr (ADS101X_GAIN_AMPLIFIER) - The programmable gain amplifier
 *                                  configuration. This setting will apply
 *                                  regardless of whether the restart is true or
 *                                  false.
 *   dr (ADS101X_DATA_RATE)         - The number of samples per second that will
 *                                  be picked up. This setting will apply
 *                                  regardless of whether the restart is true or
 *                                  false.
 *
 * Returns:
 *   ads101x_t* - Pointer to the initialized peripheral struct on success.
 *                NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL : Passed i2c_interface is NULL, or address is invalid.
 *     - ENOMEM : Out of memory during allocation.
 *     - EIO    : Communication with the ADS101X couldn't be established.
 */
ads101x_t* ads101x_init(i2c_interface_t* i2c,
			plc_i2c_addr_t addr,
			bool restart,
			bool set_continuous_mode,
			ADS101X_GAIN_AMPLIFIER fsr,
			ADS101X_DATA_RATE dr);

/**
 * ads101x_deinit
 *
 * Deinitialize an ADS101X peripheral "ads". This function currently supports ADS1015
 * only.
 *
 * Parameters:
 *   ads (ads101x_t)         - The ADS101X to interact with.
 *   shutdown (bool)         - true if you want to leave the peripheral in a
 *                             powered-down state.
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL : Passed ads101x_t is NULL, or address is invalid.
 *     - EIO    : Communication with the ADS101X couldn't be established.
 *     - EBUSY  : Hash mutex couldn't be taken.
 */
int ads101x_deinit(ads101x_t* ads, bool shutdown);

/**
 * ads101x_protect
 *
 * Protect the ADS101X with a mutex.
 *
 * Parameters:
 *   ads (ads101x_t)         - The ADS101X to protect.
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
int ads101x_protect(ads101x_t* ads);

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
 * Retrieve the reading from an ADS101X channel. This function will block until
 * a valid reading is available. To use it, the ADS101X must be in single
 * mode.
 *
 * Parameters:
 *   ads (ads101x_t)         - The ADS101X to interact with.
 *   index (ADS101x_INPUT)   - The input to single_read from the ADS101X.
 *   return_value (int16_t*) - The value in which the reading will be stored.
 *   timeout_ms (uint32_t)   - The maximum time to wait for a reading. Only
 *                             applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed ads101x_t is NULL, or address is invalid.
 *     - EIO                 : Communication with the ADS101X couldn't be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout given.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int ads101x_single_read(ads101x_t* ads,
			ADS101X_INPUT index,
			int16_t* return_value,
			uint32_t timeout_ms);

/**
 * ads101x_unsigned_single_read
 *
 * Retrieve the reading from an ADS101X channel. This function will block until
 * a valid reading is available. To use it, the ADS101X must be in single
 * mode.
 *
 * This function will return an error if the reading is 3 bits negative (less
 * than -8, triple the datasheet offset), and will set errno to ERANGE.
 *
 * Parameters:
 *   ads (ads101x_t)          - The ADS101X to interact with.
 *   index (ADS101x_INPUT)    - The input to single_read from the ADS101X.
 *   return_value (uint16_t*) - The value in which the reading will be stored.
 *   timeout_ms (uint32_t)    - The maximum time to wait for a reading. Only
 *                              applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed ads101x_t is NULL, or address is invalid.
 *     - EIO                 : Communication with the ADS101X couldn't be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout given.
 *     - ERANGE              : Reading value is less than -8.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int ads101x_unsigned_single_read(ads101x_t* ads,
				 ADS101X_INPUT index,
				 uint16_t* return_value,
				 uint32_t timeout_ms);

/**
 * ads101x_continuous_read
 *
 * Retrieve the reading from an ADS101X channel. If the asked channel is not the
 * one being mesured, this function will block until a valid reading is
 * available. To use it, the ADS101X must be in continuous mode.
 *
 * Parameters:
 *   ads (ads101x_t)         - The ADS101X to interact with.
 *   index (ADS101x_INPUT)   - The input to continuous_read from the ADS101X.
 *   return_value (int16_t*) - The value in which the reading will be stored.
 *   timeout_ms (uint32_t)   - The maximum time to wait for a reading. Only
 *                             applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed ads101x_t is NULL, or address is invalid.
 *     - EIO                 : Communication with the ADS101X couldn't be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout given.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int ads101x_continuous_read(ads101x_t* ads,
			    ADS101X_INPUT index,
			    int16_t* return_value,
			    uint32_t timeout_ms);

/**
 * ads101x_unsigned_continuous_read
 *
 * Retrieve the reading from an ADS101X channel. If the asked channel is not the
 * one being mesured, this function will block until a valid reading is
 * available. To use it, the ADS101X must be in continuous mode.
 *
 * This function will return an error if the reading is 3 bits negative (less
 * than -8, triple the datasheet offset), and will set errno to ERANGE.
 *
 * Parameters:
 *   ads (ads101x_t)          - The ADS101X to interact with.
 *   index (ADS101x_INPUT)    - The input to continuous_read from the ADS101X.
 *   return_value (uint16_t*) - The value in which the reading will be stored.
 *   timeout_ms (uint32_t)    - The maximum time to wait for a reading. Only
 *                              applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed ads101x_t is NULL, or address is invalid.
 *     - EIO                 : Communication with the ADS101X couldn't be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout given.
 *     - ERANGE              : Reading value is less than -8.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int ads101x_unsigned_continuous_read(ads101x_t* ads,
				     ADS101X_INPUT index,
				     uint16_t* return_value,
				     uint32_t timeout_ms);

/**
 * ads101x_get_fs
 *
 * Retrieve the sampling frequency set in the ADS101X.
 *
 * Parameters:
 *   ads (ads101x_t)         - The ADS101X to interact with.
 *   dr (ADS101X_DATA_RATE*) - Pointer to where the sampling frequency will be
 *                             saved.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed ads101x_t is NULL, or address is invalid.
 *     - EIO                 : Communication with the ADS101X couldn't be established.
 */
int ads101x_get_fs(ads101x_t* ads, ADS101X_DATA_RATE* dr);

/**
 * ads101x_set_fs
 *
 * Set a new sampling frequency for the ADS101X.
 *
 * Parameters:
 *   ads (ads101x_t)         - The ADS101X to interact with.
 *   dr (ADS101X_DATA_RATE)  - Sampling frequency to set.
 *   timeout_ms (uint32_t)   - The maximum time to wait for a reading. Only
 *                             applicable when the ADS101X is protected.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EINVAL (if enabled) : Passed ads101x_t is NULL, or address is invalid.
 *     - EIO                 : Communication with the ADS101X couldn't be established.
 *     - EBUSY               : Mutex couldn't be taken within the timeout given.
 *     - Linux specific:
 *       - EINVAL: The monotonic clock isn't available.
 */
int ads101x_set_fs(ads101x_t* ads, ADS101X_DATA_RATE dr, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_ADS101X_I2C_H_
