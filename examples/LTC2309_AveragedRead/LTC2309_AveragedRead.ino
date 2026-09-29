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
 * LTC2309_AveragedRead
 *
 * Demonstrates the LTC2309 ADC driver: forever, with no stop condition, it
 * cycles through the eight single-ended inputs one at a time, keeps a running
 * average of SAMPLES_PER_CHANNEL fresh conversions per channel, and prints each
 * channel's averaged reading as soon as that channel's average is ready.
 *
 * Wiring (single-ended inputs, each measured relative to GND):
 *   CH2                  -- known 5V reference
 *   CH4                  -- known 3.3V referenec
 *   CH0                  -- tied to GND
 *   CH1, CH3, CH5..CH7   -- unused
 *   SDA / SCL            -- the I2C bus given by I2C_BUS below
 *   AD0 / AD1            -- tied to GND here, giving I2C address 0x08
 *
 * ltc2309_read_unsigned() (see src/peripheral-ltc2309.c) always performs a
 * write-then-read transaction that triggers a fresh conversion.
 *
 * This example configures the LTC2309 in unipolar range (bip=false) and reads
 * each channel single-ended, so every code is 0..4095, mapping to roughly
 * 0..4.096V. Other functions in this driver (see include/peripheral-ltc2309.h),
 * not used here since this example only focuses on reading:
 *   ltc2309_read_signed / ltc2309_read_differential
 *     - Differential conversions, returned as a sign-extended int16_t.
 *   ltc2309_protect / ltc2309_unprotect
 *     - Guard the device with the resource protector's mutex, so concurrent
 *       tasks/threads sharing this LTC2309 (or its I2C bus) don't race each
 *       other.
 *   ltc2309_deinit
 *     - Tear down an ltc2309_t, optionally placing the chip in shutdown first.
 *
 * Build (against this library's Linux build):
 *   make
 *   gcc -Iinclude -x c examples/LTC2309_AveragedRead/LTC2309_AveragedRead.ino \
 *       -x none -Lbuild -lplc-peripherals -o LTC2309_AveragedRead
 */

#include <peripheral-ltc2309.h>

#include <errno.h>
#include <stdio.h>

// ---- I2C / device configuration ----
static const uint8_t I2C_BUS = 1;
static const int32_t I2C_SDA_PIN = -1;
static const int32_t I2C_SCL_PIN = -1;

static const uint8_t LTC2309_ADDR = 0x08; // AD0 = AD1 = GND
static const bool LTC2309_BIPOLAR = false; // unipolar range

#define NUM_CHANNELS 3
static const LTC2309_INPUT CHANNELS[NUM_CHANNELS] = {
	/* I0.12 in RPi PLC V6 */
	LTC2309_CH2, // wired to 5V
	/* I0.10 in RPi PLC V6 */
	LTC2309_CH4, // wired to 3.3V
	/* I0.8 in RPi PLC V6 */
	LTC2309_CH0, // wired to GND
};
static const char* CHANNEL_NAMES[NUM_CHANNELS] = {
	"CH2 (~5V ref)  ",
	"CH4 (~3.3V ref)",
	"CH0 (GND ref)  ",
};

// Number of samples to average together per channel before printing.
#define SAMPLES_PER_CHANNEL 100

static i2c_interface_t* i2c = NULL;
static ltc2309_t* ltc = NULL;

static uint32_t sample_sum[NUM_CHANNELS];
static uint32_t sample_count[NUM_CHANNELS];

static uint8_t current_channel = 0;

int main()
{
	i2c = i2c_init(I2C_BUS, I2C_SDA_PIN, I2C_SCL_PIN);
	if (i2c == NULL) {
		printf("i2c_init failed, errno=%d\n", errno);
		return 1;
	}

	ltc = ltc2309_init(i2c, LTC2309_ADDR, LTC2309_BIPOLAR);
	if (ltc == NULL) {
		printf("ltc2309_init failed, errno=%d\n", errno);
		i2c_deinit(i2c, false);
		return 1;
	}
	printf("LTC2309 initialized, sampling...\n");

	current_channel = 0;

	while (1) {
		// Take one fresh sample from the current channel, then advance to
		// the next channel for next time. Single-ended readings of inputs
		// relative to GND are never negative in unipolar range, so the
		// unsigned variant is the natural fit here.
		uint8_t sampled_channel = current_channel;
		uint16_t raw;
		if (ltc2309_read_unsigned(i2c,
			    ltc, CHANNELS[sampled_channel], &raw, 0) == 0) {
			sample_sum[sampled_channel] += raw;
			sample_count[sampled_channel]++;
		} else {
			printf("ltc2309_read_unsigned failed on %-6s, errno=%d\n",
			       CHANNEL_NAMES[sampled_channel], errno);
		}

		current_channel = (current_channel + 1) % NUM_CHANNELS;

		// Print (and reset) a channel's own average the moment IT finishes,
		// rather than trying to print all channels together -- they don't
		// finish at exactly the same read, so printing per-channel is what
		// stays correct at any SAMPLES_PER_CHANNEL, including 1.
		if (sample_count[sampled_channel] >= SAMPLES_PER_CHANNEL) {
			float average_code = (float)sample_sum[sampled_channel] /
					      (float)sample_count[sampled_channel];
			printf("%-6s: raw=%.1f (%u samples)\n",
			       CHANNEL_NAMES[sampled_channel], average_code,
			       (unsigned)sample_count[sampled_channel]);

			sample_sum[sampled_channel] = 0;
			sample_count[sampled_channel] = 0;
		}
	}

	// Not reached.
	return 0;
}
