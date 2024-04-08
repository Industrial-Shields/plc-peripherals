/*
 * Copyright (c) 2024 Industrial Shields. All rights reserved
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

#include "../include/peripheral-ads1015.h"

#include <unistd.h>
#include <errno.h>

// Registers
#define CONVERSION_REGISTER		0x00
#define CONFIG_REGISTER			0x01
#define LO_THRESH_REGISTER		0x02
#define HI_THRESH_REGISTER		0x03

// Registers values and masks
#define CONFIG_H_MODE_CONTIN	0x01
#define CONFIG_H_MODE_SINGLE	0x01
#define CONFIG_H_PGA_0			0x00
#define CONFIG_H_PGA_1			0x02
#define CONFIG_H_PGA_2			0x04
#define CONFIG_H_PGA_3			0x06
#define CONFIG_H_PGA_4			0x08
#define CONFIG_H_PGA_5			0x0a
#define CONFIG_H_PGA_6			0x0c
#define CONFIG_H_PGA_7			0x0e
#define CONFIG_H_MUX_0_1		0x00
#define CONFIG_H_MUX_0_3		0x10
#define CONFIG_H_MUX_1_3		0x20
#define CONFIG_H_MUX_2_3		0x30
#define CONFIG_H_MUX_0			0x40
#define CONFIG_H_MUX_1			0x50
#define CONFIG_H_MUX_2			0x60
#define CONFIG_H_MUX_3			0x70
#define CONFIG_H_OS_START		0x80

#define CONFIG_L_CQUE_AFTER_1	0x00
#define CONFIG_L_CQUE_AFTER_2	0x01
#define CONFIG_L_CQUE_AFTER_4	0x02
#define CONFIG_L_CQUE_NONE		0x03
#define CONFIG_L_CLAT_NONE		0x00
#define CONFIG_L_CLAT_COMP		0x04
#define CONFIG_L_CPOL_LOW		0x00
#define CONFIG_L_CPOL_HIGH		0x08
#define CONFIG_L_CMODE_HYST		0x00
#define CONFIG_L_CMODE_WINDOW	0x10
#define CONFIG_L_DR_128			0x00
#define CONFIG_L_DR_250			0x20
#define CONFIG_L_DR_490			0x40
#define CONFIG_L_DR_920			0x60
#define CONFIG_L_DR_1600		0x80
#define CONFIG_L_DR_2400		0xa0
#define CONFIG_L_DR_3300		0xc0

int ads1015_init(i2c_interface_t* i2c, uint8_t addr) {
	int16_t read_test;
	return ads1015_read(i2c, addr, 0, &read_test);
}

int ads1015_deinit(i2c_interface_t* i2c, uint8_t addr) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128) {
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	return 0;
}

int ads1015_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, int16_t* read_value) {
	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}
	uint8_t mux;
	switch (index) {
	case 0:
		mux = CONFIG_H_MUX_0;
		break;

	case 1:
		mux = CONFIG_H_MUX_1;
		break;

	case 2:
		mux = CONFIG_H_MUX_2;
		break;

	case 3:
		mux = CONFIG_H_MUX_3;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	uint8_t buffer[3];

	buffer[0] = CONFIG_REGISTER;
	buffer[1] = CONFIG_H_MODE_SINGLE | CONFIG_H_PGA_1 | CONFIG_H_OS_START | mux;
	buffer[2] = CONFIG_L_CQUE_NONE | CONFIG_L_CLAT_NONE | CONFIG_L_CPOL_LOW | CONFIG_L_CMODE_HYST | CONFIG_L_DR_1600;
	const i2c_write_t start_conversion = {.buff=buffer, .len=sizeof(buffer)};

	int i2c_ret = i2c_write(i2c, addr, &start_conversion);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	usleep(650);

	buffer[0] = CONVERSION_REGISTER;
	const i2c_write_t read_conversion_reg = {.buff=buffer, .len=1};
	const i2c_read_t read_conversion = {.buff=buffer, .len=2}; // It must return two bytes

	i2c_ret = i2c_write_then_read(i2c, addr, &read_conversion_reg, &read_conversion);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	int16_t i2c_read = ((buffer[0] << 8) | (buffer[1]));
	if ((i2c_read & 0x000F) != 0) { // Last 4 bits were not 0, invalid conversion
		errno = ERANGE;
		return -1;
	}

	*read_value = i2c_read >> 4;
	return 0;
}

int ads1015_unsigned_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t* read_value) {
	int16_t signed_value;
	int i2c_ret = ads1015_read(i2c, addr, index, &signed_value);

	if (i2c_ret < 0) {
		return i2c_ret;
	}

	else if (signed_value < 0) {
		/* Quote from the ADS1015 datasheet, page 22:
		 * Single-ended signal measurements, where VAINN = 0 V and VAINP = 0 V to +FS, only use
		 * the positive code range from 0000h to 7FF0h. However, because of device offset, the
		 * ADS101x can still output negative codes in case VAINP is close to 0 V.
		 */
		// We accept up to three bits of error
		if (signed_value >= -7) {
			signed_value = 0;
		}
		else {
			errno = ERANGE;
			return -1;
		}
	}

	*read_value = signed_value & 0x0FFF;

	return 0;
}
