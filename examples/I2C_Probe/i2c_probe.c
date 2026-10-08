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
 * I2C_Probe
 *
 * Scans the 7-bit I2C addresses 0x08..0x77 once, like i2cdetect, and prints
 * which ones answer. Each address is probed with a zero-length i2c_write: only
 * the address is sent, and the call returns 0 if a device acknowledges it.
 *
 * The scan is the same on every platform. On ESP32, I2C_Probe.ino calls it
 * from setup(). On Linux, this file has its own main():
 *
 *   make
 *   gcc -Iinclude -o i2c-probe examples/I2C_Probe/i2c_probe.c \
 *       build/libplc-peripherals.a -lpthread
 *   ./i2c-probe 1        # scans /dev/i2c-1
 *
 * A probe is a write with no data. Most devices ignore it, but a few can take
 * it as a command (i2cdetect avoids quick writes on 0x30-0x37 and 0x50-0x5F
 * for that reason, since some EEPROMs there misbehave). Only scan buses whose
 * devices you know.
 *
 * On ESP32, every address with no device is logged by the library at the
 * "Error" core debug level or higher.
 */

#include "i2c_probe.h"

#include <plc-peripherals-i2c.h>
#include <plc-peripherals-platform.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

static const plc_i2c_addr_t FIRST_ADDR = 0x08;
static const plc_i2c_addr_t LAST_ADDR = 0x77;

int i2c_probe_scan(uint8_t bus, int32_t sda, int32_t scl)
{
	i2c_interface_t* i2c = i2c_init(bus, sda, scl);
	if (i2c == NULL) {
		printf("i2c_init failed: %s\n", strerror(errno));
		return -1;
	}

	// It's never read with a zero length, but it can't be NULL
	const uint8_t unused = 0;
	int found = 0;

	for (plc_i2c_addr_t addr = FIRST_ADDR; addr <= LAST_ADDR; addr++) {
		if (i2c_write(i2c, addr, &unused, 0) == 0) {
			printf("Found a device at 0x%02x\n", addr);
			found++;
			continue;
		}

		switch (errno) {
		// Nobody answered: EIO on ESP32, ENXIO or EREMOTEIO on Linux
		case EIO:
		case ENXIO:
#ifdef EREMOTEIO
		case EREMOTEIO:
#endif
			break;
		case ENOTSUP:
			printf("This I2C adapter can't probe\n");
			i2c_deinit(i2c, false);
			return -1;
		default:
			printf("Probing 0x%02x failed: %s\n", addr, strerror(errno));
			break;
		}
	}

	printf("%d device(s) found\n", found);

	// Keep the bus initialized, in case something else uses it
	i2c_deinit(i2c, false);
	return found;
}

#if PLC_ENVIRONMENT == PLC_LINUX
#include <stdlib.h>

int main(int argc, char** argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <bus>\n", argv[0]);
		return EXIT_FAILURE;
	}

	// On Linux, the pins must be negative: /dev/i2c-<bus> already has them
	return i2c_probe_scan((uint8_t)atoi(argv[1]), -1, -1) < 0 ?
		       EXIT_FAILURE :
		       EXIT_SUCCESS;
}
#endif
