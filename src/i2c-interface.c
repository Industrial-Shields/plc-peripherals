#include "../include/i2c-interface.h"

#include <unistd.h>
#include <errno.h>

#ifdef ARDUINO_ARCH_ESP32

#else
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
struct _i2c_interface_t {
	int fd;
};
#endif



////////////////////////////////////////////////////////////////////////////////////////////////////
i2c_interface_t* i2c_init(uint8_t bus) {
	i2c_interface_t* i2c = malloc(sizeof(i2c_interface_t));
	if (i2c == NULL) {
		return NULL;
	}

	char i2c_file_name[32];
	snprintf(i2c_file_name, sizeof(i2c_file_name), "/dev/i2c-%d", bus);
	i2c->fd = open(i2c_file_name, O_RDWR);
	if (i2c->fd < 0) {
		free(i2c);
		return NULL;
	}

	errno = 0;
	return i2c;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int i2c_deinit(i2c_interface_t** i2c) {
        if (i2c == NULL || *i2c == NULL) {
	        errno = EFAULT;
	        return -1;
        }

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
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int i2c_write(i2c_interface_t* i2c, uint8_t addr, const i2c_write_t* to_write) {
	if (i2c == NULL) {
	        errno = EFAULT;
	        return -1;
        }
	if (i2c->fd < 0) {
		errno = EBADF;
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
	        return 1;
        }

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
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int i2c_read(i2c_interface_t* i2c, const uint8_t addr, const i2c_read_t* to_read) {
	if (i2c == NULL) {
	        errno = EFAULT;
	        return -1;
        }
	if (i2c->fd < 0) {
		errno = EBADF;
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
	        return 1;
        }

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
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int i2c_write_then_read(i2c_interface_t* i2c, const uint8_t addr, const i2c_write_t* read_order, const i2c_read_t* to_read) {
	if (i2c == NULL) {
	        errno = EFAULT;
	        return -1;
        }
	if (i2c->fd < 0) {
		errno = EBADF;
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
}
