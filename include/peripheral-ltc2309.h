#ifndef __PERIPHERAL_LTC2309_H__
#define __PERIPHERAL_LTC2309_H__

#include <stdint.h>
#include "../include/i2c-interface.h"

#define LTC2309_NUM_INPUTS 8

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief Initializes the LTC2309 ADC.
	 *
	 * This function initializes the LTC2309 ADC. It performs a read
	 * to check for it's existence and correct behavior.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the LTC2309.
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
	int ltc2309_init(i2c_interface_t* i2c, uint8_t addr);

	/**
	 * @brief De-initializes the LTC2309 ADC.
	 *
	 * This function de-initializes the LTC2309 ADC.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the LTC2309.
	 * @return 0 on success, -1 on failure.
	 *	   On failure, errno is set as follows:
	 *	       - EFAULT: The specified pointer to the I2C interface structure is invalid.
	 *	       - EINVAL: The specified I2C address is invalid.
	 */
	int ltc2309_deinit(i2c_interface_t* i2c, uint8_t addr);

	/**
	 * @brief Reads data from the LTC2309 ADC.
	 *
	 * This function reads data from the LTC2309 ADC for the specified channel index performing
	 * a single-ended conversion.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The I2C address of the LTC2309.
	 * @param index The channel index (0-7).
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
	int ltc2309_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t* read_value);

#ifdef __cplusplus
}
#endif

#endif
