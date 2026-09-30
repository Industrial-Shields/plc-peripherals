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
#include <peripheral-ads101x.h>

#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

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

typedef struct {
	plc_mutex_t mutex;
	plc_i2c_addr_t addr;
	uint16_t expected_cfg_reg;
	uint16_t last_cfg_reg;
	uint8_t bus;
	bool is_protected;
	bool needs_resync;
} ads101x_internal_t;

_Static_assert(sizeof(ads101x_t) == sizeof(ads101x_internal_t),
	       "Not exactly an ads101x_internal_t");
_Static_assert(PLC_PERIPHERAL_INTERNAL_ALIGNOF(ads101x_t) ==
		       PLC_PERIPHERAL_INTERNAL_ALIGNOF(ads101x_internal_t),
	       "Not aligned exactly as an ads101x_internal_t");

#define ADS(a) ((ads101x_internal_t*)(a))

#define ADS101X_RESET_REG(i2c, addr, register_name) \
	i2c_write8_16b(i2c, addr, register_name, register_name##_RESET_VALUE)

static int ads101x_lock(ads101x_internal_t* ads, uint32_t timeout_ms)
{
	if (ads->is_protected &&
	    plc_mutex_acquire(&ads->mutex, timeout_ms) != 0) {
		if (errno != EOWNERDEAD) {
			return -1;
		} else {
			ads->needs_resync = true;
		}
	}

	return 0;
}

static void ads101x_unlock(ads101x_internal_t* ads)
{
	if (ads->is_protected) {
		plc_mutex_release(&ads->mutex);
	}
}

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

static inline int16_t ads101x_conversion_reg_to_value(uint16_t read_value)
{
	uint16_t shifted = read_value >> 4;
	if (read_value & 0x8000) {
		shifted |= 0xF000;
	}
	return (int16_t)shifted;
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

#define ADS101X_IS_CONTINUOUS_MODE(ads) \
	(!(ADS(ads)->expected_cfg_reg & CONFIG_REG_MODE))
#define ADS101X_IS_CONTINUOUS_MODE_REG(reg) (!((reg) & CONFIG_REG_MODE))

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

#define ADS101X_CHANGE_CHANNEL(new_cfg, channel_index)            \
	do {                                                      \
		new_cfg &= ~CONFIG_REG_MUX;                       \
		new_cfg |= channel_index << CONFIG_REG_MUX_SHIFT; \
	} while (0)

static int ads101x_resync_if_needed(const i2c_interface_t* i2c,
				    ads101x_internal_t* ads)
{
	uint16_t actual_cfg_reg;
	bool is_reading;

	if (ads->needs_resync) {
		/*
		 * The previous owner may have left a conversion running, and
		 * written CONFIG_REG without updating last_cfg_reg with the
		 * old values.
		 */
		if (i2c_read8_16b(
			    i2c, ads->addr, CONFIG_REG, &actual_cfg_reg) != 0) {
			return -1;
		}
		ads->needs_resync = false;

		is_reading = ADS101X_IS_CONTINUOUS_MODE(ads) ||
			     !(actual_cfg_reg & CONFIG_REG_OS);

		// Synchronize the cache with the actual value
		ads->last_cfg_reg = actual_cfg_reg;

		if (ADS101X_IS_CONTINUOUS_MODE(ads)) {
			/*
			 * The conversion in flight may still use the previous rate.
			 * Take the biggest timeout possible just in case.
			 */
			ads101x_delay_until_conversion(ADS101X_128SPS);
		}
		if (is_reading) {
			ads101x_delay_until_conversion(
				ADS101X_GET_DR(actual_cfg_reg));
		}
	}

	return 0;
}

int ads101x_static_init(const i2c_interface_t* i2c,
			ads101x_t* ads,
			plc_i2c_addr_t addr,
			bool restart,
			const ads101x_config_t* cfg)
{
	ADS101X_DATA_RATE dr_to_write, in_flight_dr;
	ADS101X_GAIN_AMPLIFIER fsr_to_write;
	bool may_be_converting;
	uint16_t cfg_reg;
	uint8_t bus;

	if (ads == NULL || ((uintptr_t)ads % ADS101X_ALIGN) != 0 ||
	    cfg == NULL) {
		errno = EFAULT;
		return -1;
	}

	// Ensure we write a valid enum (0b110 and 0b111 are equivalent to +-0.256V)
	if (cfg->fsr == 0b110 || cfg->fsr == 0b111) {
		fsr_to_write = ADS101X_FSR_0_256V;
	} else {
		fsr_to_write = cfg->fsr;
	}

	// Ensure we write a valid enum (0b111 is equivalent to 3300 SPS)
	if (cfg->dr == 0b111) {
		dr_to_write = ADS101X_3300SPS;
	} else {
		dr_to_write = cfg->dr;
	}

	if (fsr_to_write > ADS101X_FSR_0_256V ||
	    dr_to_write > ADS101X_3300SPS) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_get_bus(i2c, &bus) != 0) {
		return -1;
	}

	if (i2c_read8_16b(i2c, addr, CONFIG_REG, &cfg_reg) != 0) {
		return -1;
	}

	may_be_converting = !(cfg_reg & CONFIG_REG_OS);
	if (ADS101X_IS_CONTINUOUS_MODE_REG(cfg_reg)) {
		/*
		 * Since we cannot correctly determine the sampling frequency of
		 * the continuous mode, assume the in-flight frequency sampling
		 * is the slowest available (biggest wait time)
		 */
		in_flight_dr = ADS101X_128SPS;
	} else {
		in_flight_dr = ADS101X_GET_DR(cfg_reg);
	}

	if (restart) {
		if (ADS101X_RESET_REG(i2c, addr, HIGH_THRESHOLD_REG) != 0 ||
		    ADS101X_RESET_REG(i2c, addr, LOW_THRESHOLD_REG) != 0) {
			return -1;
		}
		cfg_reg = CONFIG_REG_RESET_VALUE;
	}

	if (cfg->continuous_mode) {
		// Force continuous conversion mode
		cfg_reg &= ~CONFIG_REG_MODE;
		cfg_reg &= ~CONFIG_REG_OS;
	} else {
		// Force single-shot conversion mode
		cfg_reg |= CONFIG_REG_MODE;
		cfg_reg |= CONFIG_REG_OS;
	}

	// Setup PGA and DR
	cfg_reg &= ~CONFIG_REG_PGA;
	cfg_reg |= fsr_to_write << CONFIG_REG_PGA_SHIFT;

	cfg_reg &= ~CONFIG_REG_DR;
	cfg_reg |= dr_to_write << CONFIG_REG_DR_SHIFT;

	/*
	 * In single-shot mode the cache keeps OS set, so every
	 * ads101x_single_read starts a conversion, but init must not start one:
	 * an OS write has no effect while a conversion runs (SBAS473F, section
	 * 7.4.2.1), so a read right after init would get init's conversion, on
	 * whatever MUX CONFIG_REG had, instead of its own.
	 */
	const uint16_t init_cfg_reg =
		cfg->continuous_mode ? cfg_reg : (cfg_reg & ~CONFIG_REG_OS);

	if (i2c_write8_16b(i2c, addr, CONFIG_REG, init_cfg_reg) != 0) {
		return -1;
	}

	/*
	 * A conversion already running finishes with the previous settings
	 * (SBAS473F, sections 7.4.2.1 and 7.4.2.2): wait it out, so that a read
	 * right after init doesn't get its result.
	 */
	if (may_be_converting) {
		ads101x_delay_until_conversion(in_flight_dr);
	}

	if (cfg->continuous_mode) {
		/*
		 * Delay to wait for the first conversion. Needed so
		 * ads101x_continuous_read with the same initial index works (i.e, when
		 * calling read right after the init).
		 */
		ads101x_delay_until_conversion(dr_to_write);
	}

	ADS(ads)->addr = addr;
	ADS(ads)->bus = bus;
	ADS(ads)->expected_cfg_reg = cfg_reg;
	ADS(ads)->last_cfg_reg = cfg_reg;
	ADS(ads)->is_protected = false;
	ADS(ads)->needs_resync = false;
	return 0;
}

ads101x_t* ads101x_init(const i2c_interface_t* i2c,
			plc_i2c_addr_t addr,
			bool restart,
			const ads101x_config_t* cfg)
{
	ads101x_t* ret = malloc(sizeof(ads101x_t));

	if (ret == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	if (ads101x_static_init(i2c, ret, addr, restart, cfg) != 0) {
		free(ret);
		return NULL;
	}

	return ret;
}

int ads101x_static_deinit(const i2c_interface_t* i2c,
			  ads101x_t* ads,
			  bool shutdown)
{
	if (ads == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, ADS(ads)->bus) != 0) {
		return -1;
	}

	if (ads101x_unprotect(ads) < 0) {
		return -1;
	}

	if (shutdown) {
		ADS(ads)->expected_cfg_reg |= CONFIG_REG_MODE;

		if (i2c_write8_16b(i2c,
				   ADS(ads)->addr,
				   CONFIG_REG,
				   ADS(ads)->expected_cfg_reg) != 0) {
			return -1;
		}
	}

	return 0;
}

int ads101x_deinit(const i2c_interface_t* i2c, ads101x_t* ads, bool shutdown)
{
	if (ads101x_static_deinit(i2c, ads, shutdown) != 0) {
		return -1;
	}

	free(ads);
	return 0;
}

int ads101x_protect(const i2c_interface_t* i2c,
		    ads101x_t* ads,
		    plc_mutex_scope_t scope)
{
	if (ads == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, ADS(ads)->bus) != 0) {
		return -1;
	}

	if (ADS(ads)->is_protected) {
		return 1;
	}

	if (plc_mutex_static_create(&ADS(ads)->mutex, scope) != 0) {
		return -1;
	}

	ADS(ads)->is_protected = true;
	return 0;
}

int ads101x_unprotect(ads101x_t* ads)
{
	if (ads == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (!ADS(ads)->is_protected) {
		return 1;
	}

	if (plc_mutex_static_destroy(&ADS(ads)->mutex) != 0) {
		return -1;
	}

	ADS(ads)->is_protected = false;
	return 0;
}

int ads101x_single_read(const i2c_interface_t* i2c,
			ads101x_t* ads,
			ADS101X_INPUT index,
			int16_t* return_value,
			uint32_t timeout_ms)
{
	uint16_t read_value;
	int ret;

	if (ads == NULL || return_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (index > ADS101X_P3_GND) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, ADS(ads)->bus) != 0) {
		return -1;
	}

	if (ads101x_lock(ADS(ads), timeout_ms) != 0) {
		return -1;
	}

	if (ADS101X_IS_CONTINUOUS_MODE(ads)) {
		errno = EINVAL;
		ret = -1;
		goto ads101x_single_read_exit;
	}

	if (ads101x_resync_if_needed(i2c, ADS(ads)) != 0) {
		ret = -1;
		goto ads101x_single_read_exit;
	}

	ADS101X_CHANGE_CHANNEL(ADS(ads)->expected_cfg_reg, index);

	if (i2c_write8_16b(i2c,
			   ADS(ads)->addr,
			   CONFIG_REG,
			   ADS(ads)->expected_cfg_reg) != 0) {
		ret = -1;
		goto ads101x_single_read_exit;
	}

	// Delay for the conversion
	ads101x_delay_until_conversion(
		ADS101X_GET_DR(ADS(ads)->expected_cfg_reg));

	if (i2c_read8_16b(i2c, ADS(ads)->addr, CONVERSION_REG, &read_value) !=
	    0) {
		ret = -1;
		goto ads101x_single_read_exit;
	}

	*return_value = ads101x_conversion_reg_to_value(read_value);
	ret = 0;

ads101x_single_read_exit:
	ads101x_unlock(ADS(ads));

	return ret;
}

int ads101x_unsigned_single_read(const i2c_interface_t* i2c,
				 ads101x_t* ads,
				 ADS101X_INPUT index,
				 uint16_t* return_value,
				 uint32_t timeout_ms)

{
	int16_t signed_read_value;

	if (return_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (ads101x_single_read(
		    i2c, ads, index, &signed_read_value, timeout_ms) != 0) {
		return -1;
	}

	return ads101x_convert_signed_to_unsigned(signed_read_value,
						  return_value);
}

int ads101x_continuous_read(const i2c_interface_t* i2c,
			    ads101x_t* ads,
			    ADS101X_INPUT index,
			    int16_t* return_value,
			    uint32_t timeout_ms)
{
	uint16_t read_value;
	int ret;

	if (ads == NULL || return_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (index > ADS101X_P3_GND) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, ADS(ads)->bus) != 0) {
		return -1;
	}

	if (ads101x_lock(ADS(ads), timeout_ms) != 0) {
		return -1;
	}

	if (!ADS101X_IS_CONTINUOUS_MODE(ads)) {
		errno = EINVAL;
		ret = -1;
		goto ads101x_continuous_read_exit;
	}

	if (ads101x_resync_if_needed(i2c, ADS(ads)) != 0) {
		ret = -1;
		goto ads101x_continuous_read_exit;
	}

	ADS101X_CHANGE_CHANNEL(ADS(ads)->expected_cfg_reg, index);

	if (ADS(ads)->expected_cfg_reg != ADS(ads)->last_cfg_reg) {
		if (i2c_write8_16b(i2c,
				   ADS(ads)->addr,
				   CONFIG_REG,
				   ADS(ads)->expected_cfg_reg) != 0) {
			ret = -1;
			goto ads101x_continuous_read_exit;
		}

		/*
		 * Per the datasheet (SBAS473F 7.4.2.2), the conversion already
		 * in flight completes with the PREVIOUS settings; only the one
		 * after it uses the new ones. In continuous mode, every
		 * CONFIG_REG write (ads101x_init, ads101x_continuous_read and
		 * ads101x_set_fs) also updates last_cfg_reg, so that in-flight
		 * conversion is necessarily still running at last_cfg_reg's
		 * rate Wait the old conversion time plus one full conversion at
		 * the new rate.
		 */
		ads101x_delay_until_conversion(
			ADS101X_GET_DR(ADS(ads)->last_cfg_reg));
		ads101x_delay_until_conversion(
			ADS101X_GET_DR(ADS(ads)->expected_cfg_reg));

		ADS(ads)->last_cfg_reg = ADS(ads)->expected_cfg_reg;
	}

	if (i2c_read8_16b(i2c, ADS(ads)->addr, CONVERSION_REG, &read_value) !=
	    0) {
		ret = -1;
		goto ads101x_continuous_read_exit;
	}

	*return_value = ads101x_conversion_reg_to_value(read_value);
	ret = 0;

ads101x_continuous_read_exit:
	ads101x_unlock(ADS(ads));

	return ret;
}

int ads101x_unsigned_continuous_read(const i2c_interface_t* i2c,
				     ads101x_t* ads,
				     ADS101X_INPUT index,
				     uint16_t* return_value,
				     uint32_t timeout_ms)

{
	int16_t signed_read_value;

	if (return_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (ads101x_continuous_read(
		    i2c, ads, index, &signed_read_value, timeout_ms) != 0) {
		return -1;
	}

	return ads101x_convert_signed_to_unsigned(signed_read_value,
						  return_value);
}

int ads101x_get_fs(const ads101x_t* ads,
		   ADS101X_DATA_RATE* dr,
		   uint32_t timeout_ms)
{
	ADS101X_DATA_RATE local_dr;

	if (ads == NULL || dr == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (ads101x_lock(ADS(ads), timeout_ms) != 0) {
		return -1;
	}

	local_dr = ADS101X_GET_DR(ADS(ads)->expected_cfg_reg);
	// Ensure we return a valid enum (0b111 is equivalent to 3300 SPS)
	*dr = local_dr == 0b111 ? ADS101X_3300SPS : local_dr;

	ads101x_unlock(ADS(ads));

	return 0;
}

int ads101x_set_fs(const i2c_interface_t* i2c,
		   ads101x_t* ads,
		   ADS101X_DATA_RATE dr,
		   uint32_t timeout_ms)
{
	int ret;

	if (ads == NULL) {
		errno = EFAULT;
		return -1;
	}

	// Ensure we write a valid enum (0b111 is equivalent to 3300 SPS)
	if (dr == 0b111) {
		dr = ADS101X_3300SPS;
	}

	if (dr > ADS101X_3300SPS) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, ADS(ads)->bus) != 0) {
		return -1;
	}

	if (ads101x_lock(ADS(ads), timeout_ms) != 0) {
		return -1;
	}

	if (ADS101X_IS_CONTINUOUS_MODE(ads) &&
	    ads101x_resync_if_needed(i2c, ADS(ads)) != 0) {
		ret = -1;
		goto ads101x_set_fs_exit;
	}

	ADS101X_SET_DR(ADS(ads)->expected_cfg_reg, dr);

	if (ADS101X_IS_CONTINUOUS_MODE(ads) &&
	    ADS(ads)->expected_cfg_reg != ADS(ads)->last_cfg_reg) {
		if (i2c_write8_16b(i2c,
				   ADS(ads)->addr,
				   CONFIG_REG,
				   ADS(ads)->expected_cfg_reg) != 0) {
			ret = -1;
			goto ads101x_set_fs_exit;
		}

		/*
		 * Per the datasheet (SBAS473F 7.4.2.2), the conversion already
		 * in flight completes with the PREVIOUS settings; only the one
		 * after it uses the new ones. In continuous mode, every
		 * CONFIG_REG write (ads101x_init, ads101x_continuous_read and
		 * ads101x_set_fs) also updates last_cfg_reg, so that in-flight
		 * conversion is necessarily still running at last_cfg_reg's
		 * rate. Wait it out, then wait one full conversion at the new
		 * rate.
		 */
		ads101x_delay_until_conversion(
			ADS101X_GET_DR(ADS(ads)->last_cfg_reg));
		ads101x_delay_until_conversion(
			ADS101X_GET_DR(ADS(ads)->expected_cfg_reg));

		ADS(ads)->last_cfg_reg = ADS(ads)->expected_cfg_reg;
	}

	ret = 0;

ads101x_set_fs_exit:
	ads101x_unlock(ADS(ads));

	return ret;
}
