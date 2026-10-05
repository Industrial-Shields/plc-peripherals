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
// Only the ESP_LOGx calls use it, and they compile to nothing at level 0
#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL > 0
static const char* TAG = "plc-peripherals-i2c";
#endif

struct _i2c_interface_t {
	uint8_t bus_number;
};
static const uint32_t MAXIMUM_I2C_TIMEOUT = 25;
// The Arduino HAL explicitly supports only 7-bit addresses
static const plc_i2c_addr_t MAXIMUM_7BIT_ADDRESS = 0x7F;
static const plc_i2c_addr_t MAXIMUM_10BIT_ADDRESS = 0x3FF;
static int check_i2c_address(plc_i2c_addr_t addr)
{
	if (addr <= MAXIMUM_7BIT_ADDRESS) {
		return 0;
	}

	if (addr > MAXIMUM_10BIT_ADDRESS) {
		ESP_LOGE(TAG, "Invalid I2C address 0x%x", addr);
		errno = EINVAL;
		return -1;
	}

	ESP_LOGW(TAG,
		 "10-bit I2C address 0x%03x: the Arduino HAL only supports "
		 "7-bit ones",
		 addr);
	errno = ENOTSUP;
	return -1;
}

i2c_interface_t* i2c_init(uint8_t bus, int32_t sda, int32_t scl)
{
	esp_err_t init_result = i2cInit(bus, sda, scl, 0);
	if (init_result != ESP_OK) {
		// Report an error if bus isn't initialized
		if (!i2cIsInit(bus)) {
			ESP_LOGE(
				TAG,
				"Can't initialize I2C bus %u (SDA %ld, SCL %ld): %s",
				bus,
				sda,
				scl,
				esp_err_to_name(init_result));
			switch (init_result) {
			case ESP_ERR_INVALID_ARG:
				errno = EINVAL;
				break;
			default:
				errno = EIO;
				break;
			}
			return NULL;
		}

		ESP_LOGW(TAG,
			 "I2C bus %u was already initialized, keeping it: %s",
			 bus,
			 esp_err_to_name(init_result));
	}

	i2c_interface_t* ret = malloc(sizeof(struct _i2c_interface_t));
	if (ret == NULL) {
		ESP_LOGE(
			TAG, "Out of memory for the I2C bus %u interface", bus);
		PLC_SET_MALLOC_ERRNO();
	} else {
		ret->bus_number = bus;
		ESP_LOGD(TAG,
			 "I2C bus %u initialized (SDA %ld, SCL %ld)",
			 bus,
			 sda,
			 scl);
	}

	return ret;
}

int i2c_get_bus(const i2c_interface_t* i2c, uint8_t* bus)
{
	if (i2c == NULL || bus == NULL) {
		errno = EFAULT;
		return -1;
	}

	*bus = i2c->bus_number;
	return 0;
}

int i2c_deinit(i2c_interface_t* interface, bool deinit_i2c_bus)
{
	if (interface == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (deinit_i2c_bus) {
		esp_err_t deinit_result = i2cDeinit(interface->bus_number);
		if (deinit_result != ESP_OK) {
			// Report an error if bus is still initialized
			if (i2cIsInit(interface->bus_number)) {
				ESP_LOGE(TAG,
					 "Can't de-initialize I2C bus %u: %s",
					 interface->bus_number,
					 esp_err_to_name(deinit_result));
				errno = EIO;
				return -1;
			}

			ESP_LOGW(TAG,
				 "I2C bus %u was already de-initialized: %s",
				 interface->bus_number,
				 esp_err_to_name(deinit_result));
		}
	}

	ESP_LOGD(TAG,
		 "I2C bus %u interface released%s",
		 interface->bus_number,
		 deinit_i2c_bus ? ", and the bus de-initialized" : "");
	free(interface);
	return 0;
}

ssize_t i2c_write(const i2c_interface_t* i2c,
		  plc_i2c_addr_t addr,
		  const uint8_t* to_write,
		  uint16_t to_write_len)
{
	if (i2c == NULL || to_write == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (check_i2c_address(addr) != 0) {
		return -1;
	}

	esp_err_t write_result = i2cWrite(i2c->bus_number,
					  addr,
					  to_write,
					  to_write_len,
					  MAXIMUM_I2C_TIMEOUT);
	switch (write_result) {
	case ESP_OK:
		ESP_LOGV(TAG,
			 "Wrote %u bytes to 0x%02x on bus %u",
			 to_write_len,
			 addr,
			 i2c->bus_number);
		return to_write_len;
	case ESP_ERR_TIMEOUT:
		ESP_LOGW(TAG,
			 "Timed out writing to 0x%02x on bus %u",
			 addr,
			 i2c->bus_number);
		errno = ETIMEDOUT;
		return -1;
	case ESP_ERR_INVALID_ARG:
		ESP_LOGE(TAG,
			 "Invalid write to 0x%02x on bus %u",
			 addr,
			 i2c->bus_number);
		errno = EINVAL;
		return -1;
	default:
		ESP_LOGE(TAG,
			 "Can't write to 0x%02x on bus %u: %s",
			 addr,
			 i2c->bus_number,
			 esp_err_to_name(write_result));
		errno = EIO;
		return -1;
	}
}

ssize_t i2c_read(const i2c_interface_t* i2c,
		 plc_i2c_addr_t addr,
		 uint8_t* to_read,
		 uint16_t to_read_len)
{
	if (i2c == NULL || to_read == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (check_i2c_address(addr) != 0) {
		return -1;
	}

	size_t read_count = 0;
	esp_err_t read_result = i2cRead(i2c->bus_number,
					addr,
					to_read,
					to_read_len,
					MAXIMUM_I2C_TIMEOUT,
					&read_count);
	switch (read_result) {
	case ESP_OK:
		if (read_count != to_read_len) {
			ESP_LOGW(TAG,
				 "Read %u of %u bytes from 0x%02x on bus %u",
				 read_count,
				 to_read_len,
				 addr,
				 i2c->bus_number);
		} else {
			ESP_LOGV(TAG,
				 "Read %u bytes from 0x%02x on bus %u",
				 read_count,
				 addr,
				 i2c->bus_number);
		}
		return read_count;
	case ESP_ERR_TIMEOUT:
		ESP_LOGW(TAG,
			 "Timed out reading from 0x%02x on bus %u",
			 addr,
			 i2c->bus_number);
		errno = ETIMEDOUT;
		return -1;
	case ESP_ERR_INVALID_ARG:
		ESP_LOGE(TAG,
			 "Invalid read from 0x%02x on bus %u",
			 addr,
			 i2c->bus_number);
		errno = EINVAL;
		return -1;
	default:
		ESP_LOGE(TAG,
			 "Can't read from 0x%02x on bus %u: %s",
			 addr,
			 i2c->bus_number,
			 esp_err_to_name(read_result));
		errno = EIO;
		return -1;
	}
}

ssize_t i2c_write_then_read(const i2c_interface_t* i2c,
			    plc_i2c_addr_t addr,
			    const uint8_t* to_write,
			    uint16_t to_write_len,
			    uint8_t* to_read,
			    uint16_t to_read_len,
			    uint16_t* read_bytes)
{
	if (i2c == NULL || to_write == NULL || to_read == NULL ||
	    read_bytes == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (check_i2c_address(addr) != 0) {
		return -1;
	}

	size_t read_count = 0;
	int write_read_result = i2cWriteReadNonStop(i2c->bus_number,
						    addr,
						    to_write,
						    to_write_len,
						    to_read,
						    to_read_len,
						    MAXIMUM_I2C_TIMEOUT,
						    &read_count);
	// Never more than to_read_len, so it always fits
	*read_bytes = (uint16_t)read_count;
	switch (write_read_result) {
	case ESP_OK:
		if (read_count != to_read_len) {
			ESP_LOGW(TAG,
				 "Read %u of %u bytes from 0x%02x on bus %u",
				 read_count,
				 to_read_len,
				 addr,
				 i2c->bus_number);
		} else {
			ESP_LOGV(
				TAG,
				"Wrote %u and read %u bytes from 0x%02x on bus %u",
				to_write_len,
				read_count,
				addr,
				i2c->bus_number);
		}
		return to_write_len;
	case ESP_ERR_TIMEOUT:
		ESP_LOGW(TAG,
			 "Timed out writing then reading 0x%02x on bus %u",
			 addr,
			 i2c->bus_number);
		errno = ETIMEDOUT;
		return -1;
	case ESP_ERR_INVALID_ARG:
		ESP_LOGE(TAG,
			 "Invalid write then read of 0x%02x on bus %u",
			 addr,
			 i2c->bus_number);
		errno = EINVAL;
		return -1;
	default:
		ESP_LOGE(TAG,
			 "Can't write then read 0x%02x on bus %u: %s",
			 addr,
			 i2c->bus_number,
			 esp_err_to_name(write_read_result));
		errno = EIO;
		return -1;
	}
}

#endif // PLC_ENVIRONMENT == PLC_ARDUINO_ESP32
