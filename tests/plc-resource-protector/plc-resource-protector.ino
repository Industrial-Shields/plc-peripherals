#include "unity.h"
#include "plc-peripherals.h"
#include "plc-peripherals-platform.h"
#include "errno.h"

void setUp(void) {
  errno = 0;
}

void tearDown(void) {
  errno = 0;
}

void test_plc_mutex_simple_cycle(void) {
  plc_mutex_t *mutex = plc_mutex_create();
  TEST_ASSERT(mutex != NULL);

  TEST_ASSERT_EQUAL(0, plc_mutex_acquire(mutex, 10));
  TEST_ASSERT_EQUAL(0, errno);
  TEST_ASSERT_EQUAL(-1, plc_mutex_acquire(mutex, 10));
  TEST_ASSERT_EQUAL(EBUSY, errno);
  errno = 0;

#if PLC_ENVIRONMENT != PLC_ARDUINO_ESP32 && PLC_ENVIRONMENT != PLC_ESP_IDF
    // ESP32 can't detect if a semaphore is being freed while the mutex is locked
    TEST_ASSERT_EQUAL(-1, plc_mutex_destroy(mutex));
    TEST_ASSERT_EQUAL(EBUSY, errno);
    errno = 0;
#endif

  TEST_ASSERT_EQUAL(0, plc_mutex_release(mutex));
  TEST_ASSERT_EQUAL(0, errno);
  TEST_ASSERT_EQUAL(-1, plc_mutex_release(mutex));
  TEST_ASSERT_EQUAL(EBUSY, errno);
  errno = 0;

  TEST_ASSERT_EQUAL(0, plc_mutex_destroy(mutex));

  TEST_ASSERT_EQUAL(-1, plc_mutex_destroy(NULL));
  TEST_ASSERT_EQUAL(EINVAL, errno);
  errno = 0;
}

void test_plc_resource_simple_cycle(void) {
  static plc_resource_t example = I2C_RESOURCE(0x48);
  static plc_resource_t non_existant = I2C_RESOURCE(0x49);
  TEST_ASSERT_EQUAL(0, plc_resource_init());
  TEST_ASSERT_EQUAL(1, plc_resource_init());
  TEST_ASSERT_EQUAL(0, plc_resource_add(example));

  TEST_ASSERT_EQUAL(1, plc_resource_add(example));
  TEST_ASSERT_EQUAL(EEXIST, errno);
  errno = 0;

  TEST_ASSERT_EQUAL(1, plc_resource_remove(non_existant));
  TEST_ASSERT_EQUAL(ENODEV, errno);
  errno = 0;

  TEST_ASSERT_EQUAL(0, plc_resource_remove(example));

  TEST_ASSERT_EQUAL(0, plc_resource_deinit());
  TEST_ASSERT_EQUAL(1, plc_resource_deinit());
}

#if PLC_ENVIRONMENT == PLC_ARDUINO_ESP32
void setup() {
  Serial.begin(1000000);
#else
int main(void) {
#endif
  UNITY_BEGIN();
  int n;

  n = 100;
  do {
    RUN_TEST(test_plc_mutex_simple_cycle);
  } while (--n);

  n = 100;
  do {
    RUN_TEST(test_plc_resource_simple_cycle);
  } while (--n);
  UNITY_END();
}

void loop() {}