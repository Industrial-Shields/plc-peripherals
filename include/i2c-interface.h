#ifndef __I2C_INTERFACE_H__
#define __I2C_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief Macro for fast creation of an i2c_write_t structure.
	 *
	 * This macro simplifies the creation of an "i2c_write_t" structure by providing a convenient
	 * way to initialize its "buff" member with a given sequence of bytes.
	 *
	 * @param name The name of the variable to be created.
	 * @param ...  The bytes to be stored in the buffer. Each byte should be separated by a comma.
	 *
	 * @note The "sizeof" operator is used to determine the size of the buffer automatically.
	 *
	 * Example usage:
	 * ```
	 * FAST_CREATE_I2C_WRITE(write_data, 0x12, 0x34, 0x56);
	 * ```
	 * This creates a constant variable named "write_data" of type "i2c_write_t", containing the
	 * bytes "0x12", "0x34", and "0x56" in its buffer.
	 */
        #define FAST_CREATE_I2C_WRITE(name, ...) \
	const i2c_write_t name = {.buff = (const uint8_t[]) {__VA_ARGS__}, sizeof((const uint8_t[]) {__VA_ARGS__})}


	/**
	 * @brief Structure representing an I2C interface.
	 */
        struct _i2c_interface_t;
	typedef struct _i2c_interface_t i2c_interface_t;

	/**
	 * @brief Structure representing data to be written via I2C.
	 *
	 * This structure contains two fields:
	 * - @c buff: A pointer to the data buffer to be written.
	 * - @c len: The length of the data buffer.
	 */
	typedef struct {
		const uint8_t* buff;
		size_t len;
	} i2c_write_t;

	/**
	 * @brief Structure representing data to be read via I2C.
	 *
	 * This structure contains two fields:
	 * - @c buff: A pointer to the data buffer where read data will be stored.
	 * - @c len: The length of the data buffer.
	 */
	typedef struct {
		uint8_t* buff;
		size_t len;
	} i2c_read_t;

	/**
	 * @brief Initializes an I2C interface.
	 *
	 * This function initializes an I2C interface based on the target environment.
	 * Currently supported environments are Linux and Arduino ESP32.
	 *
	 * @param bus The I2C bus number.
	 * @return A pointer to the initialized I2C interface structure on success, or NULL on failure.
	 *         On failure, errno is set as follows:
	 *             - ENOMEM: Out of memory for the allocation.
	 *             - EIO: The I2C bus doesn't exist.
	 *             - For Linux:
	 *                 - Except ENOENT, all errors that the "open" system call could return.
	 *             - For Arduino ESP32:
	 *                 - ENOTRECOVERABLE: The I2C bus could not be opened. In this case, the esp_err_t is
	 *                                    returned instead of -1. This should never happen, as that would
	 *                                    mean that the esp32-hal-i2c library failed fatally.
	 */
	i2c_interface_t* i2c_init(uint8_t bus);

	/**
	 * @brief De-initializes an I2C interface.
	 *
	 * This function de-initializes an I2C interface based on the target environment.
	 * Currently supported environments are Linux and Arduino ESP32. In Arduino ESP32,
	 * if the bus can't be de-initialized in the last stage, it will crash (but this
	 * should be VERY rare).
	 *
	 * @param i2c Pointer to the pointer of the I2C interface structure to de-initialize.
	 *             The pointer is set to NULL after de-initialization.
	 * @return 0 on success, 1 if it is already de-initialized, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - EFAULT: One of the pointers of the I2C interface is invalid.
	 *             - For Linux:
	 *                 - All the errnos that the "close" system call could return.
	 *             - For Arduino ESP32:
	 *                 - EIO: The I2C bus doesn't exist.
	 */
	int i2c_deinit(i2c_interface_t** i2c);

	/**
	 * @brief Writes data to an I2C device.
	 *
	 * This function writes data to an I2C device based on the target environment.
	 * Currently supported environments are Linux and Arduino ESP32.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The address of the I2C device.
	 * @param to_write Pointer to the data to be written.
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - EFAULT: One of the pointers given is invalid.
	 *             - EINVAL: The I2C address is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or a more general error in the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
        int i2c_write(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* to_write);

	/**
	 * @brief Reads data to an I2C device.
	 *
	 * This function reads data to an I2C device based on the target environment.
	 * Currently supported environments are Linux and Arduino ESP32.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The address of the I2C device.
	 * @param to_read Pointer to the data to be written.
	 * @return 0 on success, -1 on failure.
	 *             - EFAULT: One of the pointers given is invalid.
	 *             - EINVAL: The I2C address is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or a more general error in the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
	int i2c_read(i2c_interface_t* i2c, const uint8_t addr, const i2c_read_t* to_read);

	/**
	 * @brief It writes data to an I2C device, and then it reads data from it. It is
	 *        normally used to read a certain register from the device.
	 *
	 * This function writes and reads data from an I2C device based on the target environment.
	 * Currently supported environments are Linux and Arduino ESP32.
	 *
	 * @param i2c Pointer to the I2C interface structure.
	 * @param addr The address of the I2C device.
	 * @param to_read Pointer to the data to be written.
	 * @return 0 on success, -1 on failure.
	 *         On failure, errno is set as follows:
	 *             - EFAULT: One of the pointers given is invalid.
	 *             - EINVAL: The I2C address or the length of "to_read" is invalid.
	 *             - EBADFD: The I2C interface contains incorrect data.
	 *             - EAGAIN: The operation is temporarily unavailable.
	 *             - EIO: The slave didn't ACK the request, or a more general error in the bus.
	 *             - EBADE: Unexpected result from the write function. If this errno is set, the
	 *                      return value will be the same as the platform's write function.
	 *             - For Linux:
	 *                 - Other errors that "ioctl" may return.
	 */
        int i2c_write_then_read(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* read_order, const i2c_read_t* to_read);

#ifdef __cplusplus
}
#endif

#endif
