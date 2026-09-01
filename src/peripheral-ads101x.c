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
	bool enabled_continuous_mode;
};

#define ADS101X_RESET_REG(i2c, addr, register_name) \
	i2c_write8_16b(i2c, addr, register_name, register_name##_RESET_VALUE)
#define PASS_ADS(ads) ads->i2c, ads->addr

#define ADS101X_LOCK(ads, timeout_ms)                                 \
	do {                                                          \
		if ((ads)->is_protected) {                            \
			if (plc_resource_lock((ads)->cached_resource, \
					      (timeout_ms)) != 0) {   \
				return -1;                            \
			}                                             \
		}                                                     \
	} while (0)

#define ADS101X_UNLOCK(ads)                                        \
	do {                                                       \
		if (ads->is_protected) {                           \
			plc_resource_unlock(ads->cached_resource); \
		}                                                  \
	} while (0)

// Calculate the conversion time of the ADS101X in microseconds.
// 1 / DR + 10% clock variation + 5% for edge cases
static inline uint32_t ads101x_get_conversion_time_us(ADS101X_DATA_RATE dr)
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

	return (1100000 + 50000) / dr_decimal;
}

static int ads101x_convert_signed_to_unsigned(int16_t signed_read_value,
					      uint16_t* return_value)
{
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

#define ADS101X_GET_DR(cfg) ((cfg & CONFIG_REG_DR) >> CONFIG_REG_DR_SHIFT)
#define ADS101X_SET_DR(cfg, dr)                   \
	do {                                      \
		cfg &= ~CONFIG_REG_DR;            \
		cfg |= dr << CONFIG_REG_DR_SHIFT; \
	} while (0)
static void ads101x_delay_until_conversion(ADS101X_DATA_RATE dr)
{
	usleep(ads101x_get_conversion_time_us(dr));
}

#define ADS101X_CHANGE_CHANNEL(new_cfg, old_cfg, channel_index)   \
	do {                                                      \
		new_cfg = old_cfg & (~CONFIG_REG_MUX);            \
		new_cfg |= channel_index << CONFIG_REG_MUX_SHIFT; \
	} while (0)

ads101x_t* ads101x_init(i2c_interface_t* i2c,
			plc_i2c_addr_t addr,
			bool restart,
			bool set_continuous_mode,
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

	if (set_continuous_mode) {
		// Force continuous conversion mode
		cfg_reg &= ~CONFIG_REG_MODE;
	} else {
		// Force single-shot conversion mode
		cfg_reg |= CONFIG_REG_MODE;
	}

	// Setup PGA and DR
	cfg_reg &= ~CONFIG_REG_PGA;
	cfg_reg |= fsr << CONFIG_REG_PGA_SHIFT;

	cfg_reg &= ~CONFIG_REG_DR;
	cfg_reg |= dr << CONFIG_REG_DR_SHIFT;

	if (i2c_write8_16b(i2c, addr, CONFIG_REG, cfg_reg) != 0) {
		goto init_error_cleanup;
	}

	if (set_continuous_mode) {
		/*
		 * Delay to wait for the first conversion. Needed so
		 * ads101x_continuous_read with the same initial index works (i.e, when
		 * calling read right after the init).
		 */
		ads101x_delay_until_conversion(dr);
	}

	ret->i2c = i2c;
	ret->addr = addr;
	ret->is_protected = false;
	ret->enabled_continuous_mode = set_continuous_mode;
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
		int ret = ads101x_unprotect(ads);
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
	if (result >= 0) {
		ads->is_protected = false;
	}

	return result;
}

int ads101x_single_read(ads101x_t* ads,
			ADS101X_INPUT index,
			int16_t* return_value,
			uint32_t timeout_ms)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (return_value == NULL || ads->enabled_continuous_mode) {
		errno = EINVAL;
		return -1;
	}
#endif
	uint16_t read_value;
	uint16_t cfg_reg;
	int ret;

	ADS101X_LOCK(ads, timeout_ms);

	if (i2c_read8_16b(PASS_ADS(ads), CONFIG_REG, &cfg_reg) != 0) {
		ret = -1;
		goto ads101x_single_read_exit;
	}
	ADS101X_CHANGE_CHANNEL(cfg_reg, cfg_reg, index);
	cfg_reg |= CONFIG_REG_OS;

	if (i2c_write8_16b(PASS_ADS(ads), CONFIG_REG, cfg_reg) != 0) {
		ret = -1;
		goto ads101x_single_read_exit;
	}

	// Delay until the first conversion
	ads101x_delay_until_conversion(ADS101X_GET_DR(cfg_reg));

	if (i2c_read8_16b(PASS_ADS(ads), CONVERSION_REG, &read_value) != 0) {
		ret = -1;
		goto ads101x_single_read_exit;
	}

	*return_value = ((int16_t)read_value) >> 4;
	ret = 0;

ads101x_single_read_exit:
	ADS101X_UNLOCK(ads);

	return ret;
}

int ads101x_unsigned_single_read(ads101x_t* ads,
				 ADS101X_INPUT index,
				 uint16_t* return_value,
				 uint32_t timeout_ms)

{
	int16_t signed_read_value;

	if (ads101x_single_read(ads, index, &signed_read_value, timeout_ms) !=
	    0) {
		return -1;
	}

	return ads101x_convert_signed_to_unsigned(signed_read_value,
						  return_value);
}

int ads101x_continuous_read(ads101x_t* ads,
			    ADS101X_INPUT index,
			    int16_t* return_value,
			    uint32_t timeout_ms)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (return_value == NULL || !ads->enabled_continuous_mode) {
		errno = EINVAL;
		return -1;
	}
#endif
	uint16_t new_cfg_reg, old_cfg_reg;
	uint16_t read_value;
	int ret;

	ADS101X_LOCK(ads, timeout_ms);

	if (i2c_read8_16b(PASS_ADS(ads), CONFIG_REG, &old_cfg_reg) != 0) {
		ret = -1;
		goto ads101x_continuous_read_exit;
	}
	ADS101X_CHANGE_CHANNEL(new_cfg_reg, old_cfg_reg, index);

	if (new_cfg_reg != old_cfg_reg) {
		if (i2c_write8_16b(PASS_ADS(ads), CONFIG_REG, new_cfg_reg) !=
		    0) {
			ret = -1;
			goto ads101x_continuous_read_exit;
		}
		// Delay until the first conversion
		ads101x_delay_until_conversion(ADS101X_GET_DR(new_cfg_reg));
	}

	if (i2c_read8_16b(PASS_ADS(ads), CONVERSION_REG, &read_value) != 0) {
		ret = -1;
		goto ads101x_continuous_read_exit;
	}

	*return_value = ((int16_t)read_value) >> 4;
	ret = 0;

ads101x_continuous_read_exit:
	ADS101X_UNLOCK(ads);

	return ret;
}

int ads101x_unsigned_continuous_read(ads101x_t* ads,
				     ADS101X_INPUT index,
				     uint16_t* return_value,
				     uint32_t timeout_ms)

{
	int16_t signed_read_value;

	if (ads101x_continuous_read(
		    ads, index, &signed_read_value, timeout_ms) != 0) {
		return -1;
	}

	return ads101x_convert_signed_to_unsigned(signed_read_value,
						  return_value);
}

int ads101x_get_fs(ads101x_t* ads, ADS101X_DATA_RATE* dr)
{
	ADS101X_DATA_RATE local_dr;
	uint16_t cfg_reg;
	if (i2c_read8_16b(PASS_ADS(ads), CONFIG_REG, &cfg_reg) != 0) {
		return -1;
	}

	local_dr = ADS101X_GET_DR(cfg_reg);
	// Ensure we return a valid enum (0b111 is equivalent to 3300 SPS)
	*dr = local_dr == 0b111 ? ADS101X_3300SPS : local_dr;
	return 0;
}

int ads101x_set_fs(ads101x_t* ads, ADS101X_DATA_RATE dr, uint32_t timeout_ms)
{
	ADS101X_LOCK(ads, timeout_ms);

	uint16_t cfg_reg;
	int ret;

	if (i2c_read8_16b(PASS_ADS(ads), CONFIG_REG, &cfg_reg) != 0) {
		ret = -1;
		goto ads101x_set_fs_exit;
	}

	ADS101X_SET_DR(cfg_reg, dr);

	if (i2c_write8_16b(PASS_ADS(ads), CONFIG_REG, cfg_reg) != 0) {
		ret = -1;
		goto ads101x_set_fs_exit;
	}

	ret = 0;

ads101x_set_fs_exit:
	ADS101X_UNLOCK(ads);

	return ret;
}
