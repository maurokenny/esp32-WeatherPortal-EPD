/* Failure handler declarations for esp32-weather-epd.
 * Copyright (C) 2026 Mauro Freitas
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __FAILURE_HANDLER_H__
#define __FAILURE_HANDLER_H__

#include <Arduino.h>

// RTC failure counters (defined in wifi_manager.cpp, declared here for visibility)
extern uint8_t connectionFailCycles;
extern uint8_t ntpFailCycles;
extern uint8_t apiFailCycles;

enum FailureType {
  FAILURE_WIFI,
  FAILURE_NTP,
  FAILURE_API,
  FAILURE_BATTERY
};

void handleFailure(FailureType type, 
                   const String& line1, 
                   const String& line2 = "");

#endif
