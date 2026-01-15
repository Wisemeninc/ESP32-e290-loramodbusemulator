#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <esp_https_server.h>

// ============================================================================
// AUTHENTICATION MANAGER CLASS
// ============================================================================

class AuthManager {
public:
    AuthManager();

    // Initialization
    void begin();

    // Authentication - for esp_https_server
    bool checkAuthentication(httpd_req_t* req);
    
    // Get password for validation (used internally)
    String getPassword();

    // Credentials
    void setCredentials(const char* username, const char* password);
    String getUsername();
    bool isEnabled();

    // Enable/Disable
    void enable();
    void disable();
    void setEnabled(bool enabled);

    // Debug settings
    void setDebugHTTPS(bool enabled);
    void setDebugAuth(bool enabled);
    bool getDebugHTTPS();
    bool getDebugAuth();

    // NVS Storage
    void save();
    void load();

private:
    Preferences preferences;

    // Authentication state
    bool auth_enabled;
    char username[33];  // 32 chars + null
    char password[33];  // 32 chars + null

    // Debug settings
    bool debug_https_enabled;
    bool debug_auth_enabled;
};

// Global instance
extern AuthManager authManager;

#endif // AUTH_MANAGER_H
