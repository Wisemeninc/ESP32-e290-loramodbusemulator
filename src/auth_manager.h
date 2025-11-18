#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>

using namespace httpsserver;

// ============================================================================
// AUTHENTICATION MANAGER CLASS
// ============================================================================

class AuthManager {
public:
    AuthManager();

    // Initialization
    void begin();

    // Authentication
    bool checkAuthentication(HTTPRequest* req, HTTPResponse* res);

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

    // Helper functions
    void sendUnauthorized(HTTPResponse* res, const char* message = "Authentication required");
};

// Global instance
extern AuthManager authManager;

#endif // AUTH_MANAGER_H
