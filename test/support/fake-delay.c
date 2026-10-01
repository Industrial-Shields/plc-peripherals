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

#include "fake-delay.h"

#include <errno.h>
#include <string.h>

fake_delay_t fake_delay;

void fake_delay_reset(void)
{
	memset(&fake_delay, 0, sizeof(fake_delay));
	fake_delay.fail_at = -1;
}

int fake_plc_delay_us(uint32_t us, int cmock_num_calls)
{
	(void)cmock_num_calls;

	size_t index = fake_delay.len++;

	if (index < FAKE_DELAY_MAX_DELAYS) {
		fake_delay.us[index] = us;
	}

	if ((int)index == fake_delay.fail_at) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}
