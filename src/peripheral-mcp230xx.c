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
#include <plc-resource-protector.h>
#include <peripheral-mcp230xx.h>

#include <malloc.h>
#include <errno.h>

// clang-format off
#define IODIR_REG                                      0x00
#define IPOL_REG                                       0x01
#define GPINTEN_REG                                    0x02
#define DEFVAL_REG                                     0x03
#define INTCON_REG                                     0x04
#define IOCON_REG                                      0x05
#define   IOCON_REG_DISSLW                             (1 << 4)
#define     IOCON_REG_DISSLW_SHIFT                     4
#define   IOCON_REG_ODR                                (1 << 2)
#define     IOCON_REG_ODR_SHIFT                        2
#define   IOCON_REG_INTPOL                             (1 << 1)
#define     IOCON_REG_INTPOL_SHIFT                     1
#define   IOCON_REG_RESET_VALUE                        0b00000000
#define GPPU_REG                                       0x06
#define INTF_REG                                       0x07
#define INTCAP_REG                                     0x08
#define GPIO_REG                                       0x09
#define OLAT_REG                                       0x0A
// clang-format on

struct _mcp230xx_t {
	i2c_interface_t* i2c;
	plc_i2c_addr_t addr;
};

#define MCP230XX_RESET_REG(i2c, addr, register_name) \
	i2c_write8_8b(i2c, addr, register_name, register_name##_RESET_VALUE)
#define PASS_MCP(mcp) mcp->i2c, mcp->addr
#define UINT8T_ARR(arr) arr, sizeof(arr)
#include <stdio.h>
static int mcp230xx_reset(i2c_interface_t* i2c, plc_i2c_addr_t addr)
{
	// First 0x00 is the register address
	static const uint8_t reset_mcp23008[] = {
		0x00, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	// Assume SEQOP is enabled
	ssize_t bytes_written =
		i2c_write(i2c, addr, UINT8T_ARR(reset_mcp23008));
	if (bytes_written != sizeof(reset_mcp23008)) {
		return -1;
	}

	return 0;
}

mcp230xx_t* mcp230xx_init(i2c_interface_t* i2c,
			  plc_i2c_addr_t addr,
			  bool restart,
			  MCP230XX_TYPE type,
			  bool disable_slew_rate,
			  MCP230XX_INT_TYPE int_type,
			  MCP230XX_INT_POLARITY int_pol)
{
	uint8_t cfg_reg;
	mcp230xx_t* ret;

	if (type != MCP230XX_008 ||
	    // Invalid configuration
	    (int_type == MCP230XX_OPEN_DRAIN_INT &&
	     int_pol != MCP230XX_INT_POLARITY_NONE)) {
		errno = EINVAL;
		return NULL;
	}

	ret = malloc(sizeof(struct _mcp230xx_t));
	if (ret == NULL) {
		return NULL;
	}

	if (restart) {
		int result = mcp230xx_reset(i2c, addr);
		if (result != 0) {
			goto init_error_cleanup;
		}
		cfg_reg = IOCON_REG_RESET_VALUE;
	} else {
		if (i2c_read8_8b(i2c, addr, IOCON_REG, &cfg_reg) != 0) {
			goto init_error_cleanup;
		}
	}

	// Enable / Disable I2C slew rate
	cfg_reg &= ~IOCON_REG_DISSLW;
	cfg_reg |= (disable_slew_rate ? 1 : 0) << IOCON_REG_DISSLW_SHIFT;

	// Set the type of the interrupt output
	cfg_reg &= ~IOCON_REG_ODR;
	cfg_reg |= int_type << IOCON_REG_ODR_SHIFT;

	// Set the interrupt polarity
	cfg_reg &= ~IOCON_REG_INTPOL; // Always clear the bit
	if (int_pol != MCP230XX_INT_POLARITY_NONE) {
		cfg_reg |= int_pol << IOCON_REG_INTPOL_SHIFT;
	}

	if (i2c_write8_8b(i2c, addr, IOCON_REG, cfg_reg) != 0) {
		goto init_error_cleanup;
	}

	ret->i2c = i2c;
	ret->addr = addr;
	return ret;

init_error_cleanup:
	free(ret);
	return NULL;
}

int mcp230xx_deinit(mcp230xx_t* mcp, bool restart)
{
	if (restart) {
		int result = mcp230xx_reset(PASS_MCP(mcp));
		if (result != 0) {
			return result;
		}
	}

	free(mcp);
	return 0;
}
