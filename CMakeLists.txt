cmake_minimum_required(VERSION 3.18.4)
project("PLC peripherals of Industrial Shields")

# Set C and C++ standards
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Enable some warning flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS " -Wall")
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)


# Determine build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -DDEBUG -fno-omit-frame-pointer")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_C_FLAGS_DEBUG}")
else()
	# Change -O3 optimization to -O2
	string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS_RELEASE}")
endif()


# Enable compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include_directories(include)

file(GLOB SOURCES src/*.c)

add_library(plc-peripherals STATIC ${SOURCES})

set_target_properties(plc-peripherals PROPERTIES OUTPUT_NAME "plc-peripherals")
