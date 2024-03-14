#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux

#include <plc-peripherals.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MCP23008_FIRST_ADDR 0x20
#define MCP23008_SECOND_ADDR 0x21
#define MCP23008_NONEXIST_ADDR 0x22

#define MCP23008_IODIR_REG 0x00
#define MCP23008_OLAT_REG 0x0A

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
	// Test bad I2C bus argument
	i2c_interface_t* i2c = i2c_init(255);
	TEST_ASSERT_NULL(i2c);
	TEST_ASSERT_EQUAL_MESSAGE(ENOENT, errno, strerror(errno));
}

void i2c_deinit_sanity_check() {
	// Test NULL arguments
	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_deinit(NULL), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

        i2c_interface_t* dummy = NULL;
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_deinit(&dummy), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	// Test bad file descriptors
	i2c_interface_t* mock = malloc(sizeof(struct _i2c_interface_t));
	TEST_ASSERT_NOT_NULL_MESSAGE(mock, strerror(errno));
	mock->fd = -1;
	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_deinit(&mock), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EBADF, errno, strerror(errno));

	mock->fd = 99;
	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_deinit(&mock), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EBADF, errno, strerror(errno));

	free(mock);
}

void i2c_write_sanity_check() {
	// Create mocks
	void* mock = malloc(sizeof(i2c_interface_t));
	TEST_ASSERT_NOT_NULL_MESSAGE(mock, strerror(errno));
	const i2c_write_t write00 = {NULL, 0};
	const i2c_write_t write01 = {NULL, 1};
	const i2c_write_t write10 = {mock, 0};
	const i2c_write_t write11 = {mock, 1};


	// Test invalid file descriptor
        ((i2c_interface_t*) mock)->fd = -1;
	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write((void*) mock, 0x8F, &write00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EBADF, errno, strerror(errno));


	// Test NULL arguments
	((i2c_interface_t*) mock)->fd = 50;

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(NULL, 0x8F, &write00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(NULL, 0x8F, &write01), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(NULL, 0x8F, &write10), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(NULL, 0x8F, &write11), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(NULL, MCP23008_FIRST_ADDR, &write00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(NULL, MCP23008_FIRST_ADDR, &write01), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(NULL, MCP23008_FIRST_ADDR, &write10), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(NULL, MCP23008_FIRST_ADDR, &write11), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write((void*) mock, 0x8F, &write00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write((void*) mock, 0x8F, &write01), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write((void*) mock, 0x8F, &write10), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write((void*) mock, 0x8F, &write11), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write((void*) mock, MCP23008_FIRST_ADDR, &write00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write((void*) mock, MCP23008_FIRST_ADDR, &write01), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(1, i2c_write((void*) mock, MCP23008_FIRST_ADDR, &write10), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

	//TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write((void*) mock, MCP23008_FIRST_ADDR, &write11), strerror(errno));

	free(mock);
}

void i2c_read_sanity_check() {
	// Create mocks
	void* mock = malloc(sizeof(i2c_interface_t));
	TEST_ASSERT_NOT_NULL_MESSAGE(mock, strerror(errno));
	const i2c_read_t read00 = {NULL, 0};
	const i2c_read_t read01 = {NULL, 1};
	const i2c_read_t read10 = {mock, 0};
	const i2c_read_t read11 = {mock, 1};


	// Test invalid file descriptor
        ((i2c_interface_t*) mock)->fd = -1;
	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read((void*) mock, 0x8F, &read00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EBADF, errno, strerror(errno));


	// Test NULL arguments
	((i2c_interface_t*) mock)->fd = 50;

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(NULL, 0x8F, &read00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(NULL, 0x8F, &read01), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(NULL, 0x8F, &read10), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(NULL, 0x8F, &read11), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(NULL, MCP23008_FIRST_ADDR, &read00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(NULL, MCP23008_FIRST_ADDR, &read01), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(NULL, MCP23008_FIRST_ADDR, &read10), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(NULL, MCP23008_FIRST_ADDR, &read11), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read((void*) mock, 0x8F, &read00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read((void*) mock, 0x8F, &read01), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read((void*) mock, 0x8F, &read10), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read((void*) mock, 0x8F, &read11), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read((void*) mock, MCP23008_FIRST_ADDR, &read00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read((void*) mock, MCP23008_FIRST_ADDR, &read01), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(1, i2c_read((void*) mock, MCP23008_FIRST_ADDR, &read10), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

	//TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read((void*) mock, MCP23008_FIRST_ADDR, &read11), strerror(errno));

	free(mock);
}

void i2c_read_then_write_sanity_check() {
	// Create mocks
	void* mock = malloc(sizeof(i2c_interface_t));
	TEST_ASSERT_NOT_NULL_MESSAGE(mock, strerror(errno));
	const i2c_write_t read_order00 = {NULL, 0};
	const i2c_write_t read_order01 = {NULL, 1};
	const i2c_write_t read_order10 = {mock, 0};
	const i2c_write_t read_order11 = {mock, 1};
        i2c_read_t to_read00 = {NULL, 0};
	i2c_read_t to_read01 = {NULL, 1};
	i2c_read_t to_read10 = {mock, 0};
	i2c_read_t to_read11 = {mock, 1};


	// Test invalid file descriptor
        ((i2c_interface_t*) mock)->fd = -1;
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write((void*) mock, 0x8F, &read_order00), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EBADF, errno, strerror(errno));


	// Test NULL arguments
	((i2c_interface_t*) mock)->fd = 50;

	// Basic null check
	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write_then_read(mock, MCP23008_FIRST_ADDR, NULL, NULL), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write_then_read(mock, MCP23008_FIRST_ADDR, NULL, &to_read11), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write_then_read(mock, MCP23008_FIRST_ADDR, &read_order11, NULL), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read11), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	// Sanity checks
	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order00, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order00, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order00, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order00, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order00, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order00, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order00, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order00, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order01, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order01, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order01, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order01, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order01, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order01, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order01, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order01, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order10, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order10, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order10, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order10, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order10, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order10, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order10, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order10, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order11, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order11, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order11, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order11, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order11, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order11, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order11, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, 0x8F, &read_order11, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));



	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order00, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order00, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order00, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order00, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order00, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order00, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order00, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order00, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order01, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order01, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order01, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order01, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order01, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order01, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order01, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order01, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order10, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order10, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order10, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order10, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order10, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order10, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order10, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order10, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read00));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read01));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read10));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));

	TEST_ASSERT_EQUAL(-1, i2c_write_then_read(NULL, MCP23008_FIRST_ADDR, &read_order11, &to_read11));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));


	free(mock);
}

void i2c_init_deinit_cycle() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));

        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void i2c_write_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


        // MCP23008 test
        FAST_CREATE_I2C_WRITE(set_output, 0x00, 0x00);
        FAST_CREATE_I2C_WRITE(pull_high, MCP23008_OLAT_REG, 0xFF);
        FAST_CREATE_I2C_WRITE(pull_down, MCP23008_OLAT_REG, 0x00);

        // First MCP23008 test
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_high), strerror(errno));
        usleep(0.5*1e6);
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_down), strerror(errno));

        // Second MCP23008 test
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &pull_high), strerror(errno));
        usleep(0.5*1e6);
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_down), strerror(errno));

        // Non-existant MCP23008
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &pull_high), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &pull_down), strerror(errno));


        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void i2c_read_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


        // MCP23008 test
        const size_t read_order_iodir_len = 1;
        uint8_t* read_order_iodir_buff = malloc(read_order_iodir_len);
        TEST_ASSERT_NOT_NULL_MESSAGE(read_order_iodir_buff, strerror(errno));
        read_order_iodir_buff[0] = MCP23008_IODIR_REG;
        const i2c_write_t read_order_iodir = {.buff=read_order_iodir_buff, .len=read_order_iodir_len};

        const size_t read_order_olat_len = 1;
        uint8_t* read_order_olat_buff = malloc(read_order_olat_len);
        TEST_ASSERT_NOT_NULL_MESSAGE(read_order_olat_buff, strerror(errno));
        read_order_olat_buff[0] = MCP23008_OLAT_REG;
        const i2c_write_t read_order_olat = {.buff=read_order_olat_buff, .len=read_order_olat_len};

        const size_t to_read_len = 1;
        uint8_t* to_read_buff = malloc(to_read_len);
        TEST_ASSERT_NOT_NULL_MESSAGE(to_read_buff, strerror(errno));
        to_read_buff[0] = 0xFF;
        const i2c_read_t to_read = {.buff=to_read_buff, .len=to_read_len};

        FAST_CREATE_I2C_WRITE(set_output, 0x00, 0x00);
        FAST_CREATE_I2C_WRITE(pull_some_high, MCP23008_OLAT_REG, 0b10101010);
        FAST_CREATE_I2C_WRITE(pull_some_down, MCP23008_OLAT_REG, 0b01010101);
        FAST_CREATE_I2C_WRITE(pull_down, MCP23008_OLAT_REG, 0x00);

        // First MCP23008 test
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &read_order_iodir), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read(i2c, MCP23008_FIRST_ADDR, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&set_output.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_some_high), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read(i2c, MCP23008_FIRST_ADDR, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_some_high.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_some_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read(i2c, MCP23008_FIRST_ADDR, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_some_down.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read(i2c, MCP23008_FIRST_ADDR, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_down.buff[1], to_read.buff, 1, strerror(errno));


        // Second MCP23008 test
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &read_order_iodir), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read(i2c, MCP23008_SECOND_ADDR, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&set_output.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &pull_some_high), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read(i2c, MCP23008_SECOND_ADDR, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_some_high.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &pull_some_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read(i2c, MCP23008_SECOND_ADDR, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_some_down.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &pull_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_read(i2c, MCP23008_SECOND_ADDR, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_down.buff[1], to_read.buff, 1, strerror(errno));


        // Non-existant MCP23008 test
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &read_order_iodir), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(i2c, MCP23008_NONEXIST_ADDR, &to_read), strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &pull_some_high), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(i2c, MCP23008_NONEXIST_ADDR, &to_read), strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &pull_some_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(i2c, MCP23008_NONEXIST_ADDR, &to_read), strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &pull_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &read_order_olat), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_read(i2c, MCP23008_NONEXIST_ADDR, &to_read), strerror(errno));


        free(read_order_iodir_buff);
        free(read_order_olat_buff);
        free(to_read_buff);

        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void i2c_read_then_write_test() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


        // MCP23008 test
        const size_t read_order_iodir_len = 1;
        uint8_t* read_order_iodir_buff = malloc(read_order_iodir_len);
        TEST_ASSERT_NOT_NULL_MESSAGE(read_order_iodir_buff, strerror(errno));
        read_order_iodir_buff[0] = MCP23008_IODIR_REG;
        const i2c_write_t read_order_iodir = {.buff=read_order_iodir_buff, .len=read_order_iodir_len};

        const size_t read_order_olat_len = 1;
        uint8_t* read_order_olat_buff = malloc(read_order_olat_len);
        TEST_ASSERT_NOT_NULL_MESSAGE(read_order_olat_buff, strerror(errno));
        read_order_olat_buff[0] = MCP23008_OLAT_REG;
        const i2c_write_t read_order_olat = {.buff=read_order_olat_buff, .len=read_order_olat_len};

        const size_t to_read_len = 1;
        uint8_t* to_read_buff = malloc(to_read_len);
        TEST_ASSERT_NOT_NULL_MESSAGE(to_read_buff, strerror(errno));
        to_read_buff[0] = 0xFF;
        const i2c_read_t to_read = {.buff=to_read_buff, .len=to_read_len};

        FAST_CREATE_I2C_WRITE(set_output, 0x00, 0x00);
        FAST_CREATE_I2C_WRITE(pull_some_high, MCP23008_OLAT_REG, 0b10101010);
        FAST_CREATE_I2C_WRITE(pull_some_down, MCP23008_OLAT_REG, 0b01010101);
        FAST_CREATE_I2C_WRITE(pull_down, MCP23008_OLAT_REG, 0x00);

        // First MCP23008 test
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_FIRST_ADDR, &read_order_iodir, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&set_output.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_some_high), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_FIRST_ADDR, &read_order_olat, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_some_high.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_some_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_FIRST_ADDR, &read_order_olat, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_some_down.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_FIRST_ADDR, &pull_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_FIRST_ADDR, &read_order_olat, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_down.buff[1], to_read.buff, 1, strerror(errno));


        // Second MCP23008 test
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_SECOND_ADDR, &read_order_iodir, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&set_output.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &pull_some_high), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_SECOND_ADDR, &read_order_olat, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_some_high.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &pull_some_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_SECOND_ADDR, &read_order_olat, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_some_down.buff[1], to_read.buff, 1, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write(i2c, MCP23008_SECOND_ADDR, &pull_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, i2c_write_then_read(i2c, MCP23008_SECOND_ADDR, &read_order_olat, &to_read), strerror(errno));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&pull_down.buff[1], to_read.buff, 1, strerror(errno));


        // Non-existant MCP23008 test
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &set_output), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write_then_read(i2c, MCP23008_NONEXIST_ADDR, &read_order_iodir, &to_read), strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &pull_some_high), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write_then_read(i2c, MCP23008_NONEXIST_ADDR, &read_order_olat, &to_read), strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &pull_some_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write_then_read(i2c, MCP23008_NONEXIST_ADDR, &read_order_olat, &to_read), strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write(i2c, MCP23008_NONEXIST_ADDR, &pull_down), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(-1, i2c_write_then_read(i2c, MCP23008_NONEXIST_ADDR, &read_order_olat, &to_read), strerror(errno));

        free(read_order_iodir_buff);
        free(read_order_olat_buff);
        free(to_read_buff);

        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

int main() {
	UNITY_BEGIN();

	RUN_TEST(i2c_init_sanity_check);
	RUN_TEST(i2c_deinit_sanity_check);
	RUN_TEST(i2c_init_deinit_cycle);

	RUN_TEST(i2c_write_sanity_check);
	RUN_TEST(i2c_write_test);

	RUN_TEST(i2c_read_sanity_check);
	RUN_TEST(i2c_read_test);

	RUN_TEST(i2c_read_then_write_sanity_check);
	RUN_TEST(i2c_read_then_write_test);

        return UNITY_END();
}

#endif // defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
