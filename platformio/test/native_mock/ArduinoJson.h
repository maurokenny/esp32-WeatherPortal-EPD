#ifndef ARDUINOJSON_H
#define ARDUINOJSON_H

#include "Arduino.h"
#include <string>

namespace ArduinoJson {

// Error codes
class DeserializationError {
public:
    enum Code { Ok, EmptyInput, IncompleteInput, InvalidInput, NoMemory };
    
    DeserializationError(Code c = Ok) : code_(c) {}
    Code code() const { return code_; }
    bool operator==(Code c) const { return code_ == c; }
    bool operator!=(Code c) const { return code_ != c; }
    const char* c_str() const { return code_ == Ok ? "Ok" : "Error"; }
    
private:
    Code code_;
};

// Dummy JsonDocument - non-template version for simplicity
class JsonDocument {
public:
    bool overflowed() const { return false; }
    void clear() {}
};

// Template version for compatibility
template<size_t CAPACITY>
class StaticJsonDocument : public JsonDocument {};

// Filter option
namespace DeserializationOption {
    struct Filter {
        Filter(bool = true) {}
    };
}

// Deserialize functions
template<typename T>
DeserializationError deserializeJson(JsonDocument&, T&) {
    return DeserializationError();
}

template<typename T, typename F>
DeserializationError deserializeJson(JsonDocument&, T&, F) {
    return DeserializationError();
}

// Serialize stubs
template<typename T>
void serializeJson(const JsonDocument&, T&) {}

template<typename T>
void serializeJsonPretty(const JsonDocument&, T&) {}

} // namespace ArduinoJson

using ArduinoJson::JsonDocument;
using ArduinoJson::StaticJsonDocument;
using ArduinoJson::DeserializationError;
using ArduinoJson::deserializeJson;
using ArduinoJson::serializeJson;

#endif // ARDUINOJSON_H
