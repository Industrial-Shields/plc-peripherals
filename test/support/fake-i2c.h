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
 * A fake I2C device for host tests. It stands in for a real chip: it records
 * what the driver put on the wire and replays a canned answer, so a test can
 * assert on byte sequences with no hardware attached.
 *
 * It is not itself an implementation of the platform API. Mock
 * plc-peripherals-i2c-hal.h with CMock and hand these functions to the
 * generated stubs from your setUp():
 *
 *	fake_i2c_reset();
 *	fake_i2c_expected_addr = EXPECTED_ADDR;
 *	i2c_write_Stub(fake_i2c_write);
 *	i2c_read_Stub(fake_i2c_read);
 *	i2c_write_then_read_Stub(fake_i2c_write_then_read);
 *	fake_i2c_bus = EXPECTED_BUS;
 *	i2c_get_bus_Stub(fake_i2c_get_bus);
 *	i2c_check_bus_Stub(fake_i2c_check_bus);
 *
 * fake_i2c_reset() must come first in every test, or state leaks between them.
 */

#ifndef TEST_FAKE_I2C_H_
#define TEST_FAKE_I2C_H_

#include <plc-peripherals-i2c-hal.h>
#include <plc-peripherals-i2c.h>

// The longest write is pca9685_set_all_outputs: a register address, 64 bytes.
#define FAKE_I2C_MAX_WIRE_BYTES 65

/*
 * i2c_interface_t is opaque, so a test cannot build one, and i2c_init() is not
 * usable on the host. Drivers never dereference the handle, they only hand it
 * to the platform layer, so any non-NULL address does.
 */
#define FAKE_I2C_IFACE ((i2c_interface_t*)(!NULL))

typedef struct {
	uint32_t calls;
	uint8_t bytes[FAKE_I2C_MAX_WIRE_BYTES];
	size_t len;

	// Byte count the fake reports back, or -1. Drivers check it.
	ssize_t retval;
} fake_i2c_write_t;

typedef struct {
	uint32_t calls;
	uint8_t bytes[FAKE_I2C_MAX_WIRE_BYTES];
	size_t len;
	size_t requested_read_len;

	// Canned answer, set by fake_i2c_answers()
	uint8_t response[FAKE_I2C_MAX_WIRE_BYTES];
	size_t response_len;
	size_t reported_read_len;
	ssize_t retval;
} fake_i2c_transfer_t;

typedef struct {
	uint32_t calls;
	size_t requested_len;

	// Canned answer, set by fake_i2c_read_answers()
	uint8_t response[FAKE_I2C_MAX_WIRE_BYTES];
	size_t response_len;
	ssize_t retval;
} fake_i2c_read_t;

extern fake_i2c_write_t fake_i2c_write_op;
extern fake_i2c_transfer_t fake_i2c_transfer_op;
extern fake_i2c_read_t fake_i2c_read_op;

/*
 * The device the test is talking to. Every call the fake receives must be
 * addressed to it, or the test fails where the wrong address was used. Set it
 * after fake_i2c_reset(); 0x00 is the reserved general-call address, so it is
 * never a device and stands for "the test declared nothing".
 */
extern plc_i2c_addr_t fake_i2c_expected_addr;

/*
 * The bus the fake interface reports from i2c_get_bus. Peripherals record it
 * at init and reject an interface for any other one, so a test makes that
 * happen by assigning a different value here. fake_i2c_bus_retval forces the
 * lookup itself to fail instead, the way a NULL interface makes the real
 * i2c_get_bus fail. fake_i2c_bus_errno is the errno that failure reports: it
 * defaults to EFAULT, what the platforms set for a NULL pointer, and can be
 * anything else to check that the caller passes it through untouched.
 *
 * Set fake_i2c_bus after fake_i2c_reset(); it zeroes all three.
 */
extern uint8_t fake_i2c_bus;
extern int fake_i2c_bus_retval;
extern int fake_i2c_bus_errno;

/**
 * fake_i2c_reset
 *
 * Forget every recorded call and every canned answer. Call it from setUp().
 */
void fake_i2c_reset(void);

/**
 * fake_i2c_answers
 *
 * Arm the fake with the bytes it hands back on the next write-then-read, as an
 * honest device would: it reports exactly as many bytes as it produced, and
 * acknowledges the single register-address byte written. Override
 * fake_i2c_transfer_op.reported_read_len or .retval afterwards to fake a
 * misbehaving device.
 */
void fake_i2c_answers(const uint8_t* bytes, size_t len);

/**
 * fake_i2c_read_answers
 *
 * Arm the fake with the bytes a standalone fake_i2c_read hands back next, as
 * an honest device would: it reports exactly as many bytes as it produced.
 * Override fake_i2c_read_op.retval afterwards to fake a misbehaving device.
 */
void fake_i2c_read_answers(const uint8_t* bytes, size_t len);

/*
 * Bodies for the CMock stubs of the platform layer. The trailing int is CMock's
 * call counter, part of the callback signature.
 */
int fake_i2c_get_bus(const i2c_interface_t* i2c, uint8_t* bus, int num_calls);

int fake_i2c_check_bus(const i2c_interface_t* i2c, uint8_t bus, int num_calls);

ssize_t fake_i2c_write(const i2c_interface_t* i2c,
		       plc_i2c_addr_t addr,
		       const uint8_t* to_write,
		       uint16_t to_write_len,
		       int num_calls);

ssize_t fake_i2c_read(const i2c_interface_t* i2c,
		      plc_i2c_addr_t addr,
		      uint8_t* to_read,
		      uint16_t to_read_len,
		      int num_calls);

ssize_t fake_i2c_write_then_read(const i2c_interface_t* i2c,
				 plc_i2c_addr_t addr,
				 const uint8_t* to_write,
				 uint16_t to_write_len,
				 uint8_t* to_read,
				 uint16_t to_read_len,
				 uint16_t* read_bytes,
				 int num_calls);

#endif // TEST_FAKE_I2C_H_
