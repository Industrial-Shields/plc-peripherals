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

#define COMMAND_BYTE_SD                                                     0x80
#define COMMAND_BYTE_CHANNEL                                                0x70
#define   COMMAND_BYTE_CHANNEL_SHIFT                                           4
#define COMMAND_BYTE_UNI                                                    0x08
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
	if (ltc2309_read_unsigned(ret, LTC2309_CH0, &read_test) != 0) {
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
ltc2309_read(ltc2309_t* ltc, uint8_t mux_field, uint16_t* conversion, bool diff)
{
	if (ltc == NULL || conversion == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (mux_field > 0b111) {
		errno = EINVAL;
		return -1;
	}

	uint8_t cmd = COMMAND_BYTE_SD | COMMAND_BYTE_UNI;
	cmd |= (mux_field << COMMAND_BYTE_CHANNEL_SHIFT) & COMMAND_BYTE_CHANNEL;

	if (diff) {
		cmd &= ~COMMAND_BYTE_SD;
	}
	if (ltc->bip) {
		cmd &= ~COMMAND_BYTE_UNI;
	}

	uint8_t buffer[2];
	buffer[0] = cmd;

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

int
ltc2309_read_signed(ltc2309_t* ltc, LTC2309_DIFF_INPUT index, int16_t* read_value)
{
	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	uint16_t conversion;

	if (ltc2309_read(ltc, (uint8_t)index, &conversion, true) != 0) {
		return -1;
	}

	*read_value = ltc2309_conversion_reg_to_value(conversion);
	return 0;
}

int
ltc2309_read_unsigned(ltc2309_t* ltc, LTC2309_INPUT index, uint16_t* read_value)
{
	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	uint16_t conversion;

	if (ltc2309_read(ltc, (uint8_t)index, &conversion, false) != 0) {
		return -1;
	}

	*read_value = conversion >> 4;
	return 0;
}
