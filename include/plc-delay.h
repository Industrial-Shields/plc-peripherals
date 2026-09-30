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

#ifndef PLC_DELAY_H_
#define PLC_DELAY_H_

// The delays the drivers wait for their datasheet timings.
#include <stdint.h>

#include <plc-peripherals-platform.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * plc_delay_us
 *
 * Wait at least us microseconds.
 *
 * Linux specific:
 *   - The delay is measured on CLOCK_MONOTONIC if available. If the kernel
 *     doesn't have it, or can't sleep on it, CLOCK_REALTIME is used instead, as
 *     a relative delay.
 *   - A signal handled while it waits doesn't end the delay early: it keeps
 *     waiting until the full delay has passed.
 *   - It sleeps in the kernel, without using the CPU, even for short delays.
 *     So a short delay can take longer than asked. The kernel's timer slack
 *     (50 us by default, 0 for SCHED_FIFO and SCHED_RR threads) plus the
 *     wake-up latency. A thread can lower its own slack with
 *     prctl(PR_SET_TIMERSLACK).
 *
 * ESP32 specific:
 *   - Whole FreeRTOS ticks are waited with vTaskDelay, which blocks the task.
 *     The rest, and any delay shorter than a tick, is spun, yielding to other
 *     ready tasks of the same priority or higher. Tasks of a lower priority,
 *     the idle task included, don't run while it spins.
 *   - Never call it from an ISR, or while the scheduler is suspended, since
 *     vTaskDelay can't be called there.
 *
 * Parameters:
 *   us (uint32_t) - The minimum time to wait, in microseconds. 0 returns at
 *                   once.
 *
 * Returns:
 *   int - 0 if successful, -1 otherwise.
 *
 * Errors:
 *   errno set to:
 *     - Linux specific:
 *       - (others) : Whatever clock_gettime or clock_nanosleep report. A
 *                    working kernel never does.
 *     - ESP32 specific: It can never fail.
 */
int plc_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif // PLC_DELAY_H_
