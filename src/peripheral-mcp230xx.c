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
 * mcp230xx_static_init leaves IOCON.BANK and IOCON.SEQOP cleared, and every
 * other function relies on them staying cleared.
 */

#include <plc-peripherals-i2c.h>
#include <peripheral-mcp230xx.h>

#include <assert.h>
#include <malloc.h>
#include <errno.h>
#include <stdint.h>

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
// ESP-IDF's soc/gpio_reg.h has a GPIO_REG(io_num), avoid name collisions
#undef GPIO_REG
#endif

// clang-format off
#define IODIR_REG                                      0x00
#define IPOL_REG                                       0x01
#define GPINTEN_REG                                    0x02
#define DEFVAL_REG                                     0x03
#define INTCON_REG                                     0x04
#define IOCON_REG                                      0x05
#define   IOCON_REG_BANK                               (1 << 7)
#define     IOCON_REG_BANK_SHIFT                       7
#define   IOCON_REG_MIRROR                             (1 << 6)
#define     IOCON_REG_MIRROR_SHIFT                     6
#define   IOCON_REG_SEQOP                              (1 << 5)
#define     IOCON_REG_SEQOP_SHIFT                      5
#define   IOCON_REG_DISSLW                             (1 << 4)
#define     IOCON_REG_DISSLW_SHIFT                     4
#define   IOCON_REG_HAEN                               (1 << 3)
#define     IOCON_REG_HAEN_SHIFT                       3
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

#define GPIO_REG_ADDR(mcp, reg, index)                              \
	((index) < MCP23008_MAX_GPIOS ? REG_A(reg, MCP_TYPE(mcp)) : \
					REG_B(reg, MCP_TYPE(mcp)))
#define GPIO_PIN_MASK(index) ((uint8_t)(1 << ((index) % MCP23008_MAX_GPIOS)))

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

// Check that the MCP230XX has GPIO "index". The caller checks mcp for NULL.
static int mcp230xx_check_gpio(const mcp230xx_t* mcp, uint8_t index)
{
	assert(mcp != NULL);

	const uint8_t max_gpios = MCP_TYPE(mcp) == MCP230XX_017 ?
					  MCP23017_MAX_GPIOS :
					  MCP23008_MAX_GPIOS;
	if (index >= max_gpios) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * Set (or clear) the bits of "mask" in register "reg_addr", only writing it if
 * they change. The caller must hold the lock before calling it (if protected).
 *
 * Returns 0 if written, 1 if the bits already were like that, -1 on error.
 */
static int mcp230xx_update_bits(const i2c_interface_t* i2c,
				const mcp230xx_t* mcp,
				uint8_t reg_addr,
				uint8_t mask,
				bool set)
{
	uint8_t reg;

	if (i2c_read8_8b(i2c, MCP(mcp)->addr, reg_addr, &reg) != 0) {
		return -1;
	}

	const uint8_t new_reg = set ? reg | mask : reg & ~mask;
	if (new_reg == reg) {
		return 1;
	}

	return i2c_write8_8b(i2c, MCP(mcp)->addr, reg_addr, new_reg);
}

static bool mcp230xx_config_is_valid(const mcp230xx_config_t* cfg)
{
	// First of all, check that the enum values are valid

	if (cfg->type != MCP230XX_008 && cfg->type != MCP230XX_017) {
		return false;
	}

	if (cfg->mirror != MCP230XX_NO_MIRRORED_INT &&
	    cfg->mirror != MCP230XX_MIRRORED_INT) {
		return false;
	}

	if (cfg->int_type != MCP230XX_OPEN_DRAIN_INT &&
	    cfg->int_type != MCP230XX_ACTIVE_DRIVER_INT) {
		return false;
	}

	if (cfg->int_pol != MCP230XX_INT_POLARITY_NONE &&
	    cfg->int_pol != MCP230XX_INT_ACTIVE_HIGH &&
	    cfg->int_pol != MCP230XX_INT_ACTIVE_LOW) {
		return false;
	}

	// If it's a MCP230XX_017, it cannot have mirrored INT
	if (cfg->type != MCP230XX_017 && cfg->mirror == MCP230XX_MIRRORED_INT) {
		return false;
	}

	/*
	 * The polarity must be a conscious decision, and not all combinations
	 * are valid.
	 */
	if (cfg->int_type == MCP230XX_OPEN_DRAIN_INT &&
	    cfg->int_pol != MCP230XX_INT_POLARITY_NONE) {
		return false;
	}
	if (cfg->int_type == MCP230XX_ACTIVE_DRIVER_INT &&
	    cfg->int_pol == MCP230XX_INT_POLARITY_NONE) {
		return false;
	}

	return true;
}

static int mcp23017_ensure_bank_state(const i2c_interface_t* i2c,
				      plc_i2c_addr_t addr)
{
	/*
	 * Ensure IOCON.BANK = 0. If it is 1 right now, IOCON can't be reached
	 * through REG_A, since REG_A/B assume IOCON.BANK = 0 addressing.
	 *
	 * 0x0B is IOCON with IOCON.BANK = 0, and maps to no register with
	 * IOCON.BANK = 1 (DS20001952C, Table 3-1). An unmapped address reads
	 * as 0x00 (not in the datasheet, checked on hardware), and a write to
	 * it is ignored (not in the datasheet, checked on hardware).
	 * So:
	 *
	 *   1. Read 0x0B. If it isn't 0x00, it is IOCON, so IOCON.BANK is
	 *      already 0.
	 *   2. Otherwise, write HAEN to 0x0B as a canary and read it back. If
	 *      it reads back, IOCON.BANK is 0. IOCON was 0x00, so only HAEN
	 *      changed.
	 *   3. Otherwise, IOCON.BANK is 1, and 0x05 is IOCON. Clear IOCON.BANK
	 *      through it, keeping its other bits.
	 *
	 * HAEN is the canary because it does nothing on the MCP23017 (address
	 * pins are always enabled, Register 3-6, Note 1), so it can stay set.
	 *
	 * Every write is a single-byte write, as the datasheet advises when
	 * changing IOCON.BANK (section 3.5.6). That way it can work whatever
	 * IOCON.SEQOP is.
	 */
	const uint8_t maybe_iocon_reg = 0x0B, bank_1_iocon_reg = 0x05;
	uint8_t maybe_cfg_reg, bank_1_cfg_reg;

	if (i2c_read8_8b(i2c, addr, maybe_iocon_reg, &maybe_cfg_reg) != 0) {
		return -1;
	}

	if (maybe_cfg_reg != 0) {
		// Only IOCON reads non-zero here. IOCON.BANK must be 0.
		assert((maybe_cfg_reg & IOCON_REG_BANK) == 0);
		return 0;
	}

	maybe_cfg_reg |= IOCON_REG_HAEN;
	if (i2c_write8_8b(i2c, addr, maybe_iocon_reg, maybe_cfg_reg) != 0) {
		return -1;
	}

	if (i2c_read8_8b(i2c, addr, maybe_iocon_reg, &maybe_cfg_reg) != 0) {
		return -1;
	}

	if (maybe_cfg_reg != 0) {
		// The canary read back, so IOCON.BANK must be 0.
		assert((maybe_cfg_reg & IOCON_REG_BANK) == 0);
		return 0;
	}

	// IOCON.BANK is 1, so we need to clear the bit
	if (i2c_read8_8b(i2c, addr, bank_1_iocon_reg, &bank_1_cfg_reg) != 0) {
		return -1;
	}
	bank_1_cfg_reg &= ~(IOCON_REG_BANK);
	if (i2c_write8_8b(i2c, addr, bank_1_iocon_reg, bank_1_cfg_reg) != 0) {
		return -1;
	}

	// IOCON.BANK is 0 now.
	return 0;
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

	if (!mcp230xx_config_is_valid(cfg)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_get_bus(i2c, &bus) != 0) {
		return -1;
	}

	if (cfg->type == MCP230XX_017 &&
	    mcp23017_ensure_bank_state(i2c, addr) != 0) {
		return -1;
	}

	const uint8_t iocon_reg = REG_A(IOCON_REG, cfg->type);

	/*
	 * With IOCON.SEQOP = 1 the address pointer doesn't increment, so the
	 * reset would write every byte to IODIR. Clear it to enable SEQOP.
	 */
	if (i2c_read8_8b(i2c, addr, iocon_reg, &cfg_reg) != 0) {
		return -1;
	}
	cfg_reg &= ~IOCON_REG_SEQOP;

	if (restart) {
		// We need to clear SEQOP before calling mcp230xx_reset
		if (i2c_write8_8b(i2c, addr, iocon_reg, cfg_reg) != 0) {
			return -1;
		}
		if (mcp230xx_reset(i2c, addr, cfg->type) != 0) {
			return -1;
		}
		cfg_reg = IOCON_REG_RESET_VALUE;
	}

	// Always clear the bits
	cfg_reg &= ~(IOCON_REG_DISSLW | IOCON_REG_ODR | IOCON_REG_INTPOL |
		     IOCON_REG_MIRROR);

	// Enable / Disable I2C slew rate
	if (cfg->disable_slew_rate) {
		cfg_reg |= IOCON_REG_DISSLW;
	}

	// Set the type of the interrupt output
	cfg_reg |= cfg->int_type << IOCON_REG_ODR_SHIFT;

	// Set the interrupt polarity
	if (cfg->int_pol != MCP230XX_INT_POLARITY_NONE) {
		cfg_reg |= cfg->int_pol << IOCON_REG_INTPOL_SHIFT;
	}

	// Set mirror byte
	cfg_reg |= cfg->mirror << IOCON_REG_MIRROR_SHIFT;

	if (cfg->type == MCP230XX_017) {
		/*
		 * Keep HAEN set, so the next init finds IOCON.BANK = 0 with one
		 * read (see mcp23017_ensure_bank_state). HAEN does nothing on
		 * the MCP23017.
		 */
		cfg_reg |= IOCON_REG_HAEN;
	}

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
		PLC_SET_MALLOC_ERRNO();
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
	uint8_t iodir_reg, gppu_reg;
	int result;

	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (mcp230xx_check_gpio(mcp, index) != 0) {
		return -1;
	}

	if (config != MCP230XX_NO_PULLUP && config != MCP230XX_PULLUP) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	const uint8_t pin_mask = GPIO_PIN_MASK(index);
	const uint8_t iodir_addr = GPIO_REG_ADDR(mcp, IODIR_REG, index);
	const uint8_t gppu_addr = GPIO_REG_ADDR(mcp, GPPU_REG, index);

	if (mcp230xx_lock(MCP(mcp), timeout_ms) != 0) {
		return -1;
	}

	if (i2c_read8_8b(i2c, MCP(mcp)->addr, iodir_addr, &iodir_reg) != 0 ||
	    i2c_read8_8b(i2c, MCP(mcp)->addr, gppu_addr, &gppu_reg) != 0) {
		result = -1;
		goto set_input_error_cleanup;
	}

	const uint8_t new_iodir = iodir_reg | pin_mask;
	const uint8_t new_gppu = config == MCP230XX_PULLUP ?
					 gppu_reg | pin_mask :
					 gppu_reg & ~pin_mask;

	if (new_iodir == iodir_reg && new_gppu == gppu_reg) {
		result = 1;
		goto set_input_error_cleanup;
	}

	/*
	 * Update the GPPU before the IODIR register, ensure no
	 * accidental pull-ups.
	 */
	if (new_gppu != gppu_reg &&
	    i2c_write8_8b(i2c, MCP(mcp)->addr, gppu_addr, new_gppu) != 0) {
		result = -1;
		goto set_input_error_cleanup;
	}
	if (new_iodir != iodir_reg &&
	    i2c_write8_8b(i2c, MCP(mcp)->addr, iodir_addr, new_iodir) != 0) {
		result = -1;
		goto set_input_error_cleanup;
	}
	result = 0;

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

	if (mcp230xx_check_gpio(mcp, index) != 0) {
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	if (mcp230xx_lock(MCP(mcp), timeout_ms) != 0) {
		return -1;
	}
	result = i2c_read8_8b(i2c,
			      MCP(mcp)->addr,
			      GPIO_REG_ADDR(mcp, GPIO_REG, index),
			      &gpio_reg);
	mcp230xx_unlock(MCP(mcp));

	if (result != 0) {
		return result;
	}

	*return_value = (gpio_reg & GPIO_PIN_MASK(index)) != 0 ? MCP230XX_HIGH :
								 MCP230XX_LOW;
	return 0;
}

int mcp230xx_set_output(const i2c_interface_t* i2c,
			mcp230xx_t* mcp,
			uint8_t index,
			uint32_t timeout_ms)
{
	int result;

	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (mcp230xx_check_gpio(mcp, index) != 0) {
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	if (mcp230xx_lock(MCP(mcp), timeout_ms) != 0) {
		return -1;
	}
	result = mcp230xx_update_bits(i2c,
				      mcp,
				      GPIO_REG_ADDR(mcp, IODIR_REG, index),
				      GPIO_PIN_MASK(index),
				      false);
	mcp230xx_unlock(MCP(mcp));

	return result;
}

int mcp230xx_write_gpio(const i2c_interface_t* i2c,
			mcp230xx_t* mcp,
			uint8_t index,
			uint8_t to_write,
			uint32_t timeout_ms)
{
	int result;

	if (mcp == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (mcp230xx_check_gpio(mcp, index) != 0) {
		return -1;
	}

	if (i2c_check_bus(i2c, MCP(mcp)->bus) != 0) {
		return -1;
	}

	if (mcp230xx_lock(MCP(mcp), timeout_ms) != 0) {
		return -1;
	}
	result = mcp230xx_update_bits(i2c,
				      mcp,
				      GPIO_REG_ADDR(mcp, OLAT_REG, index),
				      GPIO_PIN_MASK(index),
				      to_write != MCP230XX_LOW);
	mcp230xx_unlock(MCP(mcp));

	return result;
}
