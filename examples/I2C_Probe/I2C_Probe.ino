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
 * Scans the I2C bus once and prints the addresses that answer. The scan
 * itself is in i2c_probe.c, shared with Linux; see its header comment.
 * It prints with printf, which goes to the same UART as Serial.
 */

#include "i2c_probe.h"

// ---- I2C configuration (ESP32 PLC) ----
static const uint8_t I2C_BUS = 0;
static const int32_t I2C_SDA_PIN = 21;
static const int32_t I2C_SCL_PIN = 22;

void setup()
{
	Serial.begin(115200);
	delay(200); // let the USB/serial monitor catch up on boot

	Serial.println(F("I2C probe example"));
	i2c_probe_scan(I2C_BUS, I2C_SDA_PIN, I2C_SCL_PIN);
}

void loop()
{
}
