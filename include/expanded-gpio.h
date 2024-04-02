#ifndef __EXPANDED_GPIO_H__
#define __EXPANDED_GPIO_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#undef INPUT
#define INPUT 0

#undef OUTPUT
#define OUTPUT 1

#undef LOW
#define LOW 0

#undef HIGH
#define HIGH 1

#define pinToDeviceAddress(pin) (((pin) >> 8) & 0xff)
#define pinToDeviceIndex(pin) ((pin)&0xff)

#ifdef __cplusplus
extern "C" {
#endif

	extern const uint8_t I2C_BUS;

	extern const uint8_t NORMAL_GPIO_INPUT;
	extern const uint8_t NORMAL_GPIO_OUTPUT;

	extern const uint8_t MCP23008[];
	extern const size_t NUM_MCP23008;

	extern const uint8_t ADS1015[];
	extern const size_t NUM_ADS1015;

	extern const uint8_t PCA9685[];
	extern const size_t NUM_PCA9685;

	extern const uint8_t LTC2309[];
	extern const size_t NUM_LTC2309;

	extern const uint8_t MCP23017[];
	extern const size_t NUM_MCP23017;


	extern int normal_gpio_init(void);
	extern int normal_gpio_deinit(void);

	extern int normal_gpio_set_pin_mode(uint32_t pin, uint8_t mode);

	extern int normal_gpio_write(uint32_t pin, uint8_t value);

	extern int normal_gpio_pwm_frequency(uint32_t pin, uint32_t freq);

	extern int normal_gpio_pwm_write(uint32_t pin, uint16_t value);

	extern int normal_gpio_read(uint32_t pin, uint8_t* read);

	extern int normal_gpio_analog_read(uint32_t pin, uint16_t* read);


	#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	void delay(uint32_t milliseconds);

	void delayMicroseconds(uint32_t micros);
	#endif


	// Exported functions, useful for Arduino environment
	enum {
		// initExpandedGPIO & deinitExpandedGPIO
		I2C_ALREADY_INITIALIZED = 1,
		I2C_ALREADY_DEINITIALIZED,
		NORMAL_GPIO_INIT_FAIL,
		PCA9685_INIT_FAIL,
		ADS1015_INIT_FAIL,
		MCP23008_INIT_FAIL,
		LTC2309_INIT_FAIL,
		MCP23017_INIT_FAIL,
		NORMAL_GPIO_DEINIT_FAIL,
		PCA9685_DEINIT_FAIL,
		ADS1015_DEINIT_FAIL,
		MCP23008_DEINIT_FAIL,
		LTC2309_DEINIT_FAIL,
		MCP23017_DEINIT_FAIL,
		// pinMode
		NORMAL_GPIO_SET_PIN_MODE_FAIL,
		MCP23008_SET_PIN_MODE_FAIL,
		MCP23017_SET_PIN_MODE_FAIL,
		// digitalWrite
		NORMAL_GPIO_WRITE_FAIL,
		PCA9685_WRITE_FAIL,
		MCP23008_WRITE_FAIL,
		MCP23017_WRITE_FAIL,
		// digitalRead
		// analogWrite
		NORMAL_GPIO_PWM_WRITE_FAIL,
		PCA9685_PWM_WRITE_FAIL,
		// analogWriteSetFrequency
		NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL,
		PCA9685_PWM_CHANGE_FREQ_FAIL,
		// analogRead
		// digitalWriteAll
		PCA9685_WRITE_ALL_FAIL,
		MCP23008_WRITE_ALL_FAIL,
		MCP23017_WRITE_ALL_FAIL,
		// digitalReadAll
		MCP23008_READ_ALL_FAIL,
		MCP23017_READ_ALL_FAIL,
		// analogWriteAll
		PCA9685_PWM_WRITE_ALL_FAIL
	};

	int initExpandedGPIO(bool restart_peripherals);

	int deinitExpandedGPIO(void);

	int pinMode(uint32_t pin, uint8_t mode);

	int digitalWrite(uint32_t pin, uint8_t value);

	int digitalRead(uint32_t pin);

	int analogWrite(uint32_t pin, uint16_t value);

	int analogWriteSetFrequency(uint32_t pin, uint32_t desired_freq);

	uint16_t analogRead(uint32_t pin);

	int digitalWriteAll(uint8_t addr, uint32_t values);

	int digitalReadAll(uint8_t addr, uint16_t* values);

	int analogWriteAll(uint8_t addr, const uint16_t* values);

#ifdef __cplusplus
}
#endif


#endif // __EXPANDED_GPIO_H__
