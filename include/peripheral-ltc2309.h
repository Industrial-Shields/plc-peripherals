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

#include "plc-peripherals-i2c.h"
#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define LTC2309_NUM_INPUTS  8
// clang-format on

struct _ltc2309_t;
typedef struct _ltc2309_t ltc2309_t;

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
 * Initialize an LTC2309 ADC with address "addr". This function performs a read
 * on channel 0 to check its existence and correct behavior.
 *
 * Parameters:
 *   i2c (i2c_interface_t*) - The I2C interface to access the peripheral.
 *   addr (plc_i2c_addr_t)  - The I2C address of the peripheral.
 *   bip (bool)             - Selects the input range. If false, the ADC operates
 *                            in unipolar mode (UNI bit set): the COM pin should
 *                            be connected to GND and the valid input range is
 *                            0 to +FS. If true, the ADC operates in bipolar mode
 *                            (UNI bit cleared): the COM pin should be connected
 *                            midway between GND and REFCOMP and the valid input
 *                            range is -FS/2 to +FS/2.
 *
 * Returns:
 *   ltc2309_t* - Pointer to the initialized peripheral struct on success.
 *                NULL on failure.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT : Passed i2c_interface is NULL.
 *     - EINVAL : Passed address is invalid.
 *     - ENOMEM : Out of memory during allocation.
 *     - EIO    : Communication with the LTC2309 couldn't be established.
 *     - ERANGE : The conversion result from the read check is invalid.
 */
ltc2309_t* ltc2309_init(i2c_interface_t* i2c, plc_i2c_addr_t addr, bool bip);

/**
 * ltc2309_deinit
 *
 * De-initialize an LTC2309 ADC. If shutdown is true, the LTC2309 is placed in
 * shutdown mode before returning.
 *
 * Parameters:
 *   ltc (ltc2309_t*) - The LTC2309 to interact with.
 *   shutdown (bool)  - true if you want to place the LTC2309 in shutdown mode.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT : Passed ltc2309_t or its I2C interface is NULL.
 *     - EINVAL : The address is invalid.
 *     - EIO    : Communication with the LTC2309 couldn't be established.
 */
int ltc2309_deinit(ltc2309_t* ltc, bool shutdown);

/**
 * ltc2309_read_signed
 *
 * Perform a differential conversion for the input pair and store the result
 * as a signed 12-bit value sign-extended to int16_t.
 *
 * Parameters:
 *   ltc (ltc2309_t*)            - The LTC2309 to interact with.
 *   index (LTC2309_DIFF_INPUT)  - Differential input pair to read. See
 *                                 LTC2309_DIFF_INPUT: LTC2309_P<x>_N<y> reads
 *                                 channel x as positive and y as negative.
 *   read_value (int16_t*)       - The value in which the reading will be
 *                                 stored.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT : Passed ltc2309_t or the output pointer is NULL.
 *     - EINVAL : The channel index is invalid.
 *     - EIO    : Communication with the LTC2309 couldn't be established.
 *     - ERANGE : The conversion result is invalid.
 */
int ltc2309_read_signed(ltc2309_t* ltc,
			LTC2309_DIFF_INPUT index,
			int16_t* read_value);

#define ltc2309_read_differential(...) ltc2309_read_signed(__VA_ARGS__)

/**
 * ltc2309_read_unsigned
 *
 * Perform a single-ended conversion for the input index and store the 12-bit
 * conversion result as a uint16_t.
 *
 * Single-ended mode references the selected input against the COM pin. The
 * conversion is always returned as a raw unsigned 12-bit value; the UNI bit
 * set at init determines the input voltage range (0 to +FS for unipolar,
 * -FS/2 to +FS/2 for bipolar), not the sign of the output.
 *
 * Parameters:
 *   ltc (ltc2309_t*)        - The LTC2309 to interact with.
 *   index (LTC2309_INPUT)   - Input/channel to read single-ended.
 *   read_value (uint16_t*)  - The value in which the reading will be stored.
 *
 * Returns:
 *   int - 0 if successful, otherwise -1.
 *
 * Errors:
 *   errno set to:
 *     - EFAULT : Passed ltc2309_t or the output pointer is NULL.
 *     - EINVAL : The channel index is invalid.
 *     - EIO    : Communication with the LTC2309 couldn't be established.
 *     - ERANGE : The conversion result is invalid.
 */
int ltc2309_read_unsigned(ltc2309_t* ltc,
			  LTC2309_INPUT index,
			  uint16_t* read_value);

#define ltc2309_read_single(...) ltc2309_read_unsigned(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // PLC_PERIPHERAL_LTC2309_I2C_H_
