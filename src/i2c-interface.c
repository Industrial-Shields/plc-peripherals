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
size_t i2c_write(i2c_interface_t* i2c, uint8_t addr, const void* buff, size_t len) {
	if ((i2c == NULL) || (i2c->fd < 0) || (buff == NULL) || (len == 0)) {
		return 0;
	}

	if (ioctl(i2c->fd, I2C_SLAVE, addr) < 0) {
		return 0;
	}

	size_t wLen = write(i2c->fd, buff, len);
	if (wLen != len) {
		perror("i2c_write");
		fprintf(stderr, "i2c_write: invalid len (%u != %u)\n", (unsigned int) wLen, (unsigned int) len);
		return 0;
	}

	return len;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
size_t i2c_read(i2c_interface_t* i2c, uint8_t addr, uint8_t reg, void* buff, size_t len) {
	if ((i2c == NULL) || (i2c->fd < 0) || (buff == NULL) || (len == 0)) {
		fprintf(stderr, "i2cInit: invalid i2c\n");
		return 0;
	}

	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data data[1];

	msgs[0].addr = addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	msgs[1].addr = addr;
	msgs[1].flags = I2C_M_RD | I2C_M_NOSTART;
	msgs[1].len = len;
	msgs[1].buf = (uint8_t*) buff;

	data[0].msgs = msgs;
	data[0].nmsgs = 2;

	if (ioctl(i2c->fd, I2C_RDWR, &data) < 0) {
		perror("i2c_read");
		return 0;
	}

	return len;
}
