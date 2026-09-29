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
#include <stdint.h>
#include <unistd.h>

// clang-format off
static const uint8_t INITIAL_STATE   = 0b00000000;
static const uint8_t SHUTDOWN        = 0b00000100;

#define COMMAND_BYTE_SD                                                     0x80
#define COMMAND_BYTE_SGL                                                    0x80
#define COMMAND_BYTE_DIFF                                                   0x00
#define COMMAND_BYTE_CHANNEL                                                0x70
#define   COMMAND_BYTE_CHANNEL_SHIFT                                           4
#define COMMAND_BYTE_UNI                                                    0x08
#define COMMAND_BYTE_BIP                                                    0x00
// clang-format on

typedef struct {
	plc_i2c_addr_t addr;
	uint8_t bus;
	uint8_t cmd;
} ltc2309_internal_t;

_Static_assert(sizeof(ltc2309_t) == sizeof(ltc2309_internal_t),
	       "Not exactly an ltc2309_internal_t");
_Static_assert(PLC_PERIPHERAL_INTERNAL_ALIGNOF(ltc2309_t) ==
		       PLC_PERIPHERAL_INTERNAL_ALIGNOF(ltc2309_internal_t),
	       "Not aligned exactly as an ltc2309_internal_t");

#define LTC(l) ((ltc2309_internal_t*)(l))

#define LTC2309_LOCK(ltc, timeout_ms) ((void)(ltc), (void)(timeout_ms))

#define LTC2309_UNLOCK(ltc) ((void)(ltc))

int ltc2309_static_init(const i2c_interface_t* i2c,
			ltc2309_t* ltc,
			plc_i2c_addr_t addr)
{
	uint8_t bus;

	if (ltc == NULL || ((uintptr_t)ltc % LTC2309_ALIGN) != 0) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_get_bus(i2c, &bus) != 0) {
		return -1;
	}

	if (i2c_write(i2c, addr, &INITIAL_STATE, sizeof(INITIAL_STATE)) != 1) {
		return -1;
	}

	LTC(ltc)->addr = addr;
	LTC(ltc)->bus = bus;
	LTC(ltc)->cmd = INITIAL_STATE;

	/*
	 * According to the datasheet: When the LTC2309 is properly addressed,
	 * the ADC is released from sleep mode and requires 200ms (tREFWAKE) to
	 * wake up and charge the respective 2.2μF and 10μF bypass capacitors on
	 * the VREF and REFCOMP pins. A new conversion should not be initiated
	 * before this time.
	 *
	 * Assume the chip could be sleeping.
	 */
	usleep(200 * 1000);

	return 0;
}

ltc2309_t* ltc2309_init(const i2c_interface_t* i2c, plc_i2c_addr_t addr)
{
	ltc2309_t* ret = malloc(sizeof(ltc2309_t));

	if (ret == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	if (ltc2309_static_init(i2c, ret, addr) != 0) {
		free(ret);
		return NULL;
	}

	return ret;
}

int ltc2309_static_deinit(const i2c_interface_t* i2c,
			  ltc2309_t* ltc,
			  bool shutdown)
{
	if (ltc == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, LTC(ltc)->bus) != 0) {
		return -1;
	}

	if (shutdown &&
	    i2c_write(i2c, LTC(ltc)->addr, &SHUTDOWN, sizeof(SHUTDOWN)) != 1) {
		return -1;
	}

	return 0;
}

int ltc2309_deinit(const i2c_interface_t* i2c, ltc2309_t* ltc, bool shutdown)
{
	if (ltc2309_static_deinit(i2c, ltc, shutdown) != 0) {
		return -1;
	}

	free(ltc);
	return 0;
}

int ltc2309_protect(const i2c_interface_t* i2c, ltc2309_t* ltc)
{
	(void)i2c;
	(void)ltc;

	errno = ENOTSUP;
	return -1;
}

int ltc2309_unprotect(ltc2309_t* ltc)
{
	(void)ltc;

	errno = ENOTSUP;
	return -1;
}

static inline int16_t ltc2309_conversion_reg_to_value(uint16_t read_value)
{
	uint16_t shifted = read_value >> 4;

	if (read_value & 0x8000) {
		shifted |= 0xF000;
	}

	return (int16_t)shifted;
}

static int ltc2309_read(const i2c_interface_t* i2c,
			ltc2309_t* ltc,
			uint8_t mux_field,
			bool diff,
			bool bip,
			uint16_t* conversion,
			uint32_t timeout_ms)
{
	uint8_t buffer[2];
	int ret;

	if (ltc == NULL || conversion == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (mux_field > 0b111) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, LTC(ltc)->bus) != 0) {
		return -1;
	}

	/*
	 * Per the datasheet's Output Data Format (p.15), the UNI bit picks the
	 * range of every conversion: straight binary when unipolar, 2's
	 * complement when bipolar. It is sent with each command, so every read
	 * can pick its own.
	 */
	uint8_t new_cmd = LTC(ltc)->cmd;
	new_cmd &= ~(COMMAND_BYTE_CHANNEL | COMMAND_BYTE_SD | COMMAND_BYTE_UNI);
	new_cmd |= mux_field << COMMAND_BYTE_CHANNEL_SHIFT;
	new_cmd |= diff ? COMMAND_BYTE_DIFF : COMMAND_BYTE_SGL;
	new_cmd |= bip ? COMMAND_BYTE_BIP : COMMAND_BYTE_UNI;

	LTC2309_LOCK(ltc, timeout_ms);

	if (new_cmd != LTC(ltc)->cmd) {
		if (i2c_write(i2c, LTC(ltc)->addr, &new_cmd, 1) != 1) {
			ret = -1;
			goto ltc2309_read_exit;
		}
		LTC(ltc)->cmd = new_cmd;
		usleep(5); // It must wait 1.8 us minimum before reading
	}

	if (i2c_read(i2c, LTC(ltc)->addr, buffer, 2) != 2) {
		ret = -1;
		goto ltc2309_read_exit;
	}

	*conversion = ((uint16_t)buffer[0] << 8) | buffer[1];
	if ((*conversion & 0x000F) != 0) {
		// Last 4 bits were not 0, invalid conversion
		errno = ERANGE;
		ret = -1;
		goto ltc2309_read_exit;
	}

	ret = 0;

ltc2309_read_exit:
	LTC2309_UNLOCK(ltc);

	return ret;
}

static int ltc2309_read_signed(const i2c_interface_t* i2c,
			       ltc2309_t* ltc,
			       uint8_t mux_field,
			       bool diff,
			       int16_t* read_value,
			       uint32_t timeout_ms)
{
	uint16_t conversion;

	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (ltc2309_read(
		    i2c, ltc, mux_field, diff, true, &conversion, timeout_ms) !=
	    0) {
		return -1;
	}

	*read_value = ltc2309_conversion_reg_to_value(conversion);
	return 0;
}

static int ltc2309_read_unsigned(const i2c_interface_t* i2c,
				 ltc2309_t* ltc,
				 uint8_t mux_field,
				 bool diff,
				 uint16_t* read_value,
				 uint32_t timeout_ms)
{
	uint16_t conversion;

	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (ltc2309_read(
		    i2c, ltc, mux_field, diff, false, &conversion, timeout_ms) !=
	    0) {
		return -1;
	}

	*read_value = conversion >> 4;
	return 0;
}

int ltc2309_read_single_ended_unsigned(const i2c_interface_t* i2c,
				       ltc2309_t* ltc,
				       LTC2309_INPUT index,
				       uint16_t* read_value,
				       uint32_t timeout_ms)
{
	return ltc2309_read_unsigned(
		i2c, ltc, (uint8_t)index, false, read_value, timeout_ms);
}

int ltc2309_read_single_ended_signed(const i2c_interface_t* i2c,
				     ltc2309_t* ltc,
				     LTC2309_INPUT index,
				     int16_t* read_value,
				     uint32_t timeout_ms)
{
	return ltc2309_read_signed(
		i2c, ltc, (uint8_t)index, false, read_value, timeout_ms);
}

int ltc2309_read_differential_unsigned(const i2c_interface_t* i2c,
				       ltc2309_t* ltc,
				       LTC2309_DIFF_INPUT index,
				       uint16_t* read_value,
				       uint32_t timeout_ms)
{
	return ltc2309_read_unsigned(
		i2c, ltc, (uint8_t)index, true, read_value, timeout_ms);
}

int ltc2309_read_differential_signed(const i2c_interface_t* i2c,
				     ltc2309_t* ltc,
				     LTC2309_DIFF_INPUT index,
				     int16_t* read_value,
				     uint32_t timeout_ms)
{
	return ltc2309_read_signed(
		i2c, ltc, (uint8_t)index, true, read_value, timeout_ms);
}
