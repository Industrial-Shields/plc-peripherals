#include "peripheral-mcp230xx.h"
#include "plc-peripherals-i2c.h"
#include "unity.h"
#include "plc-peripherals.h"
#include "plc-peripherals-platform.h"
#include <assert.h>

static i2c_interface_t* i2c_iface;

#if defined(ESP32PLC)
// clang-format off
#define MCP230XX_ADDR         0x21
#define MCP230XX_CHIP_TYPE    MCP230XX_008
#define I0_0                  0x06
#define GPIO_0                0x07
// clang-format on
#else
#error "PLC not supported"
#endif

void setUp(void)
{
	i2c_iface = i2c_init(0, SDA, SCL);
	assert(i2c_iface);
}

void tearDown(void)
{
	int result = i2c_deinit(i2c_iface, false);
	assert(result == 0);
}

void are_mcp230xx_registers_correct(const uint8_t expected[11])
{
	uint8_t mcp230xx_registers[11];
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

	mcp = mcp230xx_init(i2c_iface,
			    MCP230XX_ADDR,
			    false,
			    MCP230XX_CHIP_TYPE,
			    true,
			    MCP230XX_OPEN_DRAIN_INT,
			    MCP230XX_INT_POLARITY_NONE);
	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, false));

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

	mcp = mcp230xx_init(i2c_iface,
			    MCP230XX_ADDR,
			    false,
			    MCP230XX_CHIP_TYPE,
			    false,
			    MCP230XX_ACTIVE_DRIVER_INT,
			    MCP230XX_INT_ACTIVE_HIGH);
	TEST_ASSERT_NOT_NULL(mcp);
	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, false));

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

	mcp = mcp230xx_init(i2c_iface,
			    MCP230XX_ADDR,
			    false,
			    MCP230XX_CHIP_TYPE,
			    false,
			    MCP230XX_OPEN_DRAIN_INT,
			    MCP230XX_INT_POLARITY_NONE);
	TEST_ASSERT_NOT_NULL(mcp);

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

	TEST_ASSERT_EQUAL(0, mcp230xx_deinit(mcp, true));
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
}

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32
void setup()
{
	Serial.begin(1000000);
#else
int main(void)
{
#endif
	UNITY_BEGIN();

	int n;

	n = 100;
	do {
		RUN_TEST(test_mcp230xx_init_deinit);
	} while (--n);

	UNITY_END();
}

void loop()
{
}
