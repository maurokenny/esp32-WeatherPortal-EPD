#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "Arduino.h"
#include "WiFi.h"

// HTTP methods
#define HTTP_CODE_OK 200
#define HTTP_CODE_NOT_FOUND 404
#define HTTP_CODE_BAD_REQUEST 400
#define HTTP_CODE_UNAUTHORIZED 401
#define HTTP_CODE_FORBIDDEN 403
#define HTTP_CODE_NOT_FOUND 404
#define HTTP_CODE_INTERNAL_SERVER_ERROR 500

class HTTPClient {
public:
    HTTPClient() = default;
    
    void begin(const char* url) { (void)url; }
    void begin(const String& url) { (void)url; }
    void begin(const char* host, uint16_t port, const char* uri) { (void)host; (void)port; (void)uri; }
    
    void end() {}
    
    void addHeader(const char* name, const char* value) { (void)name; (void)value; }
    void addHeader(const String& name, const String& value) { (void)name; (void)value; }
    
    int GET() { return HTTP_CODE_OK; }
    int POST(const char* payload) { (void)payload; return HTTP_CODE_OK; }
    int POST(const String& payload) { (void)payload; return HTTP_CODE_OK; }
    int PUT(const char* payload) { (void)payload; return HTTP_CODE_OK; }
    int PATCH(const char* payload) { (void)payload; return HTTP_CODE_OK; }
    int DELETE() { return HTTP_CODE_OK; }
    
    int getSize() { return 0; }
    WiFiClient& getStream() { return client_; }
    
    const char* getString() { return "{}"; }
    
    int getWriteError() { return 0; }
    void resetWriteError() {}
    
protected:
    WiFiClient client_;
};

#endif // HTTPCLIENT_H
