export CC := gcc
export CPPFLAGS := -I$(realpath include)
export CFLAGS := -Wall -Wextra -Werror -fPIC -fanalyzer
export LDFLAGS

BUILD_TYPE ?= Release
ifeq ($(BUILD_TYPE),Debug)
	CPPFLAGS += -DDEBUG
	CFLAGS += -O0 -g -fsanitize=address -fsanitize=undefined -fsanitize=pointer-subtract -fsanitize=leak -fsanitize=signed-integer-overflow -fno-omit-frame-pointer -coverage
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

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
LIB := $(BUILD_DIR)/$(LIBNAME)

.PHONY: all clean tests

all: $(LIB)


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $(OBJS)


tests: $(LIB)
	make -C tests/


clean:
	rm -rf $(BUILD_DIR)
