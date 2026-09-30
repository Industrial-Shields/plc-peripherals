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
#include <stdint.h>

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

typedef struct {
	plc_mutex_t mutex;
	plc_i2c_addr_t addr;
	uint8_t bus;
	uint8_t type;
	bool is_protected;
} mcp230xx_internal_t;

_Static_assert(sizeof(mcp230xx_t) == sizeof(mcp230xx_internal_t),
	       "Not exactly an mcp230xx_internal_t");
_Static_assert(PLC_PERIPHERAL_INTERNAL_ALIGNOF(mcp230xx_t) ==
		       PLC_PERIPHERAL_INTERNAL_ALIGNOF(mcp230xx_internal_t),
	       "Not aligned exactly as an mcp230xx_internal_t");

#define MCP(m) ((mcp230xx_internal_t*)(m))
#define MCP_TYPE(m) ((MCP230XX_TYPE)MCP(m)->type)

#define MCP230XX_RESET_REG(i2c, addr, register_name) \
	i2c_write8_8b(i2c, addr, register_name, register_name##_RESET_VALUE)
#define UINT8T_ARR(arr) arr, sizeof(arr)

#define REG_A(reg, type) ((type) == MCP230XX_017 ? (reg) << 1 : (reg))
#define REG_B(reg, type) ((type) == MCP230XX_017 ? ((reg) << 1) + 1 : (reg) + 1)

static int mcp230xx_lock(mcp230xx_internal_t* mcp, uint32_t timeout_ms)
{
	int saved_errno = errno;

	if (mcp->is_protected &&
	    plc_mutex_acquire(&mcp->mutex, timeout_ms) != 0) {
		if (errno != EOWNERDEAD) {
			return -1;
		}

		// The mutex is held, and there is nothing to recover.
		errno = saved_errno;
	}

	return 0;
}

static void mcp230xx_unlock(mcp230xx_internal_t* mcp)
{
	if (mcp->is_protected) {
		plc_mutex_release(&mcp->mutex);
	}
}

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

int mcp230xx_static_init(const i2c_interface_t* i2c,
			 mcp230xx_t* mcp,
			 plc_i2c_addr_t addr,
			 bool restart,
			 const mcp230xx_config_t* cfg)
{
	uint8_t cfg_reg;
	uint8_t bus;

	if (mcp == NULL || ((uintptr_t)mcp % MCP230XX_ALIGN) != 0 ||
	    cfg == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (!mcp230xx_int_config_is_valid(cfg->int_type, cfg->int_pol) ||
	    (cfg->type != MCP230XX_017 &&
	     cfg->mirror == MCP230XX_MIRRORED_INT)) {
		errno = EINVAL;
		return -1;
	}

	const uint8_t iocon_reg = REG_A(IOCON_REG, cfg->type);

	if (i2c_get_bus(i2c, &bus) != 0) {
		return -1;
	}

	if (restart) {
		if (mcp230xx_reset(i2c, addr, cfg->type) != 0) {
			return -1;
		}
		cfg_reg = IOCON_REG_RESET_VALUE;
	} else {
		if (i2c_read8_8b(i2c, addr, iocon_reg, &cfg_reg) != 0) {
			return -1;
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
		return -1;
	}

	MCP(mcp)->addr = addr;
	MCP(mcp)->bus = bus;
	MCP(mcp)->type = (uint8_t)cfg->type;
	MCP(mcp)->is_protected = false;
	return 0;
}

mcp230xx_t* mcp230xx_init(const i2c_interface_t* i2c,
			  plc_i2c_addr_t addr,
			  bool restart,
			  const mcp230xx_config_t* cfg)
{
	mcp230xx_t* ret = malloc(sizeof(mcp230xx_t));

	if (ret == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	if (mcp230xx_static_init(i2c, ret, addr, restart, cfg) != 0) {
		free(ret);
		return NULL;
	}

	return ret;
}

int mcp230xx_static_deinit(const i2c_interface_t* i2c,
			   mcp230xx_t* mcp,
			   bool restart)
{
	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	if (mcp230xx_unprotect(mcp) < 0) {
		return -1;
	}

	if (restart &&
	    mcp230xx_reset(i2c, MCP(mcp)->addr, MCP_TYPE(mcp)) != 0) {
		return -1;
	}

	return 0;
}

int mcp230xx_deinit(const i2c_interface_t* i2c, mcp230xx_t* mcp, bool restart)
{
	if (mcp230xx_static_deinit(i2c, mcp, restart) != 0) {
		return -1;
	}

	free(mcp);
	return 0;
}

int mcp230xx_protect(const i2c_interface_t* i2c,
		     mcp230xx_t* mcp,
		     plc_mutex_scope_t scope)
{
	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	if (MCP(mcp)->is_protected) {
		return 1;
	}

	if (plc_mutex_static_create(&MCP(mcp)->mutex, scope) != 0) {
		return -1;
	}

	MCP(mcp)->is_protected = true;
	return 0;
}

int mcp230xx_unprotect(mcp230xx_t* mcp)
{
	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (!MCP(mcp)->is_protected) {
		return 1;
	}

	if (plc_mutex_static_destroy(&MCP(mcp)->mutex) != 0) {
		return -1;
	}

	MCP(mcp)->is_protected = false;
	return 0;
}

int mcp230xx_set_input(const i2c_interface_t* i2c,
		       mcp230xx_t* mcp,
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

	if ((MCP_TYPE(mcp) == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (MCP_TYPE(mcp) == MCP230XX_017 && index >= MCP23017_MAX_GPIOS) ||
	    (config != MCP230XX_NO_PULLUP && config != MCP230XX_PULLUP)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	const uint8_t normalized_index = index % MCP23008_MAX_GPIOS;
	const uint8_t pin_mask = 1 << normalized_index;
	const uint8_t iodir_addr = index < MCP23008_MAX_GPIOS ?
					   REG_A(IODIR_REG, MCP_TYPE(mcp)) :
					   REG_B(IODIR_REG, MCP_TYPE(mcp));
	const uint8_t gppu_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(GPPU_REG, MCP_TYPE(mcp)) :
					  REG_B(GPPU_REG, MCP_TYPE(mcp));

	if (mcp230xx_lock(MCP(mcp), timeout_ms) != 0) {
		return -1;
	}

	if (i2c_read8_8b(i2c, MCP(mcp)->addr, iodir_addr, &iodir_reg) != 0 ||
	    i2c_read8_8b(i2c, MCP(mcp)->addr, gppu_addr, &gppu_reg) != 0) {
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
		    i2c_write8_8b(i2c, MCP(mcp)->addr, gppu_addr, gppu_reg) !=
			    0) {
			result = -1;
			goto set_input_error_cleanup;
		}
		if (change_iodir &&
		    i2c_write8_8b(i2c, MCP(mcp)->addr, iodir_addr, iodir_reg) !=
			    0) {
			result = -1;
			goto set_input_error_cleanup;
		}
		result = 0;
	}

set_input_error_cleanup:
	mcp230xx_unlock(MCP(mcp));
	return result;
}

int mcp230xx_read_gpio(const i2c_interface_t* i2c,
		       mcp230xx_t* mcp,
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

	if ((MCP_TYPE(mcp) == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (MCP_TYPE(mcp) == MCP230XX_017 && index >= MCP23017_MAX_GPIOS)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	const uint8_t gpio_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(GPIO_REG, MCP_TYPE(mcp)) :
					  REG_B(GPIO_REG, MCP_TYPE(mcp));
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);

	if (mcp230xx_lock(MCP(mcp), timeout_ms) != 0) {
		return -1;
	}
	result = i2c_read8_8b(i2c, MCP(mcp)->addr, gpio_addr, &gpio_reg);
	mcp230xx_unlock(MCP(mcp));

	if (result != 0) {
		return result;
	}

	*return_value = (gpio_reg & pin_mask) != 0 ? MCP230XX_HIGH :
						     MCP230XX_LOW;
	return 0;
}

int mcp230xx_set_output(const i2c_interface_t* i2c,
			mcp230xx_t* mcp,
			uint8_t index,
			uint32_t timeout_ms)
{
	uint8_t iodir_reg;
	int result;

	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if ((MCP_TYPE(mcp) == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (MCP_TYPE(mcp) == MCP230XX_017 && index >= MCP23017_MAX_GPIOS)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	const uint8_t iodir_addr = index < MCP23008_MAX_GPIOS ?
					   REG_A(IODIR_REG, MCP_TYPE(mcp)) :
					   REG_B(IODIR_REG, MCP_TYPE(mcp));
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);

	if (mcp230xx_lock(MCP(mcp), timeout_ms) != 0) {
		return -1;
	}

	if (i2c_read8_8b(i2c, MCP(mcp)->addr, iodir_addr, &iodir_reg) != 0) {
		result = -1;
		goto set_output_error_cleanup;
	}

	if (!(iodir_reg & pin_mask)) {
		// It's already an output
		result = 1;
		goto set_output_error_cleanup;
	}

	iodir_reg &= ~pin_mask;

	result = i2c_write8_8b(i2c, MCP(mcp)->addr, iodir_addr, iodir_reg);

set_output_error_cleanup:
	mcp230xx_unlock(MCP(mcp));
	return result;
}

int mcp230xx_write_gpio(const i2c_interface_t* i2c,
			mcp230xx_t* mcp,
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

	if ((MCP_TYPE(mcp) == MCP230XX_008 && index >= MCP23008_MAX_GPIOS) ||
	    (MCP_TYPE(mcp) == MCP230XX_017 && index >= MCP23017_MAX_GPIOS)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	const uint8_t olat_addr = index < MCP23008_MAX_GPIOS ?
					  REG_A(OLAT_REG, MCP_TYPE(mcp)) :
					  REG_B(OLAT_REG, MCP_TYPE(mcp));
	const uint8_t pin_mask = 1 << (index % MCP23008_MAX_GPIOS);

	if (mcp230xx_lock(MCP(mcp), timeout_ms) != 0) {
		return -1;
	}

	if (i2c_read8_8b(i2c, MCP(mcp)->addr, olat_addr, &old_olat_reg) != 0) {
		result = -1;
		goto write_gpio_error_cleanup;
	}

	if (to_write) {
		new_olat_reg = old_olat_reg | pin_mask;
	} else {
		new_olat_reg = old_olat_reg & (~pin_mask);
	}

	if (old_olat_reg != new_olat_reg) {
		result = i2c_write8_8b(
			i2c, MCP(mcp)->addr, olat_addr, new_olat_reg);
	} else {
		// The output is already set
		result = 1;
		goto write_gpio_error_cleanup;
	}

write_gpio_error_cleanup:
	mcp230xx_unlock(MCP(mcp));
	return result;
}
