# Copyright (c) 2024 Industrial Shields. All rights reserved
#
# This file is part of plc-peripherals.
#
# plc-peripherals is free software: you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# plc-peripherals is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

export CC := gcc
export CPPFLAGS += -I$(realpath include)
export CFLAGS += -Wall -Wextra -Werror -fanalyzer
export LDFLAGS

BUILD_TYPE ?= Release
ifeq ($(BUILD_TYPE),Debug)
	CPPFLAGS += -DDEBUG
	CFLAGS += -O0 -g -fno-omit-frame-pointer
	LDFLAGS += -fstack-protector-strong
else ifeq ($(BUILD_TYPE),Debug_Sanitize)
	CPPFLAGS += -DDEBUG
	CFLAGS += -O0 -g -fno-omit-frame-pointer -fsanitize=address
	LDFLAGS += -fstack-protector-strong -static-libasan
else
	CFLAGS += -O2
endif

SRC_DIR := src
BUILD_DIR := build
export LIBNAME := libplc-peripherals.a

export ABS_SRC_DIR := $(realpath $(SRC_DIR))
export ABS_BUILD_DIR := $(patsubst %/$(SRC_DIR), %/$(BUILD_DIR), $(ABS_SRC_DIR))
LDFLAGS += -L$(ABS_BUILD_DIR) -lplc-peripherals

SRCS := $(filter-out $(SRC_DIR)/expanded-gpio.c, $(wildcard $(SRC_DIR)/*.c))
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
LIB := $(BUILD_DIR)/$(LIBNAME)

.PHONY: all with_expanded_gpio clean tests

all: $(LIB)


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $(OBJS)


with_expanded_gpio: $(OBJS) $(BUILD_DIR)/expanded-gpio.o | $(BUILD_DIR)
	ar rcs $(LIB) $^

tests: $(LIB)
	make -C tests/

clean:
	rm -rf $(BUILD_DIR)
