TESTS_DIR := .
ABS_TESTS_BUILD_DIR := $(ABS_BUILD_DIR)/tests
UNITY_DIR := Unity/src

CPPFLAGS := $(CPPFLAGS) -I$(UNITY_DIR)
CFLAGS := $(CFLAGS) $(UNITY_DIR)/unity.c

SRCS := $(wildcard $(TESTS_DIR)/*.c)
TESTS := $(patsubst $(TESTS_DIR)/%.c, $(ABS_TESTS_BUILD_DIR)/%, $(SRCS))

.PHONY: all clean tests

all: $(TESTS)


$(ABS_TESTS_BUILD_DIR):
	mkdir -p $(ABS_TESTS_BUILD_DIR)


$(ABS_TESTS_BUILD_DIR)/%: $(TESTS_DIR)/%.c | $(ABS_TESTS_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDFLAGS)
