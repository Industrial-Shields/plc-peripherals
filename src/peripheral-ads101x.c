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
#include <plc-resource-protector.h>
#include <peripheral-ads101x.h>

#include <malloc.h>
#include <unistd.h>
#include <errno.h>

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
	plc_resource_t cached_resource;
	bool is_protected;
};

#define ADS101X_RESET_REG(i2c, addr, register_name) \
	i2c_write8_16b(i2c, addr, register_name, register_name##_RESET_VALUE)
#define PASS_ADS(ads) ads->i2c, ads->addr

// Calculate the conversion time of the ADS101X in microseconds.
// 1 / DR + 10% clock variation
static inline uint32_t get_ads101x_conversion_time_us(ADS101X_DATA_RATE dr)
{
	uint32_t dr_decimal;
	// clang-format off
	switch (dr) {
	case ADS101X_128SPS: dr_decimal = 128; break;
	case ADS101X_250SPS: dr_decimal = 250; break;
	case ADS101X_490SPS:  dr_decimal = 490;  break;
	case ADS101X_920SPS:  dr_decimal = 920;  break;
	case ADS101X_1600SPS: dr_decimal = 1600; break;
	case ADS101X_2400SPS: dr_decimal = 2400; break;
	default: dr_decimal = 3300; break;
	}
	// clang-format on

	return 1100000 / dr_decimal;
}

ads101x_t* ads101x_init(i2c_interface_t* i2c,
			plc_i2c_addr_t addr,
			bool restart,
			ADS101X_GAIN_AMPLIFIER fsr,
			ADS101X_DATA_RATE dr)
{
	uint16_t cfg_reg;
	ads101x_t* ret;

	ret = malloc(sizeof(struct _ads101x_t));
	if (ret == NULL) {
		return NULL;
	}

	if (restart) {
		if (ADS101X_RESET_REG(i2c, addr, HIGH_THRESHOLD_REG) != 0 ||
		    ADS101X_RESET_REG(i2c, addr, LOW_THRESHOLD_REG) != 0) {
			goto init_error_cleanup;
		}
		cfg_reg = CONFIG_REG_RESET_VALUE;
	} else {
		if (i2c_read8_16b(i2c, addr, CONFIG_REG, &cfg_reg) != 0) {
			goto init_error_cleanup;
		}
	}

	// Force continuous conversion mode
	cfg_reg &= ~CONFIG_REG_MODE;

	// Setup PGA and DR
	if (fsr != ADS101X_NO_FSR) {
		cfg_reg &= ~CONFIG_REG_PGA;
		cfg_reg |= fsr << CONFIG_REG_PGA_SHIFT;
	}
	if (dr != ADS101X_NO_SPS) {
		cfg_reg &= ~CONFIG_REG_DR;
		cfg_reg |= dr << CONFIG_REG_DR_SHIFT;
	}

	if (i2c_write8_16b(i2c, addr, CONFIG_REG, cfg_reg) != 0) {
		goto init_error_cleanup;
	}

	/*
	 * Delay to wait for the first conversion. Needed so ads101x_read with
	 * the same initial index works (i.e, when calling read right after the
	 * init).
	 */
	usleep(get_ads101x_conversion_time_us(dr));

	ret->i2c = i2c;
	ret->addr = addr;
	ret->is_protected = false;
	return ret;

init_error_cleanup:
	free(ret);
	return NULL;
}

int ads101x_deinit(ads101x_t* ads, bool shutdown)
{
	uint16_t cfg_reg;

	if (shutdown) {
		if (i2c_read8_16b(PASS_ADS(ads), CONFIG_REG, &cfg_reg) != 0) {
			return -1;
		}

		cfg_reg |= CONFIG_REG_MODE;

		if (i2c_write8_16b(PASS_ADS(ads), CONFIG_REG, cfg_reg) != 0) {
			return -1;
		}
	}

	if (ads->is_protected) {
		int ret = plc_resource_remove(ads->cached_resource);
		if (ret < 0) {
			return ret;
		}
	}

	free(ads);
	return 0;
}

int ads101x_protect(ads101x_t* ads)
{
	if (ads == NULL) {
		errno = EINVAL;
		return -1;
	}

	plc_resource_t res = I2C_RESOURCE(ads->addr);
	int result = plc_resource_add(res);
	if (result >= 0) {
		ads->is_protected = true;
		ads->cached_resource = res;
	}
	return result;
}

int ads101x_unprotect(ads101x_t* ads)
{
	if (ads == NULL) {
		errno = EINVAL;
		return -1;
	}

	int result = plc_resource_remove(ads->cached_resource);
	if (result < 0) {
		ads->is_protected = false;
	}
	return result;
}

int ads101x_read(ads101x_t* ads,
		 ADS101X_INPUT index,
		 int16_t* return_value,
		 uint32_t timeout_ms)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (return_value == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif
	uint16_t new_cfg_reg, old_cfg_reg;
	uint16_t read_value;
	int ret;

	if (ads->is_protected) {
		if (plc_resource_lock(ads->cached_resource, timeout_ms) != 0) {
			return -1;
		}
	}

	if (i2c_read8_16b(PASS_ADS(ads), CONFIG_REG, &old_cfg_reg) != 0) {
		ret = -1;
		goto ads101x_read_exit;
	}
	new_cfg_reg = old_cfg_reg & (~CONFIG_REG_MUX);
	new_cfg_reg |= index << CONFIG_REG_MUX_SHIFT;

	if (new_cfg_reg != old_cfg_reg) {
		if (i2c_write8_16b(PASS_ADS(ads), CONFIG_REG, new_cfg_reg) !=
		    0) {
			ret = -1;
			goto ads101x_read_exit;
		}
		// Delay to wait for the first conversion
		ADS101X_DATA_RATE dr = (new_cfg_reg & CONFIG_REG_DR) >>
				       CONFIG_REG_DR_SHIFT;
		usleep(get_ads101x_conversion_time_us(dr));
	}

	if (i2c_read8_16b(PASS_ADS(ads), CONVERSION_REG, &read_value) != 0) {
		ret = -1;
		goto ads101x_read_exit;
	}

	*return_value = ((int16_t)read_value) >> 4;
	ret = 0;

ads101x_read_exit:
	if (ads->is_protected) {
		plc_resource_unlock(ads->cached_resource);
	}

	return ret;
}

int ads101x_unsigned_read(ads101x_t* ads,
			  ADS101X_INPUT index,
			  uint16_t* return_value,
			  uint32_t timeout_ms)

{
	int16_t signed_read_value;

	if (ads101x_read(ads, index, &signed_read_value, timeout_ms) != 0) {
		return -1;
	}

	if (signed_read_value < -8) {
		/*
		 * Quote from the ADS101X datasheet, page 22:
		 * Single-ended signal measurements, where VAINN = 0 V and VAINP = 0 V to +FS, only use
		 * the positive code range from 0000h to 7FF0h. However, because of device offset, the
		 * ADS101x can still output negative codes in case VAINP is close to 0 V.
		 *
		 * We accept up to three bits of error.
		 */
		errno = ERANGE;
		return -1;
	} else if (signed_read_value < 0) {
		signed_read_value = 0;
	}

	*return_value = (uint16_t)signed_read_value;
	return 0;
}
