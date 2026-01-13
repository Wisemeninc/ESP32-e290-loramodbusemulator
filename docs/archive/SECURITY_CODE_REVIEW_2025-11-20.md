# Code and Security Review Report
**Vision Master E290 - Modbus RTU/TCP Slave with LoRaWAN**

**Report Date:** November 20, 2025
**Repository:** ESP32-e290-loramodbusemulator
**Firmware Version:** v1.51
**Framework:** Arduino (PlatformIO)
**Platform:** ESP32-S3R8 (Heltec Vision Master E290)

---

## Executive Summary

This report follows up on the review from November 18, 2025. The codebase has evolved (version updated from v1.47 to v1.51), adding features like per-profile payload format selection. However, **none of the critical security vulnerabilities identified in the previous report have been addressed.**

**Overall Assessment:** **HIGH RISK** (Unchanged)

The device remains unsuitable for production deployment in its current state due to the presence of hardcoded default credentials, embedded private keys, and sensitive data exposure.

### Key Statistics
- **Total Lines of Code:** ~6,000+ lines (C/C++)
- **Critical Issues:** 3 (Unresolved)
- **High Priority Issues:** 7 (Unresolved)
- **Medium Priority Issues:** 6 (Unresolved)
- **Low Priority Issues:** 4 (Unresolved)

---

## 1. Security Assessment

### 1.1 CRITICAL SECURITY ISSUES (UNRESOLVED)

#### 🔴 CRITICAL-01: Default Credentials in Development Mode
**Severity:** Critical
**Location:** `src/config.h:19`, `src/wifi_manager.cpp:23`, `src/auth_manager.cpp:16-17`
**Status:** **PERSISTENT**
**Issue:**
- `MODE_PRODUCTION` is set to `false`.
- Default WiFi password `"modbus123"` is hardcoded.
- Default Web UI credentials `"admin"`/`"admin"` are hardcoded.
**Recommendation:**
- Set `MODE_PRODUCTION` to `true`.
- Remove hardcoded passwords.
- Implement forced password change on first login.

#### 🔴 CRITICAL-02: Sensitive Data Exposure via Serial Console
**Severity:** Critical
**Location:** `src/main.cpp`, `src/wifi_manager.cpp`, `src/auth_manager.cpp`
**Status:** **PERSISTENT**
**Issue:**
- WiFi passwords, AppKeys, and NwkKeys are printed to the serial console.
- Example: `Serial.printf(">>> Generated password: %s\n", wifi_password);`
**Recommendation:**
- Wrap all sensitive logging in `#ifdef DEBUG_SECURE` blocks that are disabled by default.

#### 🔴 CRITICAL-03: Self-Signed SSL Certificate with Embedded Private Key
**Severity:** Critical
**Location:** `src/server_cert.h`
**Status:** **PERSISTENT**
**Issue:**
- The server private key is hardcoded in the source file.
- This allows any attacker to decrypt traffic or impersonate the device.
**Recommendation:**
- Generate unique certificates per device during provisioning.
- Store keys in secure storage (eFuse or encrypted NVS).

### 1.2 HIGH SEVERITY ISSUES (UNRESOLVED)

#### 🟠 HIGH-01: Authentication Can Be Disabled
**Location:** `src/main.cpp:2659`
**Issue:** Users can disable authentication entirely via the web UI without secondary verification.

#### 🟠 HIGH-02: No Session Management / CSRF
**Location:** All HTTP handlers
**Issue:** Relies on Basic Auth for every request. Vulnerable to CSRF.

#### 🟠 HIGH-03: Buffer Overflow Risks
**Location:** `src/main.cpp:1640`
**Issue:** String parsing for keys assumes fixed lengths and lacks robust bounds checking.

#### 🟠 HIGH-04: Cleartext Secrets in NVS
**Location:** `src/wifi_manager.cpp`, `src/lorawan_handler.cpp`
**Issue:** WiFi passwords and LoRaWAN keys are stored in cleartext in NVS.

#### 🟠 HIGH-07: Known SSL Library Crash
**Location:** `src/main.cpp:1261`
**Issue:** Acknowledged bug in `HTTPS_Server_Generic` library causes reboots.

---

## 2. Code Quality Assessment

### 2.1 Architecture & Maintainability
- **Monolithic `main.cpp`:** The `src/main.cpp` file has grown to over 3,200 lines. It contains significant logic for HTML generation, request handling, and business logic that should be refactored into dedicated classes (e.g., `WebServerHandler`, `HtmlGenerator`).
- **Global State:** Heavy reliance on global variables (`holding_regs`, `input_regs`, `lorawanHandler`, etc.) makes testing and state management difficult.
- **String Usage:** Extensive use of the Arduino `String` class for HTML generation poses a risk of heap fragmentation over long uptimes.

### 2.2 Improvements in v1.51
- **LoRaWAN Profiles:** The addition of `LoRaWANHandler` with multi-profile support is a good architectural improvement, encapsulating complex logic.
- **Payload Formats:** The strategy pattern implementation for payload builders in `LoRaWANHandler` is clean and extensible.

---

## 3. Recommendations Plan

### Phase 1: Security Hardening (Immediate)
1.  **Secrets Management:** Remove `server_cert.h` from git. Implement a mechanism to generate or load unique certs.
2.  **Credential Protection:** Stop logging passwords to Serial.
3.  **Production Mode:** Default `MODE_PRODUCTION` to `true`.

### Phase 2: Refactoring (Short Term)
1.  **Split `main.cpp`:** Move web handlers to `src/web_handlers.cpp` and `src/web_handlers.h`.
2.  **HTML Optimization:** Move static HTML strings to `PROGMEM` or a separate `html_content.h` file to reduce RAM usage and clutter.

### Phase 3: Reliability (Medium Term)
1.  **NVS Encryption:** Enable Flash Encryption and NVS Encryption to protect stored credentials.
2.  **Watchdog:** Ensure Task Watchdog Timer (TWDT) is correctly configured to recover from SSL library hangs.

---

## 4. Conclusion

The codebase is functionally rich but insecure. The priority must shift from feature addition (like new payload formats) to security hardening and refactoring. The "High Risk" assessment remains until the critical issues regarding default credentials and private key management are resolved.