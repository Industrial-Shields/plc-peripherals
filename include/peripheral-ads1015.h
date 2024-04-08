#ifndef __PERIPHERAL_ADS1015_H__
#define __PERIPHERAL_ADS1015_H__

#include <stdint.h>
#include "../include/i2c-interface.h"

#define ADS1015_NUM_INPUTS 4

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief Initializes the ADS1015 ADC.
	 *
	 * This function initializes the ADS1015 ADC. It performs a read
	 * to check for it's existence and correct behavior.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the ADS1015.
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: One of the pointers given is invalid.
	 *	       - EINVAL: The I2C address is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - ERANGE: Invalid conversion result on the read check.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int ads1015_init(i2c_interface_t* i2c, uint8_t addr);

	/**
	 * @brief De-initializes the ADS1015 ADC.
	 *
	 * This function de-initializes the ADS1015 ADC.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the ADS1015.
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: The specified pointer to the I2C interface structure is invalid.
	 *	       - EINVAL: The specified I2C address is invalid.
	 */
	int ads1015_deinit(i2c_interface_t* i2c, uint8_t addr);

	/**
	 * @brief Reads data from the ADS1015 ADC.
	 *
	 * This function reads data from the ADS1015 ADC for the specified channel index performing
	 * a single-ended conversion.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the ADS1015.
	 * @param index The channel index (0-3).
	 * @param read_value Pointer to store the read value.
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: One of the pointers given is invalid.
	 *	       - EINVAL: The I2C address or the channel index is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - ERANGE: Invalid conversion result.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int ads1015_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, int16_t* read_value);

	/**
	 * @brief Reads data from the ADS1015 ADC as an unsigned value.
	 *
	 * This function reads data from the ADS1015 ADC for the specified channel index, and
	 * returns an unsigned integer of 11 valid bits, discarding the sign bit.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the ADS1015.
	 * @param index The channel index (0-3).
	 * @param read_value Pointer to store the read value.
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: One of the pointers given is invalid.
	 *	       - EINVAL: The I2C address or the channel index is invalid.
	 *	       - EBADFD: The I2C interface contains incorrect data.
	 *	       - EAGAIN: The operation is temporarily unavailable.
	 *	       - EIO: The slave didn't ACK the request, or there is a more general error on the bus.
	 *	       - EBADE: Unexpected result from the write function. If this errno is set, the
	 *			return value will be the same as the platform's write function.
	 *	       - ERANGE: Invalid conversion result.
	 *	       - For Linux:
	 *		   - Other errors that "ioctl" may return.
	 */
	int ads1015_unsigned_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t* read_value);

#ifdef __cplusplus
}
#endif

#endif
