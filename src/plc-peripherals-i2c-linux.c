/*
 * Copyright (c) 2025 Industrial Shields. All rights reserved
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

// WIP
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"

#include <plc-peripherals-i2c.h>
#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_LINUX

#include <stdbool.h>

static inline bool is_i2c_platform_correct(i2c_interface_t* i2c)
{
	return false;
}
static inline bool is_i2c_address_valid(plc_i2c_addr_t addr)
{
	return false;
}

i2c_interface_t* i2c_init(uint8_t bus, int32_t sda, int32_t scl)
{
	return NULL;
}

int i2c_deinit(i2c_interface_t* interface, bool deinit_i2c_bus)
{
	return -1;
}

ssize_t i2c_write(i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  const uint8_t* to_write,
		  size_t to_write_len)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (is_i2c_platform_correct(i2c) || is_i2c_address_valid(addr) ||
	    to_write == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif
	return 0;
}

ssize_t i2c_read(i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t* to_read,
		 size_t to_read_len)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (is_i2c_platform_correct(i2c) || is_i2c_address_valid(addr) ||
	    to_read == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	return 0;
}

ssize_t i2c_write_then_read(i2c_interface_t* i2c,
			    plc_i2c_addr_t addr,
			    const uint8_t* to_write,
			    size_t to_write_len,
			    uint8_t* to_read,
			    size_t to_read_len,
			    size_t* read_bytes)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (is_i2c_platform_correct(i2c) || is_i2c_address_valid(addr) ||
	    to_write == NULL || to_read == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif
	return 0;
}

#endif // PLC_ENVIRONMENT == PLC_LINUX
