/*
 * Copyright (c) 2024 Industrial Shields. All rights reserved
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

#ifndef __PERIPHERAL_PCA9685_H__
#define __PERIPHERAL_PCA9685_H__

#include <stdint.h>
#include "../include/i2c-interface.h"

#define PCA9685_NUM_OUTPUTS 16
#define PCA9685_INTERNAL_CLOCK 25000000UL // 25 MHz

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief Initializes the PCA9685 PWM controller.
	 *
	 * This function initializes the PCA9685 PWM controller with the specified parameters.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the PCA9685 device.
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - EFAULT: The I2C interface is invalid.
	 *             - EINVAL: The I2C address is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
	int pca9685_init(i2c_interface_t* i2c, uint8_t addr);

	/**
	 * @brief Deinitializes the PCA9685 PWM controller.
	 *
	 * This function deinitializes the PCA9685 PWM controller and resets it to its default settings.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the PCA9685 device.
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - EFAULT: The I2C interface is invalid.
	 *             - EINVAL: The I2C address is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
	int pca9685_deinit(i2c_interface_t* i2c, uint8_t addr);

	/**
	 * @brief Writes a value to a single output on the PCA9685 PWM controller.
	 *
	 * This function writes a value to a single output on the PCA9685 PWM controller.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the PCA9685 device.
	 * @param index The index of the output (0-15).
	 * @param value The value to write to the output (0 for low, 1 for high).
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - EFAULT: The I2C interface is invalid.
	 *             - EINVAL: The I2C address or the index are invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
	int pca9685_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value);

	/**
	 * @brief Writes values to all outputs on the PCA9685 PWM controller.
	 *
	 * This function writes values to all outputs on the PCA9685 PWM controller with a mask,
	 * each bit representing it's index (bit 15 == LED15).
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the PCA9685 device.
	 * @param values The values to set for all pins (each bit corresponds to a pin, 0 for low, 1 for high).
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - EFAULT: The I2C interface is invalid.
	 *             - EINVAL: The I2C address is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
	int pca9685_write_all(i2c_interface_t* i2c, uint8_t addr, uint16_t values);

	/**
	 * @brief Sets the PWM frequency on the PCA9685 PWM controller.
	 *
	 * This function sets the PWM frequency on the PCA9685 PWM controller to the specified value.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the PCA9685 device.
	 * @param prescaler_value The prescaler value to set (3-255).
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - ERANGE: The prescaler value is out of range.
	 *             - EFAULT: The I2C interface is invalid.
	 *             - EINVAL: The I2C address is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
	int pca9685_pwm_frequency(i2c_interface_t *i2c, uint8_t addr, uint8_t prescaler_value);

	/**
	 * @brief Writes a PWM value to a single output on the PCA9685 PWM controller.
	 *
	 * This function writes a PWM value to a single output on the PCA9685 PWM controller.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the PCA9685 device.
	 * @param index The index of the output (0-15).
	 * @param value The PWM value to write to the output (0-4095).
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - ERANGE: The PWM value is out of range.
	 *             - EFAULT: The I2C interface is invalid.
	 *             - EINVAL: The I2C address is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
	int pca9685_pwm_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t value);

	/**
	 * @brief Writes PWM values to all outputs on the PCA9685 PWM controller.
	 *
	 * This function writes PWM values to all outputs on the PCA9685 PWM controller with an array,
	 * where each element represents the PWM value of it's index (value[0] == LED0).
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the PCA9685 device.
	 * @param values An array containing the PWM values to write to the outputs.
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - ERANGE: The PWM value is out of range.
	 *             - EFAULT: The I2C interface is invalid.
	 *             - EINVAL: The I2C address is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
	int pca9685_pwm_write_all(i2c_interface_t* i2c, uint8_t addr, const uint16_t values[PCA9685_NUM_OUTPUTS]);

#ifdef __cplusplus
}
#endif

#endif
