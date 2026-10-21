/*
 * Copyright (c) 2026 Industrial Shields. All rights reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "fake-normal-gpio.h"

#include "plc-xgpio.h"

#include <string.h>

fake_normal_gpio_t fake_normal_gpio;

void fake_normal_gpio_reset(void)
{
	memset(&fake_normal_gpio, 0, sizeof(fake_normal_gpio));
}

int plc_normal_gpio_set_pin_mode(plc_xgpio_pin_t pin, uint8_t mode)
{
	fake_normal_gpio.calls++;
	fake_normal_gpio.pin = pin;
	fake_normal_gpio.arg = mode;
	return fake_normal_gpio.ret;
}

int plc_normal_gpio_digital_write(plc_xgpio_pin_t pin, uint8_t value)
{
	fake_normal_gpio.calls++;
	fake_normal_gpio.pin = pin;
	fake_normal_gpio.arg = value;
	return fake_normal_gpio.ret;
}

int plc_normal_gpio_set_pwm_frequency(plc_xgpio_pin_t pin, uint32_t freq)
{
	fake_normal_gpio.calls++;
	fake_normal_gpio.pin = pin;
	fake_normal_gpio.arg = freq;
	return fake_normal_gpio.ret;
}

int plc_normal_gpio_pwm_write(plc_xgpio_pin_t pin, uint32_t value)
{
	fake_normal_gpio.calls++;
	fake_normal_gpio.pin = pin;
	fake_normal_gpio.arg = value;
	return fake_normal_gpio.ret;
}

int plc_normal_gpio_digital_read(plc_xgpio_pin_t pin, uint8_t* read)
{
	fake_normal_gpio.calls++;
	fake_normal_gpio.pin = pin;
	*read = fake_normal_gpio.level;
	return fake_normal_gpio.ret;
}

int plc_normal_gpio_analog_read(plc_xgpio_pin_t pin, uint32_t* read)
{
	fake_normal_gpio.calls++;
	fake_normal_gpio.pin = pin;
	*read = fake_normal_gpio.analog;
	return fake_normal_gpio.ret;
}
