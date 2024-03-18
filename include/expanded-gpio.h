#ifndef __EXPANDED_GPIO_H__
#define __EXPANDED_GPIO_H__

#include <stdint.h>
#include <stddef.h>

#ifndef INPUT
#define INPUT 0
#endif

#ifndef OUTPUT
#define OUTPUT 1
#endif

#ifndef LOW
#define LOW 0
#endif

#ifndef HIGH
#define HIGH 1
#endif

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

	extern int normal_gpio_read(uint32_t pin, uint8_t* read);

	extern int normal_gpio_analog_read(uint32_t pin, uint16_t* read);


	#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	void delay(uint32_t milliseconds);

	void delayMicroseconds(uint32_t micros);
	#endif


	// Exported functions, useful for Arduino environment
	enum {
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

		NORMAL_GPIO_SET_PIN_MODE_FAIL,
		MCP23008_SET_PIN_MODE_FAIL,
		MCP23017_SET_PIN_MODE_FAIL,

		NORMAL_GPIO_WRITE_FAIL,
		PCA9685_WRITE_FAIL,
		MCP23008_WRITE_FAIL,
		MCP23017_WRITE_FAIL,

		PCA9685_PWM_WRITE_FAIL,

		PCA9685_WRITE_ALL_FAIL,
		MCP23008_WRITE_ALL_FAIL,
		MCP23017_WRITE_ALL_FAIL,

		MCP23008_READ_ALL_FAIL,
		MCP23017_READ_ALL_FAIL,

		PCA9685_PWM_WRITE_ALL_FAIL
	};

	int initExpandedGPIO(void);

	int deinitExpandedGPIO(void);

	int pinMode(uint32_t pin, uint8_t mode);

	int digitalWrite(uint32_t pin, uint8_t value);

	int digitalRead(uint32_t pin);

	int analogWrite(uint32_t pin, uint16_t value);

	uint16_t analogRead(uint32_t pin);

	int digitalWriteAll(uint8_t addr, uint32_t values);

	int digitalReadAll(uint8_t addr, uint16_t* values);

	int analogWriteAll(uint8_t addr, const uint16_t* values);

#ifdef __cplusplus
}
#endif


#endif // __EXPANDED_GPIO_H__
