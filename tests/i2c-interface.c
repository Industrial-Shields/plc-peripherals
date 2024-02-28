#include <i2c-interface.h>
#include <errno.h>
#include <string.h>

struct _i2c_interface_t {
	int fd;
};

#include <unity.h>
void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void i2c_init_sanity_check() {
	i2c_interface_t* i2c = i2c_init(255);
	TEST_ASSERT_NULL(i2c);
	TEST_ASSERT(errno == ENOENT);
}

void i2c_deinit_sanity_check() {
	TEST_ASSERT(i2c_deinit(NULL) == -1);
	TEST_ASSERT(errno == EFAULT);
	
        i2c_interface_t* dummy = NULL;
        TEST_ASSERT(i2c_deinit(&dummy) == -1);
	TEST_ASSERT(errno == EFAULT);

	
	i2c_interface_t* mock = malloc(sizeof(struct _i2c_interface_t));
	TEST_ASSERT_NOT_NULL_MESSAGE(mock, strerror(errno));
	mock->fd = -1;
	TEST_ASSERT(i2c_deinit(&mock) == -1);
	TEST_ASSERT(errno == EBADF);

	mock->fd = 99;
	TEST_ASSERT(i2c_deinit(&mock) == -1);
	TEST_ASSERT(errno == EBADF);

	free(mock);
}

void i2c_init_deinit_cycle() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));

        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

int main() {
	UNITY_BEGIN();

	RUN_TEST(i2c_init_sanity_check);
	RUN_TEST(i2c_deinit_sanity_check);
	RUN_TEST(i2c_init_deinit_cycle);
	
        return UNITY_END();
}
