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

/*
 * The functions of this driver assume that both SEQOP and BANK bits are
 * always in it's default state (0).
 */

#include <plc-peripherals-i2c.h>
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
#define   IOCON_REG_MIRROR                             (1 << 6)
#define     IOCON_REG_MIRROR_SHIFT                     6
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
	plc_i2c_addr_t addr;
	uint8_t bus;
	MCP230XX_TYPE type;
};

#define MCP230XX_RESET_REG(i2c, addr, register_name) \
	i2c_write8_8b(i2c, addr, register_name, register_name##_RESET_VALUE)
#define UINT8T_ARR(arr) arr, sizeof(arr)

#define REG_A(reg, type) type == MCP230XX_017 ? reg << 1 : reg
#define REG_B(reg, type) type == MCP230XX_017 ? (reg << 1) + 1 : reg + 1

#define MCP230XX_LOCK(mcp, timeout_ms) ((void)(mcp), (void)(timeout_ms))

#define MCP230XX_UNLOCK(mcp) ((void)(mcp))

static int mcp230xx_reset(const i2c_interface_t* i2c,
			  plc_i2c_addr_t addr,
			  MCP230XX_TYPE type)
{
	// First 0x00 is the register address
	static const uint8_t reset_mcp23008[] = {
		0x00, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};
	// First 0x00 is the register address
	static const uint8_t reset_mcp23017[] = {
		0x00, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0,	  0, 0, 0, 0, 0, 0, 0, 0,
	};
	ssize_t bytes_written;

	if (type == MCP230XX_017) {
		bytes_written =
			i2c_write(i2c, addr, UINT8T_ARR(reset_mcp23017));
		if (bytes_written != sizeof(reset_mcp23017)) {
			return -1;
		}
	} else {
		bytes_written =
			i2c_write(i2c, addr, UINT8T_ARR(reset_mcp23008));
		if (bytes_written != sizeof(reset_mcp23008)) {
			return -1;
		}
	}

	return 0;
}

static bool mcp230xx_int_config_is_valid(MCP230XX_INT_TYPE int_type,
					 MCP230XX_INT_POLARITY int_pol)
{
	switch (int_type) {
	case MCP230XX_OPEN_DRAIN_INT:
		return int_pol == MCP230XX_INT_POLARITY_NONE;
	case MCP230XX_ACTIVE_DRIVER_INT:
		// The polarity must be a conscious decision, not a default.
		return int_pol == MCP230XX_INT_ACTIVE_HIGH ||
		       int_pol == MCP230XX_INT_ACTIVE_LOW;
	default:
		return false;
	}
}

mcp230xx_t* mcp230xx_init(const i2c_interface_t* i2c,
			  plc_i2c_addr_t addr,
			  bool restart,
			  const mcp230xx_config_t* cfg)
{
	mcp230xx_t* ret;
	uint8_t cfg_reg;
	uint8_t bus;

	if (cfg == NULL) {
		errno = EFAULT;
		return NULL;
	}

	if (!mcp230xx_int_config_is_valid(cfg->int_type, cfg->int_pol) ||
	    (cfg->type != MCP230XX_017 &&
	     cfg->mirror == MCP230XX_MIRRORED_INT)) {
		errno = EINVAL;
		return NULL;
	}

	const uint8_t iocon_reg = REG_A(IOCON_REG, cfg->type);

	if (i2c_get_bus(i2c, &bus) != 0) {
		return NULL;
	}

	ret = malloc(sizeof(struct _mcp230xx_t));
	if (ret == NULL) {
		return NULL;
	}

	if (restart) {
		int result = mcp230xx_reset(i2c, addr, cfg->type);
		if (result != 0) {
			goto init_error_cleanup;
		}
		cfg_reg = IOCON_REG_RESET_VALUE;
	} else {
		if (i2c_read8_8b(i2c, addr, iocon_reg, &cfg_reg) != 0) {
			goto init_error_cleanup;
		}
	}

	// Enable / Disable I2C slew rate
	cfg_reg &= ~IOCON_REG_DISSLW;
	cfg_reg |= (cfg->disable_slew_rate ? 1 : 0) << IOCON_REG_DISSLW_SHIFT;

	// Set the type of the interrupt output
	cfg_reg &= ~IOCON_REG_ODR;
	cfg_reg |= cfg->int_type << IOCON_REG_ODR_SHIFT;

	// Set the interrupt polarity
	cfg_reg &= ~IOCON_REG_INTPOL; // Always clear the bit
	if (cfg->int_pol != MCP230XX_INT_POLARITY_NONE) {
		cfg_reg |= cfg->int_pol << IOCON_REG_INTPOL_SHIFT;
	}

	// Set mirror byte
	cfg_reg &= ~IOCON_REG_MIRROR;
	cfg_reg |= cfg->mirror << IOCON_REG_MIRROR_SHIFT;

	if (i2c_write8_8b(i2c, addr, iocon_reg, cfg_reg) != 0) {
		goto init_error_cleanup;
	}

	ret->addr = addr;
	ret->bus = bus;
	ret->type = cfg->type;
	return ret;

init_error_cleanup:
	free(ret);
	return NULL;
}

int mcp230xx_deinit(const i2c_interface_t* i2c, mcp230xx_t* mcp, bool restart)
{
	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, mcp->bus) != 0) {
		return -1;
	}

	if (restart) {
		int result = mcp230xx_reset(i2c, mcp->addr, mcp->type);
		if (result != 0) {
			return result;
		}
	}

	free(mcp);
	return 0;
}

int mcp230xx_protect(const i2c_interface_t* i2c, mcp230xx_t* mcp)
{
	(void)i2c;
	(void)mcp;

	errno = ENOTSUP;
	return -1;
}

int mcp230xx_unprotect(mcp230xx_t* mcp)
{
	(void)mcp;

	errno = ENOTSUP;
	return -1;
}

int mcp230xx_set_input(const i2c_interface_t* i2c,
		       const mcp230xx_t* mcp,
		       uint8_t index,
		       MCP230XX_INPUT_CONFIG config,
		       uint32_t timeout_ms)
{
	bool change_iodir = false, change_gppu = false;
	uint8_t iodir_reg, gppu_reg;
	int result;

	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if ((mcp->type == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (mcp->type == MCP230XX_017 && index >= MCP23017_MAX_GPIOS) ||
	    (config != MCP230XX_NO_PULLUP && config != MCP230XX_PULLUP)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, mcp->bus) != 0) {
		return -1;
	}

	const uint8_t normalized_index = index % MCP23008_MAX_GPIOS;
	const uint8_t pin_mask = 1 << normalized_index;
	const uint8_t iodir_addr = index < MCP23008_MAX_GPIOS ?
					   REG_A(IODIR_REG, mcp->type) :
					   REG_B(IODIR_REG, mcp->type);
	const uint8_t gppu_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(GPPU_REG, mcp->type) :
					  REG_B(GPPU_REG, mcp->type);

	MCP230XX_LOCK(mcp, timeout_ms);

	if (i2c_read8_8b(i2c, mcp->addr, iodir_addr, &iodir_reg) != 0 ||
	    i2c_read8_8b(i2c, mcp->addr, gppu_addr, &gppu_reg) != 0) {
		result = -1;
		goto set_input_error_cleanup;
	}

	if (!(iodir_reg & pin_mask)) {
		// It's not an input
		iodir_reg |= pin_mask;
		change_iodir = true;
	}

	if ((gppu_reg & pin_mask) != (config << normalized_index)) {
		if (config == MCP230XX_NO_PULLUP) {
			gppu_reg &= ~pin_mask;
		} else {
			gppu_reg |= pin_mask;
		}
		change_gppu = true;
	}

	if (!change_iodir && !change_gppu) {
		result = 1;
	} else {
		/*
		 * Update the GPPU before the IODIR register, ensure no
		 * accidental pull-ups.
		 */
		if (change_gppu &&
		    i2c_write8_8b(i2c, mcp->addr, gppu_addr, gppu_reg) != 0) {
			result = -1;
			goto set_input_error_cleanup;
		}
		if (change_iodir &&
		    i2c_write8_8b(i2c, mcp->addr, iodir_addr, iodir_reg) != 0) {
			result = -1;
			goto set_input_error_cleanup;
		}
		result = 0;
	}

set_input_error_cleanup:
	MCP230XX_UNLOCK(mcp);
	return result;
}

int mcp230xx_read_gpio(const i2c_interface_t* i2c,
		       const mcp230xx_t* mcp,
		       uint8_t index,
		       uint8_t* return_value,
		       uint32_t timeout_ms)
{
	uint8_t gpio_reg;
	int result;

	if (mcp == NULL || return_value == NULL) {
		errno = EFAULT;
		return -1;
	}

	if ((mcp->type == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (mcp->type == MCP230XX_017 && index >= MCP23017_MAX_GPIOS)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, mcp->bus) != 0) {
		return -1;
	}

	const uint8_t gpio_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(GPIO_REG, mcp->type) :
					  REG_B(GPIO_REG, mcp->type);
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);

	MCP230XX_LOCK(mcp, timeout_ms);
	result = i2c_read8_8b(i2c, mcp->addr, gpio_addr, &gpio_reg);
	MCP230XX_UNLOCK(mcp);

	if (result != 0) {
		return result;
	}

	*return_value = (gpio_reg & pin_mask) != 0 ? MCP230XX_HIGH :
						     MCP230XX_LOW;
	return 0;
}

int mcp230xx_set_output(const i2c_interface_t* i2c,
			const mcp230xx_t* mcp,
			uint8_t index,
			uint32_t timeout_ms)
{
	uint8_t iodir_reg;
	int result;

	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if ((mcp->type == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (mcp->type == MCP230XX_017 && index >= MCP23017_MAX_GPIOS)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, mcp->bus) != 0) {
		return -1;
	}

	const uint8_t iodir_addr = index < MCP23008_MAX_GPIOS ?
					   REG_A(IODIR_REG, mcp->type) :
					   REG_B(IODIR_REG, mcp->type);
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);

	MCP230XX_LOCK(mcp, timeout_ms);

	if (i2c_read8_8b(i2c, mcp->addr, iodir_addr, &iodir_reg) != 0) {
		result = -1;
		goto set_output_error_cleanup;
	}

	if (!(iodir_reg & pin_mask)) {
		// It's already an output
		result = 1;
		goto set_output_error_cleanup;
	}

	iodir_reg &= ~pin_mask;

	result = i2c_write8_8b(i2c, mcp->addr, iodir_addr, iodir_reg);

set_output_error_cleanup:
	MCP230XX_UNLOCK(mcp);
	return result;
}

int mcp230xx_write_gpio(const i2c_interface_t* i2c,
			const mcp230xx_t* mcp,
			uint8_t index,
			uint8_t to_write,
			uint32_t timeout_ms)
{
	uint8_t old_olat_reg, new_olat_reg;
	int result;

	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if ((mcp->type == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (mcp->type == MCP230XX_017 && index >= MCP23017_MAX_GPIOS)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, mcp->bus) != 0) {
		return -1;
	}

	const uint8_t olat_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(OLAT_REG, mcp->type) :
					  REG_B(OLAT_REG, mcp->type);
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);

	MCP230XX_LOCK(mcp, timeout_ms);

	if (i2c_read8_8b(i2c, mcp->addr, olat_addr, &old_olat_reg) != 0) {
		result = -1;
		goto write_gpio_error_cleanup;
	}

	if (to_write) {
		new_olat_reg = old_olat_reg | pin_mask;
	} else {
		new_olat_reg = old_olat_reg & (~pin_mask);
	}

	if (old_olat_reg != new_olat_reg) {
		result = i2c_write8_8b(i2c, mcp->addr, olat_addr, new_olat_reg);
	} else {
		// The output is already set
		result = 1;
		goto write_gpio_error_cleanup;
	}

write_gpio_error_cleanup:
	MCP230XX_UNLOCK(mcp);
	return result;
}
