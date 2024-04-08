# Copyright (c) 2024 Industrial Shields. All rights reserved
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

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
