/*
 * Copyright (c) 2025 Industrial Shields. All rights reserved
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

#include <plc-peripherals-i2c.h>
#include <plc-peripherals-platform.h>

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32

#include <stdbool.h>
#include <malloc.h>
#include <errno.h>
#include <esp32-hal-i2c.h>

#include <esp_log.h>
static const char* TAG = "plc-peripherals-i2c";

struct _i2c_interface_t {
	uint8_t bus_number;
};
static uint32_t MAXIMUM_I2C_TIMEOUT = 25;

static inline bool is_i2c_platform_correct(i2c_interface_t* i2c)
{
	return i2c == NULL || i2c->bus_number >= SOC_I2C_NUM;
}

i2c_interface_t* i2c_init(uint8_t bus, int32_t sda, int32_t scl)
{
	esp_err_t init_result = i2cInit(bus, sda, scl, 0);
	if (init_result != ESP_OK) {
		// Report an error if bus isn't initialized
		if (!i2cIsInit(bus)) {
			ESP_LOGE(TAG,
				 "Can't initialize I2C bus: %s",
				 esp_err_to_name(init_result));
			errno = EIO;
			return NULL;
		}
	}

	i2c_interface_t* ret = malloc(sizeof(struct _i2c_interface_t));
	if (ret != NULL) {
		ret->bus_number = bus;
	}
	return ret;
}

int i2c_deinit(i2c_interface_t* interface, bool deinit_i2c_bus)
{
	if (interface == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (deinit_i2c_bus) {
		esp_err_t deinit_result = i2cDeinit(interface->bus_number);
		if (deinit_result != ESP_OK) {
			// Report an error if bus is still initialized
			if (i2cIsInit(interface->bus_number)) {
				ESP_LOGE(TAG,
					 "Can't de-initialize I2C bus: %s",
					 esp_err_to_name(deinit_result));
				errno = EIO;
				return -1;
			}
		}
	}

	free(interface);
	return 0;
}

ssize_t i2c_write(i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  const uint8_t* to_write,
		  size_t to_write_len)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (!is_i2c_platform_correct(i2c) || addr >= 1024 || to_write == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	esp_err_t write_result = i2cWrite(i2c->bus_number,
					  addr,
					  to_write,
					  to_write_len,
					  MAXIMUM_I2C_TIMEOUT);
	switch (write_result) {
	case ESP_OK:
		return to_write_len;
	case ESP_ERR_TIMEOUT:
		return 0;
	default:
		ESP_LOGE(TAG,
			 "Can't write to I2C bus: %s",
			 esp_err_to_name(write_result));
		errno = EIO;
		return -1;
	}
}

ssize_t i2c_read(i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t* to_read,
		 size_t to_read_len)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (!is_i2c_platform_correct(i2c) || addr >= 1024 || to_read == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	size_t read_count = 0;
	esp_err_t read_result = i2cRead(i2c->bus_number,
					addr,
					to_read,
					to_read_len,
					MAXIMUM_I2C_TIMEOUT,
					&read_count);
	switch (read_result) {
	case ESP_OK:
	case ESP_ERR_TIMEOUT:
		return read_count;
	default:
		ESP_LOGE(TAG,
			 "Can't read to I2C bus: %s",
			 esp_err_to_name(read_result));
		errno = EIO;
		return -1;
	}
}

ssize_t i2c_write_then_read(i2c_interface_t* i2c,
			    plc_i2c_addr_t addr,
			    const uint8_t* to_write,
			    size_t to_write_len,
			    uint8_t* to_read,
			    size_t to_read_len,
			    size_t* read_bytes)
{
#if defined(PLC_PERIPHERALS_CHECK_ARGUMENTS)
	if (!is_i2c_platform_correct(i2c) || addr >= 1024 || to_write == NULL ||
	    to_read == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif

	int write_read_result = i2cWriteReadNonStop(i2c->bus_number,
						    addr,
						    to_write,
						    to_write_len,
						    to_read,
						    to_read_len,
						    MAXIMUM_I2C_TIMEOUT,
						    read_bytes);
	switch (write_read_result) {
	case ESP_OK:
		return to_write_len;
	case ESP_ERR_TIMEOUT:
		return 0;
	default:
		ESP_LOGE(TAG,
			 "Can't write/read to I2C bus: %s",
			 esp_err_to_name(write_read_result));
		errno = EIO;
		return -1;
	}
}

#endif // PLC_ENVIRONMENT == PLC_ARDUINO_ESP32 || PLC_ENVIRONMENT == PLC_ESP_IDF
