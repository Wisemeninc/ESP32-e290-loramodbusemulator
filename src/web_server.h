#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <esp_https_server.h>
#include "server_cert.h"
#include "auth_manager.h"
#include "wifi_manager.h"
#include "modbus_handler.h"
#include "lorawan_handler.h"
#include "sf6_emulator.h"
#include "ota_manager.h"
#include "web_pages.h"

class WebServerManager {
public:
    WebServerManager();
    
    // Initialization
    void begin();
    
    // Main loop task (no-op for async)
    void handle();
    
private:
    // HTTPS server handle (esp_https_server native)
    httpd_handle_t httpsServer;
    
    // HTTP redirect server (esp_http_server native)
    httpd_handle_t httpServer;

    // Helper functions
    void setupRoutes();
    static String buildHTMLHeader();
    static String buildHTMLFooter();
    static bool getDarkMode();
    
    // Static request handlers for esp_https_server
    static esp_err_t handleRoot(httpd_req_t *req);
    static esp_err_t handleStats(httpd_req_t *req);
    static esp_err_t handleRegisters(httpd_req_t *req);
    static esp_err_t handleLoRaWAN(httpd_req_t *req);
    static esp_err_t handleLoRaWANProfiles(httpd_req_t *req);
    static esp_err_t handleWiFi(httpd_req_t *req);
    static esp_err_t handleSecurity(httpd_req_t *req);
    
    // Action Handlers
    static esp_err_t handleConfig(httpd_req_t *req);
    static esp_err_t handleLoRaWANConfig(httpd_req_t *req);
    static esp_err_t handleLoRaWANProfileUpdate(httpd_req_t *req);
    static esp_err_t handleLoRaWANProfileToggle(httpd_req_t *req);
    static esp_err_t handleLoRaWANProfileActivate(httpd_req_t *req);
    static esp_err_t handleLoRaWANAutoRotate(httpd_req_t *req);
    static esp_err_t handleWiFiScan(httpd_req_t *req);
    static esp_err_t handleWiFiConnect(httpd_req_t *req);
    static esp_err_t handleWiFiStatus(httpd_req_t *req);
    static esp_err_t handleSecurityUpdate(httpd_req_t *req);
    static esp_err_t handleDebugUpdate(httpd_req_t *req);
    static esp_err_t handleSF6Update(httpd_req_t *req);
    static esp_err_t handleSF6Reset(httpd_req_t *req);
    static esp_err_t handleEnableAuth(httpd_req_t *req);
    static esp_err_t handleDarkMode(httpd_req_t *req);
    static esp_err_t handleResetNonces(httpd_req_t *req);
    static esp_err_t handleFactoryReset(httpd_req_t *req);
    static esp_err_t handleReboot(httpd_req_t *req);
    
    // OTA Update Handlers
    static esp_err_t handleOTA(httpd_req_t *req);
    static esp_err_t handleOTAConfig(httpd_req_t *req);
    static esp_err_t handleOTACheck(httpd_req_t *req);
    static esp_err_t handleOTAStart(httpd_req_t *req);
    static esp_err_t handleOTAStatus(httpd_req_t *req);
    static esp_err_t handleOTAAutoInstall(httpd_req_t *req);
    
    // Helper for authentication check
    static bool checkAuth(httpd_req_t *req);
    static void sendUnauthorized(httpd_req_t *req);
    static String getPostBody(httpd_req_t *req);
    static bool getPostParameter(const String& body, const char* name, String& value);
    static String getQueryParameter(httpd_req_t *req, const char* name);
    static void sendRedirectPage(httpd_req_t *req, const char* title, const char* message, const char* url, int delay = 3);
};

// Global instance
extern WebServerManager webServer;

#endif // WEB_SERVER_H