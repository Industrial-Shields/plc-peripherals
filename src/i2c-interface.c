#include <unistd.h>
#include <errno.h>

#include "../include/detect-platform.h"
#include "../include/i2c-interface.h"

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
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
const size_t I2C_TIMEOUT = 50;
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
i2c_interface_t* i2c_init(uint8_t bus) {
	i2c_interface_t* i2c = malloc(sizeof(i2c_interface_t));
	if (i2c == NULL) {
		return NULL;
	}

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	char i2c_file_name[32];
	snprintf(i2c_file_name, sizeof(i2c_file_name), "/dev/i2c-%d", bus);
	i2c->fd = open(i2c_file_name, O_RDWR);
	if (i2c->fd < 0) {
		free(i2c);
		return NULL;
	}
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
	if (!i2cIsInit(bus)) {
		int ret = i2cInit(bus, SDA, SCL, 0);
		if (ret != ESP_OK) {
			errno = EBADF;
			free(i2c);
			return NULL;
                }
		else {
			i2c->bus_num = bus;
                }
        }
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#endif

	errno = 0;
	return i2c;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int i2c_deinit(i2c_interface_t** i2c) {
	if (i2c == NULL || *i2c == NULL) {
		errno = EFAULT;
		return -1;
	}

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	if ((*i2c)->fd >= 0) {
		int ret = close((*i2c)->fd);
		if (ret == 0) {
			(*i2c)->fd = -1;
			free(*i2c);
			*i2c = NULL;
		}
		return ret;
	}
	else {
		errno = EBADF;
		return -1;
	}
        // defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
	int ret = i2cDeinit((*i2c)->bus_num);
	if (ret == ESP_OK) {
		errno = ret;
		(*i2c)->bus_num = -1;
		free(*i2c);
		*i2c = NULL;
        }
	else {
		errno = ret;
		return -1;
        }
        // defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int i2c_write(i2c_interface_t* i2c, uint8_t addr, const i2c_write_t* to_write) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	if (i2c->fd < 0) {
		errno = EBADF;
		return -1;
	}
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
        if (!i2cIsInit(i2c->bus_num)) {
	        errno = EBADF;
	        return -1;
        }
        // defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#endif
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
		return 1;
	}

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	const struct i2c_msg msg = {
		.addr = addr,
		.flags = 0,
		.len = to_write->len,
		.buf = (uint8_t*) to_write->buff
	};
	struct i2c_msg msgs[1] = {msg};
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {{
			.msgs = msgs,
			.nmsgs = sizeof(msgs) / sizeof(struct i2c_msg)
		}
	};

	switch (ioctl(i2c->fd, I2C_RDWR, ioctl_data)) {
	case 1:
		return 0;
		break;
	case 0:
		errno = EAGAIN;
		return 1;
		break;
 
	default:
		errno = EPROTO;
		[[fallthrough]];
	case -1:
		return -1;
		break;
	}
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
	int i2c_ret = i2cWrite(i2c->bus_num, addr, to_write->buff, to_write->len, I2C_TIMEOUT);
        if (i2c_ret != ESP_OK) {
	        errno = EIO;
	        return -1;
        }
        else {
	        errno = 0;
	        return 0;
        }
        // defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int i2c_read(i2c_interface_t* i2c, const uint8_t addr, const i2c_read_t* to_read) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	if (i2c->fd < 0) {
		errno = EBADF;
		return -1;
	}
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
	if (!i2cIsInit(i2c->bus_num)) {
	        errno = EBADF;
	        return -1;
        }
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#endif
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
		return 1;
	}

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	const struct i2c_msg msg = {
		.addr = addr,
		.flags = I2C_M_RD,
		.len = to_read->len,
		.buf = (uint8_t*) to_read->buff
	};
	struct i2c_msg msgs[1] = {msg};
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {{
			.msgs = msgs,
			.nmsgs = sizeof(msgs) / sizeof(struct i2c_msg)
		}
	};

	switch (ioctl(i2c->fd, I2C_RDWR, ioctl_data)) {
	case 1:
		return 0;
		break;
	case 0:
		errno = EAGAIN;
		return 1;
		break;

	default:
		errno = EPROTO;
		[[fallthrough]];
	case -1:
		return -1;
		break;
	}
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
	size_t read_len = 0;
	int i2c_ret = i2cRead(i2c->bus_num, addr, to_read->buff, to_read->len, I2C_TIMEOUT, &read_len);
        if (i2c_ret != ESP_OK) {
	        errno = EIO;
	        return -1;
        }
        else if (read_len != to_read->len) {
	        errno = EAGAIN;
	        return -1;
        }
        else {
	        errno = 0;
	        return 0;
        }
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int i2c_write_then_read(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* read_order, const i2c_read_t* to_read) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	if (i2c->fd < 0) {
		errno = EBADF;
		return -1;
	}
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
	if (!i2cIsInit(i2c->bus_num)) {
	        errno = EBADF;
	        return -1;
        }
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#endif
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

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
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
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {{
			.msgs = msgs,
			.nmsgs = sizeof(msgs) / sizeof(struct i2c_msg)
		}
	};

	switch (ioctl(i2c->fd, I2C_RDWR, ioctl_data)) {
	case 2:
		return 0;
		break;
	case 1:
	case 0:
		errno = EAGAIN;
		return 1;
		break;

	default:
		errno = EPROTO;
		[[fallthrough]];
	case -1:
		return -1;
		break;
	}
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
#elif defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
	size_t read_len = 0;
	int i2c_ret = i2cWriteReadNonStop(i2c->bus_num, addr, read_order->buff, read_order->len, to_read->buff, to_read->len, I2C_TIMEOUT, &read_len);
        if (i2c_ret != ESP_OK) {
	        errno = EIO;
	        return -1;
        }
        else if (read_len != to_read->len) {
	        errno = EAGAIN;
	        return -1;
        }
        else {
	        errno = 0;
	        return 0;
        }
	// defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Arduino_ESP32
#endif
}
