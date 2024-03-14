#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux

#include <plc-peripherals.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define PCA9685_ADDRESS 0x40
#define LED0_ON_L_REGISTER 0x06

#include <unity.h>
void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void pca9685_init_sanity_check() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9685_init(NULL, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9685_init(NULL, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9685_init(i2c, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9685_init(i2c, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EIO, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9685_deinit_sanity_check() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9685_deinit(NULL, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9685_deinit(NULL, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9685_deinit(i2c, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, pca9685_deinit(i2c, 0x00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EIO, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9685_init_deinit_cycle() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));

        // Check if PCA9685 is already initialized, and de-initialize it
        int i2c_ret = 0;
        if (i2c_ret == 0 || (i2c_ret == 1 && errno == EALREADY)) {
	        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_deinit(i2c, PCA9685_ADDRESS), strerror(errno));
        }

        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_init(i2c, PCA9685_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_deinit(i2c, PCA9685_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));


        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9685_set_out_tests() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_init(i2c, PCA9685_ADDRESS), strerror(errno));


        // First test
        for (size_t c = 0; c < PCA9685_NUM_OUTPUTS; c++) {
	        if (c % 2 == 0) {
		        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_set_out_on(i2c, PCA9685_ADDRESS, c), strerror(errno));
	        }
	        else {
		        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_set_out_off(i2c, PCA9685_ADDRESS, c), strerror(errno));
	        }
        }

        // Read the written values
        FAST_CREATE_I2C_WRITE(read_order_leds, LED0_ON_L_REGISTER);

        uint8_t leds[PCA9685_NUM_OUTPUTS*4];
        const i2c_read_t read_leds = {.buff=leds, .len=sizeof(leds)};

        i2c_write_then_read(i2c, PCA9685_ADDRESS, &read_order_leds, &read_leds);

        uint8_t expected_first[] = {0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000};

        TEST_ASSERT_EQUAL_MEMORY(expected_first, leds, sizeof(leds));


        // Second test
        for (size_t c = 0; c < PCA9685_NUM_OUTPUTS; c++) {
	        if (c % 2 == 0) {
		        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_set_out_off(i2c, PCA9685_ADDRESS, c), strerror(errno));
	        }
	        else {
		        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_set_out_on(i2c, PCA9685_ADDRESS, c), strerror(errno));
	        }
        }


        // Read the written values
        i2c_write_then_read(i2c, PCA9685_ADDRESS, &read_order_leds, &read_leds);

        uint8_t expected_second[] = {0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0};

        TEST_ASSERT_EQUAL_MEMORY(expected_second, leds, sizeof(leds));


        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_deinit(i2c, PCA9685_ADDRESS), strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9685_set_out_pwm_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_init(i2c, PCA9685_ADDRESS), strerror(errno));

        uint16_t pwm_values[] = {0, 256, 512, 768, 2198, 3123, 1347, 2789, 865, 3920, 1786, 4021, 587, 3840, 3840, 4095};
        for (size_t c = 0; c < PCA9685_NUM_OUTPUTS; c++) {
	        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_set_out_pwm(i2c, PCA9685_ADDRESS, c, pwm_values[c]), strerror(errno));
        }

        FAST_CREATE_I2C_WRITE(read_order_leds, LED0_ON_L_REGISTER);

        uint8_t leds[PCA9685_NUM_OUTPUTS*4];
        const i2c_read_t read_leds = {.buff=leds, .len=sizeof(leds)};

        i2c_write_then_read(i2c, PCA9685_ADDRESS, &read_order_leds, &read_leds);

        uint8_t expected[] = {0, 0, 0, 0, 0, 0, 0, 0b1, 0, 0, 0, 0b10, 0, 0, 0, 0b11, 0, 0, 0b10010110, 0b1000, 0, 0, 0b110011, 0b1100, 0, 0, 0b1000011, 0b101, 0, 0, 0b11100101, 0b1010, 0, 0, 0b1100001, 0b11, 0, 0, 0b1010000, 0b1111, 0, 0, 0b11111010, 0b110, 0, 0, 0b10110101, 0b1111, 0, 0, 0b1001011, 0b10, 0, 0, 0, 0b1111, 0, 0, 0, 0b1111, 0, 0, 0b11111111, 0b1111};

        TEST_ASSERT_EQUAL_MEMORY(expected, leds, sizeof(leds));

        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_deinit(i2c, PCA9685_ADDRESS), strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9685_set_all_digital_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_init(i2c, PCA9685_ADDRESS), strerror(errno));

        uint16_t values = 0b1010101001010101;
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_set_all_digital(i2c, PCA9685_ADDRESS, values), strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_leds, LED0_ON_L_REGISTER);

        uint8_t leds[PCA9685_NUM_OUTPUTS*4];
        const i2c_read_t read_leds = {.buff=leds, .len=sizeof(leds)};

        i2c_write_then_read(i2c, PCA9685_ADDRESS, &read_order_leds, &read_leds);

        uint8_t expected[] = {0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0, 0, 0, 0, 0b10000, 0, 0b10000, 0, 0};

        TEST_ASSERT_EQUAL_MEMORY(expected, leds, sizeof(leds));

        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_deinit(i2c, PCA9685_ADDRESS), strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void pca9685_set_all_analog_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_init(i2c, PCA9685_ADDRESS), strerror(errno));

        const uint16_t values[] = {0, 256, 512, 768, 2198, 3123, 1347, 2789, 865, 3920, 1786, 4021, 587, 3840, 3840, 4095};
        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_set_all_analog(i2c, PCA9685_ADDRESS, values), strerror(errno));

        FAST_CREATE_I2C_WRITE(read_order_leds, LED0_ON_L_REGISTER);

        uint8_t leds[PCA9685_NUM_OUTPUTS*4];
        const i2c_read_t read_leds = {.buff=leds, .len=sizeof(leds)};

        i2c_write_then_read(i2c, PCA9685_ADDRESS, &read_order_leds, &read_leds);

        uint8_t expected[] = {0, 0, 0, 0, 0, 0, 0, 0b1, 0, 0, 0, 0b10, 0, 0, 0, 0b11, 0, 0, 0b10010110, 0b1000, 0, 0, 0b110011, 0b1100, 0, 0, 0b1000011, 0b101, 0, 0, 0b11100101, 0b1010, 0, 0, 0b1100001, 0b11, 0, 0, 0b1010000, 0b1111, 0, 0, 0b11111010, 0b110, 0, 0, 0b10110101, 0b1111, 0, 0, 0b1001011, 0b10, 0, 0, 0, 0b1111, 0, 0, 0, 0b1111, 0, 0, 0b11111111, 0b1111};

        TEST_ASSERT_EQUAL_MEMORY(expected, leds, sizeof(leds));

        TEST_ASSERT_EQUAL_MESSAGE(0, pca9685_deinit(i2c, PCA9685_ADDRESS), strerror(errno));
        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

int main() {
	UNITY_BEGIN();


	RUN_TEST(pca9685_init_sanity_check);
	RUN_TEST(pca9685_deinit_sanity_check);
	RUN_TEST(pca9685_init_deinit_cycle);

	RUN_TEST(pca9685_set_out_tests);

	RUN_TEST(pca9685_set_out_pwm_test);

	RUN_TEST(pca9685_set_all_digital_test);

	RUN_TEST(pca9685_set_all_analog_test);

        return UNITY_END();
}

#endif // PLC_ENVIRONMENT == Linux
