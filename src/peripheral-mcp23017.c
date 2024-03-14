#include "../include/peripheral-mcp23017.h"

#include <errno.h>

// Registers
#define IODIR_A_REGISTER      0x00    
#define IODIR_B_REGISTER      0x01
#define IPOL_A_REGISTER       0x02
#define IPOL_B_REGISTER       0x03
#define GPINTEN_A_REGISTER    0x04
#define GPINTEN_B_REGISTER    0x05
#define DEFVAL_A_REGISTER     0x06
#define DEFVAL_B_REGISTER     0x07
#define INTCON_A_REGISTER     0x08
#define INTCON_B_REGISTER     0x09
#define IOCON_A_REGISTER      0x0A
#define IOCON_B_REGISTER      0x0B
#define GPPU_A_REGISTER       0x0C
#define GPPU_B_REGISTER       0x0D
#define INTF_A_REGISTER       0x0E
#define INTF_B_REGISTER       0x0F
#define INTCAP_A_REGISTER     0x10
#define INTCAP_B_REGISTER     0x11
#define GPIO_A_REGISTER       0x12
#define GPIO_B_REGISTER       0x13
#define OLAT_A_REGISTER       0x14
#define OLAT_B_REGISTER       0x15

// Registers values and masks
#define IOCON_A_SEQOP           0b00100000
#define IOCON_B_SEQOP           0b00100000
#define IOCON_A_ODR             0b00000100
#define IOCON_B_ODR             0b00000100


#define GET_REGISTER(_pin, _A, _B) (_pin <= 7 ? _A : _B)


static inline int write_reg(i2c_interface_t* i2c, uint8_t addr, uint8_t reg, uint8_t value) {
	FAST_CREATE_I2C_WRITE(read_order_reg, reg, value);
	return i2c_write(i2c, addr, &read_order_reg);
}

static int mcp23017_reset(i2c_interface_t* i2c, uint8_t addr) {
	int i2c_ret;


	i2c_ret = write_reg(i2c, addr, IODIR_A_REGISTER, 0xFF);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, IODIR_B_REGISTER, 0xFF);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, IPOL_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, IPOL_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, GPINTEN_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, GPINTEN_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, DEFVAL_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, DEFVAL_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}


	i2c_ret = write_reg(i2c, addr, INTCON_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, INTCON_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, IOCON_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, IOCON_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, GPPU_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, GPPU_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, INTF_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, INTF_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, INTCAP_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, INTCAP_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, GPIO_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, GPIO_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, OLAT_A_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, OLAT_B_REGISTER, 0x00);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	return 0;
}

int mcp23017_init(i2c_interface_t* i2c, uint8_t addr) {
	int i2c_ret;

	
	FAST_CREATE_I2C_WRITE(read_order_iocon_a_reg, IOCON_A_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_iocon_b_reg, IOCON_B_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gppu_a_reg, GPPU_A_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gppu_b_reg, GPPU_B_REGISTER);

	uint8_t iocon_a, iocon_b, gppu_a, gppu_b;
	i2c_read_t read_iocon_a_reg = {.buff=&iocon_a, .len=1};
	i2c_read_t read_iocon_b_reg = {.buff=&iocon_b, .len=1};
	i2c_read_t read_gppu_a_reg = {.buff=&gppu_a, .len=1};
	i2c_read_t read_gppu_b_reg = {.buff=&gppu_b, .len=1};

	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_iocon_a_reg, &read_iocon_a_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
        i2c_ret = i2c_write_then_read(i2c, addr, &read_order_iocon_b_reg, &read_iocon_b_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gppu_a_reg, &read_gppu_a_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gppu_b_reg, &read_gppu_b_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}


	if ((iocon_a == (IOCON_A_SEQOP | IOCON_A_ODR)) &&
	    (iocon_b == (IOCON_B_SEQOP | IOCON_B_ODR)) &&
	    (gppu_a == 0x00) && (gppu_b == 0x00)) {
		// Already initialized
		errno = EALREADY;
		return 1;
	}

	i2c_ret = mcp23017_reset(i2c, addr);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	// Sequential operation disabled, and INT as open-drain in both channels
	i2c_ret = write_reg(i2c, addr, IOCON_A_REGISTER, IOCON_A_SEQOP | IOCON_A_ODR);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = write_reg(i2c, addr, IOCON_B_REGISTER, IOCON_B_SEQOP | IOCON_B_ODR);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23017_deinit(i2c_interface_t* i2c, uint8_t addr) {
	int i2c_ret;

	
	FAST_CREATE_I2C_WRITE(read_order_iocon_a_reg, IOCON_A_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_iocon_b_reg, IOCON_B_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gppu_a_reg, GPPU_A_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gppu_b_reg, GPPU_B_REGISTER);

	uint8_t iocon_a, iocon_b, gppu_a, gppu_b;
	i2c_read_t read_iocon_a_reg = {.buff=&iocon_a, .len=1};
	i2c_read_t read_iocon_b_reg = {.buff=&iocon_b, .len=1};
	i2c_read_t read_gppu_a_reg = {.buff=&gppu_a, .len=1};
	i2c_read_t read_gppu_b_reg = {.buff=&gppu_b, .len=1};

	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_iocon_a_reg, &read_iocon_a_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
        i2c_ret = i2c_write_then_read(i2c, addr, &read_order_iocon_b_reg, &read_iocon_b_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gppu_a_reg, &read_gppu_a_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gppu_b_reg, &read_gppu_b_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}


	if ((iocon_a == 0x00) && (iocon_b == 0x00) &&
	    (gppu_a == 0x00) && (gppu_b == 0x00)) {
		// Already de-initialized
		errno = EALREADY;
		return 1;
	}

	return mcp23017_reset(i2c, addr);
}

int mcp23017_set_pin_mode(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t mode) {
	if (index >= 16 || mode >= 2) {
	        errno = EINVAL;
	        return -1;
        }

	const uint8_t write_register = GET_REGISTER(index, IODIR_A_REGISTER, IODIR_B_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_iodir_reg, write_register);
	index = index % 8;

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

	i2c_ret = write_reg(i2c, addr, write_register, iodir);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23017_set_pin_mode_all(i2c_interface_t* i2c, uint8_t addr, uint16_t modes) {
	int i2c_ret = write_reg(i2c, addr, IODIR_A_REGISTER, modes >> 8);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

        i2c_ret = write_reg(i2c, addr, IODIR_B_REGISTER, modes & 0xFF);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23017_read(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t* value) {
	if (value == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (index >= 16) {
	        errno = EINVAL;
	        return -1;
        }

	const uint8_t write_register = GET_REGISTER(index, GPIO_A_REGISTER, GPIO_B_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gpio_reg, write_register);
	index = index % 8;

	i2c_read_t read_gpio_reg = {.buff=value, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gpio_reg, &read_gpio_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	*value = (*value >> index) & 0b00000001;

	errno = 0;
	return 0;
}

int mcp23017_write(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint8_t value) {
	if (index >= 16) {
	        errno = EINVAL;
	        return -1;
        }
	value = value > 0 ? 1 : 0;


	const uint8_t write_register = GET_REGISTER(index, GPIO_A_REGISTER, GPIO_B_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gpio_reg, write_register);
	index = index % 8;

	uint8_t gpio;
	i2c_read_t read_gpio_reg = {.buff=&gpio, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gpio_reg, &read_gpio_reg);

	if (!value) {
		gpio &= ~(1 << index);
	}
	else {
		gpio |= (1 << index);
	}

	i2c_ret = write_reg(i2c, addr, write_register, gpio);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23017_read_all(i2c_interface_t* i2c, uint8_t addr, uint16_t* value) {
	if (value == NULL) {
		errno = EFAULT;
		return -1;
	}

	FAST_CREATE_I2C_WRITE(read_order_gpio_a_reg, GPIO_A_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_gpio_b_reg, GPIO_B_REGISTER);

	i2c_read_t read_gpio_a_reg = {.buff=(uint8_t*) value, .len=1};
	i2c_read_t read_gpio_b_reg = {.buff=((uint8_t*) value) + 1, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gpio_a_reg, &read_gpio_a_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_gpio_b_reg, &read_gpio_b_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int mcp23017_write_all(i2c_interface_t* i2c, uint8_t addr, uint16_t value) {
	int i2c_ret = write_reg(i2c, addr, GPIO_A_REGISTER, value & 0x00FF);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	i2c_ret = write_reg(i2c, addr, GPIO_B_REGISTER, (uint8_t) (value & 0xFF00));
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}
