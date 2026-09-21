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

// The kernel doesn't define it...
#define I2C_M_SEVEN 0

struct _i2c_interface_t {
	uint8_t bus;
	int fd;
	bool does_it_have_10_bits;
};

static const plc_i2c_addr_t MAXIMUM_7BIT_ADDRESS = 0x7F;
static const plc_i2c_addr_t MAXIMUM_10BIT_ADDRESS = 0x3FF;

static int set_i2c_address_flags(plc_i2c_addr_t addr,
				 const i2c_interface_t* i2c,
				 uint16_t* flags)
{
	if (addr <= MAXIMUM_7BIT_ADDRESS) {
		*flags = I2C_M_SEVEN;
		return 0;
	}

	if (addr > MAXIMUM_10BIT_ADDRESS) {
		errno = EINVAL;
		return -1;
	}

	if (!i2c->does_it_have_10_bits) {
		errno = ENOTSUP;
		return -1;
	}

	*flags = I2C_M_TEN;
	return 0;
}

i2c_interface_t* i2c_init(uint8_t bus, int32_t sda, int32_t scl)
{
	int funcs_errno;

	if (sda >= 0 || scl >= 0) {
		errno = ENOTSUP;
		goto i2c_init_return_null;
	}

	i2c_interface_t* i2c = malloc(sizeof(i2c_interface_t));
	if (!i2c) {
		goto i2c_init_return_null;
	}

	char i2c_file_name[32];
	snprintf(i2c_file_name, sizeof(i2c_file_name), "/dev/i2c-%d", bus);

	// O_CLOEXEC: Do not propagate the I2C file descriptor to exec'd child.
	i2c->fd = open(i2c_file_name, O_RDWR | O_CLOEXEC);
	if (i2c->fd < 0) {
		// No node means no such bus, like on ESP32.
		if (errno == ENOENT) {
			errno = EINVAL;
		}
		goto free_i2c;
	}

	unsigned long funcs;
	if (ioctl(i2c->fd, I2C_FUNCS, &funcs) != 0) {
		funcs_errno = errno;
		goto free_i2c_fd;
	}

	i2c->does_it_have_10_bits = (funcs & I2C_FUNC_10BIT_ADDR) != 0;
	i2c->bus = bus;
	return i2c;

free_i2c_fd:
	close(i2c->fd);
	errno = funcs_errno;
free_i2c:
	free(i2c);
i2c_init_return_null:
	return NULL;
}

int i2c_deinit(i2c_interface_t* interface, bool deinit_i2c_bus)
{
	if (interface == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (deinit_i2c_bus) {
		errno = ENOTSUP;
		return -1;
	}

	int fd = interface->fd;

	free(interface);

	/*
	 * On Linux, close(2) releases the descriptor even when it reports an
	 * error, so the interface is gone either way and there is nothing for
	 * the caller to act on.
	 */
	int saved_errno = errno;

	close(fd);
	errno = saved_errno;

	return 0;
}

int i2c_get_bus(const i2c_interface_t* i2c, uint8_t* bus)
{
	if (i2c == NULL || bus == NULL) {
		errno = EFAULT;
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
	if (i2c == NULL || to_write == NULL) {
		errno = EFAULT;
		return -1;
	}

	uint16_t addr_flags;
	if (set_i2c_address_flags(addr, i2c, &addr_flags) != 0) {
		return -1;
	}
	if (to_write_len == 0) {
		return 0;
	}

	/*
	 * I2C_RDWR only reads this buffer: i2cdev_ioctl_rdwr() memdup_user()s
	 * every message's buf up front and only copy_to_user()s it back for
	 * I2C_M_RD messages, so it's safe to discard const here.
	 */
	const struct i2c_msg msg = { .addr = addr,
				     .flags = addr_flags,
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
		if (errno == EBADF) {
			/*
			 * If the file descriptor was bad, technically the
			 * "bus" was bad, like on ESP32. Which translates to a
			 * bad argument.
			 */
			errno = EINVAL;
		}
		return -1;
	default:
		/*
		 * i2c_transfer() only ever returns nmsgs or a negative errno.
		 * This guards against a misbehaving adapter driver.
		 */
		errno = EBADE;
		return -1;
	}
}

ssize_t i2c_read(const i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t* to_read,
		 size_t to_read_len)
{
	if (i2c == NULL || to_read == NULL) {
		errno = EFAULT;
		return -1;
	}

	uint16_t addr_flags;
	if (set_i2c_address_flags(addr, i2c, &addr_flags) != 0) {
		return -1;
	}
	if (to_read_len == 0) {
		return 0;
	}

	const struct i2c_msg msg = { .addr = addr,
				     .flags = I2C_M_RD | addr_flags,
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
		if (errno == EBADF) {
			/*
			 * If the file descriptor was bad, technically the
			 * "bus" was bad, like on ESP32. Which translates to a
			 * bad argument.
			 */
			errno = EINVAL;
		}
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
	if (i2c == NULL || to_write == NULL || to_read == NULL ||
	    read_bytes == NULL) {
		errno = EFAULT;
		return -1;
	}

	uint16_t addr_flags;
	if (set_i2c_address_flags(addr, i2c, &addr_flags) != 0) {
		return -1;
	}
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
						.flags = addr_flags,
						.len = to_write_len,
						.buf = (uint8_t*)to_write };
	const struct i2c_msg to_read_msg = { .addr = addr,
					     .flags = I2C_M_RD | addr_flags,
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
		if (errno == EBADF) {
			/*
			 * If the file descriptor was bad, technically the
			 * "bus" was bad, like on ESP32. Which translates to a
			 * bad argument.
			 */
			errno = EINVAL;
		}
		return -1;
	default:
		// i2c_transfer() only ever returns nmsgs or a negative errno;
		// this guards against a misbehaving adapter driver.
		errno = EBADE;
		return -1;
	}
}

#endif // PLC_ENVIRONMENT == PLC_LINUX
