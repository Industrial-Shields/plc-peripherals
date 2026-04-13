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

#include <plc-peripherals-i2c.h>

#include <stdbool.h>
#include <arpa/inet.h>

int i2c_write8_8b(i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  uint8_t reg,
		  uint8_t to_write)
{
	uint8_t buf[2];
	buf[0] = reg;
	buf[1] = to_write;
	ssize_t i2c_write_result = i2c_write(i2c, addr, buf, sizeof(buf));

	return i2c_write_result == 2 ? 0 : -1;
}

int i2c_write8_16b(i2c_interface_t* i2c,
		   plc_i2c_addr_t addr,
		   uint8_t reg,
		   uint16_t to_write)
{
	uint8_t buf[3];
	buf[0] = reg;
	buf[1] = (uint8_t)(to_write >> 8);
	buf[2] = (uint8_t)(to_write & 0xFF);
	ssize_t i2c_write_result = i2c_write(i2c, addr, buf, sizeof(buf));

	return i2c_write_result == 3 ? 0 : -1;
}

int i2c_read8_8b(i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t reg,
		 uint8_t* to_read)
{
	size_t bytes_read;
	ssize_t i2c_write_then_read_result = i2c_write_then_read(
		i2c, addr, &reg, 1, to_read, 1, &bytes_read);

	bool is_correct = i2c_write_then_read_result == 1 && bytes_read == 1;
	return is_correct ? 0 : -1;
}

int i2c_read8_16b(i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  uint8_t reg,
		  uint16_t* to_read)
{
	size_t bytes_read;
	ssize_t i2c_write_then_read_result = i2c_write_then_read(
		i2c, addr, &reg, 1, (uint8_t*)to_read, 2, &bytes_read);

	bool is_correct = i2c_write_then_read_result == 1 && bytes_read == 2;
	if (is_correct) {
		/*
                 * I2C returns an array in big-endian, set it to the host
		 * endianness to make the uint16_t cast correct.
		 */
		*to_read = ntohs(*to_read);
		return 0;
	}

	return -1;
}
