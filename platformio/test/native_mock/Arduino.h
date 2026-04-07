#ifndef ARDUINO_H
#define ARDUINO_H

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <stdarg.h>
#include <stdio.h>
#include <cstring>
#include <stdint.h>

// Minimal mock of Arduino's String class using std::string
typedef std::string String;

// Mock max/min functions which are globally available in Arduino
using std::max;
using std::min;

// Mock Serial for native tests
class MockSerial {
public:
    void begin(unsigned long baudrate) { (void)baudrate; }
    void end() { }
    void flush() { std::cout.flush(); }
    size_t write(uint8_t c) { std::cout << (char)c; return 1; }
    size_t write(const char* s) { std::cout << s; return strlen(s); }
    
    void print(const String& s) { std::cout << s; }
    void println(const String& s) { std::cout << s << std::endl; }
    
    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
};

extern MockSerial Serial;

#endif // ARDUINO_H
