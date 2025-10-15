#include "unity.h"
#include "plc-peripherals.h"
#include "plc-peripherals-platform.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_plc_resource_simple_cycle(void) {
  static plc_resource_t example = I2C_RESOURCE(0x48);
  static plc_resource_t non_existant = I2C_RESOURCE(0x49);
  //TEST_ASSERT_EQUAL(0, plc_resource_init());
  TEST_ASSERT_EQUAL(0, plc_resource_add(example));
  TEST_ASSERT_EQUAL(1, plc_resource_add(example));
  TEST_ASSERT_EQUAL(1, plc_resource_remove(non_existant));
  TEST_ASSERT_EQUAL(0, plc_resource_remove(example));
  TEST_ASSERT_EQUAL(0, plc_resource_deinit());
}

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32
void setup() {
  Serial.begin(1000000);
#else
int main(void) {
#endif
  UNITY_BEGIN();
  int n = 1000;
  do {
    RUN_TEST(test_plc_resource_simple_cycle);
  } while (--n);
  UNITY_END();
}

void loop() {}