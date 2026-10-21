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

/*
 * A fake of the direct GPIO functions plc-xgpio expects from the library that
 * uses it. Each one counts the call, records its arguments and returns
 * fake_normal_gpio.ret. The reads store fake_normal_gpio.level or
 * fake_normal_gpio.analog.
 *
 * Include this header in a test that links plc-xgpio, and call
 * fake_normal_gpio_reset() from setUp(), or state leaks between tests.
 */

#ifndef TEST_FAKE_NORMAL_GPIO_H_
#define TEST_FAKE_NORMAL_GPIO_H_

#include <stdint.h>

typedef struct {
	// Calls to any of the functions
	uint32_t calls;

	// The arguments of the last call. arg is its mode, value or frequency.
	uint32_t pin;
	uint32_t arg;

	// What the reads store
	uint32_t analog;
	uint8_t level;

	// What every function returns
	int ret;
} fake_normal_gpio_t;

extern fake_normal_gpio_t fake_normal_gpio;

void fake_normal_gpio_reset(void);

#endif // TEST_FAKE_NORMAL_GPIO_H_
