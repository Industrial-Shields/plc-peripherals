# Changelog

All notable changes to this project will be documented in this file.

## [2.0.0] - 2025-07-03

### 🚀 Features

- [**breaking**] Merge branch 'new-pins-enum' ([d6fef0f](d6fef0ff1876079112392af0bf36402d9df64e6a))

- Embed plc-peripherals version into the library ([f98bb9a](f98bb9a33124316de7b2bb271a7cc532b409ed82))

- Revert breaking change, and rename initExpandedGPIOV2 to initExpandedGPIO ([106de32](106de32c48b3eaeff33ca6c23fe6d110779aaded))


### 🐛 Bug Fixes

- Use -Og flag when making Debug builds ([ef9604e](ef9604e86ff8934331c50f2b19e40bca0f77f69d))

- Return zero when calling pinMode with ADS1015, PCA9685 or LTC2309 ([16c4287](16c4287dc89e73af5bfeb77be6fba44acbc560d2))

- Fix debug release for cmake ([453addd](453addd113014dde93241b84633663f0d9b4c7d6))


### 💼 Other

- Add new function deinitExpandedGPIONoReset ([05dad0a](05dad0a8d975c1a980accf7ccb7f66d4ffe501cf))

- Correct documentation ([0e62b56](0e62b5611f07a0253202a0ffe7c33085ae880580))

- Set errno to EALREADY when PCA9685 and MCP23008 return 1 ([60d7013](60d7013a56bcdb79b533cb50e8deac1608c9bf56))

- When initializing and deinitializing normal GPIOs, enable for positive return values ([0874a12](0874a12ce3132be5417382e4c37a450575387e2c))

- Prepare unit tests for EREMOTEIO (besides EIO) ([1ca2233](1ca2233a914967558afbf57ed906df1a881c6e36))

- Update .gitignore ([b4a5086](b4a50869eef6ee66944f6a039a8107503b702f79))

- Do not make the struct invalid if some NUM_ARRAYS are zero ([73e9260](73e92605e7a44611b44433380fb4d28692916570))

- Make ARRAY and NUM_ARRAY variables available again by making the struct publicly available ([05bdccb](05bdccb457f1ecc30074e41d5ac5897ee3a7367e))

- Change to a new pin enumeration system ([1abfcdd](1abfcdd6812f09e30ce028ec466a69200bdddac6))


### 📚 Documentation

- Add changelog configuration ([35f8392](35f8392441166da3eb7ae370fe576a3a00fa220f))


## [1.0.7] - 2025-03-11

### 💼 Other

- Increment again the delay to ensure all devices were initialized ([d8220b6](d8220b66f482036ad02ac996c84a65223fae7e2e))

- Add undef of assert in Arduino_ESP32

Seems that assert can be included sometimes, even without the explicit header ([06fe4cf](06fe4cfc566d7d11c611f739e883bf0a8df54b1c))


## [1.0.6] - 2025-01-07

### 🐛 Bug Fixes

- Fix CFLAGS, PIE and out of source builds ([c1009de](c1009de939b69a1355f0004b66418f4c2d1b7724))

- Fix assert for Arduino_ESP32 to use with -Werror ([b84e48a](b84e48a92cfbe18110f1343a928dcdf7cfcf20c9))


## [1.0.5] - 2024-10-03

### 💼 Other

- Always dissable assert in Arduino_ESP32 platform ([e2a697a](e2a697a178ec42568c785a0c4e3a6090cddfc0dd))


## [1.0.4] - 2024-07-09

### 💼 Other

- Increment the delay a bit to ensure that it will not fail the initialization ([974c1d0](974c1d0b537648ce1090779ab25fe96b6f301bfe))


## [1.0.3] - 2024-07-01

### 🐛 Bug Fixes

- Fix early return codes + Remove extra asserts ([6a5b638](6a5b63885946788b5df90e74f184ad1dc10ec059))


### 💼 Other

- Ensure early return in all functions if I2C_BUS is not enabled ([0e5c5e1](0e5c5e1d2bd3c492f61558ab4c15715f52a19d1c))

- Do not use relative include paths ([e49f335](e49f335a3a875d27bd96f247785bccec4e5060ef))


## [1.0.2] - 2024-06-12

### 💼 Other

- Move normal GPIOs code before I2C interface check ([cda13b1](cda13b1b15f3df760fab6c2411a4cb0d9cbe8c1c))


## [1.0.1] - 2024-05-16

### 💼 Other

- Clearer error enum in expanded-gpio ([891e07f](891e07f91d435f5c9d2a94487cad7cceca224103))

- Possibility to use the expanded-gpio without I2C bus + Change array names to avoid collisions with other libraries ([c1cb2ca](c1cb2ca12b22baac1b9fa9e258a1c8093acc6061))


## [1.0.0] - 2024-04-16

### 🐛 Bug Fixes

- Fix CXX flags and add -Wextra ([75c18eb](75c18ebb61c7e03be3d5196abc48369af3ae62dd))

- Fix init_device error handling due to new changes ([53bb592](53bb5920fd9d777831c6ad1d61f49916d154074c))

- Fix Debug flags + Use assertions and fail if return_code != 0 ([1bf3784](1bf378466f7889a565bdf514a033ebfacad5434a))

- Fix init_device when peripheral was not started ([ba5f83d](ba5f83d2ff78aadc5b9a2e094c67d58de799b934))

- Fix warnings when compiling with Arduino IDE ([ca8bd0c](ca8bd0c2820052d5fa711201905e18eea36139f1))

- Fix mcp23017_write_all calling + Disable assertions unless explicitly told ([19a8729](19a8729278f0416d995a5de34d214f1c2e6fd331))

- Fix MCP23017 peripheral bad indexing and add new methods + Add support for normal_gpio_analog_read ([0493b0d](0493b0dbc83a87ac168db328db95ce8946f18147))

- Fix platform detecting of tests + Include assert.h in expanded-gpio.c ([2373e41](2373e41f56a230d585c01d80768d8ec24f391dfd))

- Fix values correction in Reads methods + Fix LTC2309 maximum range ([761b136](761b136fd342bdb2e60f7a70bebd98a0e4e6c033))


### 💼 Other

- Do not undef macros expanded-gpio
It still conflicts with the ESP32 boards ([28641be](28641beed982302e0b7c0c0311957d620db8d23d))

- Correct README ([bb3a61d](bb3a61d3687ce6e4fb42ea4409b41c6563feaad5))

- License the library under the LGPL-3.0-or-later and tests under the GPL-3.0-or-later ([916af69](916af69eb78c12708e183653357145d98454d4c9))

- Add README.md ([ea59965](ea5996548a14c45c90e3c5fe58a7e0bd5317c626))

- Castings and signature changes to make code clearer + expanded-gpio documentation ([3412020](341202003da465edea05c09c817e6c8dd79f0181))

- Add ERANGE and remove EALREADY + PCA9685 documentation and MCP230XX source documentation ([3e41e93](3e41e9300ea0c73a9dc2b8110e14a30c1db3605b))

- Peripherals MCP230XX + More intelligent write and read operations + Other corrections and remove some EALREADY ([5241439](524143985f0ad70f304ae62a1440d9d5b276e26b))

- LTC2309 documentation + Add checks of invalid conversion ([201e01f](201e01fcd2958b11bc3d973195ab835880878a56))

- ADS1015 documentation + Change name of ads1015_se_read to avoid confusion ([7d6adad](7d6adad6c109fd2e649079918100dc1dba44c5d9))

- Add missing errnos in ESP32 code of i2c-interface ([8fe95a5](8fe95a5a120e4f5e99aa16d32abae447d0096049))

- Documentation of i2c-interface + Unify errno between platforms and make them more clear + Separate platform-specific code into functions ([7e9d8bd](7e9d8bd3a3871b3d4cb87398b0d710c22048a43c))

- Add Debug_Sanitize build type + Try to read LTC2309 and ADS1015 when initializing in order to check if they exist ([d3862ef](d3862effac23d4fb4d2adb3a98265b3210ec9289))

- Remove PERIPHERALS_WITH_ASSERTIONS macro ([df6585e](df6585e5388801adb5594f2875b678651e7fa794))

- Make expanded-gpio capable of using PWM in normal GPIOs ([6107981](610798130065942506209a34589f186b2ac40515))

- Make a bigger delay before initializing MCP23017 (sometimes the 14 IOs fails) ([8ad46ad](8ad46ad9f64e7b4d04938ffde46644f8bd4c8c95))

- Make possible to change PCA9685 PWM frequency + analogWriteSetFrequency ([2a1261e](2a1261eee831f0f1ae597c61a7709817bf110c75))

- Force macro definitions in expanded-gpio.h ([28ab122](28ab122e0bd4d8067ae3fd6258c5738b6f3835b4))

- Add argument to initExpandedGPIO to enable/disable forced restart ([22eb8ac](22eb8ac85327601229147b8a53930221b990d6f5))

- Return -1 in expanded-gpio functions if no pin is found ([94d0a89](94d0a894d15bc4a4ab9dbca190cbdba89e36125c))

- Add CMakeLists.txt ([dd7c574](dd7c574d0170b54dcef658a2aa2681e48dbb64b9))

- Standardize all peripheral functions + Fix isAddressIntoArray function (Error from commit #4195fa7) ([bd634fc](bd634fc6b4676bdd54ccc6a4b3cea4cdb2a2ce40))

- Add EXTRA_CPPFLAGS in Makefile ([ac2b9e3](ac2b9e3ed6a0f726389ad03215db63f6a5c53f90))

- Change some internal names + Only use assert when functions cannot return error ([4195fa7](4195fa787038df8ac697a79e56f94a975ce3ce30))

- Add delay to MCP23017 initialisation if using Power-on reset ([c486844](c486844a4c76cfeb56b225ee4f23c2d7221bcb23))

- "tests" target in main Makefile ([9b7def1](9b7def12d3bd15310dd5290f970cdb2329369316))

- Peripheral MCP23017 + Beter ADS1015 + Support for ESP32 platform + Start adding asserts in expanded-gpio and lower thresholds for digitalReads ([3353d9a](3353d9a1ae4a856d77c2f8102be9f4b726e13a4d))

- Rename tests name, as it conflicts when doing a static library of everything ([22ee443](22ee443495493e47533a839564610ff87db8c22e))

- Peripheral ADS1015 + Optional expanded GPIOs ([7233953](723395395e3f8a35262fce9c75651593f2626d6c))

- Peripheral PCA9685 ([d71f92e](d71f92ef1c1e2893e906f5c25797761f4c0eba14))

- Do not make i2c_write_t.len const
It is useful to keep a buffer with a certain length, but use less than it's maximum length ([e7ff907](e7ff9079c381dd7c98161b2d98fdf02f3862d034))

- Add sanity checks, and remove the redundant ones ([d479bc7](d479bc75a5e22df1097bb7ac640cbd6f6fca381e))

- Add mcp23008_set_pin_mode_all + Test MCP23008 read_all and write_all ([4b9b8ec](4b9b8ec45badd1e519f4d771e86b26e34f54de1e))

- MCP23008 peripheral ([a16271b](a16271b56e702ff4fca7abdecd7f603217233d04))

- Run ltc2309_init_deinit_cycle test ([030a27b](030a27bdb7650438b7f8bb68f75a8367289e12a9))

- Add i2c_read method + LTC2309 peripheral + Better testing ([796d030](796d030790637e6678dff474f64cfb9e8d18f947))

- Use GCC -fanalyzer + New i2c_write and i2c_read functions + Complete i2c-interface unit testing ([adf7a25](adf7a2537a4a80fbaf1e8be02b17dd23637c6713))

- Upload tests/i2c-interface.c file ([cba5ed2](cba5ed2bc61b1992630487a52619a481c0abd051))

- Update .gitignore ([ccd98e6](ccd98e6cc0271ea3f404a7adc304078592f411b5))

- Start generalizing i2c-interface + Unity unit testing framework ([84f42f4](84f42f4cf1e6e69ae55e3630a2d9adad0f4f6704))

- Initial commit ([aaeced7](aaeced7f80f8346c9d08bdafbdea19c7342e6bfc))


<!-- generated by git-cliff -->
