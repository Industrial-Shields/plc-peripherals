#include "peripheral-mcp230xx.h"
#include "plc-peripherals-i2c.h"
#include "unity.h"
#include "plc-peripherals.h"
#include "plc-peripherals-platform.h"
#include <assert.h>

static i2c_interface_t* i2c_iface;

#define TEST_MCP_23008 0
#define TEST_MCP_23017 1

// clang-format off
#if defined(ESP32PLC)
#define TO_TEST               TEST_MCP_23008
#define MCP230XX_ADDR         0x21
#define MCP230XX_CHIP_TYPE    MCP230XX_008
#define MCP230XX_N_REGISTERS  11
#define MCP230XX_INPUT        0x06 // I0.0
#define MCP230XX_OUTPUT       0x07 // GPIO 0
#elif defined(PLC14IOS)
#define TO_TEST               TEST_MCP_23017
#define MCP230XX_ADDR         0x20
#define MCP230XX_CHIP_TYPE    MCP230XX_017
#define MCP230XX_N_REGISTERS  22
#define MCP230XX_INPUT        0x0B // I0.0
// Connect 5V to one port of the relay
#define MCP230XX_OUTPUT       0x07 // R0.0
#else
#error "PLC not supported"
#endif
// clang-format on

void setUp(void)
{
	i2c_iface = i2c_init(0, SDA, SCL);
	assert(i2c_iface);

	assert(plc_resource_init() == 0);
}

void tearDown(void)
{
	assert(plc_resource_deinit() == 0);

	int result = i2c_deinit(i2c_iface, false);
	assert(result == 0);
}

#define ARE_MCP230XX_REGISTERS_CORRECT(expected_array)                  \
	do {                                                            \
		uint8_t mcp230xx_registers[MCP230XX_N_REGISTERS];       \
		size_t mcp230xx_read_bytes;                             \
                                                                        \
		TEST_ASSERT_EQUAL(                                      \
			1,                                              \
			i2c_write_then_read(i2c_iface,                  \
					    MCP230XX_ADDR,              \
					    (const uint8_t[]){ 0 },     \
					    1,                          \
					    mcp230xx_registers,         \
					    sizeof(mcp230xx_registers), \
					    &mcp230xx_read_bytes));     \
                                                                        \
		TEST_ASSERT_EQUAL(mcp230xx_read_bytes,                  \
				  sizeof(mcp230xx_registers));          \
		TEST_ASSERT_EQUAL_MEMORY(expected_array,                \
					 mcp230xx_registers,            \
					 sizeof(mcp230xx_registers));   \
	} while (0)

void test_mcp230xx_init_deinit(void)
{
	mcp230xx_t* mcp = mcp230xx_init(i2c_iface,
					MCP230XX_ADDR,
					true,
					MCP230XX_CHIP_TYPE,
					false,
					MCP230XX_ACTIVE_DRIVER_INT,
					MCP230XX_INT_ACTIVE_LOW);
	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, false));

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	mcp = mcp230xx_init(i2c_iface,
			    MCP230XX_ADDR,
			    false,
			    MCP230XX_CHIP_TYPE,
			    true,
			    MCP230XX_OPEN_DRAIN_INT,
			    MCP230XX_INT_POLARITY_NONE);
	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, false));

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0x14,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0x14,
		0x14, 0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	mcp = mcp230xx_init(i2c_iface,
			    MCP230XX_ADDR,
			    false,
			    MCP230XX_CHIP_TYPE,
			    false,
			    MCP230XX_ACTIVE_DRIVER_INT,
			    MCP230XX_INT_ACTIVE_HIGH);
	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, false));

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0x02,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0x02,
		0x02, 0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	mcp = mcp230xx_init(i2c_iface,
			    MCP230XX_ADDR,
			    false,
			    MCP230XX_CHIP_TYPE,
			    false,
			    MCP230XX_OPEN_DRAIN_INT,
			    MCP230XX_INT_POLARITY_NONE);
	TEST_ASSERT_NOT_NULL(mcp);

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0x04,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0x04,
		0x04, 0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif
}

void test_mcp230xx_set_output(void)
{
	mcp230xx_t* mcp = mcp230xx_init(i2c_iface,
					MCP230XX_ADDR,
					true,
					MCP230XX_CHIP_TYPE,
					false,
					MCP230XX_ACTIVE_DRIVER_INT,
					MCP230XX_INT_ACTIVE_LOW);
	TEST_ASSERT_NOT_NULL(mcp);

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x01, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_output(mcp, 0x01, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x01 + 0x08, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_output(mcp, 0x01 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFD,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFD, 0xFD, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x07, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_output(mcp, 0x07, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x07 + 0x08, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_output(mcp, 0x07 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x7D,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x7D, 0x7D, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif
}

void test_mcp230xx_set_output_locked(void)
{
	mcp230xx_t* mcp = mcp230xx_init(i2c_iface,
					MCP230XX_ADDR,
					true,
					MCP230XX_CHIP_TYPE,
					false,
					MCP230XX_ACTIVE_DRIVER_INT,
					MCP230XX_INT_ACTIVE_LOW);
	TEST_ASSERT_NOT_NULL(mcp);

	TEST_ASSERT_EQUAL(0, mcp230xx_protect(mcp));
	TEST_ASSERT_EQUAL(1, mcp230xx_protect(mcp));
	TEST_ASSERT_EQUAL(EEXIST, errno);
	errno = 0;

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x01, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_output(mcp, 0x01, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x01 + 0x08, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_output(mcp, 0x01 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFD,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFD, 0xFD, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x07, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_output(mcp, 0x07, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x07 + 0x08, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_output(mcp, 0x07 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x7D,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x7D, 0x7D, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif
}

void test_mcp230xx_set_input(void)
{
	mcp230xx_t* mcp = mcp230xx_init(i2c_iface,
					MCP230XX_ADDR,
					true,
					MCP230XX_CHIP_TYPE,
					false,
					MCP230XX_ACTIVE_DRIVER_INT,
					MCP230XX_INT_ACTIVE_LOW);
	TEST_ASSERT_NOT_NULL(mcp);

	// Set all pins as outputs
	TEST_ASSERT_EQUAL(0, i2c_write8_8b(i2c_iface, MCP230XX_ADDR, 0x00, 0));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, i2c_write8_8b(i2c_iface, MCP230XX_ADDR, 0x01, 0));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x01, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, 0x01, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x01 + 0x08, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, 0x01 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x02,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x02, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x07, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, 0x07, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x07 + 0x08, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, 0x07 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x82,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x82, 0x82, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif
}

void test_mcp230xx_set_input_locked(void)
{
	mcp230xx_t* mcp = mcp230xx_init(i2c_iface,
					MCP230XX_ADDR,
					true,
					MCP230XX_CHIP_TYPE,
					false,
					MCP230XX_ACTIVE_DRIVER_INT,
					MCP230XX_INT_ACTIVE_LOW);
	TEST_ASSERT_NOT_NULL(mcp);

	TEST_ASSERT_EQUAL(0, mcp230xx_protect(mcp));
	TEST_ASSERT_EQUAL(1, mcp230xx_protect(mcp));
	TEST_ASSERT_EQUAL(EEXIST, errno);
	errno = 0;

	// Set all pins as outputs
	TEST_ASSERT_EQUAL(0, i2c_write8_8b(i2c_iface, MCP230XX_ADDR, 0x00, 0));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, i2c_write8_8b(i2c_iface, MCP230XX_ADDR, 0x01, 0));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x01, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, 0x01, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x01 + 0x08, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, 0x01 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x02,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x02, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x07, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, 0x07, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x07 + 0x08, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, 0x07 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x82,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0x82, 0x82, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));

#if TO_TEST == TEST_MCP_23008
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
	}));
#elif TO_TEST == TEST_MCP_23017
	ARE_MCP230XX_REGISTERS_CORRECT(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif
}

void test_mcp230xx_write_read_gpios(void)
{
	mcp230xx_t* mcp = mcp230xx_init(i2c_iface,
					MCP230XX_ADDR,
					true,
					MCP230XX_CHIP_TYPE,
					false,
					MCP230XX_ACTIVE_DRIVER_INT,
					MCP230XX_INT_ACTIVE_LOW);
	TEST_ASSERT_NOT_NULL(mcp);

	uint8_t read_value;

	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, MCP230XX_OUTPUT, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, MCP230XX_INPUT, 100));

	TEST_ASSERT_EQUAL(
		1,
		mcp230xx_write_gpio(mcp, MCP230XX_OUTPUT, MCP230XX_LOW, 100));
	usleep(100000); // 100ms
	TEST_ASSERT_EQUAL(
		0, mcp230xx_read_gpio(mcp, MCP230XX_OUTPUT, &read_value, 100));
	usleep(100000); // 100ms

	TEST_ASSERT_EQUAL(
		0,
		mcp230xx_write_gpio(mcp, MCP230XX_OUTPUT, MCP230XX_HIGH, 100));
	usleep(100000); // 100ms
	TEST_ASSERT_EQUAL(
		0, mcp230xx_read_gpio(mcp, MCP230XX_OUTPUT, &read_value, 100));
	TEST_ASSERT_EQUAL(MCP230XX_HIGH, read_value);
	usleep(100000); // 100ms

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));
}

void test_mcp230xx_write_read_gpios_locked(void)
{
	mcp230xx_t* mcp = mcp230xx_init(i2c_iface,
					MCP230XX_ADDR,
					true,
					MCP230XX_CHIP_TYPE,
					false,
					MCP230XX_ACTIVE_DRIVER_INT,
					MCP230XX_INT_ACTIVE_LOW);
	TEST_ASSERT_NOT_NULL(mcp);

	TEST_ASSERT_EQUAL(0, mcp230xx_protect(mcp));
	TEST_ASSERT_EQUAL(1, mcp230xx_protect(mcp));
	TEST_ASSERT_EQUAL(EEXIST, errno);
	errno = 0;

	uint8_t read_value;

	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, MCP230XX_OUTPUT, 100));
	TEST_ASSERT_EQUAL(1, mcp230xx_set_input(mcp, MCP230XX_INPUT, 100));

	TEST_ASSERT_EQUAL(
		1,
		mcp230xx_write_gpio(mcp, MCP230XX_OUTPUT, MCP230XX_LOW, 100));
	usleep(100000); // 100ms
	TEST_ASSERT_EQUAL(
		0, mcp230xx_read_gpio(mcp, MCP230XX_OUTPUT, &read_value, 100));
	usleep(100000); // 100ms

	TEST_ASSERT_EQUAL(
		0,
		mcp230xx_write_gpio(mcp, MCP230XX_OUTPUT, MCP230XX_HIGH, 100));
	usleep(100000); // 100ms
	TEST_ASSERT_EQUAL(
		0, mcp230xx_read_gpio(mcp, MCP230XX_OUTPUT, &read_value, 100));
	TEST_ASSERT_EQUAL(MCP230XX_HIGH, read_value);
	usleep(100000); // 100ms

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));
}

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32
void setup()
{
	Serial.begin(1000000);
#else
int main(void)
{
#endif

	// Wait a second to let the chips initialize.
	sleep(1);

	UNITY_BEGIN();

	int n;

	n = 100;
	do {
		RUN_TEST(test_mcp230xx_init_deinit);
	} while (--n);

	n = 10;
	do {
		RUN_TEST(test_mcp230xx_set_output);
	} while (--n);

	n = 10;
	do {
		RUN_TEST(test_mcp230xx_set_output_locked);
	} while (--n);

	n = 10;
	do {
		RUN_TEST(test_mcp230xx_set_input);
	} while (--n);

	n = 10;
	do {
		RUN_TEST(test_mcp230xx_set_input_locked);
	} while (--n);

	n = 5;
	do {
		RUN_TEST(test_mcp230xx_write_read_gpios);
	} while (--n);

	n = 5;
	do {
		RUN_TEST(test_mcp230xx_write_read_gpios_locked);
	} while (--n);

	UNITY_END();
}

void loop()
{
}
