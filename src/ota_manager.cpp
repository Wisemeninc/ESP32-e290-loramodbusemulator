#include "ota_manager.h"
#include "config.h"
#include <ArduinoJson.h>

// Global instance
OTAManager otaManager;

// GitHub Root CA Certificate (DigiCert Global Root G2 - valid for github.com)
const char* OTAManager::github_root_ca = R"(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMCcit6E7dckgrpPFRGepvRQDxPXSsJkE7ks3GyXkFGO/GhPRDkwMo5VVvQM
9TNFRa2NlhCfN8Z/2eYrVU+WHp2hQI7UDKXJlbgBrdO8yXnp5s9pMfaKC+Q5OISZ
i9TqL/iVcYJHpXXB4BzJH/c3GKxk0xbvvLrY5g9PDAw8k0h0KiExGgbPmR56rl4T
OLTXCDpPHYAWRB7TvUJWwf8X3e7AjJ8jguKBkkr8l0onFBZmCE2PTfmksrhaDfCb
9sJxnyDj2AUfD8sJ9QotYfvrQlgDsU3LKJRXOUKL4Bsn5xNPpgVgVkksh3J48RAh
YJU=
-----END CERTIFICATE-----
)";

OTAManager::OTAManager() : tokenLoaded(false), otaTaskHandle(NULL), taskRunning(false) {
    result.status = OTA_IDLE;
    result.progress = 0;
    result.updateAvailable = false;
    result.totalBytes = 0;
    result.downloadedBytes = 0;
}

void OTAManager::begin() {
    loadToken();
    result.currentVersion = getCurrentVersion();
    Serial.println("[OTA] Manager initialized, current version: " + result.currentVersion);
}

void OTAManager::setUpdateCheckInterval(uint8_t minutes) {
    if (minutes == 0 || minutes > 1440) return;  // Invalid range (max 1 day = 1440 minutes)
    
    Preferences prefs;
    if (prefs.begin("ota", false)) {  // false = read-write
        prefs.putUChar("check_interval", minutes);
        prefs.end();
        Serial.printf("[OTA] Auto-check interval set to %d minutes\n", minutes);
    } else {
        Serial.println("[OTA] Failed to save update check interval to preferences");
    }
}

uint8_t OTAManager::getUpdateCheckInterval() {
    Preferences prefs;
    uint8_t interval = AUTO_UPDATE_CHECK_INTERVAL_MINUTES;  // Default value
    
    if (prefs.begin("ota", false)) {  // false = read-write, creates namespace if needed
        interval = prefs.getUChar("check_interval", AUTO_UPDATE_CHECK_INTERVAL_MINUTES);
        prefs.end();
    } else {
        Serial.println("[OTA] Failed to open preferences, using default interval");
    }
    
    return interval;
}

String OTAManager::getCurrentVersion() {
    char version[16];
    snprintf(version, sizeof(version), "v%d.%02d", FIRMWARE_VERSION / 100, FIRMWARE_VERSION % 100);
    return String(version);
}

void OTAManager::loadToken() {
    // Check for hardcoded token in config.h first
    #ifdef GITHUB_PAT
        const char* hardcodedToken = GITHUB_PAT;
        #if GITHUB_PAT_PREFER_HARDCODED
            // Always use hardcoded token if it's not empty
            if (strlen(hardcodedToken) > 0) {
                githubToken = hardcodedToken;
                tokenLoaded = true;
                Serial.println("[OTA] Using hardcoded GitHub token from config.h");
                return;
            }
        #endif
    #endif
    
    // Try to load from NVS
    Preferences prefs;
    if (prefs.begin("ota", false)) {  // false = read-write, creates namespace if needed
        githubToken = prefs.getString("gh_token", "");
        prefs.end();
    } else {
        Serial.println("[OTA] Failed to open preferences for token loading");
        githubToken = "";
    }
    
    // Fall back to hardcoded token if NVS is empty
    #ifdef GITHUB_PAT
        if (githubToken.length() == 0) {
            const char* fallbackToken = GITHUB_PAT;
            if (strlen(fallbackToken) > 0) {
                githubToken = fallbackToken;
                Serial.println("[OTA] Using hardcoded GitHub token (NVS empty)");
            }
        }
    #endif
    
    tokenLoaded = true;
}

void OTAManager::saveToken() {
    Preferences prefs;
    if (prefs.begin("ota", false)) {
        prefs.putString("gh_token", githubToken);
        prefs.end();
        Serial.println("[OTA] GitHub token saved to preferences");
    } else {
        Serial.println("[OTA] Failed to save GitHub token to preferences");
    }
}

void OTAManager::setGitHubToken(const char* token) {
    githubToken = String(token);
    saveToken();
}

String OTAManager::getGitHubToken() {
    if (!tokenLoaded) loadToken();
    return githubToken;
}

bool OTAManager::hasToken() {
    if (!tokenLoaded) loadToken();
    return githubToken.length() > 0;
}

OTAResult OTAManager::getStatus() {
    return result;
}

bool OTAManager::isUpdating() {
    return result.status == OTA_CHECKING || 
           result.status == OTA_DOWNLOADING || 
           result.status == OTA_INSTALLING;
}

String OTAManager::makeGitHubApiRequest(const String& endpoint) {
    if (!hasToken()) {
        Serial.println("[OTA] No GitHub token configured");
        return "";
    }
    
    Serial.println("[OTA] Using token: " + githubToken.substring(0, 10) + "..." + githubToken.substring(githubToken.length() - 4));
    
    WiFiClientSecure client;
    // For testing: skip certificate verification (INSECURE but works around cert issues)
    // TODO: Use proper root CA once working
    client.setInsecure();
    client.setTimeout(15);  // 15 second timeout for SSL handshake
    
    HTTPClient http;
    String url = String("https://api.github.com") + endpoint;
    
    Serial.println("[OTA] API Request: " + url);
    
    // Reset watchdog before SSL connection
    if (xTaskGetCurrentTaskHandle() != NULL) {
        esp_task_wdt_reset();
    }
    
    if (!http.begin(client, url)) {
        Serial.println("[OTA] Failed to begin HTTP connection");
        return "";
    }
    
    http.addHeader("Authorization", "Bearer " + githubToken);
    http.addHeader("Accept", "application/vnd.github.v3+json");
    http.addHeader("User-Agent", "ESP32-OTA-Updater");
    http.setTimeout(15000);  // 15 second HTTP timeout
    
    // Reset watchdog before HTTP GET (SSL handshake happens here)
    if (xTaskGetCurrentTaskHandle() != NULL) {
        esp_task_wdt_reset();
    }
    
    int httpCode = http.GET();
    String response = "";
    
    // Reset watchdog after HTTP GET completes
    if (xTaskGetCurrentTaskHandle() != NULL) {
        esp_task_wdt_reset();
    }
    
    if (httpCode == HTTP_CODE_OK) {
        response = http.getString();
    } else {
        Serial.printf("[OTA] HTTP error: %d\n", httpCode);
        if (httpCode > 0) {
            Serial.println("[OTA] Response: " + http.getString());
        }
    }
    
    http.end();
    return response;
}

void OTAManager::checkForUpdate() {
    if (isUpdating()) {
        Serial.println("[OTA] Update already in progress");
        return;
    }
    
    if (!hasToken()) {
        result.status = OTA_FAILED;
        result.message = "GitHub token not configured";
        return;
    }
    
    // Test DNS resolution
    Serial.println("[OTA] Testing DNS resolution...");
    IPAddress ip;
    if (WiFi.hostByName("api.github.com", ip) == 1) {
        Serial.println("[OTA] DNS OK: api.github.com -> " + ip.toString());
    } else {
        Serial.println("[OTA] DNS FAILED for api.github.com");
        result.status = OTA_FAILED;
        result.message = "DNS resolution failed. Check WiFi connection.";
        return;
    }
    
    result.status = OTA_CHECKING;
    result.message = "Checking for updates...";
    result.progress = 0;
    
    // Reset watchdog before API call
    if (xTaskGetCurrentTaskHandle() != NULL) {
        esp_task_wdt_reset();
    }
    
    // Get latest release info
    String endpoint = String("/repos/") + String(GITHUB_REPO_OWNER) + "/" + String(GITHUB_REPO_NAME) + "/releases/latest";
    String response = makeGitHubApiRequest(endpoint);
    
    // Reset watchdog after API call
    if (xTaskGetCurrentTaskHandle() != NULL) {
        esp_task_wdt_reset();
    }
    
    if (response.length() == 0) {
        // Try to get releases list if no "latest" release
        endpoint = String("/repos/") + String(GITHUB_REPO_OWNER) + "/" + String(GITHUB_REPO_NAME) + "/releases";
        response = makeGitHubApiRequest(endpoint);
        
        // Reset watchdog after second API call
        if (xTaskGetCurrentTaskHandle() != NULL) {
            esp_task_wdt_reset();
        }
        
        if (response.length() == 0) {
            result.status = OTA_FAILED;
            result.message = "Failed to check for updates. Check token and network.";
            return;
        }
    }
    
    Serial.println("[OTA] Response length: " + String(response.length()));
    Serial.println("[OTA] Response preview: " + response.substring(0, min(500, (int)response.length())));
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.println("[OTA] JSON parse error: " + String(error.c_str()));
        result.status = OTA_FAILED;
        result.message = "Failed to parse release info";
        return;
    }
    
    // Handle both single release and array of releases
    JsonObject release;
    if (doc.is<JsonArray>()) {
        JsonArray releases = doc.as<JsonArray>();
        Serial.println("[OTA] Got array with " + String(releases.size()) + " releases");
        if (releases.size() == 0) {
            result.status = OTA_IDLE;
            result.message = "No releases found";
            result.updateAvailable = false;
            return;
        }
        release = releases[0];
    } else {
        Serial.println("[OTA] Got single release object");
        release = doc.as<JsonObject>();
    }
    
    String tagName = release["tag_name"] | "unknown";
    Serial.println("[OTA] Found tag: " + tagName);
    result.latestVersion = tagName;
    
    Serial.println("[OTA] Latest release: " + tagName);
    Serial.println("[OTA] Current version: " + result.currentVersion);
    
    // Parse version numbers for proper comparison
    int remoteMajor = 0, remoteMinor = 0;
    int localMajor = 0, localMinor = 0;
    
    // Parse remote version (e.g., "v1.66" -> 1, 66)
    if (tagName.startsWith("v") || tagName.startsWith("V")) {
        int dotIdx = tagName.indexOf('.');
        if (dotIdx > 1) {
            remoteMajor = tagName.substring(1, dotIdx).toInt();
            remoteMinor = tagName.substring(dotIdx + 1).toInt();
        }
    }
    
    // Parse local version from FIRMWARE_VERSION constant
    // FIRMWARE_VERSION format: MMmm (e.g., 165 = v1.65)
    localMajor = FIRMWARE_VERSION / 100;
    localMinor = FIRMWARE_VERSION % 100;
    
    Serial.printf("[OTA] Remote: %d.%d, Local: %d.%d\n", remoteMajor, remoteMinor, localMajor, localMinor);
    
    // Compare versions numerically
    bool isNewer = false;
    if (remoteMajor > localMajor) {
        isNewer = true;
    } else if (remoteMajor == localMajor && remoteMinor > localMinor) {
        isNewer = true;
    }
    
    if (isNewer) {
        result.updateAvailable = true;
        result.message = "Update available: " + tagName + " (current: " + result.currentVersion + ")";
    } else if (remoteMajor == localMajor && remoteMinor == localMinor) {
        result.updateAvailable = false;
        result.message = "Already up to date (" + result.currentVersion + ")";
    } else {
        result.updateAvailable = false;
        result.message = "Latest release " + tagName + " is older than current " + result.currentVersion;
    }
    
    result.status = OTA_IDLE;
}

void OTAManager::startUpdate() {
    if (isUpdating()) {
        Serial.println("[OTA] Update already in progress");
        return;
    }
    
    if (!hasToken()) {
        result.status = OTA_FAILED;
        result.message = "GitHub token not configured";
        return;
    }
    
    // Start OTA task
    taskRunning = true;
    xTaskCreatePinnedToCore(
        otaTask,
        "OTATask",
        16384,  // 16KB stack
        this,
        1,
        &otaTaskHandle,
        0  // Core 0
    );
}

void OTAManager::otaTask(void* parameter) {
    OTAManager* self = (OTAManager*)parameter;
    
    // Add this task to watchdog monitoring
    esp_task_wdt_add(NULL);
    
    self->result.status = OTA_DOWNLOADING;
    self->result.message = "Fetching firmware URL...";
    self->result.progress = 0;
    
    // Reset watchdog before starting operations
    esp_task_wdt_reset();
    
    // Get the firmware download URL from the latest release
    String endpoint = String("/repos/") + String(GITHUB_REPO_OWNER) + "/" + String(GITHUB_REPO_NAME) + "/releases/latest";
    String response = self->makeGitHubApiRequest(endpoint);
    
    if (response.length() == 0) {
        // Try releases list
        endpoint = String("/repos/") + String(GITHUB_REPO_OWNER) + "/" + String(GITHUB_REPO_NAME) + "/releases";
        response = self->makeGitHubApiRequest(endpoint);
    }
    
    if (response.length() == 0) {
        self->result.status = OTA_FAILED;
        self->result.message = "Failed to get release info";
        self->taskRunning = false;
        vTaskDelete(NULL);
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        self->result.status = OTA_FAILED;
        self->result.message = "Failed to parse release info";
        self->taskRunning = false;
        vTaskDelete(NULL);
        return;
    }
    
    // Find firmware.bin asset
    JsonObject release;
    if (doc.is<JsonArray>()) {
        release = doc.as<JsonArray>()[0];
    } else {
        release = doc.as<JsonObject>();
    }
    
    String firmwareUrl = "";
    JsonArray assets = release["assets"];
    
    for (JsonObject asset : assets) {
        String name = asset["name"] | "";
        if (name.endsWith(".bin") || name == "firmware.bin") {
            // Use the browser_download_url but we need to add auth header
            // Actually for private repos, we need to use the API URL
            int assetId = asset["id"] | 0;
            if (assetId > 0) {
                firmwareUrl = String("https://api.github.com/repos/") + String(GITHUB_REPO_OWNER) + "/" + String(GITHUB_REPO_NAME) + "/releases/assets/" + String(assetId);
            }
            Serial.println("[OTA] Found firmware asset: " + name);
            break;
        }
    }
    
    if (firmwareUrl.length() == 0) {
        // No release assets, try to get from raw content (main branch)
        // For private repos, use the contents API
        firmwareUrl = String("https://raw.githubusercontent.com/") + String(GITHUB_REPO_OWNER) + "/" + String(GITHUB_REPO_NAME) + "/main/" + String(GITHUB_FIRMWARE_PATH);
        Serial.println("[OTA] No release assets, trying raw content URL");
    }
    
    // Download and install firmware
    self->result.message = "Downloading firmware...";
    
    if (!self->downloadAndInstall(firmwareUrl)) {
        self->taskRunning = false;
        vTaskDelete(NULL);
        return;
    }
    
    self->result.status = OTA_SUCCESS;
    self->result.message = "Update successful! Rebooting...";
    self->result.progress = 100;
    
    Serial.println("[OTA] Update complete, rebooting in 3 seconds...");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    ESP.restart();
    
    self->taskRunning = false;
    vTaskDelete(NULL);
}

bool OTAManager::downloadAndInstall(const String& url) {
    Serial.println("[OTA] Downloading from: " + url);
    
    WiFiClientSecure client;
    // For testing: skip certificate verification
    client.setInsecure();
    client.setTimeout(30);  // 30 second timeout
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(60000);  // 60 second timeout
    
    if (!http.begin(client, url)) {
        result.status = OTA_FAILED;
        result.message = "Failed to connect to download server";
        return false;
    }
    
    // Add authentication headers
    http.addHeader("Authorization", "Bearer " + githubToken);
    http.addHeader("User-Agent", "ESP32-OTA-Updater");
    
    // For asset downloads, we need to accept octet-stream
    if (url.indexOf("/assets/") > 0) {
        http.addHeader("Accept", "application/octet-stream");
    }
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] HTTP error: %d\n", httpCode);
        result.status = OTA_FAILED;
        result.message = "Download failed: HTTP " + String(httpCode);
        http.end();
        return false;
    }
    
    int contentLength = http.getSize();
    Serial.printf("[OTA] Content length: %d bytes\n", contentLength);
    
    if (contentLength <= 0) {
        result.status = OTA_FAILED;
        result.message = "Invalid firmware size";
        http.end();
        return false;
    }
    
    result.totalBytes = contentLength;
    result.downloadedBytes = 0;
    
    // Start OTA update
    if (!Update.begin(contentLength)) {
        Serial.println("[OTA] Not enough space for update");
        result.status = OTA_FAILED;
        result.message = "Not enough space for update";
        http.end();
        return false;
    }
    
    result.status = OTA_INSTALLING;
    result.message = "Installing firmware...";
    
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buff[1024];
    size_t written = 0;
    size_t totalSize = contentLength;  // Save original size for progress calculation
    
    while (http.connected() && (contentLength > 0 || contentLength == -1)) {
        // Reset watchdog timer frequently during download
        esp_task_wdt_reset();
        
        size_t available = stream->available();
        
        if (available) {
            size_t readBytes = stream->readBytes(buff, min(available, sizeof(buff)));
            
            if (readBytes > 0) {
                // Reset watchdog before flash write operation
                esp_task_wdt_reset();
                
                size_t writeResult = Update.write(buff, readBytes);
                
                if (writeResult != readBytes) {
                    Serial.println("[OTA] Write error");
                    result.status = OTA_FAILED;
                    result.message = "Firmware write error";
                    Update.abort();
                    http.end();
                    return false;
                }
                
                written += readBytes;
                result.downloadedBytes = written;
                result.progress = (written * 100) / totalSize;  // Use saved total size
                
                if (contentLength > 0) {
                    contentLength -= readBytes;
                }
                
                Serial.printf("[OTA] Progress: %d%% (%d/%d)\n", result.progress, written, result.totalBytes);
                
                // Reset watchdog after flash operations
                esp_task_wdt_reset();
            }
        }
        
        vTaskDelay(1);  // Yield to other tasks
    }
    
    http.end();
    
    // Reset watchdog before final flash operations
    esp_task_wdt_reset();
    Serial.println("[OTA] Finalizing firmware update...");
    
    if (!Update.end(true)) {
        Serial.println("[OTA] Update.end() failed: " + String(Update.errorString()));
        result.status = OTA_FAILED;
        result.message = "Update failed: " + String(Update.errorString());
        return false;
    }
    
    // Reset watchdog after completion
    esp_task_wdt_reset();
    
    if (!Update.isFinished()) {
        Serial.println("[OTA] Update not finished");
        result.status = OTA_FAILED;
        result.message = "Update incomplete";
        return false;
    }
    
    Serial.println("[OTA] Update successful!");
    return true;
}

void OTAManager::handle() {
    // Currently no background processing needed
    // The OTA update runs in its own task
}
