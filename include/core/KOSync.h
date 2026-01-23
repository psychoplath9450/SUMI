/**
 * @file KOSync.h
 * @brief KOReader Progress Sync Client
 * @version 1.0.0
 * 
 * Implements the KOReader sync protocol to synchronize
 * reading progress across devices.
 * 
 * Protocol: https://github.com/koreader/koreader-sync-server
 */

#ifndef SUMI_KOSYNC_H
#define SUMI_KOSYNC_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "core/SettingsManager.h"

// Forward declaration
extern SettingsManager settingsManager;

class KOSync {
public:
    struct Progress {
        String document;     // Document hash/ID
        String progress;     // Progress string (e.g., "0.5" or "page:50")
        String percentage;   // Percentage as string
        String device;       // Device name
        String deviceId;     // Device ID
        unsigned long timestamp;
    };
    
    KOSync() : _lastError("") {}
    
    /**
     * Register a new user account (optional - most servers have open registration)
     */
    bool registerUser(const char* username, const char* password) {
        if (!isEnabled()) return false;
        
        HTTPClient http;
        String url = String(settingsManager.sync.kosyncUrl) + "/users/create";
        
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        
        JsonDocument doc;
        doc["username"] = username;
        doc["password"] = password;
        
        String body;
        serializeJson(doc, body);
        
        int code = http.POST(body);
        bool success = (code == 200 || code == 201);
        
        if (!success) {
            _lastError = "Registration failed: " + String(code);
        }
        
        http.end();
        return success;
    }
    
    /**
     * Authorize and get session token
     */
    bool authorize() {
        if (!isEnabled()) return false;
        
        HTTPClient http;
        String url = String(settingsManager.sync.kosyncUrl) + "/users/auth";
        
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        
        JsonDocument doc;
        doc["username"] = settingsManager.sync.kosyncUser;
        doc["password"] = settingsManager.sync.kosyncPass;
        
        String body;
        serializeJson(doc, body);
        
        int code = http.POST(body);
        bool success = (code == 200);
        
        if (success) {
            String response = http.getString();
            JsonDocument resp;
            if (deserializeJson(resp, response) == DeserializationError::Ok) {
                _authorized = true;
                Serial.println("[KOSYNC] Authorization successful");
            }
        } else {
            _lastError = "Auth failed: " + String(code);
            _authorized = false;
        }
        
        http.end();
        return success;
    }
    
    /**
     * Get progress for a document from the server
     */
    bool getProgress(const char* documentHash, Progress& outProgress) {
        if (!isEnabled()) return false;
        
        HTTPClient http;
        String url = String(settingsManager.sync.kosyncUrl) + "/syncs/progress/" + documentHash;
        
        http.begin(url);
        http.addHeader("x-auth-user", settingsManager.sync.kosyncUser);
        http.addHeader("x-auth-key", settingsManager.sync.kosyncPass);
        
        int code = http.GET();
        
        if (code == 200) {
            String response = http.getString();
            JsonDocument doc;
            
            if (deserializeJson(doc, response) == DeserializationError::Ok) {
                outProgress.document = doc["document"] | "";
                outProgress.progress = doc["progress"] | "";
                outProgress.percentage = doc["percentage"] | "";
                outProgress.device = doc["device"] | "";
                outProgress.deviceId = doc["device_id"] | "";
                outProgress.timestamp = doc["timestamp"] | 0;
                
                http.end();
                return true;
            }
        } else if (code == 404) {
            // No progress found - not an error
            _lastError = "";
            http.end();
            return false;
        } else {
            _lastError = "Get progress failed: " + String(code);
        }
        
        http.end();
        return false;
    }
    
    /**
     * Update progress for a document on the server
     */
    bool updateProgress(const char* documentHash, const char* progress, float percentage) {
        if (!isEnabled()) return false;
        
        HTTPClient http;
        String url = String(settingsManager.sync.kosyncUrl) + "/syncs/progress";
        
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("x-auth-user", settingsManager.sync.kosyncUser);
        http.addHeader("x-auth-key", settingsManager.sync.kosyncPass);
        
        JsonDocument doc;
        doc["document"] = documentHash;
        doc["progress"] = progress;
        doc["percentage"] = String(percentage, 4);
        doc["device"] = "SUMI E-Reader";
        doc["device_id"] = getDeviceId();
        
        String body;
        serializeJson(doc, body);
        
        Serial.printf("[KOSYNC] Updating: %s -> %s (%.1f%%)\n", 
                      documentHash, progress, percentage * 100);
        
        int code = http.PUT(body);
        bool success = (code == 200 || code == 201);
        
        if (!success) {
            _lastError = "Update failed: " + String(code);
        } else {
            Serial.println("[KOSYNC] Progress updated successfully");
        }
        
        http.end();
        return success;
    }
    
    /**
     * Generate a document hash from file path (compatible with KOReader)
     * KOReader uses MD5 of the document content, but we use a simpler path hash
     */
    static String hashDocument(const char* bookPath) {
        // Simple hash for now - in production, use MD5 of first N bytes
        uint32_t hash = 5381;
        const char* p = bookPath;
        while (*p) {
            hash = ((hash << 5) + hash) + *p;
            p++;
        }
        
        char buf[17];
        snprintf(buf, sizeof(buf), "%08x%08x", hash, hash ^ 0xDEADBEEF);
        return String(buf);
    }
    
    /**
     * Convert page/chapter position to KOReader-compatible progress string
     */
    static String formatProgress(int chapter, int page, int totalChapters) {
        // KOReader uses various formats, we use a simple chapter.page format
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d/%d", chapter, page, totalChapters);
        return String(buf);
    }
    
    /**
     * Parse KOReader progress string to chapter/page
     */
    static bool parseProgress(const String& progress, int& chapter, int& page) {
        // Handle "chapter.page/total" format
        int dot = progress.indexOf('.');
        int slash = progress.indexOf('/');
        
        if (dot > 0 && slash > dot) {
            chapter = progress.substring(0, dot).toInt();
            page = progress.substring(dot + 1, slash).toInt();
            return true;
        }
        
        // Handle percentage format "0.xxxx"
        if (progress.startsWith("0.")) {
            // This is a percentage - we'd need total pages to convert
            return false;
        }
        
        return false;
    }
    
    bool isEnabled() const {
        return settingsManager.sync.kosyncEnabled && 
               strlen(settingsManager.sync.kosyncUrl) > 0 &&
               strlen(settingsManager.sync.kosyncUser) > 0;
    }
    
    const String& getLastError() const { return _lastError; }
    
private:
    String _lastError;
    bool _authorized = false;
    
    String getDeviceId() {
        // Use ESP32 MAC address as device ID
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(buf);
    }
};

// Global instance
extern KOSync koSync;

#endif // SUMI_KOSYNC_H
