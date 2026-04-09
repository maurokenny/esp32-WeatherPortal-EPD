#ifndef WIFI_H
#define WIFI_H

#include "Arduino.h"
#include <cstring>

// WiFi modes
#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP 2
#define WIFI_AP_STA 3

// WiFi status
#define WL_CONNECTED 3
#define WL_NO_SHIELD 255
#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_SCAN_COMPLETED 2
#define WL_CONNECT_FAILED 4
#define WL_CONNECTION_LOST 5
#define WL_DISCONNECTED 6

// WiFi events
typedef enum {
    WIFI_EVENT_WIFI_READY = 0,
    WIFI_EVENT_SCAN_DONE,
    WIFI_EVENT_STA_START,
    WIFI_EVENT_STA_STOP,
    WIFI_EVENT_STA_CONNECTED,
    WIFI_EVENT_STA_DISCONNECTED,
    WIFI_EVENT_STA_AUTHMODE_CHANGE,
    WIFI_EVENT_STA_GOT_IP,
    WIFI_EVENT_STA_LOST_IP,
    WIFI_EVENT_STA_WPS_ER_SUCCESS,
    WIFI_EVENT_STA_WPS_ER_FAILED,
    WIFI_EVENT_STA_WPS_ER_TIMEOUT,
    WIFI_EVENT_STA_WPS_ER_PIN,
    WIFI_EVENT_AP_START,
    WIFI_EVENT_AP_STOP,
    WIFI_EVENT_AP_STACONNECTED,
    WIFI_EVENT_AP_STADISCONNECTED,
    WIFI_EVENT_AP_STAIPASSIGNED,
    WIFI_EVENT_AP_PROBEREQRECVED,
    WIFI_EVENT_GOT_IP6,
    WIFI_EVENT_ETH_START,
    WIFI_EVENT_ETH_STOP,
    WIFI_EVENT_ETH_CONNECTED,
    WIFI_EVENT_ETH_DISCONNECTED,
    WIFI_EVENT_ETH_GOT_IP,
    WIFI_EVENT_MAX
} system_event_id_t;

// IP Address class (minimal)
class IPAddress {
protected:
    uint8_t bytes[4];
    
public:
    IPAddress() { memset(bytes, 0, 4); }
    IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
        bytes[0] = first;
        bytes[1] = second;
        bytes[2] = third;
        bytes[3] = fourth;
    }
    IPAddress(uint32_t address) {
        bytes[0] = (address >> 0) & 0xFF;
        bytes[1] = (address >> 8) & 0xFF;
        bytes[2] = (address >> 16) & 0xFF;
        bytes[3] = (address >> 24) & 0xFF;
    }
    
    uint8_t operator[](int index) const { return bytes[index]; }
    uint8_t& operator[](int index) { return bytes[index]; }
    
    operator uint32_t() const {
        return ((uint32_t)bytes[0] << 0) | ((uint32_t)bytes[1] << 8) |
               ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    }
    
    const char* toString() const {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
        return buf;
    }
};

// WiFiClient class
class WiFiClient {
protected:
    bool connected_ = false;
    std::string buffer_;
    size_t position_ = 0;
    
public:
    WiFiClient() = default;
    
    int connect(const char* host, uint16_t port) { 
        (void)host; (void)port; 
        connected_ = true; 
        return 1; 
    }
    int connect(IPAddress ip, uint16_t port) { 
        (void)ip; (void)port; 
        connected_ = true; 
        return 1; 
    }
    
    size_t write(const uint8_t* buf, size_t size) { 
        (void)buf;
        return size; 
    }
    size_t write(uint8_t c) { (void)c; return 1; }
    
    int available() { return buffer_.size() - position_; }
    
    int read() {
        if (position_ < buffer_.size()) {
            return static_cast<uint8_t>(buffer_[position_++]);
        }
        return -1;
    }
    int read(uint8_t* buf, size_t size) {
        size_t toRead = std::min(size, buffer_.size() - position_);
        for (size_t i = 0; i < toRead; i++) {
            buf[i] = buffer_[position_++];
        }
        return static_cast<int>(toRead);
    }
    
    int peek() {
        if (position_ < buffer_.size()) {
            return static_cast<uint8_t>(buffer_[position_]);
        }
        return -1;
    }
    
    void flush() {}
    void stop() { connected_ = false; }
    uint8_t connected() { return connected_; }
    
    // Test helper
    void setTestData(const std::string& data) {
        buffer_ = data;
        position_ = 0;
    }
    
    operator bool() { return connected_; }
};

// WiFiServer class
class WiFiServer {
protected:
    uint16_t port_;
    
public:
    WiFiServer(uint16_t port = 80) : port_(port) {}
    
    void begin() {}
    WiFiClient available() { return WiFiClient(); }
    WiFiClient accept() { return WiFiClient(); }
};

// WiFiUDP class (minimal)
class WiFiUDP {
public:
    uint8_t begin(uint16_t port) { (void)port; return 1; }
    void stop() {}
    int beginPacket(const char* host, uint16_t port) { (void)host; (void)port; return 1; }
    size_t write(const uint8_t* buffer, size_t size) { (void)buffer; return size; }
    int endPacket() { return 1; }
    int parsePacket() { return 0; }
    int available() { return 0; }
    int read(unsigned char* buffer, size_t len) { (void)buffer; (void)len; return 0; }
};

// WiFi class
class WiFiClass {
protected:
    int status_ = WL_DISCONNECTED;
    String ssid_;
    int rssi_ = -50;
    IPAddress localIP_;
    
public:
    int begin(const char* ssid, const char* passphrase = nullptr, int32_t channel = 0, 
              const uint8_t* bssid = nullptr, bool connect = true) {
        (void)passphrase; (void)channel; (void)bssid; (void)connect;
        ssid_ = ssid;
        status_ = WL_CONNECTED;
        localIP_ = IPAddress(192, 168, 1, 100);
        return WL_CONNECTED;
    }
    
    int begin() { return WL_CONNECTED; }
    void disconnect(bool wifioff = false) { (void)wifioff; status_ = WL_DISCONNECTED; }
    
    int status() { return status_; }
    
    String SSID() { return ssid_; }
    String psk() { return "password"; }
    
    int32_t RSSI() { return rssi_; }
    
    IPAddress localIP() { return localIP_; }
    IPAddress subnetMask() { return IPAddress(255, 255, 255, 0); }
    IPAddress gatewayIP() { return IPAddress(192, 168, 1, 1); }
    IPAddress dnsIP(uint8_t dns_no = 0) { (void)dns_no; return IPAddress(8, 8, 8, 8); }
    IPAddress broadcastIP() { return IPAddress(192, 168, 1, 255); }
    
    uint8_t* macAddress(uint8_t* mac) {
        mac[0] = 0xDE; mac[1] = 0xAD; mac[2] = 0xBE; mac[3] = 0xEF; mac[4] = 0xFE; mac[5] = 0xED;
        return mac;
    }
    
    String macAddress() { return "DE:AD:BE:EF:FE:ED"; }
    
    void mode(int m) { (void)m; }
    int getMode() { return WIFI_STA; }
    
    void setAutoConnect(bool autoConnect) { (void)autoConnect; }
    bool getAutoConnect() { return true; }
    
    void setAutoReconnect(bool autoReconnect) { (void)autoReconnect; }
    bool getAutoReconnect() { return true; }
    
    // Scanning (mock)
    int8_t scanNetworks() { return 3; }
    String SSID(uint8_t i) { 
        (void)i;
        const char* nets[] = {"Network1", "Network2", "Network3"};
        return nets[i % 3];
    }
    int32_t RSSI(uint8_t i) { (void)i; return -40 - (i * 10); }
    
    // Hostname
    bool setHostname(const char* hostname) { (void)hostname; return true; }
    const char* getHostname() { return "esp32-test"; }
    
    // Test helpers
    void setStatus(int s) { status_ = s; }
    void setRSSI(int rssi) { rssi_ = rssi; }
};

// Global WiFi instance
static WiFiClass WiFi;

#endif // WIFI_H
