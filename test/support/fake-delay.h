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
 * A fake delay for host tests. It records every delay a driver asks for, and
 * returns at once, so a test can assert on the delays instead of on the time
 * a call took.
 *
 * Mock plc-delay.h with CMock and hand fake_plc_delay_us to the generated
 * stub from your setUp():
 *
 *	fake_delay_reset();
 *	plc_delay_us_Stub(fake_plc_delay_us);
 *
 * fake_delay_reset() must come first in every test, or state leaks between
 * them.
 */

#ifndef TEST_FAKE_DELAY_H_
#define TEST_FAKE_DELAY_H_

#include <stddef.h>
#include <stdint.h>

#define FAKE_DELAY_MAX_DELAYS 8

typedef struct {
	// Every delay asked for, in order. Only the first FAKE_DELAY_MAX_DELAYS.
	uint32_t us[FAKE_DELAY_MAX_DELAYS];
	size_t len;

	// The delay, counted from 0, that fails with EINVAL. -1 for none.
	int fail_at;
} fake_delay_t;

extern fake_delay_t fake_delay;

void fake_delay_reset(void);

int fake_plc_delay_us(uint32_t us, int cmock_num_calls);

#endif // TEST_FAKE_DELAY_H_
