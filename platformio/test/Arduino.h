#ifndef ARDUINO_H
#define ARDUINO_H

#include <string>
#include <vector>
#include <iostream>

#include <algorithm>

// Minimal mock of Arduino's String class using std::string
typedef std::string String;

// Mock max/min functions which are globally available in Arduino
using std::max;
using std::min;

// Mock Serial for native tests if needed
class MockSerial {
public:
    void print(const String& s) { std::cout << s; }
    void println(const String& s) { std::cout << s << std::endl; }
    void printf(const char* format, ...) { /* ignore for now */ }
};

extern MockSerial Serial;

#endif
