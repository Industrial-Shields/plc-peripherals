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

#include "fake-i2c.h"

#include "unity.h"

#include <string.h>

fake_i2c_write_t fake_i2c_write_op;
fake_i2c_transfer_t fake_i2c_transfer_op;
plc_i2c_addr_t fake_i2c_expected_addr;

static void check_addressed_device(plc_i2c_addr_t addr)
{
	TEST_ASSERT_TRUE_MESSAGE(fake_i2c_expected_addr != 0,
				 "the test did not set fake_i2c_expected_addr");
	TEST_ASSERT_EQUAL_HEX16_MESSAGE(
		fake_i2c_expected_addr, addr, "addressed the wrong I2C device");
}

void fake_i2c_reset(void)
{
	memset(&fake_i2c_write_op, 0, sizeof(fake_i2c_write_op));
	memset(&fake_i2c_transfer_op, 0, sizeof(fake_i2c_transfer_op));
	fake_i2c_expected_addr = 0;
}

void fake_i2c_answers(const uint8_t* bytes, size_t len)
{
	TEST_ASSERT_TRUE_MESSAGE(len <= FAKE_I2C_MAX_WIRE_BYTES,
				 "canned answer longer than the fake's buffer");

	memcpy(fake_i2c_transfer_op.response, bytes, len);
	fake_i2c_transfer_op.response_len = len;
	fake_i2c_transfer_op.reported_read_len = len;
	fake_i2c_transfer_op.retval = 1;
}

ssize_t fake_i2c_write(const i2c_interface_t* i2c,
		       plc_i2c_addr_t addr,
		       const uint8_t* to_write,
		       size_t to_write_len,
		       int num_calls)
{
	(void)num_calls;

	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, "no test uses a NULL interface");
	TEST_ASSERT_NOT_NULL(to_write);
	TEST_ASSERT_TRUE_MESSAGE(to_write_len <= FAKE_I2C_MAX_WIRE_BYTES,
				 "write longer than the fake's buffer");
	check_addressed_device(addr);

	fake_i2c_write_op.calls++;
	fake_i2c_write_op.len = to_write_len;
	memcpy(fake_i2c_write_op.bytes, to_write, to_write_len);

	return fake_i2c_write_op.retval;
}

ssize_t fake_i2c_write_then_read(const i2c_interface_t* i2c,
				 plc_i2c_addr_t addr,
				 const uint8_t* to_write,
				 size_t to_write_len,
				 uint8_t* to_read,
				 size_t to_read_len,
				 size_t* read_bytes,
				 int num_calls)
{
	(void)num_calls;

	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, "no test uses a NULL interface");
	TEST_ASSERT_NOT_NULL(to_write);
	TEST_ASSERT_NOT_NULL(to_read);
	TEST_ASSERT_NOT_NULL(read_bytes);
	TEST_ASSERT_TRUE_MESSAGE(to_write_len <= FAKE_I2C_MAX_WIRE_BYTES,
				 "write longer than the fake's buffer");
	check_addressed_device(addr);

	fake_i2c_transfer_op.calls++;
	fake_i2c_transfer_op.len = to_write_len;
	memcpy(fake_i2c_transfer_op.bytes, to_write, to_write_len);
	fake_i2c_transfer_op.requested_read_len = to_read_len;

	size_t to_copy = fake_i2c_transfer_op.response_len < to_read_len ?
				 fake_i2c_transfer_op.response_len :
				 to_read_len;
	memcpy(to_read, fake_i2c_transfer_op.response, to_copy);
	*read_bytes = fake_i2c_transfer_op.reported_read_len;

	return fake_i2c_transfer_op.retval;
}
