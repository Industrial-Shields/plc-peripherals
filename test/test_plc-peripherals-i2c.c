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
 * Tests for the portable register helpers of src/plc-peripherals-i2c.c. The
 * platform layer they sit on (plc-peripherals-i2c-hal.h) is mocked, and the
 * mocks are driven by the shared fake device in test/support/fake-i2c.c, which
 * records what went on the wire.
 */

#include "unity.h"

#include "fake-i2c.h"
#include "mock_plc-peripherals-i2c-hal.h"
#include "plc-peripherals-i2c.h"

#define TEST_ADDR ((plc_i2c_addr_t)0x48)

void setUp(void)
{
	fake_i2c_reset();
	fake_i2c_expected_addr = TEST_ADDR;
	i2c_write_Stub(fake_i2c_write);
	i2c_write_then_read_Stub(fake_i2c_write_then_read);
}

void tearDown(void)
{
}

/* ----------------------------- i2c_write8_8b ----------------------------- */

void test_i2c_write8_8b_writes_the_register_address_then_the_value(void)
{
	fake_i2c_write_op.retval = 2; // Register address and value acknowledged

	TEST_ASSERT_EQUAL_INT(
		0, i2c_write8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x12, 0xA5));

	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
	TEST_ASSERT_EQUAL_UINT(2, fake_i2c_write_op.len);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(
		((const uint8_t[]){ 0x12, 0xA5 }), fake_i2c_write_op.bytes, 2);
}

void test_i2c_write8_8b_fails_when_the_device_takes_fewer_bytes(void)
{
	fake_i2c_write_op.retval = 1; // Only the register address got through

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_write8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x12, 0xA5));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
}

void test_i2c_write8_8b_fails_when_the_device_takes_too_much_bytes(void)
{
	fake_i2c_write_op.retval = 4; // Extra data has gone

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_write8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x12, 0xA5));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
}

void test_i2c_write8_8b_fails_when_the_device_takes_0_bytes(void)
{
	fake_i2c_write_op.retval = 0; // No byte was written

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_write8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x12, 0xA5));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
}

void test_i2c_write8_8b_fails_when_the_bus_write_fails(void)
{
	fake_i2c_write_op.retval = -1; // An error occurred

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_write8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x12, 0xA5));
}

/* ---------------------------- i2c_write8_16b ----------------------------- */

void test_i2c_write8_16b_writes_the_value_most_significant_byte_first(void)
{
	fake_i2c_write_op.retval = 3; // All three bytes acknowledged

	TEST_ASSERT_EQUAL_INT(
		0, i2c_write8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, 0xBEEF));

	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
	TEST_ASSERT_EQUAL_UINT(3, fake_i2c_write_op.len);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(((const uint8_t[]){ 0x01, 0xBE, 0xEF }),
				     fake_i2c_write_op.bytes,
				     3);
}

void test_i2c_write8_16b_fails_when_the_device_takes_fewer_bytes(void)
{
	fake_i2c_write_op.retval = 2; // The low byte was dropped

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_write8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, 0xBEEF));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
}

void test_i2c_write8_16b_fails_when_the_device_takes_too_much_bytes(void)
{
	fake_i2c_write_op.retval = 4; // Extra data has gone

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_write8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, 0xBEEF));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
}

void test_i2c_write8_16b_fails_when_the_device_takes_0_bytes(void)
{
	fake_i2c_write_op.retval = 0; // No byte was written

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_write8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, 0xBEEF));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_write_op.calls);
}

void test_i2c_write8_16b_fails_when_the_bus_write_fails(void)
{
	fake_i2c_write_op.retval = -1;

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_write8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, 0xBEEF));
}

/* ------------------------------ i2c_read8_8b ----------------------------- */

void test_i2c_read8_8b_selects_the_register_then_reads_a_single_byte(void)
{
	uint8_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x5A }, 1);

	TEST_ASSERT_EQUAL_INT(
		0, i2c_read8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x09, &value));

	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
	TEST_ASSERT_EQUAL_UINT(1, fake_i2c_transfer_op.len);
	TEST_ASSERT_EQUAL_HEX8(0x09, fake_i2c_transfer_op.bytes[0]);
	TEST_ASSERT_EQUAL_UINT(1, fake_i2c_transfer_op.requested_read_len);
	TEST_ASSERT_EQUAL_HEX8(0x5A, value);
}

void test_i2c_read8_8b_fails_when_the_device_returns_no_byte(void)
{
	uint8_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x5A }, 1);
	fake_i2c_transfer_op.reported_read_len = 0;

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x09, &value));
}

void test_i2c_read8_8b_fails_when_the_device_returns_too_much_bytes(void)
{
	uint8_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x5A }, 1);
	fake_i2c_transfer_op.reported_read_len = 2; // Extra data has come

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x09, &value));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
}

void test_i2c_read8_8b_fails_when_the_register_address_is_not_acknowledged(void)
{
	uint8_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x5A }, 1);
	fake_i2c_transfer_op.retval =
		0; // The register address was not acknowledged

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x09, &value));
}

void test_i2c_read8_8b_fails_when_the_device_acknowledges_too_much_bytes(void)
{
	uint8_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x5A }, 1);
	fake_i2c_transfer_op.retval = 2; // More than the register address acked

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x09, &value));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
}

void test_i2c_read8_8b_fails_when_the_bus_read_fails(void)
{
	uint8_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x5A }, 1);
	fake_i2c_transfer_op.retval = -1; // An error occurred

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_8b(FAKE_I2C_IFACE, TEST_ADDR, 0x09, &value));
}

/* ----------------------------- i2c_read8_16b ----------------------------- */

void test_i2c_read8_16b_assembles_the_two_read_bytes_as_big_endian(void)
{
	uint16_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x12, 0x34 }, 2);

	TEST_ASSERT_EQUAL_INT(
		0, i2c_read8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, &value));

	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
	TEST_ASSERT_EQUAL_UINT(1, fake_i2c_transfer_op.len);
	TEST_ASSERT_EQUAL_HEX8(0x01, fake_i2c_transfer_op.bytes[0]);
	TEST_ASSERT_EQUAL_UINT(2, fake_i2c_transfer_op.requested_read_len);
	TEST_ASSERT_EQUAL_HEX16(0x1234, value);
}

void test_i2c_read8_16b_keeps_the_top_bit_of_the_register_value(void)
{
	uint16_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x83, 0x00 }, 2);

	TEST_ASSERT_EQUAL_INT(
		0, i2c_read8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, &value));
	TEST_ASSERT_EQUAL_HEX16(0x8300, value);
}

void test_i2c_read8_16b_fails_on_a_short_read(void)
{
	uint16_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x12, 0x34 }, 2);
	fake_i2c_transfer_op.reported_read_len = 1;

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, &value));
}

void test_i2c_read8_16b_fails_when_the_device_returns_no_byte(void)
{
	uint16_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x12, 0x34 }, 2);
	fake_i2c_transfer_op.reported_read_len = 0; // No byte was read

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, &value));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
}

void test_i2c_read8_16b_fails_when_the_device_returns_too_much_bytes(void)
{
	uint16_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x12, 0x34 }, 2);
	fake_i2c_transfer_op.reported_read_len = 3; // Extra data has come

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, &value));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
}

void test_i2c_read8_16b_fails_when_the_register_address_is_not_acknowledged(void)
{
	uint16_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x12, 0x34 }, 2);
	fake_i2c_transfer_op.retval = 0;

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, &value));
}

void test_i2c_read8_16b_fails_when_the_device_acknowledges_too_much_bytes(void)
{
	uint16_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x12, 0x34 }, 2);
	fake_i2c_transfer_op.retval = 2; // More than the register address acked

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, &value));
	TEST_ASSERT_EQUAL_UINT32(1, fake_i2c_transfer_op.calls);
}

void test_i2c_read8_16b_fails_when_the_bus_read_fails(void)
{
	uint16_t value = 0;
	fake_i2c_answers((const uint8_t[]){ 0x12, 0x34 }, 2);
	fake_i2c_transfer_op.retval = -1; // An error occurred

	TEST_ASSERT_EQUAL_INT(
		-1, i2c_read8_16b(FAKE_I2C_IFACE, TEST_ADDR, 0x01, &value));
}
