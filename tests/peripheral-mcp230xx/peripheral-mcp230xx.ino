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
#define I0_0                  0x06
#define GPIO_0                0x07
#elif defined(PLC14IOS)
#define TO_TEST               TEST_MCP_23017
#define MCP230XX_ADDR         0x20
#define MCP230XX_CHIP_TYPE    MCP230XX_017
#define MCP230XX_N_REGISTERS  22
#define I0_0                  0x0B
#define R0_0                  0x07
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

void are_mcp230xx_registers_correct(const uint8_t expected[MCP230XX_N_REGISTERS])
{
	uint8_t mcp230xx_registers[MCP230XX_N_REGISTERS];
	size_t mcp230xx_read_bytes;

	TEST_ASSERT_EQUAL(1,
			  i2c_write_then_read(i2c_iface,
					      MCP230XX_ADDR,
					      (const uint8_t[]){ 0 },
					      1,
					      mcp230xx_registers,
					      sizeof(mcp230xx_registers),
					      &mcp230xx_read_bytes));
	TEST_ASSERT_EQUAL(mcp230xx_read_bytes, sizeof(mcp230xx_registers));
	TEST_ASSERT_EQUAL_MEMORY(
		expected, mcp230xx_registers, sizeof(mcp230xx_registers));
}

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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0x04,
		0x04, 0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));

#if TO_TEST == TEST_MCP_23008
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif
}

void test_mcp230xx_set_input_output_locked(void)
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x01, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x01 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
		0xFD, 0xFD, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x01, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x01 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));
}

void test_mcp230xx_set_input_output(void)
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x01, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_output(mcp, 0x01 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
		0xFD, 0xFD, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x01, 100));
#if TO_TEST == TEST_MCP_23017
	TEST_ASSERT_EQUAL(0, mcp230xx_set_input(mcp, 0x01 + 0x08, 100));
#endif

#if TO_TEST == TEST_MCP_23008
	are_mcp230xx_registers_correct(((const uint8_t[]){
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
	are_mcp230xx_registers_correct(((const uint8_t[]){
		0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
	}));
#else
#error "Invalid MCP type"
#endif

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
		RUN_TEST(test_mcp230xx_set_input_output);
	} while (--n);

	n = 10;
	do {
		RUN_TEST(test_mcp230xx_set_input_output_locked);
	} while (--n);

	UNITY_END();
}

void loop()
{
}
