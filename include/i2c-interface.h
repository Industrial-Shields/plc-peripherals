#ifndef __I2C_INTERFACE_H__
#define __I2C_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

        #define FAST_CREATE_I2C_WRITE(name, ...) \
	const i2c_write_t name = {.buff = (const uint8_t[]) {__VA_ARGS__}, sizeof((const uint8_t[]) {__VA_ARGS__})}


        struct _i2c_interface_t;
	typedef struct _i2c_interface_t i2c_interface_t;

	typedef struct {
		const uint8_t* buff;
	        size_t len;
	} i2c_write_t;

	typedef struct {
		uint8_t* buff;
		size_t len;
	} i2c_read_t;

	i2c_interface_t* i2c_init(uint8_t bus);
	int i2c_deinit(i2c_interface_t** i2c);
        int i2c_write(i2c_interface_t* i2c, uint8_t addr, const i2c_write_t* to_write);
	int i2c_read(i2c_interface_t* i2c, uint8_t addr, const i2c_read_t* to_read);
        int i2c_write_then_read(i2c_interface_t* i2c, uint8_t addr, const i2c_write_t* read_order, const i2c_read_t* to_read);

#ifdef __cplusplus
}
#endif

#endif
