/*
 * Copyright (c) 2026 Industrial Shields. All rights reserved
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

#include <plc-peripherals-i2c.h>
#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_LINUX

#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/stat.h>

struct _i2c_interface_t {
	uint8_t bus;
	int fd;
};
static const uint16_t MAXIMUM_I2C_ADDRESS = 1024;

static inline bool is_i2c_platform_correct(const i2c_interface_t* i2c)
{
	return i2c != NULL && i2c->fd >= 0;
}
static inline bool is_i2c_address_valid(plc_i2c_addr_t addr)
{
	return addr < MAXIMUM_I2C_ADDRESS;
}

i2c_interface_t* i2c_init(uint8_t bus, int32_t sda, int32_t scl)
{
	if (sda >= 0 || scl >= 0) {
		errno = ENOTSUP;
		return NULL;
	}

	i2c_interface_t* i2c = malloc(sizeof(i2c_interface_t));
	if (!i2c) {
		return NULL;
	}
	// Assuming that we use two's compliment (-1 for ints)...
	memset(i2c, 0b11111111, sizeof(i2c_interface_t));

	char i2c_file_name[32];
	snprintf(i2c_file_name, sizeof(i2c_file_name), "/dev/i2c-%d", bus);

	i2c->fd = open(i2c_file_name, O_RDWR);
	if (i2c->fd < 0) {
		free(i2c);
		return NULL;
	}

	i2c->bus = bus;
	errno = 0;
	return i2c;
}

int i2c_deinit(i2c_interface_t* interface, bool deinit_i2c_bus)
{
	if (interface == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (deinit_i2c_bus) {
		errno = ENOTSUP;
		return -1;
	}

	int ret = close(interface->fd);
	if (ret != 0) {
		return -1;
	}

	interface->fd = -1;
	free(interface);

	errno = 0;
	return 0;
}

int i2c_get_bus(const i2c_interface_t* i2c, uint8_t* bus)
{
	if (!is_i2c_platform_correct(i2c) || bus == NULL) {
		errno = EINVAL;
		return -1;
	}

	*bus = i2c->bus;
	return 0;
}

ssize_t i2c_write(const i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  const uint8_t* to_write,
		  size_t to_write_len)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (i2c == NULL || !is_i2c_platform_correct(i2c) ||
	    !is_i2c_address_valid(addr) || to_write == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif
	if (to_write_len == 0) {
		errno = 0;
		return 0;
	}

	/*
	 * I2C_RDWR only reads this buffer: i2cdev_ioctl_rdwr() memdup_user()s
	 * every message's buf up front and only copy_to_user()s it back for
	 * I2C_M_RD messages, so it's safe to discard const here.
	 */
	const struct i2c_msg msg = { .addr = addr,
				     .flags = 0,
				     .len = to_write_len,
				     .buf = (uint8_t*)to_write };
	struct i2c_msg msgs[1] = { msg };
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {
		{ .msgs = msgs, .nmsgs = sizeof(msgs) / sizeof(struct i2c_msg) }
	};

	int ioctl_ret = ioctl(i2c->fd, I2C_RDWR, ioctl_data);
	switch (ioctl_ret) {
	case 1:
		return to_write_len;
	case 0:
		errno = EAGAIN;
		return 0;
	case -1:
		return -1;
	default:
		// i2c_transfer() only ever returns nmsgs or a negative errno;
		// this guards against a misbehaving adapter driver.
		errno = EBADE;
		return -1;
	}
}

ssize_t i2c_read(const i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t* to_read,
		 size_t to_read_len)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (i2c == NULL || !is_i2c_platform_correct(i2c) ||
	    !is_i2c_address_valid(addr) || to_read == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif
	if (to_read_len == 0) {
		errno = 0;
		return 0;
	}

	const struct i2c_msg msg = { .addr = addr,
				     .flags = I2C_M_RD,
				     .len = to_read_len,
				     .buf = to_read };
	struct i2c_msg msgs[1] = { msg };
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {
		{ .msgs = msgs, .nmsgs = sizeof(msgs) / sizeof(struct i2c_msg) }
	};

	int ioctl_ret = ioctl(i2c->fd, I2C_RDWR, ioctl_data);
	switch (ioctl_ret) {
	case 1:
		return to_read_len;
	case 0:
		errno = EAGAIN;
		return 0;
	case -1:
		return -1;
	default:
		// i2c_transfer() only ever returns nmsgs or a negative errno;
		// this guards against a misbehaving adapter driver.
		errno = EBADE;
		return -1;
	}
}

ssize_t i2c_write_then_read(const i2c_interface_t* i2c,
			    plc_i2c_addr_t addr,
			    const uint8_t* to_write,
			    size_t to_write_len,
			    uint8_t* to_read,
			    size_t to_read_len,
			    size_t* read_bytes)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (i2c == NULL || !is_i2c_platform_correct(i2c) ||
	    !is_i2c_address_valid(addr) || to_write == NULL ||
	    to_read == NULL || read_bytes == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif
	if (to_write_len == 0 || to_read_len == 0) {
		*read_bytes = 0;
		return 0;
	}

	/*
	 * I2C_RDWR only reads this buffer: i2cdev_ioctl_rdwr() memdup_user()s
	 * every message's buf up front and only copy_to_user()s it back for
	 * I2C_M_RD messages, so it's safe to discard const here.
	 */
	const struct i2c_msg read_order_msg = { .addr = addr,
						.flags = 0,
						.len = to_write_len,
						.buf = (uint8_t*)to_write };
	const struct i2c_msg to_read_msg = { .addr = addr,
					     .flags = I2C_M_RD,
					     .len = to_read_len,
					     .buf = to_read };

	struct i2c_msg msgs[2] = { read_order_msg, to_read_msg };
	const struct i2c_rdwr_ioctl_data ioctl_data[1] = {
		{ .msgs = msgs, .nmsgs = sizeof(msgs) / sizeof(struct i2c_msg) }
	};

	int ioctl_ret = ioctl(i2c->fd, I2C_RDWR, ioctl_data);
	switch (ioctl_ret) {
	case 2:
		*read_bytes = to_read_len;
		return to_write_len;
	case 1:
		*read_bytes = 0;
		return to_write_len;
	case 0:
		errno = EAGAIN;
		return 0;
	case -1:
		return -1;
	default:
		// i2c_transfer() only ever returns nmsgs or a negative errno;
		// this guards against a misbehaving adapter driver.
		errno = EBADE;
		return -1;
	}
}

#endif // PLC_ENVIRONMENT == PLC_LINUX
