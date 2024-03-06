#include "../include/peripheral-mcp23008.h"

#include <stdio.h>
#include <errno.h>

// Registers
#define IODIR_REGISTER		0x00
#define IPOL_REGISTER		0x01
#define GPINTEN_REGISTER	0x02
#define DEFVAL_REGISTER		0x03
#define INTCON_REGISTER		0x04
#define IOCON_REGISTER		0x05
#define GPPU_REGISTER		0x06
#define INTF_REGISTER		0x07
#define INTCAP_REGISTER		0x08
#define GPIO_REGISTER		0x09
#define OLAT_REGISTER		0x0a

// Registers values and masks
#define IOCON_INTPOL		0x02
#define IOCON_ODR		0x04
#define IOCON_DISSLW		0x10
#define IOCON_SEQOP		0x20

static inline int write_reg(i2c_interface_t* i2c, uint8_t addr, uint8_t reg, uint8_t value) {
	FAST_CREATE_I2C_WRITE(read_order_reg, reg, value);
	return i2c_write(i2c, addr, &read_order_reg);
}

static int mcp23008_reset(i2c_interface_t* i2c, uint8_t addr) {
	int i2c_ret;

	i2c_ret = write_reg(i2c, addr, IODIR_REGISTER, 0xFF);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, IPOL_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, GPINTEN_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, DEFVAL_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, INTCON_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, IOCON_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, GPPU_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, INTF_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, INTCAP_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, GPIO_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, OLAT_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	return 0;
}

int mcp23008_init(i2c_interface_t* i2c, uint8_t addr) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128) {
	        errno = EINVAL;
	        return -1;
        }

	FAST_CREATE_I2C_WRITE(read_order_iocon_reg, IOCON_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gppu_reg, GPPU_REGISTER);

	uint8_t iocon, gppu;
	i2c_read_t read_iocon_reg = {.buff=&iocon, .len=1};
	i2c_read_t read_gppu_reg = {.buff=&gppu, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_iocon_reg, &read_iocon_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gppu_reg, &read_gppu_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	if ((iocon == (IOCON_SEQOP | IOCON_ODR)) && (gppu == 0x00)) {
		// Already initialized
		errno = EALREADY;
		return 1;
	}

	i2c_ret = mcp23008_reset(i2c, addr);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	// Sequential operation disabled, and INT as open-drain
	i2c_ret = write_reg(i2c, addr, IOCON_REGISTER, IOCON_SEQOP | IOCON_ODR);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23008_deinit(i2c_interface_t* i2c, uint8_t addr) {
        if (i2c == NULL) {
	        errno = EFAULT;
	        return -1;
        }
        if (addr >= 128) {
	        errno = EINVAL;
	        return -1;
        }

        FAST_CREATE_I2C_WRITE(read_order_iocon_reg, IOCON_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gppu_reg, GPPU_REGISTER);

	uint8_t iocon, gppu;
	i2c_read_t read_iocon_reg = {.buff=&iocon, .len=1};
	i2c_read_t read_gppu_reg = {.buff=&gppu, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_iocon_reg, &read_iocon_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gppu_reg, &read_gppu_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

        if (iocon == 0x00 && gppu == 0x00) {
		// Not initialized
		errno = EALREADY;
		return 1;
	}

        i2c_ret = mcp23008_reset(i2c, addr);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

        errno = 0;
        return 0;
}


int mcp23008_set_pin_mode(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t mode) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128 || index >= 8 || mode >= 2) {
	        errno = EINVAL;
	        return -1;
        }

	FAST_CREATE_I2C_WRITE(read_order_iodir_reg, IODIR_REGISTER);

	uint8_t iodir;
	i2c_read_t read_iodir_reg = {.buff=&iodir, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_iodir_reg, &read_iodir_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	if (mode & 0x01) {
		iodir |= (1 << index);
	}
	else {
		iodir &= ~(1 << index);
	}

	i2c_ret = write_reg(i2c, addr, IODIR_REGISTER, iodir);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23008_set_pin_mode_all(i2c_interface_t* i2c, uint8_t addr, uint8_t modes) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128) {
	        errno = EINVAL;
	        return -1;
        }

	int i2c_ret = write_reg(i2c, addr, IODIR_REGISTER, modes);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23008_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t* value) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128 || index >= 8) {
	        errno = EINVAL;
	        return -1;
        }

	FAST_CREATE_I2C_WRITE(read_order_gpio_reg, GPIO_REGISTER);

	i2c_read_t read_gpio_reg = {.buff=value, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gpio_reg, &read_gpio_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	*value = (*value >> index) & 0b00000001;

	errno = 0;
	return 0;
}

int mcp23008_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128 || index >= 8) {
	        errno = EINVAL;
	        return -1;
        }
	value = value > 0 ? 1 : 0;


	FAST_CREATE_I2C_WRITE(read_order_gpio_reg, GPIO_REGISTER);

	uint8_t gpio;
	i2c_read_t read_gpio_reg = {.buff=&gpio, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gpio_reg, &read_gpio_reg);

	if (!value) {
		gpio &= ~(1 << index);
	}
	else {
		gpio |= (1 << index);
	}

	i2c_ret = write_reg(i2c, addr, GPIO_REGISTER, gpio);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23008_read_all(i2c_interface_t* i2c, uint8_t addr, uint8_t* value) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128) {
	        errno = EINVAL;
	        return -1;
        }

	FAST_CREATE_I2C_WRITE(read_order_gpio_reg, GPIO_REGISTER);

	i2c_read_t read_gpio_reg = {.buff=value, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gpio_reg, &read_gpio_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23008_write_all(i2c_interface_t* i2c, uint8_t addr, uint8_t value) {
	if (i2c == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (addr >= 128) {
	        errno = EINVAL;
	        return -1;
        }

	int i2c_ret = write_reg(i2c, addr, GPIO_REGISTER, value);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}
