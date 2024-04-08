# plc-peripherals

The `plc-peripherals` library provides an unified I2C interface to interact with GPIO peripherals like the MCP230XX, the LTC2309, the ADS1015 and the PCA9685.

It also provides an interface to interact with GPIOs in the same way, both direct GPIOs of the chip you are using, and expanded GPIOs provided by external peripherals.

This library was thought as a static library to be included in other libraries (like in [librpiplc](https://github.com/Industrial-Shields/librpiplc)), but it can also be used as a standalone dynamic library.


## Licensing
This library is licensed under the LGPL-3.0-or-later. The test programs are licensed under the GPL-3.0-or-later.


## Peripheral generic interface
### Mandatory methods
- init
- deinit

### ADC-like peripherals
#### Mandatory methods
* read: It returns the ADC's value as it is, within it's corresponding sign (or not).
#### Defined macros
* NUM_INPUTS: Number of channels that can be used as inputs.
#### Extensions
They may have additional read functions, like the ADS1015, which has the unsigned_read. This methods adjusts the result to compensate for the -1 or -2 that sometimes appear when there's nothing connected.

The LTC2309 doesn't have this extension, since unlike the ADS1015, in unipolar mode it only switches between 0 and 1 (as the reading is unsigned).

### Digital GPIOs peripherals
#### Mandatory methods
* set_pin_mode: Sets a pin as INPUT or OUTPUT.
* set_pin_mode_all: Same as set_pin_mode, but applies to all pins at the same time.
* read: Returns a value of 0 or 1 of the given pin.
* write: Writes 0 or 1 to the given pin.
* read_all: Returns a uintX_t of all pins of a peripheral. Where X is (ideally) the number of pins.
* write_all: Writes a uintX_t to the peripheral. Where X is (ideally) the number of pins.
* pwm_write: Writes a uintX_t as the duty cycle of a PWM. Where X is (ideally) the PWM resolution.
* pwm_write_all: Writes an array of X uintY_t as duty cycle of a PWM. Where X is the number of outputs of the peripheral and Y is (ideally) the PWM resolution.
#### Defined macros
* X_INPUT and X_OUTPUT: The values to pass as `mode` to the set_pin functions. This is useful because, for example, "1" in the MCP230XX is INPUT, not OUTPUT (as it normally is in the Arduino environment).
* NUM_IO: Number if GPIOs.
* NUM_OUTPUTS: Number of outputs-only..
