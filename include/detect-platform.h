#ifndef __DETECT_PLATFORM_H__
#define __DETECT_PLATFORM_H__

// Compatible platforms
#define Linux 0
#define Arduino_ESP32 1

#if defined(ARDUINO_ESP32) || defined(ARDUINO_ARCH_ESP32)
#define PLC_ENVIRONMENT Arduino_ESP32

#elif defined(__linux__)
#define PLC_ENVIRONMENT Linux

#else
    #error "Unknown environment detected"
#endif

#endif // __DETECT_PLATFORM_H__
