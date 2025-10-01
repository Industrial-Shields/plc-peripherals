/**
 * Copyright (c) 2024 Industrial Shields. All rights reserved
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

#include <plc-peripherals.h>

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux

#include <string.h>
#include <unistd.h>
#include <errno.h>

#define PCA9534D_ADDRESS 0x20
#define INPUT_PORT_REGISTER 0x00
#define OUTPUT_PORT_REGISTER 0x01
#define CONFIGURATION_REGISTER 0x03

#include <unity.h>
void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void pca9534d_init_sanity_check() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9534d_init(NULL, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9534d_init(NULL, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9534d_init(i2c, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9534d_init(i2c, 0x00), strerror(errno));
	TEST_ASSERT_MESSAGE(errno == EIO || errno == EREMOTEIO, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9534d_deinit_sanity_check() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9534d_deinit(NULL, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9534d_deinit(NULL, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9534d_deinit(i2c, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9534d_deinit(i2c, 0x00), strerror(errno));
	TEST_ASSERT_MESSAGE(errno == EIO || errno == EREMOTEIO, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9534d_init_deinit_cycle() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));

        pca9534d_deinit(i2c, PCA9534D_ADDRESS);

        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_init(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_deinit(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9534d_set_pin_mode_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_init(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        // Set pin to output
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_set_pin_mode(i2c, PCA9534D_ADDRESS, 0x07, 0x00), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_config_reg, CONFIGURATION_REGISTER);
	uint8_t config;
	i2c_read_t read_config_reg = {.buff=&config, .len=1};
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_config_reg, &read_config_reg), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0b01111111, config, strerror(errno));


	// Set pin to input
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_set_pin_mode(i2c, PCA9534D_ADDRESS, 0x07, 0x01), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_config_reg, &read_config_reg), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0b11111111, config, strerror(errno));


        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_deinit(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9534d_set_pin_mode_all_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_init(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        // Set pin modes to 0b01011010 (1 is input, 0 is output)
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_set_pin_mode_all(i2c, PCA9534D_ADDRESS, 0b01011010), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_config_reg, CONFIGURATION_REGISTER);
	uint8_t config;
	i2c_read_t read_config_reg = {.buff=&config, .len=1};
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_config_reg, &read_config_reg), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0b01011010, config, strerror(errno));


	// Set pin modes to 0
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_set_pin_mode_all(i2c, PCA9534D_ADDRESS, 0b11111111), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_config_reg, &read_config_reg), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0b11111111, config, strerror(errno));


        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_deinit(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9534d_read_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_init(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_set_pin_mode(i2c, PCA9534D_ADDRESS, 0x05, 0x01), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_input_reg, INPUT_PORT_REGISTER);
	uint8_t input_reg;
	i2c_read_t read_input_reg = {.buff=&input_reg, .len=1};

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_input_reg, &read_input_reg), strerror(errno));

	uint8_t value = 0b11111111;
	TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_read(i2c, PCA9534D_ADDRESS, 5, &value), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(value, (input_reg >> 5) & 0b00000001, strerror(errno));
	// printf("Value in channel 5: %d; Register value: %d\n", value, input_reg);



        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_deinit(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9534d_write_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_init(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_set_pin_mode(i2c, PCA9534D_ADDRESS, 0x05, 0x00), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_output_reg, OUTPUT_PORT_REGISTER);
	uint8_t output_reg;
	i2c_read_t read_output_reg = {.buff=&output_reg, .len=1};

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_output_reg, &read_output_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0, output_reg);

	// Write 1
	TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_write(i2c, PCA9534D_ADDRESS, 0x05, 1), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_output_reg, &read_output_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0b00100000, output_reg);

	// Write 0
	TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_write(i2c, PCA9534D_ADDRESS, 0x05, 0), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_output_reg, &read_output_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0, output_reg);


        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_deinit(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9534d_read_all_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_init(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_set_pin_mode_all(i2c, PCA9534D_ADDRESS, 0b11111111), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_input_reg, INPUT_PORT_REGISTER);
	uint8_t input_reg;
	i2c_read_t read_input_reg = {.buff=&input_reg, .len=1};

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_input_reg, &read_input_reg), strerror(errno));

	uint8_t value = 0b11111111;
	TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_read_all(i2c, PCA9534D_ADDRESS, &value), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(value, input_reg, strerror(errno));
	// printf("Value in channel 5: %d; Register value: %d\n", value, input_reg);



        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_deinit(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9534d_write_all_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_init(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_set_pin_mode_all(i2c, PCA9534D_ADDRESS, 0), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_output_reg, OUTPUT_PORT_REGISTER);
	uint8_t output_reg;
	i2c_read_t read_output_reg = {.buff=&output_reg, .len=1};

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_output_reg, &read_output_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0, output_reg);

	// Write 0b10101010
	TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_write_all(i2c, PCA9534D_ADDRESS, 0b10101010), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_output_reg, &read_output_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0b10101010, output_reg);

	// Write 0
	TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_write_all(i2c, PCA9534D_ADDRESS, 0), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, PCA9534D_ADDRESS, &read_order_output_reg, &read_output_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0, output_reg);


        TEST_ASSERT_EQUAL_MESSAGE(0, pca9534d_deinit(i2c, PCA9534D_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

int main() {
	UNITY_BEGIN();

	RUN_TEST(pca9534d_init_sanity_check);
	RUN_TEST(pca9534d_deinit_sanity_check);
	RUN_TEST(pca9534d_init_deinit_cycle);

	RUN_TEST(pca9534d_set_pin_mode_test);

	RUN_TEST(pca9534d_set_pin_mode_all_test);

	RUN_TEST(pca9534d_read_test);

	RUN_TEST(pca9534d_write_test);

	RUN_TEST(pca9534d_read_all_test);

	RUN_TEST(pca9534d_write_all_test);

        return UNITY_END();
}

#endif // PLC_ENVIRONMENT == Linux
