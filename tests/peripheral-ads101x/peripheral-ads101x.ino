#include "unity.h"
#include "plc-peripherals.h"
#include "plc-peripherals-platform.h"
#include <assert.h>

static i2c_interface_t* i2c_iface;

#if defined(ESP32PLC)
#define ADS1015_ADDR 0x48
#define ADS1015_FSR ADS101X_FSR_4_096V
#define ADS1015_MAX_VALUE 2047
#define I0_12 ADS101X_P0_GND  // 5V
#define I0_11 ADS101X_P1_GND  // 3.3V
#define I0_10 ADS101X_P2_GND  // Connected to nothing
#else
#error "PLC not supported"
#endif

void setUp(void) {
  i2c_iface = i2c_init(0, SDA, SCL);
  assert(i2c_iface);
}

void tearDown(void) {
  int result = i2c_deinit(i2c_iface, false);
  assert(result == 0);
}

static uint16_t ads101x_config_check_results[2];
static void ads101x_config_check(plc_i2c_addr_t addr, bool restart, bool set_continuous_mode,
                                 ADS101X_GAIN_AMPLIFIER fsr, ADS101X_DATA_RATE dr, bool shutdown) {
  int16_t dummyread;
  ads101x_t* ads1015 = ads101x_init(i2c_iface, addr, restart, set_continuous_mode, fsr, dr);
  assert(ads1015);

  TEST_ASSERT_EQUAL(0, i2c_read8_16b(i2c_iface, addr, 0x01, &ads101x_config_check_results[0]));
  ads101x_continuous_read(ads1015, I0_12, &dummyread, 1000);
  TEST_ASSERT_EQUAL(0, i2c_read8_16b(i2c_iface, addr, 0x01, &ads101x_config_check_results[1]));

  TEST_ASSERT_EQUAL(0, ads101x_deinit(ads1015, shutdown));
}

void test_ads101x_init_deinit(void) {
  ads101x_config_check(ADS1015_ADDR, true, true, ADS101X_NO_FSR, ADS101X_NO_SPS, false);
  TEST_ASSERT_EQUAL_HEX(0x0483, ads101x_config_check_results[0]);
  TEST_ASSERT_EQUAL_HEX(0x4483, ads101x_config_check_results[1]);

  ads101x_config_check(ADS1015_ADDR, true, true, ADS101X_NO_FSR, ADS101X_2400SPS, false);
  TEST_ASSERT_EQUAL_HEX(0x04A3, ads101x_config_check_results[0]);
  TEST_ASSERT_EQUAL_HEX(0x44A3, ads101x_config_check_results[1]);

  ads101x_config_check(ADS1015_ADDR, true, true, ADS101X_FSR_4_096V, ADS101X_NO_SPS, false);
  TEST_ASSERT_EQUAL_HEX(0x0283, ads101x_config_check_results[0]);
  TEST_ASSERT_EQUAL_HEX(0x4283, ads101x_config_check_results[1]);

  ads101x_config_check(ADS1015_ADDR, true, true, ADS101X_FSR_4_096V, ADS101X_2400SPS, false);
  TEST_ASSERT_EQUAL_HEX(0x02A3, ads101x_config_check_results[0]);
  TEST_ASSERT_EQUAL_HEX(0x42A3, ads101x_config_check_results[1]);

  ads101x_config_check(ADS1015_ADDR, false, true, ADS101X_NO_FSR, ADS101X_NO_SPS, false);
  TEST_ASSERT_EQUAL_HEX(0x42A3, ads101x_config_check_results[0]);
  TEST_ASSERT_EQUAL_HEX(0x42A3, ads101x_config_check_results[1]);

  ads101x_config_check(ADS1015_ADDR, true, true, ADS101X_FSR_4_096V, ADS101X_2400SPS, true);
  TEST_ASSERT_EQUAL_HEX(0x02A3, ads101x_config_check_results[0]);
  TEST_ASSERT_EQUAL_HEX(0x42A3, ads101x_config_check_results[1]);
  uint16_t config_after_shutdown;
  TEST_ASSERT_EQUAL(0, i2c_read8_16b(i2c_iface, ADS1015_ADDR, 0x01, &config_after_shutdown));
  TEST_ASSERT_EQUAL_HEX(0xC3A3, config_after_shutdown);
}

void test_ads101x_continuous_read(void) {
  ads101x_t* ads1015 = ads101x_init(i2c_iface, ADS1015_ADDR, true, true, ADS1015_FSR, ADS101X_NO_SPS);
  TEST_ASSERT(ads1015 != NULL);

  int16_t i0_12_reading;
  TEST_ASSERT_EQUAL(0, ads101x_continuous_read(ads1015, I0_12, &i0_12_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(100, 5. * ADS1015_MAX_VALUE / 10., i0_12_reading);

  int16_t i0_11_reading;
  TEST_ASSERT_EQUAL(0, ads101x_continuous_read(ads1015, I0_11, &i0_11_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(80, 3.3 * ADS1015_MAX_VALUE / 10., i0_11_reading);

  int16_t i0_10_reading;
  TEST_ASSERT_EQUAL(0, ads101x_continuous_read(ads1015, I0_10, &i0_10_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(8., 0., i0_10_reading);

  TEST_ASSERT_EQUAL(0, ads101x_deinit(ads1015, true));
}

void test_ads101x_continuous_read_locked(void) {
  ads101x_t* ads1015 = ads101x_init(i2c_iface, ADS1015_ADDR, true, true, ADS1015_FSR, ADS101X_NO_SPS);
  TEST_ASSERT(ads1015 != NULL);
  TEST_ASSERT_EQUAL(0, plc_resource_init());

  TEST_ASSERT_EQUAL(0, ads101x_protect(ads1015));
  TEST_ASSERT_EQUAL(1, ads101x_protect(ads1015));
  TEST_ASSERT_EQUAL(EEXIST, errno);
  errno = 0;

  int16_t i0_12_reading;
  TEST_ASSERT_EQUAL(0, ads101x_continuous_read(ads1015, I0_12, &i0_12_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(100, 5. * ADS1015_MAX_VALUE / 10., i0_12_reading);

  int16_t i0_11_reading;
  TEST_ASSERT_EQUAL(0, ads101x_continuous_read(ads1015, I0_11, &i0_11_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(80, 3.3 * ADS1015_MAX_VALUE / 10., i0_11_reading);

  int16_t i0_10_reading;
  TEST_ASSERT_EQUAL(0, ads101x_continuous_read(ads1015, I0_10, &i0_10_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(8., 0., i0_10_reading);

  TEST_ASSERT_EQUAL(0, ads101x_unprotect(ads1015));
  TEST_ASSERT_EQUAL(1, ads101x_unprotect(ads1015));
  TEST_ASSERT_EQUAL(ENODEV, errno);
  errno = 0;

  TEST_ASSERT_EQUAL(0, ads101x_deinit(ads1015, true));
  TEST_ASSERT_EQUAL(0, plc_resource_deinit());
}

void test_ads101x_unsigned_continuous_read(void) {
  ads101x_t* ads1015 = ads101x_init(i2c_iface, ADS1015_ADDR, true, true, ADS1015_FSR, ADS101X_NO_SPS);
  TEST_ASSERT(ads1015 != NULL);

  uint16_t i0_12_reading;
  TEST_ASSERT_EQUAL(0, ads101x_unsigned_continuous_read(ads1015, I0_12, &i0_12_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(100, 5. * ADS1015_MAX_VALUE / 10., i0_12_reading);

  uint16_t i0_11_reading;
  TEST_ASSERT_EQUAL(0, ads101x_unsigned_continuous_read(ads1015, I0_11, &i0_11_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(80, 3.3 * ADS1015_MAX_VALUE / 10., i0_11_reading);

  uint16_t i0_10_reading;
  TEST_ASSERT_EQUAL(0, ads101x_unsigned_continuous_read(ads1015, I0_10, &i0_10_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(8., 0., i0_10_reading);

  TEST_ASSERT_EQUAL(0, ads101x_deinit(ads1015, true));
}

void test_ads101x_unsigned_continuous_read_locked(void) {
  ads101x_t* ads1015 = ads101x_init(i2c_iface, ADS1015_ADDR, true, true, ADS1015_FSR, ADS101X_NO_SPS);
  TEST_ASSERT(ads1015 != NULL);
  TEST_ASSERT_EQUAL(0, plc_resource_init());

  TEST_ASSERT_EQUAL(0, ads101x_protect(ads1015));
  TEST_ASSERT_EQUAL(1, ads101x_protect(ads1015));
  TEST_ASSERT_EQUAL(EEXIST, errno);
  errno = 0;

  uint16_t i0_12_reading;
  TEST_ASSERT_EQUAL(0, ads101x_unsigned_continuous_read(ads1015, I0_12, &i0_12_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(100, 5. * ADS1015_MAX_VALUE / 10., i0_12_reading);

  uint16_t i0_11_reading;
  TEST_ASSERT_EQUAL(0, ads101x_unsigned_continuous_read(ads1015, I0_11, &i0_11_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(80, 3.3 * ADS1015_MAX_VALUE / 10., i0_11_reading);

  uint16_t i0_10_reading;
  TEST_ASSERT_EQUAL(0, ads101x_unsigned_continuous_read(ads1015, I0_10, &i0_10_reading, 1000));
  TEST_ASSERT_FLOAT_WITHIN(8., 0., i0_10_reading);

  TEST_ASSERT_EQUAL(0, ads101x_unprotect(ads1015));
  TEST_ASSERT_EQUAL(1, ads101x_unprotect(ads1015));
  TEST_ASSERT_EQUAL(ENODEV, errno);
  errno = 0;

  TEST_ASSERT_EQUAL(0, ads101x_deinit(ads1015, true));
  TEST_ASSERT_EQUAL(0, plc_resource_deinit());
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
    RUN_TEST(test_ads101x_init_deinit);
  } while (--n);

  n = 100;
  do {
    RUN_TEST(test_ads101x_continuous_read);
  } while (--n);
  n = 100;
  do {
    RUN_TEST(test_ads101x_continuous_read_locked);
  } while (--n);

  n = 100;
  do {
    RUN_TEST(test_ads101x_unsigned_continuous_read);
  } while (--n);
  n = 100;
  do {
    RUN_TEST(test_ads101x_unsigned_continuous_read_locked);
  } while (--n);

  UNITY_END();
}

void loop() {}