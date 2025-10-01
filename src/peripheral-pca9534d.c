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

#include <peripheral-pca9534d.h>

#include <stdio.h>
#include <errno.h>

// Registers
#define INPUT_PORT_REGISTER		0x00
#define OUTPUT_PORT_REGISTER		0x01
#define POLARITY_INVERSION_REGISTER	0x02
#define CONFIGURATION_REGISTER		0x03

/**
 * @brief Writes a value to a register on the PCA9534D GPIO expander.
 *
 * This function writes a value to a specific register on the PCA9534D GPIO expander.
 *
 * @param i2c Pointer to the I2C interface structure.
 * @param addr The I2C address of the PCA9534D.
 * @param reg The register address to write to.
 * @param value The value to write to the register.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EFAULT: One of the pointers given is invalid.
 *             - EINVAL: The I2C address is invalid.
 *             - EBADFD: The I2C interface contains incorrect data.
 *             - EAGAIN: The operation is temporarily unavailable.
 *             - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
 *             - EBADE: Unexpected result from the write function. If this errno is set, the
 *                      return value will be the same as the platform's write function.
 *             - For Linux:
 *                 - Other errors that "ioctl" may return.
 */
static inline int write_reg(i2c_interface_t* i2c, uint8_t addr, uint8_t reg, uint8_t value) {
	FAST_CREATE_I2C_WRITE(read_order_reg, reg, value);
	return i2c_write(i2c, addr, &read_order_reg);
}

/**
 * @brief Resets the PCA9534D GPIO expander.
 *
 * This function resets the PCA9534D GPIO expander to its default configuration.
 *
 * @param i2c Pointer to the I2C interface structure.
 * @param addr The I2C address of the PCA9534D.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as described in the write_reg function documentation.
 */
static int pca9534d_reset(i2c_interface_t* i2c, uint8_t addr) {
	int i2c_ret;

	// Set all pins as inputs (configuration register = 0xFF)
	i2c_ret = write_reg(i2c, addr, CONFIGURATION_REGISTER, 0xFF);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	// Clear output port register
	i2c_ret = write_reg(i2c, addr, OUTPUT_PORT_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	// Clear polarity inversion register (normal polarity)
	i2c_ret = write_reg(i2c, addr, POLARITY_INVERSION_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	return 0;
}

int pca9534d_init(i2c_interface_t* i2c, uint8_t addr) {
	FAST_CREATE_I2C_WRITE(read_order_config_reg, CONFIGURATION_REGISTER);
//	FAST_CREATE_I2C_WRITE(read_order_polarity_reg, POLARITY_INVERSION_REGISTER);

	uint8_t config;
	i2c_read_t read_config_reg = {.buff=&config, .len=1};
//	i2c_read_t read_polarity_reg = {.buff=&polarity, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_config_reg, &read_config_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
//	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_polarity_reg, &read_polarity_reg);
//	if (i2c_ret != 0) {
//		return i2c_ret;
//	}

//	if ((config == 0xFF) && (polarity == 0x00)) {
//		// Already initialized
//		errno = EALREADY;
//		return 1;
//	}

//	i2c_ret = pca9534d_reset(i2c, addr);
//	if (i2c_ret != 0) {
//		return i2c_ret;
//	}

	errno = 0;
	return 0;
}

int pca9534d_deinit(i2c_interface_t* i2c, uint8_t addr) {
	FAST_CREATE_I2C_WRITE(read_order_config_reg, CONFIGURATION_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_polarity_reg, POLARITY_INVERSION_REGISTER);

	uint8_t config, polarity;
	i2c_read_t read_config_reg = {.buff=&config, .len=1};
	i2c_read_t read_polarity_reg = {.buff=&polarity, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_config_reg, &read_config_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_polarity_reg, &read_polarity_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	if (config == 0xFF && polarity == 0x00) {
		// Already deinitialized
		errno = 0;
		return 0;
	}

	i2c_ret = pca9534d_reset(i2c, addr);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int pca9534d_set_pin_mode(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t mode) {
	if (index >= 8 || mode >= 2) {
		errno = EINVAL;
		return -1;
	}

	FAST_CREATE_I2C_WRITE(read_order_config_reg, CONFIGURATION_REGISTER);

	uint8_t config;
	i2c_read_t read_config_reg = {.buff=&config, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_config_reg, &read_config_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	uint8_t new_config;
	if (mode == PCA9534D_INPUT) {
		new_config = config | (1 << index);
	}
	else {
		new_config = config & ~(1 << index);
	}

	if (config != new_config) {
		i2c_ret = write_reg(i2c, addr, CONFIGURATION_REGISTER, new_config);
		if (i2c_ret != 0) {
			return i2c_ret;
		}
	}

	errno = 0;
	return 0;
}

int pca9534d_set_pin_mode_all(i2c_interface_t* i2c, uint8_t addr, uint8_t modes) {
	int i2c_ret = write_reg(i2c, addr, CONFIGURATION_REGISTER, modes);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int pca9534d_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t* value) {
	if (value == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (index >= 8) {
		errno = EINVAL;
		return -1;
	}

	FAST_CREATE_I2C_WRITE(read_order_input_reg, INPUT_PORT_REGISTER);

	i2c_read_t read_input_reg = {.buff=value, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_input_reg, &read_input_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	*value = (*value >> index) & 0b00000001;

	errno = 0;
	return 0;
}

int pca9534d_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value) {
	if (index >= 8) {
		errno = EINVAL;
		return -1;
	}
	value = value > 0 ? 1 : 0;

	FAST_CREATE_I2C_WRITE(read_order_output_reg, OUTPUT_PORT_REGISTER);

	uint8_t output;
	i2c_read_t read_output_reg = {.buff=&output, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_output_reg, &read_output_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	uint8_t new_output;
	if (value) {
		new_output = output | (1 << index);
	}
	else {
		new_output = output & ~(1 << index);
	}

	if (new_output != output) {
		i2c_ret = write_reg(i2c, addr, OUTPUT_PORT_REGISTER, new_output);
		if (i2c_ret != 0) {
			return i2c_ret;
		}
	}

	errno = 0;
	return 0;
}

int pca9534d_read_all(i2c_interface_t* i2c, uint8_t addr, uint8_t* value) {
	if (value == NULL) {
		errno = EFAULT;
		return -1;
	}

	FAST_CREATE_I2C_WRITE(read_order_input_reg, INPUT_PORT_REGISTER);

	i2c_read_t read_input_reg = {.buff=value, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_input_reg, &read_input_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int pca9534d_write_all(i2c_interface_t* i2c, uint8_t addr, uint8_t value) {
	int i2c_ret = write_reg(i2c, addr, OUTPUT_PORT_REGISTER, value);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}
