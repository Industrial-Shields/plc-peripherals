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

#define LTC2309_IS_BIPOLAR(ltc) (!((ltc)->cmd & COMMAND_BYTE_UNI))

struct _ltc2309_t {
	i2c_interface_t* i2c;
	plc_i2c_addr_t addr;
	uint8_t cmd;
};

ltc2309_t* ltc2309_init(i2c_interface_t* i2c, plc_i2c_addr_t addr, bool bip)
{
	if (i2c == NULL) {
		errno = EFAULT;
		return NULL;
	}

	ltc2309_t* ret = malloc(sizeof(struct _ltc2309_t));
	if (ret == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	ret->i2c = i2c;
	ret->addr = addr;
	ret->cmd = INITIAL_STATE | (bip ? COMMAND_BYTE_BIP : COMMAND_BYTE_UNI);

	if (i2c_write(i2c, addr, &INITIAL_STATE, sizeof(INITIAL_STATE)) != 1) {
		free(ret);
		return NULL;
	}

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

	return ret;
}

int ltc2309_deinit(ltc2309_t* ltc, bool shutdown)
{
	if (ltc == NULL || ltc->i2c == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (shutdown &&
	    i2c_write(PASS_LTC(ltc), &SHUTDOWN, sizeof(SHUTDOWN)) != 1) {
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
	if (mux_field > 0b111) {
		errno = EINVAL;
		return -1;
	}

	uint8_t diff_value = diff ? COMMAND_BYTE_DIFF : COMMAND_BYTE_SGL;
	uint8_t new_mux = mux_field << COMMAND_BYTE_CHANNEL_SHIFT;
	uint8_t new_cmd = ltc->cmd;

	new_cmd &= (~COMMAND_BYTE_CHANNEL) & (~COMMAND_BYTE_SD);
	new_cmd |= new_mux | diff_value;
	if (new_cmd != ltc->cmd) {
		if (i2c_write(PASS_LTC(ltc), &new_cmd, 1) != 1) {
			return -1;
		}
		ltc->cmd = new_cmd;
		usleep(5); // It must wait 1.8 us minimum before reading
	}

	uint8_t buffer[2];
	if (i2c_read(PASS_LTC(ltc), buffer, 2) != 2) {
		return -1;
	}

	*conversion = ((uint16_t)buffer[0] << 8) | buffer[1];
	if ((*conversion & 0x000F) != 0) {
		// Last 4 bits were not 0, invalid conversion
		errno = ERANGE;
		return -1;
	}

	return 0;
}

int ltc2309_read_signed(ltc2309_t* ltc,
			LTC2309_DIFF_INPUT index,
			int16_t* read_value)
{
	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}

#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (!LTC2309_IS_BIPOLAR(ltc)) {
		/*
		 * Per the datasheet's Output Data Format (p.15): the conversion
		 * result is 2's complement only when the UNI bit selects bipolar
		 * range.
		 */
		errno = EINVAL;
		return -1;
	}
#endif

	uint16_t conversion;
	if (ltc2309_read(ltc, (uint8_t)index, &conversion, true) != 0) {
		return -1;
	}

	*read_value = ltc2309_conversion_reg_to_value(conversion);
	return 0;
}

int ltc2309_read_unsigned(ltc2309_t* ltc,
			  LTC2309_INPUT index,
			  uint16_t* read_value)
{
	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}

#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (LTC2309_IS_BIPOLAR(ltc)) {
		/*
		 * Per the datasheet's Output Data Format (p.15): the conversion
		 * result is straight binary only when the UNI bit selects
		 * unipolar range.
		 */
		errno = EINVAL;
		return -1;
	}
#endif

	if (ltc2309_read(ltc, (uint8_t)index, read_value, false) != 0) {
		return -1;
	}

	*read_value >>= 4;
	return 0;
}
