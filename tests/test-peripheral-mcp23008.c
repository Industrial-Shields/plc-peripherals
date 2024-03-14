#include "../include/plc-peripherals.h"

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux

#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MCP23008_ADDRESS 0x20
#define IODIR_REGISTER 0x00
#define GPIO_REGISTER 0x09

#include <unity.h>
void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void mcp23008_init_sanity_check() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, mcp23008_init(NULL, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, mcp23008_init(NULL, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, mcp23008_init(i2c, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, mcp23008_init(i2c, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EIO, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void mcp23008_deinit_sanity_check() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, mcp23008_deinit(NULL, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, mcp23008_deinit(NULL, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, mcp23008_deinit(i2c, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, mcp23008_deinit(i2c, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EIO, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void mcp23008_init_deinit_cycle() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));

        mcp23008_deinit(i2c, MCP23008_ADDRESS);

        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_init(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_deinit(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));


        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void mcp23008_set_pin_mode_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_init(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        // Set pin to output
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_set_pin_mode(i2c, MCP23008_ADDRESS, 0x07, 0x00), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_iodir_reg, IODIR_REGISTER);
	uint8_t iodir;
	i2c_read_t read_iodir_reg = {.buff=&iodir, .len=1};
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_iodir_reg, &read_iodir_reg), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0b01111111, iodir, strerror(errno));


	// Set pin to input
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_set_pin_mode(i2c, MCP23008_ADDRESS, 0x07, 0x01), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_iodir_reg, &read_iodir_reg), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0b11111111, iodir, strerror(errno));


        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_deinit(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void mcp23008_set_pin_mode_all_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_init(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        // Set pin modes to 0b01011010 (1 is input, 0 is output)
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_set_pin_mode_all(i2c, MCP23008_ADDRESS, 0b01011010), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_iodir_reg, IODIR_REGISTER);
	uint8_t iodir;
	i2c_read_t read_iodir_reg = {.buff=&iodir, .len=1};
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_iodir_reg, &read_iodir_reg), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0b01011010, iodir, strerror(errno));


	// Set pin modes to 0
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_set_pin_mode_all(i2c, MCP23008_ADDRESS, 0b11111111), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_iodir_reg, &read_iodir_reg), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0b11111111, iodir, strerror(errno));


        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_deinit(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void mcp23008_read_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_init(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_set_pin_mode(i2c, MCP23008_ADDRESS, 0x05, 0x01), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_gpio_reg, GPIO_REGISTER);
	uint8_t gpio_reg;
	i2c_read_t read_gpio_reg = {.buff=&gpio_reg, .len=1};

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_gpio_reg, &read_gpio_reg), strerror(errno));

	uint8_t value = 0b11111111;
	TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_read(i2c, MCP23008_ADDRESS, 5, &value), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(value, (gpio_reg >> 5) & 0b00000001, strerror(errno));
	// printf("Value in channel 5: %d; Register value: %d\n", value, gpio_reg);



        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_deinit(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void mcp23008_write_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_init(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_set_pin_mode(i2c, MCP23008_ADDRESS, 0x05, 0x00), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_gpio_reg, GPIO_REGISTER);
	uint8_t gpio_reg;
	i2c_read_t read_gpio_reg = {.buff=&gpio_reg, .len=1};

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_gpio_reg, &read_gpio_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0, gpio_reg);

	// Write 1
	TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_write(i2c, MCP23008_ADDRESS, 0x05, 1), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_gpio_reg, &read_gpio_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0b00100000, gpio_reg);

	// Write 0
	TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_write(i2c, MCP23008_ADDRESS, 0x05, 0), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_gpio_reg, &read_gpio_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0, gpio_reg);


        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_deinit(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void mcp23008_read_all_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_init(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_set_pin_mode_all(i2c, MCP23008_ADDRESS, 0b11111111), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_gpio_reg, GPIO_REGISTER);
	uint8_t gpio_reg;
	i2c_read_t read_gpio_reg = {.buff=&gpio_reg, .len=1};

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_gpio_reg, &read_gpio_reg), strerror(errno));

	uint8_t value = 0b11111111;
	TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_read_all(i2c, MCP23008_ADDRESS, &value), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(value, gpio_reg, strerror(errno));
	// printf("Value in channel 5: %d; Register value: %d\n", value, gpio_reg);



        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_deinit(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void mcp23008_write_all_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_init(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_set_pin_mode_all(i2c, MCP23008_ADDRESS, 0), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_gpio_reg, GPIO_REGISTER);
	uint8_t gpio_reg;
	i2c_read_t read_gpio_reg = {.buff=&gpio_reg, .len=1};

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_gpio_reg, &read_gpio_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0, gpio_reg);

	// Write 0b10101010
	TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_write_all(i2c, MCP23008_ADDRESS, 0b10101010), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_gpio_reg, &read_gpio_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0b10101010, gpio_reg);

	// Write 0
	TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_write_all(i2c, MCP23008_ADDRESS, 0), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_ADDRESS, &read_order_gpio_reg, &read_gpio_reg), strerror(errno));
	TEST_ASSERT_EQUAL(0, gpio_reg);


        TEST_ASSERT_EQUAL_MESSAGE(0, mcp23008_deinit(i2c, MCP23008_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

int main() {
	UNITY_BEGIN();

	RUN_TEST(mcp23008_init_sanity_check);
	RUN_TEST(mcp23008_deinit_sanity_check);
	RUN_TEST(mcp23008_init_deinit_cycle);

	RUN_TEST(mcp23008_set_pin_mode_test);

	RUN_TEST(mcp23008_set_pin_mode_all_test);

	RUN_TEST(mcp23008_read_test);

	RUN_TEST(mcp23008_write_test);

	RUN_TEST(mcp23008_read_all_test);

	RUN_TEST(mcp23008_write_all_test);

        return UNITY_END();
}

#endif // PLC_ENVIRONMENT == Linux
