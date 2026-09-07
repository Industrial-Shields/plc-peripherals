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
#include <peripheral-ltc2309.h>

#include <malloc.h>
#include <errno.h>
#include <unistd.h>

#define PASS_LTC(ltc) ltc->i2c, ltc->addr

// clang-format off
// #define SHUTDOWN  0b10001100

#define CHANNEL_0 0b10001000
#define CHANNEL_1 0b11001000
#define CHANNEL_2 0b10011000
#define CHANNEL_3 0b11011000
#define CHANNEL_4 0b10101000
#define CHANNEL_5 0b11101000
#define CHANNEL_6 0b10111000
#define CHANNEL_7 0b11111000
// clang-format on

struct _ltc2309_t {
	i2c_interface_t* i2c;
	plc_i2c_addr_t addr;
	bool bip;
};

ltc2309_t* ltc2309_init(i2c_interface_t* i2c, plc_i2c_addr_t addr, bool bip)
{
	if (i2c == NULL) {
		errno = EFAULT;
		return NULL;
	}

	if (addr >= 128) {
		errno = EINVAL;
		return NULL;
	}

	ltc2309_t* ret = malloc(sizeof(struct _ltc2309_t));
	if (ret == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	ret->i2c = i2c;
	ret->addr = addr;
	ret->bip = bip;

	uint16_t read_test;
	if (ltc2309_read_unsigned(ret, 0, &read_test) != 0) {
		free(ret);
		return NULL;
	}
	return ret;
}

int ltc2309_deinit(ltc2309_t* ltc, bool shutdown)
{
	if (ltc == NULL || ltc->i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (ltc->addr >= 128) {
		errno = EINVAL;
		return -1;
	}

	if (shutdown) {
		errno = ENOTSUP;
		return -1;
	}

	free(ltc);

	errno = 0;
	return 0;
}

static inline int16_t ltc2309_conversion_reg_to_value(uint16_t read_value)
{
	uint16_t shifted = read_value >> 4;
	if (read_value & 0x8000) {
		shifted |= 0xF000;
	}
	return (int16_t)shifted;
}

static int
ltc2309_read(ltc2309_t* ltc, uint8_t index, uint16_t* conversion, bool diff)
{
	if (ltc == NULL || conversion == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (index > 7) {
		errno = EINVAL;
		return -1;
	}

	uint8_t mask = 0xFF;

	if (diff) {
		mask &= 0x7F; // 0b01111111 -> S/D Bit to 0
	}
	if (ltc->bip) {
		mask &= 0xF7; // 0b11110111 -> UNI Bit to 0
	}

	static const uint8_t channels[8] = { CHANNEL_0, CHANNEL_1, CHANNEL_2,
					     CHANNEL_3, CHANNEL_4, CHANNEL_5,
					     CHANNEL_6, CHANNEL_7 };

	uint8_t mux = channels[index] & mask;

	uint8_t buffer[2];
	buffer[0] = mux;

	if (i2c_write(PASS_LTC(ltc), buffer, 1) != 1) {
		errno = EIO;
		return -1;
	}

	usleep(5); // It must wait 1.8 us minimum before reading

	if (i2c_read(PASS_LTC(ltc), buffer, 2) != 2) {
		errno = EIO;
		return -1;
	}

	uint16_t raw_value = ((buffer[0] << 8) | (buffer[1]));

	if ((raw_value & 0x000F) != 0) {
		// Last 4 bits were not 0, invalid raw_value
		errno = ERANGE;
		return -1;
	}

	*conversion = raw_value;
	return 0;
}

int ltc2309_read_signed(ltc2309_t* ltc, uint8_t index, int16_t* read_value)
{
	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	uint16_t conversion;

	if (ltc2309_read(ltc, index, &conversion, true) != 0) {
		return -1;
	}

	*read_value = ltc2309_conversion_reg_to_value(conversion);
	return 0;
}

int ltc2309_read_unsigned(ltc2309_t* ltc, uint8_t index, uint16_t* read_value)
{
	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	uint16_t conversion;

	if (ltc2309_read(ltc, index, &conversion, false) != 0) {
		return -1;
	}

	*read_value = conversion >> 4;
	return 0;
}
