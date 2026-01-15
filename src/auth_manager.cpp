#include "auth_manager.h"
#include "mbedtls/base64.h"

// Global instance
AuthManager authManager;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

AuthManager::AuthManager() :
    auth_enabled(true),
    debug_https_enabled(false),
    debug_auth_enabled(false) {

    strcpy(username, "admin");
    strcpy(password, "admin");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void AuthManager::begin() {
    load();
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

bool AuthManager::checkAuthentication(httpd_req_t* req) {
    if (debug_auth_enabled) {
        Serial.printf(">>> Auth check: enabled=%d, user=%s\n", auth_enabled, username);
    }

    if (!auth_enabled) {
        if (debug_auth_enabled) {
            Serial.println(">>> Auth disabled, allowing access");
        }
        return true;  // Auth disabled, allow access
    }

    // Get Authorization header length
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (auth_len == 0) {
        if (debug_auth_enabled) {
            Serial.println(">>> No Authorization header, requesting credentials");
        }
        return false;
    }

    // Get Authorization header value
    char* auth_buf = (char*)malloc(auth_len + 1);
    if (!auth_buf) {
        return false;
    }
    
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_buf, auth_len + 1) != ESP_OK) {
        free(auth_buf);
        return false;
    }
    
    String authValue = String(auth_buf);
    free(auth_buf);

    // Parse Basic authentication header
    if (!authValue.startsWith("Basic ")) {
        if (debug_auth_enabled) {
            Serial.println(">>> Invalid auth type (not Basic)");
        }
        return false;
    }

    // Extract base64 encoded credentials
    String base64Credentials = authValue.substring(6);
    base64Credentials.trim();

    // Decode base64 using mbedtls
    unsigned char decoded[128];
    size_t decoded_len = 0;
    int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_len,
                                     (const unsigned char*)base64Credentials.c_str(),
                                     base64Credentials.length());

    if (ret != 0) {
        if (debug_auth_enabled) {
            Serial.printf(">>> Base64 decode failed: %d\n", ret);
        }
        return false;
    }

    String decodedCredentials = String((char*)decoded).substring(0, decoded_len);

    // Split credentials into username and password
    int colonIndex = decodedCredentials.indexOf(':');
    if (colonIndex == -1) {
        if (debug_auth_enabled) {
            Serial.println(">>> Invalid credentials format (no colon)");
        }
        return false;
    }

    String providedUsername = decodedCredentials.substring(0, colonIndex);
    String providedPassword = decodedCredentials.substring(colonIndex + 1);

    // Compare credentials
    if (providedUsername.equals(username) && providedPassword.equals(password)) {
        if (debug_auth_enabled) {
            Serial.println(">>> Auth successful");
        }
        return true;
    } else {
        if (debug_auth_enabled) {
            Serial.println(">>> Auth failed - incorrect credentials");
            Serial.printf(">>> Provided username: %s, expected: %s\n",
                         providedUsername.c_str(), username);
        }
        return false;
    }
}

// ============================================================================
// CREDENTIALS
// ============================================================================

void AuthManager::setCredentials(const char* user, const char* pass) {
    strncpy(username, user, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';

    strncpy(password, pass, sizeof(password) - 1);
    password[sizeof(password) - 1] = '\0';

    Serial.printf(">>> Auth credentials updated: username=%s\n", username);
}

String AuthManager::getUsername() {
    return String(username);
}

String AuthManager::getPassword() {
    return String(password);
}

bool AuthManager::isEnabled() {
    return auth_enabled;
}

// ============================================================================
// ENABLE/DISABLE
// ============================================================================

void AuthManager::enable() {
    auth_enabled = true;
    Serial.println(">>> Authentication enabled");
}

void AuthManager::disable() {
    auth_enabled = false;
    Serial.println(">>> Authentication disabled");
}

void AuthManager::setEnabled(bool enabled) {
    auth_enabled = enabled;
    Serial.printf(">>> Authentication %s\n", enabled ? "enabled" : "disabled");
}

// ============================================================================
// DEBUG SETTINGS
// ============================================================================

void AuthManager::setDebugHTTPS(bool enabled) {
    debug_https_enabled = enabled;
    Serial.printf(">>> HTTPS Debug: %s\n", enabled ? "ENABLED" : "DISABLED");
}

void AuthManager::setDebugAuth(bool enabled) {
    debug_auth_enabled = enabled;
    Serial.printf(">>> Auth Debug: %s\n", enabled ? "ENABLED" : "DISABLED");
}

bool AuthManager::getDebugHTTPS() {
    return debug_https_enabled;
}

bool AuthManager::getDebugAuth() {
    return debug_auth_enabled;
}

// ============================================================================
// NVS STORAGE
// ============================================================================

void AuthManager::save() {
    if (!preferences.begin("auth", false)) {  // Read-write
        Serial.println(">>> Failed to open auth preferences for writing");
        return;
    }

    preferences.putBool("enabled", auth_enabled);
    preferences.putString("username", username);
    preferences.putString("password", password);
    preferences.putBool("debug_https", debug_https_enabled);
    preferences.putBool("debug_auth", debug_auth_enabled);
    preferences.end();

    Serial.println(">>> Authentication credentials saved to NVS");
    Serial.printf("    HTTPS Debug: %s\n", debug_https_enabled ? "YES" : "NO");
    Serial.printf("    Auth Debug: %s\n", debug_auth_enabled ? "YES" : "NO");
}

void AuthManager::load() {
    if (!preferences.begin("auth", false)) {  // Read-write (creates if not exists)
        Serial.println(">>> Failed to open auth preferences");
        return;
    }

    auth_enabled = preferences.getBool("enabled", true);
    debug_https_enabled = preferences.getBool("debug_https", false);
    debug_auth_enabled = preferences.getBool("debug_auth", false);

    String user = preferences.getString("username", "admin");
    String pass = preferences.getString("password", "admin");

    if (user.length() > 0 && user.length() <= 32) {
        strncpy(username, user.c_str(), sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    }

    if (pass.length() > 0 && pass.length() <= 32) {
        strncpy(password, pass.c_str(), sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
    }

    preferences.end();

    Serial.println(">>> Authentication credentials loaded");
    Serial.printf("    Enabled: %s\n", auth_enabled ? "YES" : "NO");
    Serial.printf("    Username: %s\n", username);
    Serial.printf("    HTTPS Debug: %s\n", debug_https_enabled ? "YES" : "NO");
    Serial.printf("    Auth Debug: %s\n", debug_auth_enabled ? "YES" : "NO");
}
