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
 * pca9685_static_init leaves MODE1.AI set, and every function relies on it
 * staying set to read or write the four registers of an output at once.
 */

#include <plc-delay.h>
#include <plc-peripherals-i2c.h>
#include <peripheral-pca9685.h>

#include <malloc.h>
#include <errno.h>
#include <stdint.h>

// clang-format off
#define MODE1_REG                                      0x00
#define   MODE1_REG_RESTART                            (1 << 7)
#define   MODE1_REG_EXTCLK                             (1 << 6)
#define   MODE1_REG_AI                                 (1 << 5)
#define   MODE1_REG_SLEEP                              (1 << 4)
#define   MODE1_REG_ALLCALL                            (1 << 0)
#define   MODE1_REG_RESET_VALUE                        0b00010001
#define MODE2_REG                                      0x01
#define   MODE2_REG_INVRT                              (1 << 4)
#define   MODE2_REG_INVRT_SHIFT                        4
#define   MODE2_REG_OUTDRV                             (1 << 2)
#define   MODE2_REG_OUTDRV_SHIFT                       2
#define   MODE2_REG_RESET_VALUE                        0b00000100
#define SUBADR1_REG_RESET_VALUE                        0xE2
#define SUBADR2_REG_RESET_VALUE                        0xE4
#define SUBADR3_REG_RESET_VALUE                        0xE8
#define ALLCALLADR_REG_RESET_VALUE                     0xE0
#define LED0_ON_L_REG                                  0x06
#define LED_REGS_PER_OUTPUT                            4
#define   LED_COUNT_MASK                               0x0FFF

#define   LED_FULL                                     (1 << 12)
#define ALL_LED_ON_L_REG                               0xFA
#define ALL_LED_OFF_H_REG                              0xFD
#define   ALL_LED_ON_L_REG_RESET_VALUE                 0x00
#define   ALL_LED_ON_H_REG_RESET_VALUE                 0x10
#define   ALL_LED_OFF_L_REG_RESET_VALUE                0x00
#define   ALL_LED_OFF_H_REG_RESET_VALUE                0x10
#define PRE_SCALE_REG                                  0xFE
#define   PRE_SCALE_REG_RESET_VALUE                    0x1E

#define PCA9685_OSC_HZ                                 25000000UL
#define PCA9685_PWM_STEPS                              4096UL

// The oscillator is up at most 500 us after leaving sleep
#define PCA9685_OSC_STARTUP_US                         500
// clang-format on

#define LED_ON_L_REG(index) (LED0_ON_L_REG + (index) * LED_REGS_PER_OUTPUT)
#define UINT8T_ARR(arr) arr, sizeof(arr)
#define DIV_ROUND_CLOSEST(n, d) (((n) + (d) / 2) / (d))

typedef struct {
	plc_mutex_t mutex;
	plc_i2c_addr_t addr;
	uint8_t bus;
	bool is_protected;
} pca9685_internal_t;

_Static_assert(sizeof(pca9685_t) == sizeof(pca9685_internal_t),
	       "Not exactly a pca9685_internal_t");
_Static_assert(PLC_PERIPHERAL_INTERNAL_ALIGNOF(pca9685_t) ==
		       PLC_PERIPHERAL_INTERNAL_ALIGNOF(pca9685_internal_t),
	       "Not aligned exactly as a pca9685_internal_t");

#define PCA(p) ((pca9685_internal_t*)(p))

static int pca9685_lock(pca9685_internal_t* pca, uint32_t timeout_ms)
{
	int saved_errno = errno;

	if (pca->is_protected &&
	    plc_mutex_acquire(&pca->mutex, timeout_ms) != 0) {
		if (errno != EOWNERDEAD) {
			return -1;
		}

		// The mutex is held, and there is nothing to recover.
		errno = saved_errno;
	}

	return 0;
}

static void pca9685_unlock(pca9685_internal_t* pca)
{
	if (pca->is_protected) {
		plc_mutex_release(&pca->mutex);
	}
}

static bool pca9685_config_is_valid(const pca9685_config_t* cfg)
{
	if (cfg->drive != PCA9685_OPEN_DRAIN &&
	    cfg->drive != PCA9685_TOTEM_POLE) {
		return false;
	}

	if (cfg->logic != PCA9685_NOT_INVERTED &&
	    cfg->logic != PCA9685_INVERTED) {
		return false;
	}

	return true;
}

static void pca9685_encode_output(uint16_t value, uint8_t regs[4])
{
	uint16_t on = 0, off = value;

	if (value == PCA9685_OUTPUT_OFF) {
		off = LED_FULL;
	}

	if (value == PCA9685_OUTPUT_ON) {
		on = LED_FULL;
		off = 0;
	}

	regs[0] = (uint8_t)(on & 0xFF);
	regs[1] = (uint8_t)(on >> 8);
	regs[2] = (uint8_t)(off & 0xFF);
	regs[3] = (uint8_t)(off >> 8);
}

static uint16_t pca9685_decode_output(const uint8_t regs[4])
{
	const uint16_t on = (uint16_t)(regs[0] | (regs[1] << 8));
	const uint16_t off = (uint16_t)(regs[2] | (regs[3] << 8));

	/*
	 * Full OFF takes precedence over full ON (section 7.3.3, fig 11,
	 * example 4).
	 */
	if (off & LED_FULL) {
		return PCA9685_OUTPUT_OFF;
	}

	if (on & LED_FULL) {
		return PCA9685_OUTPUT_ON;
	}

	return (uint16_t)(off & LED_COUNT_MASK);
}

/*
 * Write a block of registers in one transfer, block[0] being the address of the
 * first one. The HAL reports a short write as a byte count, without setting
 * errno, so it fails here with EIO.
 */
static int pca9685_write_block(const i2c_interface_t* i2c,
			       plc_i2c_addr_t addr,
			       const uint8_t* block,
			       uint16_t len)
{
	const ssize_t bytes_written = i2c_write(i2c, addr, block, len);

	if (bytes_written < 0) {
		return -1;
	}

	if (bytes_written != len) {
		errno = EIO;
		return -1;
	}

	return 0;
}

/*
 * Read len consecutive registers, starting at reg, in one transfer. The HAL
 * reports a short transfer as byte counts, without setting errno, so it fails
 * here with EIO.
 */
static int pca9685_read_block(const i2c_interface_t* i2c,
			      plc_i2c_addr_t addr,
			      uint8_t reg,
			      uint8_t* block,
			      uint16_t len)
{
	uint16_t bytes_read;
	const ssize_t bytes_written = i2c_write_then_read(
		i2c, addr, &reg, 1, block, len, &bytes_read);

	if (bytes_written < 0) {
		return -1;
	}

	if (bytes_written != 1 || bytes_read != len) {
		errno = EIO;
		return -1;
	}

	return 0;
}

/*
 * Enter sleep mode. RESTART reads as a state, but writing it as 1 is a command
 * (section 7.3.1.1). Clear it always, so a RESTART condition read as 1 is never
 * sent back as a restart while going to sleep. EXTCLK is always written as 0.
 *
 * This method assumes the PCA9685 is already awake.
 */
static int
pca9685_sleep(const i2c_interface_t* i2c, plc_i2c_addr_t addr, uint8_t mode1)
{
	const uint8_t asleep = (mode1 | MODE1_REG_SLEEP) &
			       ~(MODE1_REG_RESTART | MODE1_REG_EXTCLK);

	return i2c_write8_8b(i2c, addr, MODE1_REG, asleep);
}

/*
 * Leave sleep mode. Once the oscillator is up, writing RESTART resumes the
 * outputs that were running when the chip went to sleep (section 7.3.1.1). We
 * set RESTART unconditionally; if it is 0, there is nothing to resume, and
 * writing it does nothing. EXTCLK is always written as 0, as in pca9685_sleep.
 *
 * This method assumes the PCA9685 is already sleeping.
 */
static int
pca9685_wake(const i2c_interface_t* i2c, plc_i2c_addr_t addr, uint8_t mode1)
{
	const uint8_t awake = mode1 & ~(MODE1_REG_SLEEP | MODE1_REG_RESTART |
					MODE1_REG_EXTCLK);

	if (i2c_write8_8b(i2c, addr, MODE1_REG, awake) != 0) {
		return -1;
	}

	if (plc_delay_us(PCA9685_OSC_STARTUP_US) != 0) {
		return -1;
	}

	return i2c_write8_8b(i2c, addr, MODE1_REG, awake | MODE1_REG_RESTART);
}

/*
 * Set every register to its power-on value (Table 4), which leaves the chip
 * asleep with every output fully off.
 */
static int pca9685_reset(const i2c_interface_t* i2c, plc_i2c_addr_t addr)
{
	/*
	 * MODE1 to ALLCALLADR. Setting AI in the first byte already
	 * auto-increments the rest (Figure 21), and SLEEP lets PRE_SCALE be
	 * written next.
	 */
	const uint8_t mode_regs[] = {
		// The first address to write
		MODE1_REG,
		MODE1_REG_RESET_VALUE | MODE1_REG_AI,
		MODE2_REG_RESET_VALUE,
		SUBADR1_REG_RESET_VALUE,
		SUBADR2_REG_RESET_VALUE,
		SUBADR3_REG_RESET_VALUE,
		ALLCALLADR_REG_RESET_VALUE,
	};
	/*
	 * ALL_LED_ON_L to ALL_LED_OFF_H load every output's four registers at
	 * once (section 7.3.4), then PRE_SCALE.
	 */
	const uint8_t all_led_regs[] = {
		// The first address to write
		ALL_LED_ON_L_REG,
		ALL_LED_ON_L_REG_RESET_VALUE,
		ALL_LED_ON_H_REG_RESET_VALUE,
		ALL_LED_OFF_L_REG_RESET_VALUE,
		ALL_LED_OFF_H_REG_RESET_VALUE,
		PRE_SCALE_REG_RESET_VALUE,
	};

	if (pca9685_write_block(i2c, addr, UINT8T_ARR(mode_regs)) != 0) {
		return -1;
	}

	if (pca9685_write_block(i2c, addr, UINT8T_ARR(all_led_regs)) != 0) {
		return -1;
	}

	return i2c_write8_8b(i2c, addr, MODE1_REG, MODE1_REG_RESET_VALUE);
}

/*
 * Turn every output fully off, then go to sleep: an orderly shutdown (section
 * 7.3.1.1, footnote 1). The PWM values are lost, so a later wake resumes
 * nothing.
 *
 * This method assumes the PCA9685 is awake.
 */
static int pca9685_shutdown(const i2c_interface_t* i2c, plc_i2c_addr_t addr)
{
	uint8_t mode1_reg;

	if (i2c_read8_8b(i2c, addr, MODE1_REG, &mode1_reg) != 0) {
		return -1;
	}

	if (i2c_write8_8b(i2c, addr, ALL_LED_OFF_H_REG, LED_FULL >> 8) != 0) {
		return -1;
	}

	return pca9685_sleep(i2c, addr, mode1_reg);
}

int pca9685_static_init(const i2c_interface_t* i2c,
			pca9685_t* pca,
			plc_i2c_addr_t addr,
			bool restart,
			const pca9685_config_t* cfg)
{
	uint8_t mode1_reg, mode2_reg;
	uint8_t bus;

	if (pca == NULL || ((uintptr_t)pca % PCA9685_ALIGN) != 0 ||
	    cfg == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (!pca9685_config_is_valid(cfg)) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_get_bus(i2c, &bus) != 0) {
		return -1;
	}

	if (restart) {
		if (pca9685_reset(i2c, addr) != 0) {
			return -1;
		}
		mode1_reg = MODE1_REG_RESET_VALUE;
	} else if (i2c_read8_8b(i2c, addr, MODE1_REG, &mode1_reg) != 0) {
		return -1;
	}

	/*
	 * Enable auto-increment, clear the RESTART flag, and never switch to
	 * the external clock.
	 */
	mode1_reg |= MODE1_REG_AI;
	mode1_reg &= ~MODE1_REG_RESTART;
	mode1_reg &= ~MODE1_REG_EXTCLK;

	mode2_reg = MODE2_REG_RESET_VALUE & (~MODE2_REG_INVRT) &
		    (~MODE2_REG_OUTDRV);
	// OCH and OUTNE are already cleared; set INVRT and OUTDRV accordingly.
	mode2_reg |= (uint8_t)(cfg->logic << MODE2_REG_INVRT_SHIFT);
	mode2_reg |= (uint8_t)(cfg->drive << MODE2_REG_OUTDRV_SHIFT);

	/*
	 * Sequence:
	 *   - Write MODE2 first, so outputs restarted by a wake already use it.
	 *   - If asleep, wake the chip.
	 *   - Otherwise, write MODE1 (waking the chip already writes MODE1).
	 */

	if (i2c_write8_8b(i2c, addr, MODE2_REG, mode2_reg) != 0) {
		return -1;
	}

	if (mode1_reg & MODE1_REG_SLEEP) {
		// Disable SLEEP in MODE1 reg and wake it up
		mode1_reg &= ~MODE1_REG_SLEEP;
		if (pca9685_wake(i2c, addr, mode1_reg) != 0) {
			return -1;
		}
	} else if (i2c_write8_8b(i2c, addr, MODE1_REG, mode1_reg) != 0) {
		return -1;
	}

	PCA(pca)->addr = addr;
	PCA(pca)->bus = bus;
	PCA(pca)->is_protected = false;
	return 0;
}

pca9685_t* pca9685_init(const i2c_interface_t* i2c,
			plc_i2c_addr_t addr,
			bool restart,
			const pca9685_config_t* cfg)
{
	pca9685_t* ret = malloc(sizeof(pca9685_t));

	if (ret == NULL) {
		PLC_SET_MALLOC_ERRNO();
		return NULL;
	}

	if (pca9685_static_init(i2c, ret, addr, restart, cfg) != 0) {
		free(ret);
		return NULL;
	}

	return ret;
}

int pca9685_static_deinit(const i2c_interface_t* i2c,
			  pca9685_t* pca,
			  bool shutdown)
{
	if (pca == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, PCA(pca)->bus) != 0) {
		return -1;
	}

	if (pca9685_unprotect(pca) < 0) {
		return -1;
	}

	if (shutdown && pca9685_shutdown(i2c, PCA(pca)->addr) != 0) {
		return -1;
	}

	return 0;
}

int pca9685_deinit(const i2c_interface_t* i2c, pca9685_t* pca, bool shutdown)
{
	if (pca9685_static_deinit(i2c, pca, shutdown) != 0) {
		return -1;
	}

	free(pca);
	return 0;
}

int pca9685_protect(const i2c_interface_t* i2c,
		    pca9685_t* pca,
		    plc_mutex_scope_t scope)
{
	if (pca == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, PCA(pca)->bus) != 0) {
		return -1;
	}

	if (PCA(pca)->is_protected) {
		return 1;
	}

	if (plc_mutex_static_create(&PCA(pca)->mutex, scope) != 0) {
		return -1;
	}

	PCA(pca)->is_protected = true;
	return 0;
}

int pca9685_unprotect(pca9685_t* pca)
{
	if (pca == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (!PCA(pca)->is_protected) {
		return 1;
	}

	if (plc_mutex_static_destroy(&PCA(pca)->mutex) != 0) {
		return -1;
	}

	PCA(pca)->is_protected = false;
	return 0;
}

int pca9685_set_prescaler(const i2c_interface_t* i2c,
			  pca9685_t* pca,
			  uint8_t prescale,
			  uint32_t timeout_ms)
{
	uint8_t mode1_reg, prescale_reg;
	int result;

	if (pca == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (prescale < PCA9685_MIN_PRESCALER) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, PCA(pca)->bus) != 0) {
		return -1;
	}

	if (pca9685_lock(PCA(pca), timeout_ms) != 0) {
		return -1;
	}

	if (i2c_read8_8b(i2c, PCA(pca)->addr, PRE_SCALE_REG, &prescale_reg) !=
	    0) {
		result = -1;
		goto set_prescaler_cleanup;
	}

	if (prescale_reg == prescale) {
		result = 1;
		goto set_prescaler_cleanup;
	}

	if (i2c_read8_8b(i2c, PCA(pca)->addr, MODE1_REG, &mode1_reg) != 0) {
		result = -1;
		goto set_prescaler_cleanup;
	}

	// PRE_SCALE can only be written while asleep (Table 4, note 1)
	if (pca9685_sleep(i2c, PCA(pca)->addr, mode1_reg) != 0) {
		result = -1;
		goto set_prescaler_cleanup;
	}

	if (i2c_write8_8b(i2c, PCA(pca)->addr, PRE_SCALE_REG, prescale) != 0) {
		result = -1;
		goto set_prescaler_cleanup;
	}

	result = pca9685_wake(i2c, PCA(pca)->addr, mode1_reg);

set_prescaler_cleanup:
	pca9685_unlock(PCA(pca));
	return result;
}

int pca9685_set_frequency(const i2c_interface_t* i2c,
			  pca9685_t* pca,
			  uint16_t freq_hz,
			  uint32_t timeout_ms)
{
	if (freq_hz < PCA9685_MIN_FREQ_HZ || freq_hz > PCA9685_MAX_FREQ_HZ) {
		errno = EINVAL;
		return -1;
	}

	const uint32_t divisor = PCA9685_PWM_STEPS * freq_hz;
	const uint8_t prescale =
		(uint8_t)(DIV_ROUND_CLOSEST(PCA9685_OSC_HZ, divisor) - 1);

	return pca9685_set_prescaler(i2c, pca, prescale, timeout_ms);
}

int pca9685_set_output(const i2c_interface_t* i2c,
		       pca9685_t* pca,
		       uint8_t index,
		       uint16_t value,
		       uint32_t timeout_ms)
{
	uint8_t buffer[1 + LED_REGS_PER_OUTPUT];
	int result;

	if (pca == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (index >= PCA9685_NUM_OUTPUTS || value > PCA9685_OUTPUT_ON) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, PCA(pca)->bus) != 0) {
		return -1;
	}

	buffer[0] = LED_ON_L_REG(index);
	pca9685_encode_output(value, &buffer[1]);

	if (pca9685_lock(PCA(pca), timeout_ms) != 0) {
		return -1;
	}

	result = pca9685_write_block(i2c, PCA(pca)->addr, UINT8T_ARR(buffer));

	pca9685_unlock(PCA(pca));

	return result;
}

int pca9685_set_all_outputs(const i2c_interface_t* i2c,
			    pca9685_t* pca,
			    const uint16_t values[PCA9685_NUM_OUTPUTS],
			    uint32_t timeout_ms)
{
	uint8_t buffer[1 + PCA9685_NUM_OUTPUTS * LED_REGS_PER_OUTPUT];
	int result;

	if (pca == NULL || values == NULL) {
		errno = EFAULT;
		return -1;
	}

	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		if (values[i] > PCA9685_OUTPUT_ON) {
			errno = EINVAL;
			return -1;
		}
	}

	if (i2c_check_bus(i2c, PCA(pca)->bus) != 0) {
		return -1;
	}

	buffer[0] = LED_ON_L_REG(0);
	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		pca9685_encode_output(values[i],
				      &buffer[1 + i * LED_REGS_PER_OUTPUT]);
	}

	if (pca9685_lock(PCA(pca), timeout_ms) != 0) {
		return -1;
	}

	result = pca9685_write_block(i2c, PCA(pca)->addr, UINT8T_ARR(buffer));

	pca9685_unlock(PCA(pca));

	return result;
}

int pca9685_get_output(const i2c_interface_t* i2c,
		       pca9685_t* pca,
		       uint8_t index,
		       uint16_t* value,
		       uint32_t timeout_ms)
{
	uint8_t regs[LED_REGS_PER_OUTPUT];
	int result;

	if (pca == NULL || value == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (index >= PCA9685_NUM_OUTPUTS) {
		errno = EINVAL;
		return -1;
	}

	if (i2c_check_bus(i2c, PCA(pca)->bus) != 0) {
		return -1;
	}

	if (pca9685_lock(PCA(pca), timeout_ms) != 0) {
		return -1;
	}

	result = pca9685_read_block(
		i2c, PCA(pca)->addr, LED_ON_L_REG(index), UINT8T_ARR(regs));

	pca9685_unlock(PCA(pca));

	if (result != 0) {
		return -1;
	}

	*value = pca9685_decode_output(regs);
	return 0;
}

int pca9685_get_all_outputs(const i2c_interface_t* i2c,
			    pca9685_t* pca,
			    uint16_t values[PCA9685_NUM_OUTPUTS],
			    uint32_t timeout_ms)
{
	uint8_t regs[PCA9685_NUM_OUTPUTS * LED_REGS_PER_OUTPUT];
	int result;

	if (pca == NULL || values == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (i2c_check_bus(i2c, PCA(pca)->bus) != 0) {
		return -1;
	}

	if (pca9685_lock(PCA(pca), timeout_ms) != 0) {
		return -1;
	}

	result = pca9685_read_block(
		i2c, PCA(pca)->addr, LED_ON_L_REG(0), UINT8T_ARR(regs));

	pca9685_unlock(PCA(pca));

	if (result != 0) {
		return -1;
	}

	for (uint8_t i = 0; i < PCA9685_NUM_OUTPUTS; i++) {
		values[i] =
			pca9685_decode_output(&regs[i * LED_REGS_PER_OUTPUT]);
	}

	return 0;
}
