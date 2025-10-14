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

#include "plc-peripherals-i2c.h"
#include <peripheral-ads101x.h>

#include <malloc.h>
#include <unistd.h>

// clang-format off
#define CONVERSION_REG                                                         0x00
#define CONFIG_REG                                                             0x01
#define   CONFIG_REG_OS                                                      0x8000
#define   CONFIG_REG_MUX                                                     0x7000
#define     CONFIG_REG_MUX_SHIFT                                                 12
#define   CONFIG_REG_PGA                                                      0xE00
#define     CONFIG_REG_PGA_SHIFT                                                  9
#define   CONFIG_REG_MODE                                                     0x100
#define     CONFIG_REG_MODE_SHIFT                                                 8
#define   CONFIG_REG_DR                                                        0xE0
#define     CONFIG_REG_DR_SHIFT                                                   5
#define   CONFIG_REG_COMP_MODE                                                 0x10
#define   CONFIG_REG_COMP_POL                                                   0x8
#define   CONFIG_REG_COMP_LAT                                                   0x4
#define   CONFIG_REG_COMP_QUE                                                   0x3
#define    CONFIG_REG_RESET_VALUE                                            0x8583
#define LOW_THRESHOLD_REG                                                      0x02
#define   LOW_THRESHOLD_REG_RESET_VALUE                                      0x8000
#define HIGH_THRESHOLD_REG                                                     0x03
#define   HIGH_THRESHOLD_REG_RESET_VALUE                                     0x7FFF
// clang-format on

struct _ads101x_t {
	i2c_interface_t* i2c;
	plc_i2c_addr_t addr;
};

#define ADS101X_RESET_REG(i2c, addr, register_name) \
	i2c_write8_16b(i2c, addr, register_name, register_name##_RESET_VALUE)

ads101x_t* ads101x_init(i2c_interface_t* i2c,
			plc_i2c_addr_t addr,
			bool restart,
			ADS101X_GAIN_AMPLIFIER fsr,
			ADS101X_DATA_RATE dr)
{
	ads101x_t* ret = malloc(sizeof(struct _ads101x_t));
	if (ret == NULL) {
		goto init_error_ret;
	}

	uint16_t configuration_reg;

	if (restart) {
		if (ADS101X_RESET_REG(i2c, addr, HIGH_THRESHOLD_REG) != 0 ||
		    ADS101X_RESET_REG(i2c, addr, LOW_THRESHOLD_REG) != 0) {
			goto init_error_cleanup;
		}
		configuration_reg = CONFIG_REG_RESET_VALUE;
	} else {
		int read_result = i2c_read8_16b(
			i2c, addr, CONFIG_REG, &configuration_reg);
		if (read_result != 0) {
			goto init_error_cleanup;
		}
	}

	// Cleanup OS bit, we don't want to start a conversion
	configuration_reg &= ~CONFIG_REG_OS;
	// Force single-shot mode
	configuration_reg |= CONFIG_REG_MODE;

	if (fsr != ADS101X_NO_FSR) {
		configuration_reg &= ~CONFIG_REG_PGA;
		configuration_reg |= fsr << CONFIG_REG_PGA_SHIFT;
	}
	if (dr != ADS101X_NO_SPS) {
		configuration_reg &= ~CONFIG_REG_DR;
		configuration_reg |= dr << CONFIG_REG_DR_SHIFT;
	}

	int write_result =
		i2c_write8_16b(i2c, addr, CONFIG_REG, configuration_reg);
	if (write_result != 0) {
		goto init_error_cleanup;
	}

	ret->i2c = i2c;
	ret->addr = addr;
	return ret;

init_error_cleanup:
	free(ret);
init_error_ret:
	return NULL;
}

int ads101x_read(ads101x_t* ads, ADS101X_INPUT index, int16_t* return_value)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (return_value == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	uint16_t new_config_reg;
	int first_read_result =
		i2c_read8_16b(ads->i2c, ads->addr, CONFIG_REG, &new_config_reg);
	if (first_read_result != 0) {
		return -1;
	}
	new_config_reg |= index << CONFIG_REG_MUX_SHIFT;

	new_config_reg |= CONFIG_REG_OS;

	int write_result =
		i2c_write8_16b(ads->i2c, ads->addr, CONFIG_REG, new_config_reg);
	if (write_result != 0) {
		return -1;
	}

	// Delay to wait for the single-shot conversion
	usleep(650);

	uint16_t read_value;
	int second_read_result =
		i2c_read8_16b(ads->i2c, ads->addr, CONVERSION_REG, &read_value);
	if (second_read_result != 0) {
		return -1;
	}

	*return_value = ((int16_t)read_value) >> 4;
	return 0;
}
