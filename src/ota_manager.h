#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <Preferences.h>

// GitHub repository configuration
#define GITHUB_REPO_OWNER "Wisemeninc"
#define GITHUB_REPO_NAME "ESP32-e290-loramodbusemulator"
#define GITHUB_FIRMWARE_PATH ".pio/build/vision-master-e290-arduino/firmware.bin"

// OTA Update Status
enum OTAStatus {
    OTA_IDLE = 0,
    OTA_CHECKING,
    OTA_DOWNLOADING,
    OTA_INSTALLING,
    OTA_SUCCESS,
    OTA_FAILED
};

// OTA Update Result Structure
struct OTAResult {
    OTAStatus status;
    int progress;           // 0-100 percentage
    String message;
    String latestVersion;
    String currentVersion;
    bool updateAvailable;
    size_t totalBytes;
    size_t downloadedBytes;
};

class OTAManager {
public:
    OTAManager();
    
    // Initialize OTA manager
    void begin();
    
    // Set GitHub Personal Access Token (stored in NVS)
    void setGitHubToken(const char* token);
    String getGitHubToken();
    bool hasToken();
    
    // Check for updates (non-blocking, returns immediately)
    void checkForUpdate();
    
    // Start firmware update (non-blocking)
    void startUpdate();
    
    // Get current status
    OTAResult getStatus();
    
    // Get current firmware version string
    String getCurrentVersion();
    
    // Task handler (call in loop or run as task)
    void handle();
    
    // Check if update is in progress
    bool isUpdating();
    
    // Auto-update checking configuration
    void setUpdateCheckInterval(uint8_t minutes);
    uint8_t getUpdateCheckInterval();
    
    // Auto-install when update found
    void setAutoInstall(bool enabled);
    bool getAutoInstall();
    
private:
    OTAResult result;
    String githubToken;
    bool tokenLoaded;
    TaskHandle_t otaTaskHandle;
    bool taskRunning;
    
    // Internal functions
    void loadToken();
    void saveToken();
    String getLatestReleaseVersion();
    String getLatestReleaseFirmwareUrl();
    bool downloadAndInstall(const String& url);
    
    // GitHub API helpers
    String makeGitHubApiRequest(const String& endpoint);
    
    // Static task wrapper
    static void otaTask(void* parameter);
    
    // Root CA for GitHub
    static const char* github_root_ca;
};

// Global instance
extern OTAManager otaManager;

#endif // OTA_MANAGER_H
