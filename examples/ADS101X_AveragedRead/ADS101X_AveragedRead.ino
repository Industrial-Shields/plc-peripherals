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
 * ADS101X_AveragedRead
 *
 * Demonstrates the ADS101X ADC driver in continuous-conversion mode.
 * Forever, with no stop condition, it cycles through three single-ended
 * inputs one at a time, keeps a running average of SAMPLES_PER_CHANNEL
 * fresh conversions per channel, and prints each channel's averaged
 * reading as soon as that channel's average is ready.
 *
 * Wiring (single-ended inputs, each measured relative to GND):
 *   AIN0 (ADS101X_P0_GND) -- known 5V reference
 *   AIN1 (ADS101X_P1_GND) -- known 3.3V reference
 *   AIN2 (ADS101X_P2_GND) -- tied to GND
 *   AIN3                  -- unused
 *   SDA / SCL             -- ESP32 I2C pins, see I2C_SDA_PIN / I2C_SCL_PIN below
 *   ADDR                  -- tied to GND here, giving I2C address 0x48
 *
 * ads101x_continuous_read() (see src/peripheral-ads101x.c) only re-delays
 * for a fresh conversion when the requested channel differs from the one
 * last configured; reading the same channel twice in a row with no delay
 * would just return the same stale conversion. This example sidesteps
 * that entirely by always moving to the next channel one sample at a
 * time -- every single call therefore changes the channel, so every call
 * naturally waits out a fresh conversion.
 *
 * Once every channel has printed its SAMPLES_PER_CHANNEL-sample average
 * (a full round), ads101x_set_fs() steps the data rate to the next one in
 * DATA_RATES, wrapping back to the slowest after the fastest -- see the
 * comment on DATA_RATES below.
 *
 * Uncomment USE_SINGLE_SHOT_READ below to demonstrate single-shot mode
 * instead. That macro guards BOTH the continuous_mode field of the
 * ads101x_config_t given to ads101x_init and which read function loop()
 * calls, so they can't drift out of sync -- mismatching them (e.g. calling
 * ads101x_single_read on a device initialized in continuous mode) fails
 * with EINVAL: see ads101x_single_read's docs in peripheral-ads101x.h.
 *
 * Other functions in this driver (see include/peripheral-ads101x.h), not
 * used here since this example only focuses on reading:
 *   ads101x_get_fs
 *     - Read back the currently configured data rate.
 *   ads101x_protect / ads101x_unprotect
 *     - Guard the device with the resource protector's mutex, so
 *       concurrent tasks/threads sharing this ADS101X (or its I2C bus)
 *       don't race each other.
 *   ads101x_deinit
 *     - Tear down an ads101x_t, optionally powering the chip down first.
 */

#include <peripheral-ads101x.h>

#include <errno.h>

// ---- I2C / device configuration ----
static const uint8_t I2C_BUS = 0;
static const int32_t I2C_SDA_PIN = 21;
static const int32_t I2C_SCL_PIN = 22;

static const uint8_t ADS101X_ADDR = 0x48;
static const ADS101X_GAIN_AMPLIFIER ADS101X_FSR = ADS101X_FSR_4_096V;

// Every named data rate, slowest to fastest. There's no separate entry for
// the raw 0b111 encoding since it's just a duplicate of ADS101X_3300SPS
// (see ads101x_get_fs's docs) -- nothing to cycle through twice.
static const ADS101X_DATA_RATE DATA_RATES[] = {
	ADS101X_128SPS,
	ADS101X_250SPS,
	ADS101X_490SPS,
	ADS101X_920SPS,
	ADS101X_1600SPS,
	ADS101X_2400SPS,
	ADS101X_3300SPS,
};
static const char* DATA_RATE_NAMES[] = {
	"128SPS", "250SPS", "490SPS", "920SPS", "1600SPS", "2400SPS", "3300SPS",
};
#define NUM_DATA_RATES (sizeof(DATA_RATES) / sizeof(DATA_RATES[0]))
static uint8_t data_rate_index = 0;

#define NUM_CHANNELS 3
static const ADS101X_INPUT CHANNELS[NUM_CHANNELS] = {
	/* I0.12 in ESP32PLC V3 */
	ADS101X_P0_GND, // AIN0, wired to 5V
	/* I0.11 in ESP32PLC V3 */
	ADS101X_P1_GND, // AIN1, wired to 3.3V
	/* I0.10 in ESP32PLC V3 */
	ADS101X_P2_GND, // AIN2, wired to GND
};
static const char* CHANNEL_NAMES[NUM_CHANNELS] = {
	"AIN0 (~5V ref)  ",
	"AIN1 (~3.3V ref)",
	"AIN2 (GND ref)  ",
};

// Number of samples to average together per channel before printing.
#define SAMPLES_PER_CHANNEL 100

// Uncomment to use single-shot mode instead of continuous-conversion mode.
// See the file header comment above for why this one macro is the only
// thing that should ever need changing to switch modes.
// #define USE_SINGLE_SHOT_READ

static i2c_interface_t* i2c = NULL;
static ads101x_t* ads = NULL;

// Running sum/count of samples accumulated since the last print, per channel.
static uint32_t sample_sum[NUM_CHANNELS];
static uint32_t sample_count[NUM_CHANNELS];

// How many channels have finished their window since the last data rate
// change; the rate only steps once every channel has had its turn.
static uint8_t channels_finished_this_round = 0;

static uint8_t current_channel = 0;

void setup()
{
	Serial.begin(115200);
	delay(200); // let the USB/serial monitor catch up on boot

#ifdef USE_SINGLE_SHOT_READ
	Serial.println(F("ADS101X single-shot averaged read example"));
#else
	Serial.println(F("ADS101X continuous averaged read example"));
#endif

	i2c = i2c_init(I2C_BUS, I2C_SDA_PIN, I2C_SCL_PIN);
	if (i2c == NULL) {
		Serial.print(F("i2c_init failed, errno="));
		Serial.println(errno);
		return;
	}

	ads101x_config_t cfg;
#ifdef USE_SINGLE_SHOT_READ
	// Single-shot mode -- required for ads101x_single_read()'s OS-bit
	// trigger to actually do anything.
	cfg.continuous_mode = false;
#else
	// Run free-running continuous conversions.
	cfg.continuous_mode = true;
#endif
	cfg.fsr = ADS101X_FSR;
	cfg.dr = DATA_RATES[0];

	// restart=true: reset the ADS101X registers to their defaults first.
	ads = ads101x_init(i2c, ADS101X_ADDR, true, &cfg);
	if (ads == NULL) {
		Serial.print(F("ads101x_init failed, errno="));
		Serial.println(errno);
		return;
	}
	Serial.print(F("-- data rate is "));
				Serial.println(DATA_RATE_NAMES[0]);

	Serial.println(F("ADS101X initialized, sampling..."));
}

void loop()
{
	if (ads == NULL) {
		// setup() failed; nothing more this sketch can do.
		return;
	}

	// Take one fresh sample from the current channel, then advance to the
	// next channel for next time. Single-ended readings against GND are
	// never negative in principle, so the unsigned variant is the natural
	// fit here (it also folds ADS101X's small negative offset error, up to
	// 3 bits, into a clean 0).
	uint8_t sampled_channel = current_channel;
	uint16_t raw;
#ifdef USE_SINGLE_SHOT_READ
	int ret = ads101x_unsigned_single_read(i2c,
		ads, CHANNELS[sampled_channel], &raw, 0);
#else
	int ret = ads101x_unsigned_continuous_read(i2c,
		ads, CHANNELS[sampled_channel], &raw, 0);
#endif
	if (ret == 0) {
		sample_sum[sampled_channel] += raw;
		sample_count[sampled_channel]++;
	} else {
#ifdef USE_SINGLE_SHOT_READ
		Serial.print(F("ads101x_unsigned_single_read failed on "));
#else
		Serial.print(F("ads101x_unsigned_continuous_read failed on "));
#endif
		Serial.print(CHANNEL_NAMES[sampled_channel]);
		Serial.print(F(", errno="));
		Serial.println(errno);
	}

	current_channel = (current_channel + 1) % NUM_CHANNELS;

	// Print (and reset) a channel's own average the moment IT finishes,
	// rather than trying to print all three channels together -- they
	// don't finish at exactly the same read, so printing per-channel is
	// what stays correct at any SAMPLES_PER_CHANNEL, including 1.
	if (sample_count[sampled_channel] >= SAMPLES_PER_CHANNEL) {
		float average_code = (float)sample_sum[sampled_channel] /
				      (float)sample_count[sampled_channel];
		Serial.print(CHANNEL_NAMES[sampled_channel]);
		Serial.print(F(": raw="));
		Serial.print(average_code, 1);
		Serial.print(F(" ("));
		Serial.print(sample_count[sampled_channel]);
		Serial.println(F(" samples)"));

		sample_sum[sampled_channel] = 0;
		sample_count[sampled_channel] = 0;

		// Only step the data rate once every channel has had its turn
		// this round, not after each individual channel's window.
		channels_finished_this_round++;
		if (channels_finished_this_round >= NUM_CHANNELS) {
			channels_finished_this_round = 0;

			data_rate_index = (data_rate_index + 1) % NUM_DATA_RATES;
			if (ads101x_set_fs(i2c, ads, DATA_RATES[data_rate_index], 0) !=
			    0) {
				Serial.print(F("ads101x_set_fs failed, errno="));
				Serial.println(errno);
			} else {
				Serial.print(F("-- data rate now "));
				Serial.println(DATA_RATE_NAMES[data_rate_index]);
			}
		}
	}
}
