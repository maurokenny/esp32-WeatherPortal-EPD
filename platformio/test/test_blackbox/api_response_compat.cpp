/* API Response Compatibility Implementation
 * Copyright (C) 2025
 */

#include "api_response_compat.h"
#include <sstream>
#include <iomanip>

namespace blackbox {

std::string RenderEvent::toJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"event_type\":\"" << event_type << "\",";
    oss << "\"icon_name\":\"" << icon_name << "\",";
    oss << "\"x\":" << x << ",";
    oss << "\"y\":" << y << ",";
    oss << "\"weather_code\":" << weather_code << ",";
    oss << "\"pop\":" << pop << ",";
    oss << "\"precipitation\":" << std::fixed << std::setprecision(1) << precipitation;
    if (!timestamp.empty()) {
        oss << ",\"timestamp\":\"" << timestamp << "\"";
    }
    oss << "}";
    return oss.str();
}

} // namespace blackbox
