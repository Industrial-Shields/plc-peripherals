#include "../include/peripheral-ltc2309.h"

#include <unistd.h>
#include <stdio.h>
#include <errno.h>

// Registers
#define SD_SINGLE 0x80
#define SD_DIFF   0x00
#define OS_ODD    0x40
#define OS_SIGN   0x00
#define S_CHANNEL_0     0x00
#define S_CHANNEL_1     0x00
#define S_CHANNEL_2     0x01
#define S_CHANNEL_3     0x01
#define S_CHANNEL_4     0x02
#define S_CHANNEL_5     0x02
#define S_CHANNEL_6     0x03
#define S_CHANNEL_7     0x03
#define SO_UNIP   0x00

#define	CHANNEL_0 0b10001000
#define	CHANNEL_1 0b11001000
#define	CHANNEL_2 0b10011000
#define	CHANNEL_3 0b11011000
#define	CHANNEL_4 0b10101000
#define	CHANNEL_5 0b11101000
#define	CHANNEL_6 0b10111000
#define CHANNEL_7 0b11111000


int ltc2309_init(i2c_interface_t* i2c, uint8_t addr) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128) {
	        errno = EINVAL;
	        return -1;
        }

	uint16_t read_test;
	return ltc2309_read(i2c, addr, 0, &read_test);
}

int ltc2309_deinit(i2c_interface_t* i2c, uint8_t addr) {
        if (i2c == NULL) {
	        errno = EFAULT;
	        return -1;
        }
        if (addr >= 128) {
	        errno = EINVAL;
	        return -1;
        }

        errno = 0;
        return 0;
}

int ltc2309_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t* read_value) {
	if (read_value == NULL) {
		errno = EFAULT;
		return -1;
	}
        uint8_t mux;
	switch (index) {
	case 0:
		mux = CHANNEL_0;
		break;
	case 1:
		mux = CHANNEL_1;
		break;
	case 2:
		mux = CHANNEL_2;
		break;
	case 3:
		mux = CHANNEL_3;
		break;
	case 4:
		mux = CHANNEL_4;
		break;
	case 5:
		mux = CHANNEL_5;
		break;
	case 6:
		mux = CHANNEL_6;
		break;
	case 7:
		mux = CHANNEL_7;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	uint8_t buffer[2];
	buffer[0] = mux;
	const i2c_write_t write_conversion_reg = {.buff=buffer, .len=1};
	int i2c_ret = i2c_write(i2c, addr, &write_conversion_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	usleep(5); // It must wait 1.8 us minimum before reading

	const i2c_read_t read_conversion = {.buff=buffer, .len=2}; // It must return two bytes
        i2c_ret = i2c_read(i2c, addr, &read_conversion);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	*read_value = (buffer[0] << 4) | (buffer[1] >> 4);

	return 0;
}
