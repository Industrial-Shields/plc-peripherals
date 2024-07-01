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

#ifndef __PERIPHERAL_MCP23017_H__
#define __PERIPHERAL_MCP23017_H__

#include <stdint.h>
#include <i2c-interface.h>

#define MCP23017_NUM_IO 16

#define MCP23017_OUTPUT 0
#define MCP23017_INPUT 1

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief Initializes the MCP23017 GPIO expander.
	 *
	 * This function initializes the MCP23017 GPIO expander at the specified address.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the MCP23017.
	 * @return 0 on success, 1 if it is already initialized, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: The I2C interface is invalid.
	 *	       - EINVAL: The I2C address is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int mcp23017_init(i2c_interface_t* i2c, uint8_t addr);

	/**
	 * @brief De-initializes the MCP23017.
	 *
	 * This function de-initializes the MCP23017.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the MCP23017.
	 * @return 0 on success, 1 if it is already initialized, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: The I2C interface is invalid.
	 *	       - EINVAL: The I2C address is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int mcp23017_deinit(i2c_interface_t* i2c, uint8_t addr);

	/**
	 * @brief Sets the pin mode for a specific pin on the MCP23017 GPIO expander.
	 *
	 * This function sets the mode for a specific pin on the MCP23017 GPIO expander.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the MCP23017.
	 * @param index The pin index (0-7) for which to set the mode.
	 * @param mode The mode to set (1 for input, 0 for output).
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: The I2C interface is invalid.
	 *	       - EINVAL: The I2C address or the index given are invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int mcp23017_set_pin_mode(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t mode);

	/**
	 * @brief Sets the pin mode for all pins on the MCP23017 GPIO expander.
	 *
	 * This function sets the mode for all pins on the MCP23017 GPIO expander at the same time
	 * with a mask, each bit representing it's index (bit 15 == GPA7, bit 7 == GPB7). The most
	 * significant byte corresponds to register A, and the least significant byte corresponds
	 * to register B.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the MCP23017.
	 * @param modes The mode to set for all pins (each bit corresponds to a pin, 1 for input, 0 for output).
	 *              MS Byte is register A, LS Byte is register B.
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: The I2C interface is invalid.
	 *	       - EINVAL: The I2C address is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int mcp23017_set_pin_mode_all(i2c_interface_t* i2c, uint8_t addr, uint16_t modes);

	/**
	 * @brief Reads the value of a specific input pin on the MCP23017 GPIO expander.
	 *
	 * This function reads the value of a specific input pin on the MCP23017 GPIO expander.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the MCP23017.
	 * @param index The pin index (0-7) to read the value from.
	 * @param value Pointer to store the read value (0 if low, 1 if high).
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: One of the pointers given is invalid.
	 *	       - EINVAL: The I2C address or the index given are invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int mcp23017_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t* value);

	/**
	 * @brief Writes the value of a specific input pin on the MCP23017 GPIO expander.
	 *
	 * This function writes the value of a specific input pin on the MCP23017 GPIO expander.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the MCP23017.
	 * @param index The pin index (0-7) to write the value from.
	 * @param value The value to write (0 for low value, 1 for high value).
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: The I2C interface is invalid.
	 *	       - EINVAL: The I2C address is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
        int mcp23017_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value);

	/**
	 * @brief Reads the values of all pins on the MCP23017 GPIO expander.
	 *
	 * This function reads the values of all pins on the MCP23017 GPIO expander, and returns
	 * it as a 8 bit integer, each bit representing it's index (bit 15 == GPA7, bit 7 == GPB7).
	 * The most significant byte corresponds to register A, and the least significant byte
	 * corresponds to register B.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the MCP23017.
	 * @param value Pointer to store the read values (each bit corresponds to a pin, 0 if low, 1 if high).
	 *              MS Byte is register A, LS Byte is register B.
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: One of the pointers given is invalid.
	 *	       - EINVAL: The I2C address is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int mcp23017_read_all(i2c_interface_t* i2c, uint8_t addr, uint16_t* value);

	/**
	 * @brief Writes a value to all pins on the MCP23017 GPIO expander.
	 *
	 * This function writes a value to all pins on the MCP23017 GPIO expander
	 * directly, each bit representing it's index (bit 15 == GPA7, bit 7 == GPB7).
	 * The most significant byte corresponds to register A, and the least significant byte
	 * corresponds to register B.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the MCP23017.
	 * @param value The value to write (each bit corresponds to a pin, 0 for low value, 1 for high value).
	 *              MS Byte is register A, LS Byte is register B.
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: The I2C interface is invalid.
	 *	       - EINVAL: The I2C address is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int mcp23017_write_all(i2c_interface_t* i2c, uint8_t addr, uint16_t value);

#ifdef __cplusplus
}
#endif


#endif
