#include "../include/i2c-interface.h"

#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "../include/detect-platform.h"

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/stat.h>
struct _i2c_interface_t {
	int fd;
};

#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#include <esp32-hal-i2c.h>
#include <esp32-hal-log.h>
#include <pins_arduino.h>
struct _i2c_interface_t {
        uint8_t bus_num;
};
const size_t MAXIMUM_I2C_TIMEOUT = 50;
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
/**
 * @brief Initializes an I2C interface for Linux.
 *
 * This function initializes an I2C interface on a Linux system by opening
 * a file descriptor of the selected I2C bus, and saving it in the struct
 * for further use.
 *
 * @param i2c Pointer to the I2C interface structure to initialize.
 * @param bus The I2C bus number.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EIO: The I2C bus doesn't exist.
 *             - Except ENOENT, all errors that the "open" system call could return.
 */
static inline int _init_i2c_platform(i2c_interface_t* i2c, uint8_t bus) {
	char i2c_file_name[32];
	snprintf(i2c_file_name, sizeof(i2c_file_name), "/dev/i2c-%d", bus);

        i2c->fd = open(i2c_file_name, O_RDWR);
        if (i2c->fd < 0) {
	        if (errno == ENOENT) {
		        errno = EIO;
	        }
	        return -1;
        }

        errno = 0;
	return 0;
}
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
/**
 * @brief Initializes an I2C interface for Arduino ESP32.
 *
 * This function initializes an I2C interface on an Arduino ESP32 platform
 * by calling the init function provided by the esp32-hal-i2c. We know that
 * it's a valid bus because it has value of 0 or 1.
 *
 * @param i2c Pointer to the I2C interface structure to initialize.
 * @param bus The I2C bus number.
 * @return 0 on success, 1 if it is already de-initialized, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EIO: The I2C bus doesn't exist.
 *             - ENOTRECOVERABLE: The I2C bus could not be opened. In this case, the esp_err_t is
 *                                returned instead of -1. This should never happen, as that would
 *                                mean that the esp32-hal-i2c library failed fatally.
 */
static inline int _init_i2c_platform(i2c_interface_t* i2c, uint8_t bus) {
	if (bus >= SOC_I2C_NUM) {
		errno = EIO;
		return -1;
	}
	i2c->bus_num = bus;

	if (i2cIsInit(bus)) {
		return 1;
	}

	int init = i2cInit(bus, SDA, SCL, 0);
	if (init != ESP_OK) {
		errno = ENOTRECOVERABLE;
		return init;
	}

	errno = 0;
	return 0;
}
#endif

i2c_interface_t* i2c_init(uint8_t bus) {
	i2c_interface_t* i2c = malloc(sizeof(i2c_interface_t));
	if (!i2c) {
		return NULL;
	}
	// Assuming that we use two's complement (-1 for ints)...
	memset(i2c, 0b11111111, sizeof(i2c_interface_t));

	if (_init_i2c_platform(i2c, bus) < 0) {
		free(i2c);
		return NULL;
	}

	errno = 0;
	return i2c;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
/**
 * @brief De-initializes an I2C interface for Linux.
 *
 * This function de-initializes an I2C interface on a Linux system, by
 * closing the file descriptor inside the I2C struct.
 *
 * @param i2c Pointer to the I2C interface structure to de-initialize.
 * @return 0 on success, 1 if it is already de-initialized, -1 on failure.
 *         On failure, errno is set as follows:
 *             - All the errnos that the "close" system call could return.
 */
static inline int _i2c_deinit_platform(i2c_interface_t* i2c) {
	if (i2c->fd < 0) {
		return 1;
	}

        return close(i2c->fd);
}
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
/**
 * @brief De-initializes an I2C interface for Arduino ESP32.
 *
 * This function de-initializes an I2C interface on an Arduino ESP32 platform
 * with the de-init function of esp32-hal-i2c. If the bus can't be
 * de-initialized in the last stage, it will crash (but this should be VERY rare).
 *
 * @param i2c Pointer to the I2C interface structure to de-initialize.
 * @return 0 on success, 1 if it is already initialized, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EIO: The I2C bus doesn't exist.
 */
static inline int _i2c_deinit_platform(i2c_interface_t* i2c) {
	if (bus >= SOC_I2C_NUM) {
		errno = EIO;
		return -1;
	}
	if (!i2cIsInit(i2c->bus_num)) {
		return 1;
	}

	ESP_ERROR_CHECK(i2cDeinit(i2c->bus_num));
	errno = 0;
	return 0;
}
#endif

int i2c_deinit(i2c_interface_t** pointer_to_i2c) {
	if (pointer_to_i2c == NULL || *pointer_to_i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	i2c_interface_t* i2c = *pointer_to_i2c;

        int ret = _i2c_deinit_platform(i2c);
        if (ret != 0) {
	        return ret;
        }

        // Assuming that we use two's complement (-1 for ints)...
        memset(i2c, 0b11111111, sizeof(i2c_interface_t));
        free(i2c);
        *pointer_to_i2c = NULL;
        errno = 0;
        return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Checks if the I2C interface is correctly initialized.
 *
 * This function checks if the I2C interface is correctly initialized based on the target
 * environment. Currently supported environments are Linux and Arduino ESP32.
 *
 * @param i2c Pointer to the I2C interface structure to check.
 * @return true if the I2C interface is correctly initialized, false otherwise.
 */
static inline bool is_i2c_correct_platform(i2c_interface_t* i2c) {
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	bool is_correct = i2c->fd < 0;
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
	bool is_correct = !i2cIsInit(i2c->bus_num);
#endif
	return is_correct;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
/**
 * @brief Writes data to an I2C device on Linux.
 *
 * This function writes data to an I2C device on a Linux system, using
 * the "write" system call.
 *
 * @param i2c Pointer to the I2C interface structure.
 * @param addr The address of the I2C device.
 * @param to_write Pointer to the data to be written.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EAGAIN: The operation is temporarily unavailable.
 *             - EIO: The slave didn't ACK the request, or a more general error (may be set by "ioctl").
 *             - EBADE: "ioctl" returned something unexpected. If this errno is set, the return
 *                       value will be the same as the "ioctl" call.
 *             - Other errnos returned by "ioctl" may be set, but they are not expected.
 */
static inline int _i2c_write_platform(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* to_write) {
	const struct i2c_msg msg = {
		.addr = addr,
		.flags = 0,
		.len = to_write->len,
		.buf = (uint8_t*) to_write->buff
	};
	struct i2c_msg msgs[1] = {msg};
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {
		{
			.msgs = msgs,
			.nmsgs = sizeof(msgs) / sizeof(struct i2c_msg)
		}
	};

	int ioctl_ret = ioctl(i2c->fd, I2C_RDWR, ioctl_data);
	switch (ioctl_ret) {
	case 1:
		return 0;
	case 0:
		errno = EAGAIN;
		[[fallthrough]];
	case -1:
		return -1;
	default:
		errno = EBADE;
		return ioctl_ret;
	}
}
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
/**
 * @brief Writes data to an I2C device on Arduino ESP32.
 *
 * This function writes data to an I2C device on an Arduino ESP32 platform.
 *
 * @param i2c Pointer to the I2C interface structure.
 * @param addr The address of the I2C device.
 * @param to_write Pointer to the data to be written.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EAGAIN: The operation is temporarily unavailable.
 *             - EIO: The slave didn't ACK the request, or a more general error.
 *             - EBADE: "i2cWrite" returned something unexpected. If this errno is set, the return
 *                      value will be the same as the "i2cWrite" call.
 */
static inline int _i2c_write_platform(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* to_write) {
	int i2c_ret = i2cWrite(i2c->bus_num, addr, to_write->buff, to_write->len, MAXIMUM_I2C_TIMEOUT);

	switch (i2c_ret) {
	case ESP_OK:
		errno = 0;
		return 0;
	case ESP_ERR_TIMEOUT:
		errno = EAGAIN;
		return -1;
	case ESP_FAIL:
		errno = EIO;
		return -1;
	default:
	        errno = EBADE;
	        return i2c_ret;
	}
}
#endif

int i2c_write(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* to_write) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (is_i2c_correct_platform(i2c)) {
	        errno = EBADFD;
	        return -1;
        }
	if (addr >= 128) {
		errno = EINVAL;
		return -1;
	}
	if (to_write == NULL || to_write->buff == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (to_write->len == 0) {
		errno = 0;
	        return 0;
	}

	return _i2c_write_platform(i2c, addr, to_write);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
/**
 * @brief Reads data to an I2C device on Linux.
 *
 * This function reads data to an I2C device on a Linux platform.
 *
 * @param i2c Pointer to the I2C interface structure.
 * @param addr The address of the I2C device.
 * @param to_read Pointer to the data to be written.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EAGAIN: The operation is temporarily unavailable.
 *             - EIO: The slave didn't ACK the request, or a more general error (may be set by "ioctl").
 *             - EBADE: "ioctl" returned something unexpected. If this errno is set, the return
 *                       value will be the same as the "ioctl" call.
 *             - Other errnos returned by "ioctl" may be set, but they are not expected.
 */
static inline int _i2c_read_platform(i2c_interface_t* i2c, const uint8_t addr, const i2c_read_t* to_read) {
	const struct i2c_msg msg = {
		.addr = addr,
		.flags = I2C_M_RD,
		.len = to_read->len,
		.buf = (uint8_t*) to_read->buff
	};
	struct i2c_msg msgs[1] = {msg};
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {
		{
			.msgs = msgs,
			.nmsgs = sizeof(msgs) / sizeof(struct i2c_msg)
		}
	};

	int ioctl_ret = ioctl(i2c->fd, I2C_RDWR, ioctl_data);
	switch (ioctl_ret) {
	case 1:
		return 0;
	case 0:
		errno = EAGAIN;
		[[fallthrough]];
	case -1:
		return -1;
	default:
		errno = EBADE;
		return ioctl_ret;
	}
}
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
/**
 * @brief Reads data to an I2C device on Arduino ESP32.
 *
 * This function reads data to an I2C device on an Arduino ESP32 platform.
 *
 * @param i2c Pointer to the I2C interface structure.
 * @param addr The address of the I2C device.
 * @param to_read Pointer to the data to be written.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EAGAIN: The operation is temporarily unavailable.
 *             - EIO: The slave didn't ACK the request, or a more general error.
 *             - EBADE: "i2cRead" returned something unexpected. If this errno is set, the return
 *                      value will be the same as the "i2cRead" call.
 */
static inline int _i2c_read_platform(i2c_interface_t* i2c, const uint8_t addr, const i2c_read_t* to_read) {
	int i2c_ret = i2cRead(i2c->bus_num, addr, to_read->buff, to_read->len, MAXIMUM_I2C_TIMEOUT);

	switch (i2c_ret) {
	case ESP_OK:
		errno = 0;
		return 0;
	case ESP_ERR_TIMEOUT:
		errno = EAGAIN;
		return -1;
	case ESP_FAIL:
		errno = EIO;
		return -1;
	default:
	        errno = EBADE;
	        return i2c_ret;
	}
}
#endif

int i2c_read(i2c_interface_t* i2c, const uint8_t addr, const i2c_read_t* to_read) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (is_i2c_correct_platform(i2c)) {
	        errno = EBADFD;
	        return -1;
        }
	if (addr >= 128) {
		errno = EINVAL;
		return -1;
	}
	if (to_read == NULL || to_read->buff == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (to_read->len == 0) {
		errno = 0;
		return 0;
	}

	return _i2c_read_platform(i2c, addr, to_read);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
/**
 * @brief It writes data to an I2C device, and then it reads data from it.
 *
 * This function writes and reads data from an I2C device on a Linux platform.
 *
 * @param i2c Pointer to the I2C interface structure.
 * @param addr The address of the I2C device.
 * @param to_read Pointer to the data to be written.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EAGAIN: The operation is temporarily unavailable.
 *             - EIO: The slave didn't ACK the request, or a more general error (may be set by "ioctl").
 *             - EBADE: "ioctl" returned something unexpected. If this errno is set, the return
 *                       value will be the same as the "ioctl" call.
 *             - Other errnos returned by "ioctl" may be set, but they are not expected.
 */
static inline int _i2c_write_then_read_platform(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* read_order, const i2c_read_t* to_read) {
	const struct i2c_msg read_order_msg = {
		.addr = addr,
		.flags = 0,
		.len = read_order->len,
		.buf = (uint8_t*) read_order->buff
	};
	const struct i2c_msg to_read_msg = {
		.addr = addr,
		.flags = I2C_M_RD,
		.len = to_read->len,
		.buf = (uint8_t*) to_read->buff
	};
	struct i2c_msg msgs[2] = {read_order_msg, to_read_msg};
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {
		{
			.msgs = msgs,
			.nmsgs = sizeof(msgs) / sizeof(struct i2c_msg)
		}
	};

	int ioctl_ret = ioctl(i2c->fd, I2C_RDWR, ioctl_data);
	switch (ioctl_ret) {
	case 2:
		return 0;
	case 0:
		errno = EAGAIN;
		[[fallthrough]];
	case -1:
		return -1;
	default:
		errno = EBADE;
		return ioctl_ret;
	}
}
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
/**
 * @brief It writes data to an I2C device, and then it reads data from it.
 *
 * This function writes and reads data from an I2C device on an Arduino ESP32 platform.
 *
 * @param i2c Pointer to the I2C interface structure.
 * @param addr The address of the I2C device.
 * @param to_read Pointer to the data to be written.
 * @return 0 on success, -1 on failure.
 *         On failure, errno is set as follows:
 *             - EAGAIN: The operation is temporarily unavailable.
 *             - EIO: The slave didn't ACK the request, or a more general error.
 *             - EBADE: "i2cWriteReadNonStop" returned something unexpected. If this errno
 *                      is set, the return value will be the same as the "i2cRead" call.
 */
static inline int _i2c_write_then_read_platform(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* read_order, const i2c_read_t* to_read) {
	size_t read_len = 0;
	int i2c_ret = i2cWriteReadNonStop(i2c->bus_num, addr, read_order->buff, read_order->len, to_read->buff, to_read->len, MAXIMUM_I2C_TIMEOUT, &read_len);

	switch (i2c_ret) {
	case ESP_OK:
		errno = 0;
		return 0;
	case ESP_ERR_TIMEOUT:
		errno = EAGAIN;
		return -1;
	case ESP_FAIL:
		errno = EIO;
		return -1;
	default:
	        errno = EBADE;
	        return i2c_ret;
	}
}
#endif

int i2c_write_then_read(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* read_order, const i2c_read_t* to_read) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (is_i2c_correct_platform(i2c)) {
	        errno = EBADFD;
	        return -1;
        }
	if (addr >= 128) {
		errno = EINVAL;
		return -1;
	}
	if (read_order == NULL || read_order->buff == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (read_order->len == 0) {
		errno = EINVAL;
		return -1;
	}
	if (to_read == NULL || to_read->buff == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (to_read->len == 0) {
		errno = EINVAL;
		return -1;
	}

	return _i2c_write_then_read_platform(i2c, addr, read_order, to_read);
}
