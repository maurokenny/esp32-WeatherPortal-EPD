/* Arduino Compatibility Layer for Native Blackbox Testing
 * Copyright (C) 2025
 *
 * This file includes the native mocks for Arduino functionality.
 * The actual mocks are in test/native_mock/ which is added to the include path.
 */

#ifndef ARDUINO_COMPAT_H
#define ARDUINO_COMPAT_H

// Arduino.h is provided by test/native_mock/ which is in the include path
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>

#endif // ARDUINO_COMPAT_H
