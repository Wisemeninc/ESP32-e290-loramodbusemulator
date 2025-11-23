#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <HTTPServer.hpp>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include "server_cert.h"
#include "auth_manager.h"
#include "wifi_manager.h"
#include "modbus_handler.h"
#include "lorawan_handler.h"
#include "sf6_emulator.h"
#include "web_pages.h"

using namespace httpsserver;

class WebServerManager {
public:
    WebServerManager();
    
    // Initialization
    void begin();
    
    // Main loop task
    void handle();
    
private:
    // Server instances
    HTTPSServer* server;
    HTTPServer* redirectServer;
    SSLCert* cert;
    
    // Task handle
    TaskHandle_t serverTaskHandle;
    static void serverTask(void* parameter);

    // Helper functions
    void setupRoutes();
    String readPostBody(HTTPRequest * req);
    bool getPostParameterFromBody(const String& body, const char* name, String& value);
    void sendRedirect(HTTPResponse * res, const char* title, const char* message, const char* url, int delay = 3);
    void printHTMLHeader(HTTPResponse * res);
    bool getDarkMode();
    void setDarkMode(bool enabled);
    
    // Request Handlers
    static void handleRoot(HTTPRequest * req, HTTPResponse * res);
    static void handleStats(HTTPRequest * req, HTTPResponse * res);
    static void handleRegisters(HTTPRequest * req, HTTPResponse * res);
    static void handleLoRaWAN(HTTPRequest * req, HTTPResponse * res);
    static void handleLoRaWANProfiles(HTTPRequest * req, HTTPResponse * res);
    static void handleWiFi(HTTPRequest * req, HTTPResponse * res);
    static void handleSecurity(HTTPRequest * req, HTTPResponse * res);
    
    // Action Handlers
    static void handleConfig(HTTPRequest * req, HTTPResponse * res);
    static void handleLoRaWANConfig(HTTPRequest * req, HTTPResponse * res);
    static void handleLoRaWANProfileUpdate(HTTPRequest * req, HTTPResponse * res);
    static void handleLoRaWANProfileToggle(HTTPRequest * req, HTTPResponse * res);
    static void handleLoRaWANProfileActivate(HTTPRequest * req, HTTPResponse * res);
    static void handleLoRaWANAutoRotate(HTTPRequest * req, HTTPResponse * res);
    static void handleWiFiScan(HTTPRequest * req, HTTPResponse * res);
    static void handleWiFiConnect(HTTPRequest * req, HTTPResponse * res);
    static void handleWiFiStatus(HTTPRequest * req, HTTPResponse * res);
    static void handleSecurityUpdate(HTTPRequest * req, HTTPResponse * res);
    static void handleDebugUpdate(HTTPRequest * req, HTTPResponse * res);
    static void handleSF6Update(HTTPRequest * req, HTTPResponse * res);
    static void handleSF6Reset(HTTPRequest * req, HTTPResponse * res);
    static void handleEnableAuth(HTTPRequest * req, HTTPResponse * res);
    static void handleDarkMode(HTTPRequest * req, HTTPResponse * res);
    static void handleResetNonces(HTTPRequest * req, HTTPResponse * res);
    static void handleFactoryReset(HTTPRequest * req, HTTPResponse * res);
    static void handleReboot(HTTPRequest * req, HTTPResponse * res);
    static void handleRedirect(HTTPRequest * req, HTTPResponse * res);
};

// Global instance
extern WebServerManager webServer;

#endif // WEB_SERVER_H