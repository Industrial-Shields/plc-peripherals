export CC := gcc
export CPPFLAGS += -I$(realpath include)
export CFLAGS += -Wall -Wextra -Werror -fanalyzer
export LDFLAGS

BUILD_TYPE ?= Release
ifeq ($(BUILD_TYPE),Debug)
	CPPFLAGS += -DDEBUG
	CFLAGS += -O0 -g -fno-omit-frame-pointer
	LDFLAGS += -fstack-protector-strong
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
