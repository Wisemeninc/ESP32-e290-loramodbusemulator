#include "web_server.h"
#include "web_pages.h"
#include "config.h"
#include <Preferences.h>
#include <esp_tls.h>

// Global instance
WebServerManager webServer;

// Helper to access the instance from static methods
static WebServerManager* instance = nullptr;

// HTTP redirect handler
static esp_err_t http_redirect_handler(httpd_req_t *req) {
    char host[64] = {0};
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));
    
    // Remove port from host if present
    char* colonPtr = strchr(host, ':');
    if (colonPtr) *colonPtr = '\0';
    
    String redirectUrl = "https://" + String(host) + String(req->uri);
    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", redirectUrl.c_str());
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

WebServerManager::WebServerManager() : httpsServer(nullptr), httpServer(nullptr) {
    instance = this;
}

void WebServerManager::begin() {
    // Configure HTTPS server with SSL certificate
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    
    // Use PEM certificate from server_cert.h
    // Note: ESP-IDF uses "cacert" to mean "server cert" (confusing naming as per their comments)
    // For PEM format, mbedtls requires null terminator included in length
    config.cacert_pem = (const uint8_t*)server_cert_pem;
    config.cacert_len = strlen(server_cert_pem) + 1;
    config.prvtkey_pem = (const uint8_t*)server_key_pem;
    config.prvtkey_len = strlen(server_key_pem) + 1;
    
    Serial.printf("Cert len: %d, Key len: %d\n", config.cacert_len, config.prvtkey_len);
    Serial.printf("Cert starts: %.30s\n", server_cert_pem);
    Serial.printf("Key starts: %.30s\n", server_key_pem);
    
    // Configure server settings
    config.httpd.max_uri_handlers = 30;
    config.httpd.stack_size = 16384;  // Large stack for SSL
    config.httpd.server_port = 443;
    config.port_secure = 443;
    
    // Start HTTPS server
    esp_err_t ret = httpd_ssl_start(&httpsServer, &config);
    if (ret != ESP_OK) {
        Serial.printf("Error starting HTTPS server: 0x%x\n", ret);
        return;
    }
    
    Serial.println("HTTPS server started on port 443");
    
    // Setup routes
    setupRoutes();
    
    // Start HTTP redirect server on port 80
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.server_port = 80;
    http_config.uri_match_fn = httpd_uri_match_wildcard;
    
    ret = httpd_start(&httpServer, &http_config);
    if (ret == ESP_OK) {
        // Register wildcard handler to redirect all HTTP to HTTPS
        httpd_uri_t redirect_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = http_redirect_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(httpServer, &redirect_uri);
        Serial.println("HTTP redirect server started on port 80");
    } else {
        Serial.printf("Failed to start HTTP redirect server: %d\n", ret);
    }
}

void WebServerManager::handle() {
    // No-op - esp_https_server handles requests asynchronously
}

void WebServerManager::setupRoutes() {
    // GET routes
    httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = handleRoot, .user_ctx = nullptr };
    httpd_uri_t uri_stats = { .uri = "/stats", .method = HTTP_GET, .handler = handleStats, .user_ctx = nullptr };
    httpd_uri_t uri_registers = { .uri = "/registers", .method = HTTP_GET, .handler = handleRegisters, .user_ctx = nullptr };
    httpd_uri_t uri_lorawan = { .uri = "/lorawan", .method = HTTP_GET, .handler = handleLoRaWAN, .user_ctx = nullptr };
    httpd_uri_t uri_lorawan_profiles = { .uri = "/lorawan/profiles", .method = HTTP_GET, .handler = handleLoRaWANProfiles, .user_ctx = nullptr };
    httpd_uri_t uri_wifi = { .uri = "/wifi", .method = HTTP_GET, .handler = handleWiFi, .user_ctx = nullptr };
    httpd_uri_t uri_wifi_scan = { .uri = "/wifi/scan", .method = HTTP_GET, .handler = handleWiFiScan, .user_ctx = nullptr };
    httpd_uri_t uri_wifi_status = { .uri = "/wifi/status", .method = HTTP_GET, .handler = handleWiFiStatus, .user_ctx = nullptr };
    httpd_uri_t uri_security = { .uri = "/security", .method = HTTP_GET, .handler = handleSecurity, .user_ctx = nullptr };
    httpd_uri_t uri_ota = { .uri = "/ota", .method = HTTP_GET, .handler = handleOTA, .user_ctx = nullptr };
    httpd_uri_t uri_ota_check = { .uri = "/ota/check", .method = HTTP_GET, .handler = handleOTACheck, .user_ctx = nullptr };
    httpd_uri_t uri_ota_start = { .uri = "/ota/start", .method = HTTP_GET, .handler = handleOTAStart, .user_ctx = nullptr };
    httpd_uri_t uri_ota_status = { .uri = "/ota/status", .method = HTTP_GET, .handler = handleOTAStatus, .user_ctx = nullptr };
    httpd_uri_t uri_ota_auto = { .uri = "/ota/auto-install", .method = HTTP_GET, .handler = handleOTAAutoInstall, .user_ctx = nullptr };
    httpd_uri_t uri_reboot = { .uri = "/reboot", .method = HTTP_GET, .handler = handleReboot, .user_ctx = nullptr };
    httpd_uri_t uri_factory_reset = { .uri = "/factory-reset", .method = HTTP_GET, .handler = handleFactoryReset, .user_ctx = nullptr };
    httpd_uri_t uri_reset_nonces = { .uri = "/lorawan/reset-nonces", .method = HTTP_GET, .handler = handleResetNonces, .user_ctx = nullptr };
    httpd_uri_t uri_sf6_update = { .uri = "/sf6/update", .method = HTTP_GET, .handler = handleSF6Update, .user_ctx = nullptr };
    httpd_uri_t uri_sf6_reset = { .uri = "/sf6/reset", .method = HTTP_GET, .handler = handleSF6Reset, .user_ctx = nullptr };
    httpd_uri_t uri_profile_toggle = { .uri = "/lorawan/profile/toggle", .method = HTTP_GET, .handler = handleLoRaWANProfileToggle, .user_ctx = nullptr };
    httpd_uri_t uri_profile_activate = { .uri = "/lorawan/profile/activate", .method = HTTP_GET, .handler = handleLoRaWANProfileActivate, .user_ctx = nullptr };
    httpd_uri_t uri_auto_rotate = { .uri = "/lorawan/auto-rotate", .method = HTTP_GET, .handler = handleLoRaWANAutoRotate, .user_ctx = nullptr };
    httpd_uri_t uri_darkmode = { .uri = "/darkmode", .method = HTTP_GET, .handler = handleDarkMode, .user_ctx = nullptr };
    httpd_uri_t uri_enable_auth = { .uri = "/security/enable", .method = HTTP_GET, .handler = handleEnableAuth, .user_ctx = nullptr };

    // POST routes
    httpd_uri_t uri_config = { .uri = "/config", .method = HTTP_POST, .handler = handleConfig, .user_ctx = nullptr };
    httpd_uri_t uri_lorawan_config = { .uri = "/lorawan/config", .method = HTTP_POST, .handler = handleLoRaWANConfig, .user_ctx = nullptr };
    httpd_uri_t uri_profile_update = { .uri = "/lorawan/profile/update", .method = HTTP_POST, .handler = handleLoRaWANProfileUpdate, .user_ctx = nullptr };
    httpd_uri_t uri_wifi_connect = { .uri = "/wifi/connect", .method = HTTP_POST, .handler = handleWiFiConnect, .user_ctx = nullptr };
    httpd_uri_t uri_security_update = { .uri = "/security/update", .method = HTTP_POST, .handler = handleSecurityUpdate, .user_ctx = nullptr };
    httpd_uri_t uri_debug_update = { .uri = "/security/debug", .method = HTTP_POST, .handler = handleDebugUpdate, .user_ctx = nullptr };
    httpd_uri_t uri_ota_config = { .uri = "/ota/config", .method = HTTP_POST, .handler = handleOTAConfig, .user_ctx = nullptr };

    // Register all handlers
    httpd_register_uri_handler(httpsServer, &uri_root);
    httpd_register_uri_handler(httpsServer, &uri_stats);
    httpd_register_uri_handler(httpsServer, &uri_registers);
    httpd_register_uri_handler(httpsServer, &uri_lorawan);
    httpd_register_uri_handler(httpsServer, &uri_lorawan_profiles);
    httpd_register_uri_handler(httpsServer, &uri_wifi);
    httpd_register_uri_handler(httpsServer, &uri_wifi_scan);
    httpd_register_uri_handler(httpsServer, &uri_wifi_status);
    httpd_register_uri_handler(httpsServer, &uri_security);
    httpd_register_uri_handler(httpsServer, &uri_ota);
    httpd_register_uri_handler(httpsServer, &uri_ota_check);
    httpd_register_uri_handler(httpsServer, &uri_ota_start);
    httpd_register_uri_handler(httpsServer, &uri_ota_status);
    httpd_register_uri_handler(httpsServer, &uri_ota_auto);
    httpd_register_uri_handler(httpsServer, &uri_reboot);
    httpd_register_uri_handler(httpsServer, &uri_factory_reset);
    httpd_register_uri_handler(httpsServer, &uri_reset_nonces);
    httpd_register_uri_handler(httpsServer, &uri_sf6_update);
    httpd_register_uri_handler(httpsServer, &uri_sf6_reset);
    httpd_register_uri_handler(httpsServer, &uri_profile_toggle);
    httpd_register_uri_handler(httpsServer, &uri_profile_activate);
    httpd_register_uri_handler(httpsServer, &uri_auto_rotate);
    httpd_register_uri_handler(httpsServer, &uri_darkmode);
    httpd_register_uri_handler(httpsServer, &uri_enable_auth);
    httpd_register_uri_handler(httpsServer, &uri_config);
    httpd_register_uri_handler(httpsServer, &uri_lorawan_config);
    httpd_register_uri_handler(httpsServer, &uri_profile_update);
    httpd_register_uri_handler(httpsServer, &uri_wifi_connect);
    httpd_register_uri_handler(httpsServer, &uri_security_update);
    httpd_register_uri_handler(httpsServer, &uri_debug_update);
    httpd_register_uri_handler(httpsServer, &uri_ota_config);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool WebServerManager::checkAuth(httpd_req_t *req) {
    if (!authManager.checkAuthentication(req)) {
        sendUnauthorized(req);
        return false;
    }
    return true;
}

void WebServerManager::sendUnauthorized(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Vision Master E290\"");
    httpd_resp_set_type(req, "text/html");
    const char* html = "<!DOCTYPE html><html><body><h1>401 Unauthorized</h1><p>Authentication required</p></body></html>";
    httpd_resp_send(req, html, strlen(html));
}

String WebServerManager::getPostBody(httpd_req_t *req) {
    int total_len = req->content_len;
    if (total_len > 1024) total_len = 1024;  // Limit to 1KB
    
    char* buf = (char*)malloc(total_len + 1);
    if (!buf) return "";
    
    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) break;
        received += ret;
    }
    buf[received] = '\0';
    
    String result = String(buf);
    free(buf);
    return result;
}

bool WebServerManager::getPostParameter(const String& body, const char* name, String& value) {
    int startIdx = body.indexOf(String(name) + "=");
    if (startIdx == -1) return false;

    startIdx += strlen(name) + 1;
    int endIdx = body.indexOf('&', startIdx);
    if (endIdx == -1) endIdx = body.length();

    value = body.substring(startIdx, endIdx);

    // URL decode
    value.replace("+", " ");
    value.replace("%20", " ");
    value.replace("%21", "!");
    value.replace("%22", "\"");
    value.replace("%23", "#");
    value.replace("%24", "$");
    value.replace("%25", "%");
    value.replace("%26", "&");
    value.replace("%27", "'");
    value.replace("%28", "(");
    value.replace("%29", ")");
    value.replace("%2A", "*");
    value.replace("%2B", "+");
    value.replace("%2C", ",");
    value.replace("%2D", "-");
    value.replace("%2E", ".");
    value.replace("%2F", "/");
    value.replace("%3A", ":");
    value.replace("%3B", ";");
    value.replace("%3C", "<");
    value.replace("%3D", "=");
    value.replace("%3E", ">");
    value.replace("%3F", "?");
    value.replace("%40", "@");

    return true;
}

String WebServerManager::getQueryParameter(httpd_req_t *req, const char* name) {
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len <= 1) return "";
    
    char* buf = (char*)malloc(buf_len);
    if (!buf) return "";
    
    if (httpd_req_get_url_query_str(req, buf, buf_len) != ESP_OK) {
        free(buf);
        return "";
    }
    
    char value[64];
    if (httpd_query_key_value(buf, name, value, sizeof(value)) != ESP_OK) {
        free(buf);
        return "";
    }
    
    free(buf);
    return String(value);
}

void WebServerManager::sendRedirectPage(httpd_req_t *req, const char* title, const char* message, const char* url, int delay) {
    char buffer[1024];
    snprintf_P(buffer, sizeof(buffer), HTML_REDIRECT, delay, url, title, message, delay);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, buffer, strlen(buffer));
}

bool WebServerManager::getDarkMode() {
    return WEB_DARK_MODE;
}

String WebServerManager::buildHTMLHeader() {
    String html;
    bool darkMode = getDarkMode();
    
    html += "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    
    if (darkMode) {
        html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#1a1a1a;color:#e0e0e0;}";
        html += ".container{max-width:900px;margin:0 auto;background:#2d2d2d;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.5);}";
        html += "h1{color:#e0e0e0;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}";
        html += "h2{color:#d0d0d0;}";
        html += ".card{background:#383838;padding:20px;margin:15px 0;border-radius:8px;border-left:4px solid #3498db;color:#e0e0e0;}";
        html += ".info-item{background:#383838;padding:15px;border-radius:5px;border:1px solid #555;}";
        html += ".info-label{font-size:12px;color:#aaa;text-transform:uppercase;margin-bottom:5px;}";
        html += ".info-value{font-size:24px;font-weight:bold;color:#e0e0e0;}";
        html += "form{background:#383838;padding:20px;border-radius:8px;margin:20px 0;}";
        html += "label{display:block;margin-bottom:8px;color:#e0e0e0;font-weight:600;}";
        html += "input[type=text],input[type=password],input[type=number],select{width:100%;padding:10px;border:2px solid #555;border-radius:5px;font-size:16px;box-sizing:border-box;margin-bottom:15px;background:#2d2d2d;color:#e0e0e0;}";
        html += "th{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:12px;text-align:left;font-weight:600;}";
        html += "td{border:1px solid #555;padding:10px;background:#383838;color:#e0e0e0;}";
        html += "tr:nth-child(even) td{background:#2d2d2d;}";
        html += ".value{font-weight:bold;color:#e0e0e0;}";
        html += ".warning{background:#3d3519;border:1px solid#ffc107;padding:15px;border-radius:5px;margin:20px 0;color:#ffca28;}";
        html += ".footer{text-align:center;margin-top:30px;color:#888;font-size:14px;}";
    } else {
        html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
        html += ".container{max-width:900px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
        html += "h1{color:#2c3e50;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}";
        html += "h2{color:#34495e;margin-top:30px;}";
        html += ".card{background:#ecf0f1;padding:20px;margin:15px 0;border-radius:8px;border-left:4px solid #3498db;}";
        html += ".info-item{background:#fff;padding:15px;border-radius:5px;border:1px solid #ddd;}";
        html += ".info-label{font-size:12px;color:#7f8c8d;text-transform:uppercase;margin-bottom:5px;}";
        html += ".info-value{font-size:24px;font-weight:bold;color:#2c3e50;}";
        html += "form{background:#ecf0f1;padding:20px;border-radius:8px;margin:20px 0;}";
        html += "label{display:block;margin-bottom:8px;color:#2c3e50;font-weight:600;}";
        html += "input[type=text],input[type=password],input[type=number],select{width:100%;padding:10px;border:2px solid #bdc3c7;border-radius:5px;font-size:16px;box-sizing:border-box;margin-bottom:15px;}";
        html += "th{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:12px;text-align:left;font-weight:600;}";
        html += "td{border:1px solid #e0e0e0;padding:10px;background:white;}";
        html += "tr:nth-child(even) td{background:#f8f9fa;}";
        html += ".value{font-weight:bold;color:#2c3e50;}";
        html += ".warning{background:#fff3cd;border:1px solid #ffc107;padding:15px;border-radius:5px;margin:20px 0;color:#856404;}";
        html += ".footer{text-align:center;margin-top:30px;color:#7f8c8d;font-size:14px;}";
    }
    
    // Common styles
    html += ".nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;flex-wrap:wrap;}";
    html += ".nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}";
    html += ".nav a:hover{background:#21618c;}";
    html += ".nav .reboot{margin-left:auto;background:#e74c3c;}";
    html += ".nav .reboot:hover{background:#c0392b;}";
    html += ".info{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;margin:20px 0;}";
    html += "input[type=submit],button{background:#27ae60;color:white;padding:12px 30px;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin-top:10px;}";
    html += "input[type=submit]:hover,button:hover{background:#229954;}";
    html += "table{border-collapse:collapse;width:100%;margin:15px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += ".spinner{border:4px solid #f3f3f3;border-top:4px solid #3498db;border-radius:50%;width:40px;height:40px;animation:spin 1s linear infinite;display:inline-block;vertical-align:middle;}";
    html += "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}";
    html += "</style></head><body><div class='container'>";
    
    // Navigation
    html += "<div class='nav'>";
    html += "<a href='/'>Home</a>";
    html += "<a href='/stats'>Statistics</a>";
    html += "<a href='/registers'>Registers</a>";
    html += "<a href='/lorawan'>LoRaWAN</a>";
    html += "<a href='/lorawan/profiles'>Profiles</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/ota'>Update</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";
    
    return html;
}

String WebServerManager::buildHTMLFooter() {
    return String(FPSTR(HTML_FOOTER));
}

// ============================================================================
// PAGE HANDLERS
// ============================================================================

esp_err_t WebServerManager::handleRoot(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String html = buildHTMLHeader();
    
    html += "<h1>Vision Master E290</h1>";
    html += "<div class='card'><strong>Status:</strong> System Running | <strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</div>";

    html += "<div class='info'>";
    html += "<div class='info-item'><div class='info-label'>Modbus Slave ID</div><div class='info-value'>" + String(modbusHandler.getSlaveId()) + "</div></div>";
    
    ModbusStats& modbus_stats = modbusHandler.getStats();
    html += "<div class='info-item'><div class='info-label'>Modbus RTU Requests</div><div class='info-value'>" + String(modbus_stats.request_count) + "</div></div>";
    html += "<div class='info-item'><div class='info-label'>Modbus TCP</div><div class='info-value'>Configured</div></div>";

    // WiFi status
    if (wifiManager.isClientConnected()) {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value' style='font-size:16px;'>Client<br><small style='font-size:12px;color:#7f8c8d;'>" + wifiManager.getClientSSID() + "<br>" + wifiManager.getClientIP().toString() + "</small></div></div>";
    } else if (wifiManager.isAPActive()) {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value' style='font-size:16px;'>AP Mode<br><small style='font-size:12px;color:#7f8c8d;'>" + String(wifiManager.getAPClients()) + " clients</small></div></div>";
    } else {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value'>OFF</div></div>";
    }

    html += "<div class='info-item'><div class='info-label'>LoRaWAN Status</div><div class='info-value'>" + String(lorawanHandler.isJoined() ? "JOINED" : "NOT JOINED") + "</div></div>";
    html += "<div class='info-item'><div class='info-label'>LoRa Uplinks</div><div class='info-value'>" + String(lorawanHandler.getUplinkCount()) + "</div></div>";
    html += "</div>";

    // Configuration Form
    html += "<h2>Configuration</h2>";
    html += "<form action='/config' method='POST'>";
    html += "<label>Modbus Slave ID:</label>";
    html += "<input type='number' name='slave_id' min='1' max='247' value='" + String(modbusHandler.getSlaveId()) + "' required>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 15px 0;'>Valid range: 1-247</p>";
    
    Preferences prefs;
    bool tcp_enabled = false;
    if (prefs.begin("modbus", false)) {
        tcp_enabled = prefs.getBool("tcp_enabled", false);
        prefs.end();
    }
    
    html += "<div style='text-align:left;margin:20px 0;'>";
    html += "<label style='display:flex;align-items:center;cursor:pointer;'>";
    html += "<input type='checkbox' name='tcp_enabled' value='1' " + String(tcp_enabled ? "checked" : "") + " style='width:20px;height:20px;margin-right:10px;'>";
    html += "<span>Enable Modbus TCP (port 502)</span>";
    html += "</label></div>";

    html += "<input type='submit' value='Save Configuration'>";
    html += "</form>";

    html += buildHTMLFooter();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleStats(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String html = buildHTMLHeader();
    
    // Auto-refresh every 30 seconds
    html += "<script>\n";
    html += "setTimeout(function(){ window.location.reload(); }, 30000);\n";
    html += "function saveAutoInstall() {\n";
    html += "  var enabled = document.getElementById('autoInstall').checked ? '1' : '0';\n";
    html += "  fetch('/ota/auto-install?enabled=' + enabled).then(function(r){ return r.json(); }).then(function(data){\n";
    html += "    console.log('Auto-install saved:', data);\n";
    html += "  });\n";
    html += "}\n";
    html += "</script>\n";
    
    html += "<h1>System Statistics</h1>";
    
    ModbusStats& stats = modbusHandler.getStats();
    html += "<h2>Modbus Communication</h2>";
    html += "<table><tr><th>Metric</th><th>Value</th><th>Description</th></tr>";
    html += "<tr><td>Total Requests</td><td class='value'>" + String(stats.request_count) + "</td><td>Total Modbus RTU requests received</td></tr>";
    html += "<tr><td>Read Operations</td><td class='value'>" + String(stats.read_count) + "</td><td>Number of read operations</td></tr>";
    html += "<tr><td>Write Operations</td><td class='value'>" + String(stats.write_count) + "</td><td>Number of write operations</td></tr>";
    html += "<tr><td>Error Count</td><td class='value'>" + String(stats.error_count) + "</td><td>Communication errors</td></tr>";
    html += "</table>";

    html += "<h2>System Information</h2>";
    html += "<table><tr><th>Metric</th><th>Value</th><th>Description</th></tr>";
    html += "<tr><td>Uptime</td><td class='value'>" + String(millis() / 1000) + " seconds</td><td>System uptime since last boot</td></tr>";
    html += "<tr><td>Free Heap</td><td class='value'>" + String(ESP.getFreeHeap() / 1024) + " KB</td><td>Available RAM memory</td></tr>";
    html += "<tr><td>Min Free Heap</td><td class='value'>" + String(ESP.getMinFreeHeap() / 1024) + " KB</td><td>Minimum free heap since boot</td></tr>";
    html += "<tr><td>Temperature</td><td class='value'>" + String(temperatureRead(), 1) + " C</td><td>Internal CPU temperature</td></tr>";
    html += "<tr><td>WiFi Clients</td><td class='value'>" + String(wifiManager.getAPClients()) + "</td><td>Connected WiFi clients</td></tr>";
    html += "</table>";

    // Firmware Updates section
    html += "<h2>Firmware Updates</h2>";
    html += "<table><tr><th>Metric</th><th>Value</th><th>Description</th></tr>";
    
    OTAResult otaStatus = otaManager.getStatus();
    uint8_t checkInterval = otaManager.getUpdateCheckInterval();
    
    html += "<tr><td>Current Version</td><td class='value'>" + otaStatus.currentVersion + "</td><td>Currently installed firmware version</td></tr>";
    html += "<tr><td>Check Interval</td><td class='value'>" + String(checkInterval) + " minutes</td><td>How often to check for updates when WiFi connected</td></tr>";
    
    if (wifiManager.isClientConnected()) {
        // Calculate approximate time until next check
        unsigned long uptimeSeconds = millis() / 1000;
        unsigned long intervalSeconds = checkInterval * 60UL;
        unsigned long timeSinceLastPossibleCheck = uptimeSeconds % intervalSeconds;
        unsigned long nextCheckIn = intervalSeconds - timeSinceLastPossibleCheck;
        
        String timeUntilNext;
        if (nextCheckIn >= 3600) {
            timeUntilNext = String(nextCheckIn / 3600) + "h " + String((nextCheckIn % 3600) / 60) + "m";
        } else if (nextCheckIn >= 60) {
            timeUntilNext = String(nextCheckIn / 60) + " minutes";
        } else {
            timeUntilNext = String(nextCheckIn) + " seconds";
        }
        
        html += "<tr><td>Next Check In</td><td class='value'>" + timeUntilNext + "</td><td>Estimated time until next automatic check</td></tr>";
        html += "<tr><td>Update Status</td><td class='value'>";
        
        if (otaStatus.updateAvailable) {
            html += "<span style='color: #e74c3c; font-weight: bold;'>Update Available</span>";
        } else {
            switch (otaStatus.status) {
                case OTA_IDLE: html += "Up to date"; break;
                case OTA_CHECKING: html += "Checking..."; break;
                case OTA_DOWNLOADING: html += "Downloading..."; break;
                case OTA_INSTALLING: html += "Installing..."; break;
                case OTA_SUCCESS: html += "Update Successful"; break;
                case OTA_FAILED: html += "Check Failed"; break;
                default: html += "Unknown";
            }
        }
        html += "</td><td>Current update status</td></tr>";
        
        if (otaStatus.updateAvailable && !otaStatus.latestVersion.isEmpty()) {
            html += "<tr><td>Latest Version</td><td class='value' style='color: #e74c3c; font-weight: bold;'>" + otaStatus.latestVersion + "</td><td>Available firmware update</td></tr>";
        }
        
        if (!otaStatus.message.isEmpty()) {
            String messageStyle = "";
            if (otaStatus.status == OTA_FAILED) {
                messageStyle = "style='color: #e74c3c;'";
            } else if (otaStatus.updateAvailable) {
                messageStyle = "style='color: #f39c12;'";
            }
            html += "<tr><td>Last Check Result</td><td class='value' " + messageStyle + ">" + otaStatus.message + "</td><td>Result of most recent update check</td></tr>";
        }
    } else {
        html += "<tr><td>WiFi Status</td><td class='value' style='color: #f39c12;'>Disconnected</td><td>Connect to WiFi to enable automatic update checks</td></tr>";
    }
    
    html += "</table>";
    
    // Auto-install checkbox
    html += "<div style='margin-top: 15px; padding: 10px; background: #f8f9fa; border-radius: 5px;'>";
    html += "<form id='autoInstallForm' style='margin: 0;'>";
    html += "<label style='cursor: pointer; display: flex; align-items: center; gap: 8px;'>";
    html += "<input type='checkbox' id='autoInstall' name='autoInstall' ";
    if (otaManager.getAutoInstall()) {
        html += "checked ";
    }
    html += "onchange='saveAutoInstall()' style='width: 18px; height: 18px; cursor: pointer;'>";
    html += "<span><strong>Auto-install updates</strong> - Automatically install new firmware when detected</span>";
    html += "</label></form></div>";

    html += buildHTMLFooter();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleRegisters(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String html = buildHTMLHeader();
    
    html += R"(<script>
    function submitSF6Values() {
      var d = parseFloat(document.getElementById('density-input').value);
      var p = parseFloat(document.getElementById('pressure-input').value);
      var t = parseFloat(document.getElementById('temperature-input').value);
      fetch('/sf6/update?density=' + Math.round(d * 100) + '&pressure=' + Math.round(p * 10) + '&temperature=' + Math.round(t * 10));
      alert('Values updated!');
      return false;
    }
    function resetSF6Values() {
      if (!confirm('Reset SF6 values?')) return;
      fetch('/sf6/reset');
      alert('Values reset!');
      setTimeout(function() { location.reload(); }, 1000);
    }
    </script>)";

    html += "<h1>Modbus Registers</h1>";
    
    // SF6 Control Panel
    html += "<div class='card'>";
    html += "<h3>SF6 Manual Control</h3>";
    html += "<form onsubmit='return submitSF6Values();'>";
    html += "<label>Density (kg/m&sup3;):</label><input type='number' id='density-input' step='0.01' value='" + String(sf6Emulator.getDensity(), 2) + "'>";
    html += "<label>Pressure (kPa):</label><input type='number' id='pressure-input' step='0.1' value='" + String(sf6Emulator.getPressure(), 1) + "'>";
    html += "<label>Temperature (K):</label><input type='number' id='temperature-input' step='0.1' value='" + String(sf6Emulator.getTemperature(), 1) + "'>";
    html += "<button type='submit'>Update</button> <button type='button' onclick='resetSF6Values()'>Reset</button>";
    html += "</form></div>";

    // Holding Registers
    HoldingRegisters& holding = modbusHandler.getHoldingRegisters();
    html += "<h2>Holding Registers (0-12) - Read/Write</h2>";
    html += "<table><tr><th>Address</th><th>Value</th><th>Hex</th><th>Description</th></tr>";
    html += "<tr><td>0</td><td class='value'>" + String(holding.sequential_counter) + "</td><td>0x" + String(holding.sequential_counter, HEX) + "</td><td>Sequential Counter</td></tr>";
    html += "<tr><td>1</td><td class='value'>" + String(holding.random_number) + "</td><td>0x" + String(holding.random_number, HEX) + "</td><td>Random Number</td></tr>";
    
    uint16_t uptime_low = (uint16_t)(holding.uptime_seconds & 0xFFFF);
    uint16_t uptime_high = (uint16_t)(holding.uptime_seconds >> 16);
    html += "<tr><td>2</td><td class='value'>" + String(uptime_low) + "</td><td>0x" + String(uptime_low, HEX) + "</td><td>Uptime (low word)</td></tr>";
    html += "<tr><td>3</td><td class='value'>" + String(uptime_high) + "</td><td>0x" + String(uptime_high, HEX) + "</td><td>Uptime (high word) = <strong>" + String(holding.uptime_seconds) + " seconds</strong></td></tr>";

    uint32_t total_heap = ((uint32_t)holding.free_heap_kb_high << 16) | holding.free_heap_kb_low;
    html += "<tr><td>4</td><td class='value'>" + String(holding.free_heap_kb_low) + "</td><td>0x" + String(holding.free_heap_kb_low, HEX) + "</td><td>Free Heap (low word)</td></tr>";
    html += "<tr><td>5</td><td class='value'>" + String(holding.free_heap_kb_high) + "</td><td>0x" + String(holding.free_heap_kb_high, HEX) + "</td><td>Free Heap (high word) = <strong>" + String(total_heap) + " KB total</strong></td></tr>";
    html += "<tr><td>6</td><td class='value'>" + String(holding.min_heap_kb) + "</td><td>0x" + String(holding.min_heap_kb, HEX) + "</td><td>Min Free Heap (KB)</td></tr>";
    html += "<tr><td>7</td><td class='value'>" + String(holding.cpu_freq_mhz) + "</td><td>0x" + String(holding.cpu_freq_mhz, HEX) + "</td><td>CPU Frequency (MHz)</td></tr>";
    html += "<tr><td>8</td><td class='value'>" + String(holding.task_count) + "</td><td>0x" + String(holding.task_count, HEX) + "</td><td>FreeRTOS Tasks</td></tr>";
    html += "<tr><td>9</td><td class='value'>" + String(holding.temperature_x10) + "</td><td>0x" + String(holding.temperature_x10, HEX) + "</td><td>Temperature = <strong>" + String(holding.temperature_x10 / 10.0, 1) + " C</strong></td></tr>";
    html += "<tr><td>10</td><td class='value'>" + String(holding.cpu_cores) + "</td><td>0x" + String(holding.cpu_cores, HEX) + "</td><td>CPU Cores</td></tr>";
    html += "<tr><td>11</td><td class='value'>" + String(holding.wifi_enabled) + "</td><td>0x" + String(holding.wifi_enabled, HEX) + "</td><td>WiFi AP Enabled</td></tr>";
    html += "<tr><td>12</td><td class='value'>" + String(holding.wifi_clients) + "</td><td>0x" + String(holding.wifi_clients, HEX) + "</td><td>WiFi Clients</td></tr>";
    html += "</table>";

    // Input Registers
    InputRegisters& input = modbusHandler.getInputRegisters();
    html += "<h2>Input Registers (0-8) - Read Only (SF6 Sensor)</h2>";
    html += "<table><tr><th>Address</th><th>Raw Value</th><th>Scaled Value</th><th>Description</th></tr>";
    html += "<tr><td>0</td><td class='value'>" + String(input.sf6_density) + "</td><td>" + String(input.sf6_density / 100.0, 2) + " kg/m&sup3;</td><td>SF6 Density</td></tr>";
    html += "<tr><td>1</td><td class='value'>" + String(input.sf6_pressure_20c) + "</td><td>" + String(input.sf6_pressure_20c / 10.0, 1) + " kPa</td><td>SF6 Pressure @20C</td></tr>";
    html += "<tr><td>2</td><td class='value'>" + String(input.sf6_temperature) + "</td><td>" + String(input.sf6_temperature / 10.0, 1) + " K (" + String(input.sf6_temperature / 10.0 - 273.15, 1) + "C)</td><td>SF6 Temperature</td></tr>";
    html += "<tr><td>3</td><td class='value'>" + String(input.sf6_pressure_var) + "</td><td>" + String(input.sf6_pressure_var / 10.0, 1) + " kPa</td><td>SF6 Pressure Variance</td></tr>";
    html += "<tr><td>4</td><td class='value'>" + String(input.slave_id) + "</td><td>-</td><td>Slave ID</td></tr>";

    uint32_t serial = ((uint32_t)input.serial_hi << 16) | input.serial_lo;
    html += "<tr><td>5</td><td class='value'>" + String(input.serial_hi) + "</td><td>0x" + String(input.serial_hi, HEX) + "</td><td>Serial Number (high)</td></tr>";
    html += "<tr><td>6</td><td class='value'>" + String(input.serial_lo) + "</td><td>0x" + String(input.serial_lo, HEX) + "</td><td>Serial Number (low) = <strong>0x" + String(serial, HEX) + "</strong></td></tr>";
    char version_buf[10];
    snprintf(version_buf, sizeof(version_buf), "v%d.%02d", input.sw_release / 100, input.sw_release % 100);
    html += "<tr><td>7</td><td class='value'>" + String(input.sw_release) + "</td><td>" + String(version_buf) + "</td><td>Software Version</td></tr>";
    html += "<tr><td>8</td><td class='value'>" + String(input.quartz_freq) + "</td><td>" + String(input.quartz_freq / 100.0, 2) + " Hz</td><td>Quartz Frequency</td></tr>";
    html += "</table>";

    html += buildHTMLFooter();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleLoRaWAN(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String html = buildHTMLHeader();

    html += "<h1>LoRaWAN Configuration</h1>";

    // Network status
    html += "<h2>Network Status</h2>";
    html += "<table><tr><th>Parameter</th><th>Value</th></tr>";
    html += "<tr><td>Connection Status</td><td style='background:" + String(lorawanHandler.isJoined() ? "#1e5631" : "#5c2626") + ";color:#fff;font-weight:bold;'>" + String(lorawanHandler.isJoined() ? "JOINED" : "NOT JOINED") + "</td></tr>";
    if (lorawanHandler.isJoined()) {
        html += "<tr><td>DevAddr</td><td>0x" + String(lorawanHandler.getDevAddr(), HEX) + "</td></tr>";
    }
    html += "<tr><td>Total Uplinks</td><td>" + String(lorawanHandler.getUplinkCount()) + "</td></tr>";
    html += "<tr><td>Total Downlinks</td><td>" + String(lorawanHandler.getDownlinkCount()) + "</td></tr>";
    html += "<tr><td>Last RSSI</td><td>" + String(lorawanHandler.getLastRSSI()) + " dBm</td></tr>";
    html += "</table>";

    // Active profile
    uint8_t active_idx = lorawanHandler.getActiveProfileIndex();
    LoRaProfile* active_prof = lorawanHandler.getProfile(active_idx);
    if (active_prof) {
        html += "<h2>Active Profile</h2>";
        html += "<table><tr><th>Parameter</th><th>Value</th></tr>";
        html += "<tr><td>Profile</td><td>" + String(active_idx) + " - " + String(active_prof->name) + "</td></tr>";
        html += "</table>";
        html += "<p><a href='/lorawan/profiles' style='background:#3498db;color:white;padding:10px;text-decoration:none;border-radius:5px;'>Manage Profiles &raquo;</a></p>";
    }

    // Current credentials
    html += "<h2>Current Credentials</h2>";
    html += "<table><tr><th>Parameter</th><th>Value</th></tr>";
    
    char devEUIStr[17], joinEUIStr[17];
    sprintf(devEUIStr, "%016llX", lorawanHandler.getDevEUI());
    sprintf(joinEUIStr, "%016llX", lorawanHandler.getJoinEUI());
    html += "<tr><td>DevEUI</td><td>0x" + String(devEUIStr) + "</td></tr>";
    html += "<tr><td>JoinEUI</td><td>0x" + String(joinEUIStr) + "</td></tr>";
    
    uint8_t appKey[16], nwkKey[16];
    lorawanHandler.getAppKey(appKey);
    lorawanHandler.getNwkKey(nwkKey);
    
    html += "<tr><td>AppKey</td><td>";
    for (int i = 0; i < 16; i++) { char buf[3]; sprintf(buf, "%02X", appKey[i]); html += buf; }
    html += "</td></tr>";
    html += "<tr><td>NwkKey</td><td>";
    for (int i = 0; i < 16; i++) { char buf[3]; sprintf(buf, "%02X", nwkKey[i]); html += buf; }
    html += "</td></tr></table>";

    // DevNonce reset
    html += "<h2>DevNonce Reset</h2>";
    html += "<div class='card'>";
    html += "<p>If join failures occur, reset the DevNonce counter:</p>";
    html += "<button onclick='if(confirm(\"Reset DevNonce?\")) window.location.href=\"/lorawan/reset-nonces\"' style='background:#ffc107;color:#000;'>Reset DevNonce</button>";
    html += "</div>";

    html += buildHTMLFooter();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleLoRaWANProfiles(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String html = buildHTMLHeader();
    
    html += R"(<script>
    function toggleProfile(idx){ fetch('/lorawan/profile/toggle?index='+idx).then(()=>location.reload()); }
    function activateProfile(idx){ if(confirm('Switch profile? Device will restart.')) fetch('/lorawan/profile/activate?index='+idx).then(()=>{ alert('Restarting...'); setTimeout(()=>location.href='/',10000); }); }
    function toggleAutoRotate(e){ fetch('/lorawan/auto-rotate?enabled='+(e?'1':'0')).then(()=>location.reload()); }
    </script>)";

    html += "<h1>LoRaWAN Profiles</h1>";

    // Auto-rotation
    bool auto_rotate = lorawanHandler.getAutoRotation();
    int enabled_count = lorawanHandler.getEnabledProfileCount();
    html += "<div class='card' style='background:#1e5631;border:2px solid #27ae60;color:#fff;'>";
    html += "<h3>Auto-Rotation</h3>";
    html += "<p>Status: <strong>" + String(auto_rotate ? "ENABLED" : "DISABLED") + "</strong> | Enabled profiles: " + String(enabled_count) + "</p>";
    html += "<label><input type='checkbox' " + String(auto_rotate ? "checked" : "") + " " + String(enabled_count < 2 ? "disabled" : "") + " onchange='toggleAutoRotate(this.checked)'> Enable Auto-Rotation</label>";
    html += "</div>";

    // Profile table
    html += "<h2>Profile Overview</h2>";
    html += "<table><tr><th>Profile</th><th>Name</th><th>DevEUI</th><th>Status</th><th>Actions</th></tr>";
    
    uint8_t active_idx = lorawanHandler.getActiveProfileIndex();
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        LoRaProfile* prof = lorawanHandler.getProfile(i);
        if (!prof) continue;
        
        char devEUIStr[17];
        sprintf(devEUIStr, "%016llX", prof->devEUI);
        
        html += "<tr><td><strong>" + String(i) + "</strong>";
        if (i == active_idx) html += " <span style='background:#27ae60;color:white;padding:2px 6px;border-radius:3px;font-size:11px;'>ACTIVE</span>";
        html += "</td>";
        html += "<td>" + String(prof->name) + "</td>";
        html += "<td style='font-family:monospace;font-size:11px;'>0x" + String(devEUIStr) + "</td>";
        html += "<td style='color:" + String(prof->enabled ? "#27ae60" : "#95a5a6") + ";font-weight:bold;'>" + String(prof->enabled ? "ENABLED" : "DISABLED") + "</td>";
        html += "<td>";
        if (i != active_idx || !prof->enabled) {
            html += "<button onclick='toggleProfile(" + String(i) + ")' style='padding:5px 10px;margin:2px;'>" + String(prof->enabled ? "Disable" : "Enable") + "</button>";
        }
        if (prof->enabled && i != active_idx) {
            html += "<button onclick='activateProfile(" + String(i) + ")' style='padding:5px 10px;margin:2px;background:#3498db;'>Activate</button>";
        }
        html += "</td></tr>";
    }
    html += "</table>";

    // Edit forms for each profile
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        LoRaProfile* prof = lorawanHandler.getProfile(i);
        if (!prof) continue;
        
        html += "<div class='card' id='profile" + String(i) + "'>";
        html += "<h3>Edit Profile " + String(i) + ": " + String(prof->name) + "</h3>";
        html += "<form method='POST' action='/lorawan/profile/update'>";
        html += "<input type='hidden' name='index' value='" + String(i) + "'>";
        
        html += "<label>Name:</label><input type='text' name='name' value='" + String(prof->name) + "' maxlength='32'>";
        
        html += "<label>Payload Format:</label><select name='payload_type'>";
        for (int pt = 0; pt <= PAYLOAD_VISTRON_LORA_MOD_CON; pt++) {
            html += "<option value='" + String(pt) + "'" + String(pt == prof->payload_type ? " selected" : "") + ">" + String(PAYLOAD_TYPE_NAMES[pt]) + "</option>";
        }
        html += "</select>";
        
        char joinEUIStr[17], devEUIStr[17];
        sprintf(joinEUIStr, "%016llX", prof->joinEUI);
        sprintf(devEUIStr, "%016llX", prof->devEUI);
        
        html += "<label>JoinEUI:</label><input type='text' name='joinEUI' value='" + String(joinEUIStr) + "' pattern='[0-9A-Fa-f]{16}'>";
        html += "<label>DevEUI:</label><input type='text' name='devEUI' value='" + String(devEUIStr) + "' pattern='[0-9A-Fa-f]{16}'>";
        
        html += "<label>AppKey:</label><input type='text' name='appKey' value='";
        for (int j = 0; j < 16; j++) { char buf[3]; sprintf(buf, "%02X", prof->appKey[j]); html += buf; }
        html += "' pattern='[0-9A-Fa-f]{32}'>";
        
        html += "<label>NwkKey:</label><input type='text' name='nwkKey' value='";
        for (int j = 0; j < 16; j++) { char buf[3]; sprintf(buf, "%02X", prof->nwkKey[j]); html += buf; }
        html += "' pattern='[0-9A-Fa-f]{32}'>";
        
        html += "<button type='submit'>Save Profile</button>";
        html += "</form></div>";
    }
    
    html += buildHTMLFooter();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleWiFi(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String html = buildHTMLHeader();
    html += String(FPSTR(WIFI_PAGE_ASSETS));

    html += "<h1>WiFi Configuration</h1>";
    html += "<div id='statusArea'><div class='status'><div class='spinner'></div> Loading...</div></div>";

    html += "<h2>Connect to WiFi Network</h2>";
    html += "<div class='card'>";
    html += "<button id='scanBtn' class='secondary' onclick='scanNetworks()'>Scan for Networks</button>";
    html += "<div id='scanResults'></div>";
    html += "</div>";

    html += "<form action='/wifi/connect' method='POST'>";
    html += "<label>WiFi SSID:</label><input type='text' id='ssid' name='ssid' placeholder='Network name' required>";
    html += "<label>WiFi Password:</label><input type='password' name='password' placeholder='Password'>";
    html += "<input type='submit' value='Connect'>";
    html += "</form>";

    html += buildHTMLFooter();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleSecurity(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String html = buildHTMLHeader();

    html += "<h1>Security Settings</h1>";

    html += "<div class='warning'><strong>Warning:</strong> Changing these settings affects web interface access.</div>";

    html += "<div class='card'>";
    html += "<p><strong>Status:</strong></p>";
    html += "<p>Authentication: <strong>" + String(authManager.isEnabled() ? "ENABLED" : "DISABLED") + "</strong></p>";
    html += "<p>Username: <strong>" + authManager.getUsername() + "</strong></p>";
    html += "</div>";

    html += "<h2>Update Authentication</h2>";
    html += "<form action='/security/update' method='POST'>";
    html += "<label><input type='checkbox' name='auth_enabled' value='1'" + String(authManager.isEnabled() ? " checked" : "") + "> Enable Authentication</label>";
    html += "<label>Username:</label><input type='text' name='username' value='" + authManager.getUsername() + "' maxlength='32'>";
    html += "<label>New Password:</label><input type='password' name='password' placeholder='Leave empty to keep current' maxlength='32'>";
    html += "<input type='submit' value='Save'>";
    html += "</form>";

    html += "<h2>Debug Settings</h2>";
    html += "<form action='/security/debug' method='POST'>";
    html += "<label><input type='checkbox' name='debug_https' value='1'" + String(authManager.getDebugHTTPS() ? " checked" : "") + "> HTTPS Debug</label>";
    html += "<label><input type='checkbox' name='debug_auth' value='1'" + String(authManager.getDebugAuth() ? " checked" : "") + "> Auth Debug</label>";
    html += "<input type='submit' value='Save Debug Settings'>";
    html += "</form>";

    html += "<h2 style='color:#e74c3c;'>Factory Reset</h2>";
    html += "<div class='card'>";
    html += "<button onclick='if(confirm(\"Erase ALL settings?\")) window.location.href=\"/factory-reset\"' style='background:#e74c3c;'>Factory Reset</button>";
    html += "</div>";

    html += buildHTMLFooter();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleOTA(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String html = buildHTMLHeader();
    
    html += "<script>\n";
    html += "function checkForUpdates(){\n";
    html += "  var btn = document.getElementById('checkBtn');\n";
    html += "  var status = document.getElementById('updateStatus');\n";
    html += "  btn.disabled=true;\n";
    html += "  btn.innerHTML='Checking...';\n";
    html += "  fetch('/ota/check').then(function(r){return r.json();}).then(function(data){\n";
    html += "    btn.disabled=false;\n";
    html += "    btn.innerHTML='Check for Updates';\n";
    html += "    if(data.error) status.innerHTML='<div class=\"warning\">'+data.error+'</div>';\n";
    html += "    else if(data.updateAvailable) status.innerHTML='<div class=\"card\" style=\"background:#1e5631;border-color:#27ae60;color:#fff;\"><strong>Update available!</strong><br>Current: '+data.currentVersion+'<br>Latest: '+data.latestVersion+'<br><br><button onclick=\"startUpdate()\">Install Update</button></div>';\n";
    html += "    else status.innerHTML='<div class=\"card\">Up to date: '+data.currentVersion+'</div>';\n";
    html += "  }).catch(function(e){ btn.disabled=false; btn.innerHTML='Check for Updates'; });\n";
    html += "}\n";
    html += "function startUpdate(){\n";
    html += "  if(!confirm('Start firmware update?'))return;\n";
    html += "  var status = document.getElementById('updateStatus');\n";
    html += "  status.innerHTML='<div class=\"card\">Updating...<div class=\"spinner\"></div></div>';\n";
    html += "  fetch('/ota/start').then(function(r){return r.json();}).then(function(data){\n";
    html += "    if(data.started) setInterval(checkProgress,1000);\n";
    html += "    else status.innerHTML='<div class=\"warning\">'+(data.error||'Failed')+'</div>';\n";
    html += "  });\n";
    html += "}\n";
    html += "function checkProgress(){\n";
    html += "  fetch('/ota/status').then(function(r){return r.json();}).then(function(data){\n";
    html += "    var status = document.getElementById('updateStatus');\n";
    html += "    if(data.status==='success'){ status.innerHTML='<div class=\"card\" style=\"background:#1e5631;color:#fff;\">Update complete! Rebooting...</div>'; setTimeout(function(){location.reload();},15000); }\n";
    html += "    else if(data.status==='failed') status.innerHTML='<div class=\"warning\">Failed: '+data.message+'</div>';\n";
    html += "    else status.innerHTML='<div class=\"card\">Progress: '+data.progress+'%<br>'+data.message+'</div>';\n";
    html += "  });\n";
    html += "}\n";
    html += "</script>\n";

    html += "<h1>Firmware Update (OTA)</h1>";

    html += "<div class='card'>";
    html += "<h3>Current Firmware</h3>";
    html += "<p><strong>Version:</strong> " + otaManager.getCurrentVersion() + "</p>";
    html += "</div>";

    html += "<div class='card'>";
    html += "<h3>Check for Updates</h3>";
    if (!otaManager.hasToken()) {
        html += "<p style='color:#e74c3c;'>GitHub token not configured.</p>";
    } else {
        html += "<button id='checkBtn' onclick='checkForUpdates()'>Check for Updates</button>";
    }
    html += "<div id='updateStatus'></div>";
    html += "</div>";

    html += buildHTMLFooter();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

// ============================================================================
// ACTION HANDLERS
// ============================================================================

esp_err_t WebServerManager::handleConfig(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String body = getPostBody(req);
    String slave_id_str, tcp_enabled_str;
    
    if (getPostParameter(body, "slave_id", slave_id_str)) {
        int new_id = slave_id_str.toInt();
        bool tcp_enabled = getPostParameter(body, "tcp_enabled", tcp_enabled_str);
        
        if (new_id >= 1 && new_id <= 247) {
            modbusHandler.setSlaveId(new_id);
            
            Preferences prefs;
            prefs.begin("modbus", false);
            prefs.putUChar("slave_id", new_id);
            prefs.putBool("tcp_enabled", tcp_enabled);
            prefs.end();
            
            sendRedirectPage(req, "Configuration Saved", "Settings updated successfully.", "/");
        } else {
            sendRedirectPage(req, "Error", "Invalid Slave ID", "/");
        }
    } else {
        sendRedirectPage(req, "Error", "Missing parameters", "/");
    }
    return ESP_OK;
}

esp_err_t WebServerManager::handleLoRaWANConfig(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String body = getPostBody(req);
    String joinEUIStr, devEUIStr, appKeyStr, nwkKeyStr;
    
    if (getPostParameter(body, "joinEUI", joinEUIStr) &&
        getPostParameter(body, "devEUI", devEUIStr) &&
        getPostParameter(body, "appKey", appKeyStr) &&
        getPostParameter(body, "nwkKey", nwkKeyStr)) {
        
        uint8_t active_idx = lorawanHandler.getActiveProfileIndex();
        LoRaProfile* prof = lorawanHandler.getProfile(active_idx);
        
        if (prof) {
            prof->joinEUI = strtoull(joinEUIStr.c_str(), NULL, 16);
            prof->devEUI = strtoull(devEUIStr.c_str(), NULL, 16);
            
            for (int i = 0; i < 16; i++) {
                char buf[3] = {appKeyStr[i*2], appKeyStr[i*2+1], 0};
                prof->appKey[i] = strtol(buf, NULL, 16);
                char buf2[3] = {nwkKeyStr[i*2], nwkKeyStr[i*2+1], 0};
                prof->nwkKey[i] = strtol(buf2, NULL, 16);
            }
            
            lorawanHandler.updateProfile(active_idx, *prof);
            
            sendRedirectPage(req, "Credentials Updated", "Device restarting...", "/", 10);
            delay(1000);
            ESP.restart();
            return ESP_OK;
        }
    }
    
    sendRedirectPage(req, "Error", "Invalid credentials", "/lorawan");
    return ESP_OK;
}

esp_err_t WebServerManager::handleLoRaWANProfileUpdate(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    String body = getPostBody(req);
    String indexStr, name, joinEUIStr, devEUIStr, appKeyStr, nwkKeyStr, payloadTypeStr;
    
    if (getPostParameter(body, "index", indexStr)) {
        int index = indexStr.toInt();
        
        LoRaProfile profile;
        memset(&profile, 0, sizeof(LoRaProfile));
        
        LoRaProfile* existing = lorawanHandler.getProfile(index);
        if (existing) {
            profile.enabled = existing->enabled;
        }

        if (getPostParameter(body, "name", name)) {
            strncpy(profile.name, name.c_str(), 32);
        }

        if (getPostParameter(body, "payload_type", payloadTypeStr)) {
            int pt = payloadTypeStr.toInt();
            profile.payload_type = (pt >= 0 && pt <= PAYLOAD_VISTRON_LORA_MOD_CON) ? (PayloadType)pt : PAYLOAD_ADEUNIS_MODBUS_SF6;
        }

        getPostParameter(body, "joinEUI", joinEUIStr);
        getPostParameter(body, "devEUI", devEUIStr);
        getPostParameter(body, "appKey", appKeyStr);
        getPostParameter(body, "nwkKey", nwkKeyStr);

        if (joinEUIStr.length() == 16 && devEUIStr.length() == 16 &&
            appKeyStr.length() == 32 && nwkKeyStr.length() == 32) {
            
            profile.joinEUI = strtoull(joinEUIStr.c_str(), NULL, 16);
            profile.devEUI = strtoull(devEUIStr.c_str(), NULL, 16);
            
            for (int i = 0; i < 16; i++) {
                char buf[3] = {appKeyStr[i*2], appKeyStr[i*2+1], 0};
                profile.appKey[i] = strtol(buf, NULL, 16);
                char buf2[3] = {nwkKeyStr[i*2], nwkKeyStr[i*2+1], 0};
                profile.nwkKey[i] = strtol(buf2, NULL, 16);
            }
            
            lorawanHandler.updateProfile(index, profile);
            sendRedirectPage(req, "Profile Updated", "Profile saved.", "/lorawan/profiles");
        } else {
            sendRedirectPage(req, "Error", "Invalid credentials format", "/lorawan/profiles");
        }
    } else {
        sendRedirectPage(req, "Error", "Missing index", "/lorawan/profiles");
    }
    return ESP_OK;
}

esp_err_t WebServerManager::handleLoRaWANProfileToggle(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String indexStr = getQueryParameter(req, "index");
    if (indexStr.length() > 0) {
        lorawanHandler.toggleProfileEnabled(indexStr.toInt());
    }
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t WebServerManager::handleLoRaWANProfileActivate(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String indexStr = getQueryParameter(req, "index");
    if (indexStr.length() > 0) {
        if (lorawanHandler.setActiveProfile(indexStr.toInt())) {
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "OK");
            delay(1000);
            ESP.restart();
        }
    }
    
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Failed");
    return ESP_OK;
}

esp_err_t WebServerManager::handleLoRaWANAutoRotate(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String enabledStr = getQueryParameter(req, "enabled");
    lorawanHandler.setAutoRotation(enabledStr == "1");
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t WebServerManager::handleWiFiScan(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    int n = wifiManager.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + wifiManager.getScannedSSID(i) + "\",\"rssi\":" + String(wifiManager.getScannedRSSI(i)) + "}";
    }
    json += "]}";
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleWiFiConnect(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String body = getPostBody(req);
    String ssid, password;
    
    if (getPostParameter(body, "ssid", ssid)) {
        getPostParameter(body, "password", password);
        
        wifiManager.saveClientCredentials(ssid.c_str(), password.c_str());
        
        sendRedirectPage(req, "WiFi Saved", ("Connecting to " + ssid + "...").c_str(), "/", 15);
        delay(1000);
        ESP.restart();
    } else {
        sendRedirectPage(req, "Error", "Missing SSID", "/wifi");
    }
    return ESP_OK;
}

esp_err_t WebServerManager::handleWiFiStatus(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String json = "{";
    json += "\"client_connected\":" + String(wifiManager.isClientConnected() ? "true" : "false") + ",";
    json += "\"ap_active\":" + String(wifiManager.isAPActive() ? "true" : "false") + ",";
    if (wifiManager.isClientConnected()) {
        json += "\"client_ssid\":\"" + wifiManager.getClientSSID() + "\",";
        json += "\"client_ip\":\"" + wifiManager.getClientIP().toString() + "\",";
        json += "\"client_rssi\":" + String(wifiManager.getClientRSSI());
    } else {
        json += "\"client_ssid\":\"\",\"client_ip\":\"\",\"client_rssi\":0";
    }
    json += "}";
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleSecurityUpdate(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String body = getPostBody(req);
    String username, password, enabledStr;
    
    if (getPostParameter(body, "username", username)) {
        bool hasPassword = getPostParameter(body, "password", password);
        bool enabled = getPostParameter(body, "auth_enabled", enabledStr);
        
        if (hasPassword && password.length() > 0) {
            authManager.setCredentials(username.c_str(), password.c_str());
        } else {
            // Keep old password
            authManager.setCredentials(username.c_str(), authManager.getPassword().c_str());
        }
        
        authManager.setEnabled(enabled);
        authManager.save();
        
        sendRedirectPage(req, "Security Updated", "Settings saved.", "/security");
    } else {
        sendRedirectPage(req, "Error", "Missing username", "/security");
    }
    return ESP_OK;
}

esp_err_t WebServerManager::handleDebugUpdate(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String body = getPostBody(req);
    String httpsStr, authStr;
    
    bool https = getPostParameter(body, "debug_https", httpsStr);
    bool auth = getPostParameter(body, "debug_auth", authStr);
    
    authManager.setDebugHTTPS(https);
    authManager.setDebugAuth(auth);
    authManager.save();
    
    sendRedirectPage(req, "Debug Updated", "Settings saved.", "/security");
    return ESP_OK;
}

esp_err_t WebServerManager::handleSF6Update(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String densityStr = getQueryParameter(req, "density");
    String pressureStr = getQueryParameter(req, "pressure");
    String temperatureStr = getQueryParameter(req, "temperature");
    
    float d = sf6Emulator.getDensity();
    float p = sf6Emulator.getPressure();
    float t = sf6Emulator.getTemperature();
    
    if (densityStr.length() > 0) d = densityStr.toInt() / 100.0;
    if (pressureStr.length() > 0) p = pressureStr.toInt() / 10.0;
    if (temperatureStr.length() > 0) t = temperatureStr.toInt() / 10.0;
    
    sf6Emulator.setValues(d, p, t);
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t WebServerManager::handleSF6Reset(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    sf6Emulator.resetToDefaults();
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t WebServerManager::handleEnableAuth(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    authManager.enable();
    authManager.save();
    sendRedirectPage(req, "Auth Enabled", "Authentication enabled.", "/security");
    return ESP_OK;
}

esp_err_t WebServerManager::handleDarkMode(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebServerManager::handleResetNonces(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    Serial.println("\n>>> Web request: Reset LoRaWAN nonces");
    lorawanHandler.resetNonces();
    
    sendRedirectPage(req, "Reset Nonces", "DevNonce counters reset.", "/lorawan", 5);
    return ESP_OK;
}

esp_err_t WebServerManager::handleFactoryReset(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    Preferences prefs;
    prefs.begin("modbus", false); prefs.clear(); prefs.end();
    prefs.begin("auth", false); prefs.clear(); prefs.end();
    prefs.begin("wifi", false); prefs.clear(); prefs.end();
    prefs.begin("sf6", false); prefs.clear(); prefs.end();
    prefs.begin("lorawan", false); prefs.clear(); prefs.end();
    prefs.begin("lorawan_prof", false); prefs.clear(); prefs.end();
    
    sendRedirectPage(req, "Factory Reset", "Reset complete. Rebooting...", "/", 10);
    delay(1000);
    ESP.restart();
    return ESP_OK;
}

esp_err_t WebServerManager::handleReboot(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    sendRedirectPage(req, "Rebooting", "Device is restarting...", "/", 10);
    delay(1000);
    ESP.restart();
    return ESP_OK;
}

// ============================================================================
// OTA HANDLERS
// ============================================================================

esp_err_t WebServerManager::handleOTAConfig(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    String body = getPostBody(req);
    String token;
    
    httpd_resp_set_type(req, "application/json");
    
    if (getPostParameter(body, "token", token) && token.length() > 0) {
        otaManager.setGitHubToken(token.c_str());
        httpd_resp_sendstr(req, "{\"success\":true}");
    } else {
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"No token\"}");
    }
    return ESP_OK;
}

esp_err_t WebServerManager::handleOTACheck(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    httpd_resp_set_type(req, "application/json");
    
    if (!otaManager.hasToken()) {
        httpd_resp_sendstr(req, "{\"error\":\"GitHub token not configured\"}");
        return ESP_OK;
    }
    
    otaManager.checkForUpdate();
    OTAResult status = otaManager.getStatus();
    
    String json = "{";
    json += "\"updateAvailable\":" + String(status.updateAvailable ? "true" : "false") + ",";
    json += "\"currentVersion\":\"" + status.currentVersion + "\",";
    json += "\"latestVersion\":\"" + status.latestVersion + "\",";
    json += "\"message\":\"" + status.message + "\"";
    json += "}";
    
    httpd_resp_send(req, json.c_str(), json.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleOTAStart(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    httpd_resp_set_type(req, "application/json");
    
    if (!otaManager.hasToken()) {
        httpd_resp_sendstr(req, "{\"started\":false,\"error\":\"No token\"}");
        return ESP_OK;
    }
    
    if (otaManager.isUpdating()) {
        httpd_resp_sendstr(req, "{\"started\":false,\"error\":\"Update in progress\"}");
        return ESP_OK;
    }
    
    otaManager.startUpdate();
    httpd_resp_sendstr(req, "{\"started\":true}");
    return ESP_OK;
}

esp_err_t WebServerManager::handleOTAStatus(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    httpd_resp_set_type(req, "application/json");
    
    OTAResult status = otaManager.getStatus();
    
    const char* statusStr;
    switch (status.status) {
        case OTA_IDLE: statusStr = "idle"; break;
        case OTA_CHECKING: statusStr = "checking"; break;
        case OTA_DOWNLOADING: statusStr = "downloading"; break;
        case OTA_INSTALLING: statusStr = "installing"; break;
        case OTA_SUCCESS: statusStr = "success"; break;
        case OTA_FAILED: statusStr = "failed"; break;
        default: statusStr = "unknown"; break;
    }
    
    String json = "{";
    json += "\"status\":\"" + String(statusStr) + "\",";
    json += "\"progress\":" + String(status.progress) + ",";
    json += "\"message\":\"" + status.message + "\"";
    json += "}";
    
    httpd_resp_send(req, json.c_str(), json.length());
    return ESP_OK;
}

esp_err_t WebServerManager::handleOTAAutoInstall(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    
    httpd_resp_set_type(req, "application/json");
    
    String enabledStr = getQueryParameter(req, "enabled");
    if (enabledStr.length() > 0) {
        bool enabled = (enabledStr == "1" || enabledStr == "true");
        otaManager.setAutoInstall(enabled);
        String json = "{\"success\":true,\"enabled\":" + String(enabled ? "true" : "false") + "}";
        httpd_resp_send(req, json.c_str(), json.length());
    } else {
        bool enabled = otaManager.getAutoInstall();
        String json = "{\"success\":true,\"enabled\":" + String(enabled ? "true" : "false") + "}";
        httpd_resp_send(req, json.c_str(), json.length());
    }
    return ESP_OK;
}
