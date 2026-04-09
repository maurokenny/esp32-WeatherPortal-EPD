#ifndef ARDUINO_H
#define ARDUINO_H

#include <string>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdarg>
#include <cmath>

#ifndef _WIN32
#include <unistd.h>
#endif

// Basic types
typedef unsigned char byte;
typedef uint16_t word;

// String class
class String : public std::string {
public:
    String() : std::string() {}
    String(const char* str) : std::string(str ? str : "") {}
    String(const std::string& str) : std::string(str) {}
    String(int val) : std::string(std::to_string(val)) {}
    String(long val) : std::string(std::to_string(val)) {}
    String(float val) : std::string(std::to_string(val)) {}
    String(double val) : std::string(std::to_string(val)) {}
    
    int length() const { return static_cast<int>(std::string::length()); }
    const char* c_str() const { return std::string::c_str(); }
    bool isEmpty() const { return empty(); }
};

// Serial mock
class MockSerial {
public:
    void begin(unsigned long) {}
    void println(const char* s) { (void)s; }
    void println(const String& s) { (void)s; }
    void println() {}
    void print(const char* s) { (void)s; }
    void print(const String& s) { (void)s; }
    void printf(const char* fmt, ...) { 
        va_list args; 
        va_start(args, fmt); 
        vprintf(fmt, args); 
        va_end(args); 
    }
};

// Math
#ifndef PI
#define PI 3.14159265358979323846
#endif

#define DEG_TO_RAD 0.017453292519943295
#define RAD_TO_DEG 57.29577951308232

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

inline float constrain(float x, float a, float b) {
    return x < a ? a : (x > b ? b : x);
}

// Time
inline unsigned long millis() { return 0; }
inline void delay(unsigned long ms) { 
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000); 
#endif
}

// I/O stubs
#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }

// Random
inline void randomSeed(unsigned long seed) { srand(seed); }
inline long random(long max) { return rand() % max; }

// min/max
using std::min;
using std::max;

// Bit operations
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))

#endif // ARDUINO_H
