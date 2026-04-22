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
	plc_resource_t cached_resource;
	bool is_protected;
	MCP230XX_TYPE type;
};

#define MCP230XX_RESET_REG(i2c, addr, register_name) \
	i2c_write8_8b(i2c, addr, register_name, register_name##_RESET_VALUE)
#define PASS_MCP(mcp) mcp->i2c, mcp->addr
#define UINT8T_ARR(arr) arr, sizeof(arr)

#define REG_A(reg, type) type == MCP230XX_017 ? reg << 1 : reg
#define REG_B(reg, type) type == MCP230XX_017 ? (reg << 1) + 1 : reg + 1

#define MCP230XX_LOCK(mcp, timeout_ms)                                \
	do {                                                          \
		if ((mcp)->is_protected) {                            \
			if (plc_resource_lock((mcp)->cached_resource, \
					      (timeout_ms)) != 0) {   \
				return -1;                            \
			}                                             \
		}                                                     \
	} while (0)

#define MCP230XX_UNLOCK(mcp)                                       \
	do {                                                       \
		if (mcp->is_protected) {                           \
			plc_resource_unlock(mcp->cached_resource); \
		}                                                  \
	} while (0)

static int
mcp230xx_reset(i2c_interface_t* i2c, plc_i2c_addr_t addr, MCP230XX_TYPE type)
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

mcp230xx_t* mcp230xx_init(i2c_interface_t* i2c,
			  plc_i2c_addr_t addr,
			  bool restart,
			  MCP230XX_TYPE type,
			  bool disable_slew_rate,
			  MCP230XX_INT_TYPE int_type,
			  MCP230XX_INT_POLARITY int_pol)
{
	uint8_t iocon_reg = REG_A(IOCON_REG, type);
	uint8_t cfg_reg;
	mcp230xx_t* ret;

	if (int_type == MCP230XX_OPEN_DRAIN_INT &&
	    int_pol != MCP230XX_INT_POLARITY_NONE) {
		errno = EINVAL;
		return NULL;
	}

	ret = malloc(sizeof(struct _mcp230xx_t));
	if (ret == NULL) {
		return NULL;
	}

	if (restart) {
		int result = mcp230xx_reset(i2c, addr, type);
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
	cfg_reg |= (disable_slew_rate ? 1 : 0) << IOCON_REG_DISSLW_SHIFT;

	// Set the type of the interrupt output
	cfg_reg &= ~IOCON_REG_ODR;
	cfg_reg |= int_type << IOCON_REG_ODR_SHIFT;

	// Set the interrupt polarity
	cfg_reg &= ~IOCON_REG_INTPOL; // Always clear the bit
	if (int_pol != MCP230XX_INT_POLARITY_NONE) {
		cfg_reg |= int_pol << IOCON_REG_INTPOL_SHIFT;
	}

	if (i2c_write8_8b(i2c, addr, iocon_reg, cfg_reg) != 0) {
		goto init_error_cleanup;
	}

	ret->i2c = i2c;
	ret->addr = addr;
	ret->is_protected = false;
	ret->type = type;
	return ret;

init_error_cleanup:
	free(ret);
	return NULL;
}

int mcp230xx_deinit(mcp230xx_t* mcp, bool restart)
{
	if (restart) {
		int result = mcp230xx_reset(PASS_MCP(mcp), mcp->type);
		if (result != 0) {
			return result;
		}
	}

	if (mcp->is_protected) {
		int ret = mcp230xx_unprotect(mcp);
		if (ret < 0) {
			return ret;
		}
	}

	free(mcp);
	return 0;
}

int mcp230xx_protect(mcp230xx_t* mcp)
{
	if (mcp == NULL) {
		errno = EINVAL;
		return -1;
	}

	plc_resource_t res = I2C_RESOURCE(mcp->addr);
	int result = plc_resource_add(res);
	if (result >= 0) {
		mcp->is_protected = true;
		mcp->cached_resource = res;
	}

	return result;
}

int mcp230xx_unprotect(mcp230xx_t* mcp)
{
	if (mcp == NULL) {
		errno = EINVAL;
		return -1;
	}

	int result = plc_resource_remove(mcp->cached_resource);
	if (result >= 0) {
		mcp->is_protected = false;
	}

	return result;
}

#if !defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
static __attribute__((unused)) int check_arguments(mcp230xx_t* mcp,
						   uint8_t index)
#else
static int check_arguments(mcp230xx_t* mcp, uint8_t index)
#endif
{
	if (mcp == NULL ||
	    (mcp->type == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (mcp->type == MCP230XX_017 && index >= MCP23017_MAX_GPIOS)) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

int mcp230xx_set_input(mcp230xx_t* mcp,
		       uint8_t index,
		       MCP230XX_INPUT_CONFIG config,
		       uint32_t timeout_ms)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	int _check = check_arguments(mcp, index);
	if (_check != 0) {
		return _check;
	}

	const uint8_t normalized_config = config == MCP230XX_NO_PULLUP ?
						  MCP230XX_NO_PULLUP :
						  MCP230XX_PULLUP;
#else
	const uint8_t normalized_config = config;
#endif

	const uint8_t normalized_index = index % MCP23008_MAX_GPIOS;
	const uint8_t pin_mask = 1 << normalized_index;
	const uint8_t iodir_addr = index < MCP23008_MAX_GPIOS ?
					   REG_A(IODIR_REG, mcp->type) :
					   REG_B(IODIR_REG, mcp->type);
	const uint8_t gppu_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(GPPU_REG, mcp->type) :
					  REG_B(GPPU_REG, mcp->type);
	bool change_iodir = false, change_gppu = false;
	uint8_t iodir_reg, gppu_reg;
	int result;

	MCP230XX_LOCK(mcp, timeout_ms);

	if (i2c_read8_8b(PASS_MCP(mcp), iodir_addr, &iodir_reg) != 0 ||
	    i2c_read8_8b(PASS_MCP(mcp), gppu_addr, &gppu_reg) != 0) {
		result = -1;
		goto set_input_error_cleanup;
	}

	if (!(iodir_reg & pin_mask)) {
		// It's not an input
		iodir_reg |= pin_mask;
		change_iodir = true;
	}

	if ((gppu_reg & pin_mask) != (normalized_config << normalized_index)) {
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
		if (change_gppu) {
			result = i2c_write8_8b(
				PASS_MCP(mcp), gppu_addr, gppu_reg);
			if (result != 0) {
				goto set_input_error_cleanup;
			}
		}
		if (change_iodir) {
			result = i2c_write8_8b(
				PASS_MCP(mcp), iodir_addr, iodir_reg);
			if (result != 0) {
				goto set_input_error_cleanup;
			}
		}
	}

set_input_error_cleanup:
	MCP230XX_UNLOCK(mcp);
	return result;
}

int mcp230xx_read_gpio(mcp230xx_t* mcp,
		       uint8_t index,
		       uint8_t* return_value,
		       uint32_t timeout_ms)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	int _check = check_arguments(mcp, index);
	if (_check != 0) {
		return _check;
	}
#endif

	const uint8_t gpio_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(GPIO_REG, mcp->type) :
					  REG_B(GPIO_REG, mcp->type);
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);
	uint8_t gpio_reg;
	int result;

	MCP230XX_LOCK(mcp, timeout_ms);
	result = i2c_read8_8b(PASS_MCP(mcp), gpio_addr, &gpio_reg);
	MCP230XX_UNLOCK(mcp);

	*return_value = (gpio_reg & pin_mask) != 0 ? MCP230XX_HIGH :
						     MCP230XX_LOW;
	return result;
}

int mcp230xx_set_output(mcp230xx_t* mcp, uint8_t index, uint32_t timeout_ms)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	int _check = check_arguments(mcp, index);
	if (_check != 0) {
		return _check;
	}
#endif

	const uint8_t iodir_addr = index < MCP23008_MAX_GPIOS ?
					   REG_A(IODIR_REG, mcp->type) :
					   REG_B(IODIR_REG, mcp->type);
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);
	uint8_t iodir_reg;
	int result;

	MCP230XX_LOCK(mcp, timeout_ms);

	if (i2c_read8_8b(PASS_MCP(mcp), iodir_addr, &iodir_reg) != 0) {
		result = -1;
		goto set_output_error_cleanup;
	}

	if (!(iodir_reg & pin_mask)) {
		// It's already an output
		result = 1;
		goto set_output_error_cleanup;
	}

	iodir_reg &= ~pin_mask;

	result = i2c_write8_8b(PASS_MCP(mcp), iodir_addr, iodir_reg);

set_output_error_cleanup:
	MCP230XX_UNLOCK(mcp);
	return result;
}

int mcp230xx_write_gpio(mcp230xx_t* mcp,
			uint8_t index,
			uint8_t to_write,
			uint32_t timeout_ms)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	int _check = check_arguments(mcp, index);
	if (_check != 0) {
		return _check;
	}
#endif

	const uint8_t olat_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(OLAT_REG, mcp->type) :
					  REG_B(OLAT_REG, mcp->type);
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);
	uint8_t old_olat_reg, new_olat_reg;
	int result;

	MCP230XX_LOCK(mcp, timeout_ms);

	if (i2c_read8_8b(PASS_MCP(mcp), olat_addr, &old_olat_reg) != 0) {
		result = -1;
		goto write_gpio_error_cleanup;
	}

	if (to_write) {
		new_olat_reg = old_olat_reg | pin_mask;
	} else {
		new_olat_reg = old_olat_reg & (~pin_mask);
	}

	if (old_olat_reg != new_olat_reg) {
		result = i2c_write8_8b(PASS_MCP(mcp), olat_addr, new_olat_reg);
	} else {
		// The output is already set
		result = 1;
		goto write_gpio_error_cleanup;
	}

write_gpio_error_cleanup:
	MCP230XX_UNLOCK(mcp);
	return result;
}
