/**
 * @file WebServerLite.cpp
 * @brief Minimal WebServer for ESP32-C3 setup portal
 * 
 * Only includes essential endpoints:
 * - WiFi scan/connect
 * - Settings get/set
 * - Portal serving from SD
 * - Setup complete
 * - Captive portal detection handlers
 */

#include "config.h"
#if FEATURE_WEBSERVER

#include "core/WebServer.h"
#include "core/WiFiManager.h"
#include <pgmspace.h>

// Embedded portal HTML - no SD card needed
#include "core/portal_html.h"

#include "core/BatteryMonitor.h"
#include "core/SettingsManager.h"
#if FEATURE_BLUETOOTH
#include "core/BluetoothManager.h"
#endif
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <Preferences.h>

SumiWebServer webServer;

// Global flags to signal main loop - checked in loop()
volatile bool g_settingsDeployed = false;
volatile bool g_wifiJustConnected = false;

// =============================================================================
// Constructor & Lifecycle
// =============================================================================
SumiWebServer::SumiWebServer() : _server(80), _running(false) {}

void SumiWebServer::begin() {
    if (_running) return;
    Serial.println("[WEB] Starting server...");
    Serial.printf("[WEB] Free heap: %d\n", ESP.getFreeHeap());
    setupRoutes();
    _server.begin();
    _running = true;
    Serial.println("[WEB] Server ready on port 80");
}

void SumiWebServer::stop() {
    if (!_running) return;
    _server.end();
    _running = false;
}

// =============================================================================
// Route Setup - MINIMAL
// =============================================================================
void SumiWebServer::setupRoutes() {
    // CORS preflight
    _server.on("/*", HTTP_OPTIONS, [this](AsyncWebServerRequest* r) {
        AsyncWebServerResponse* resp = r->beginResponse(200);
        addCORSHeaders(resp);
        r->send(resp);
    });
    
    setupCaptivePortalRoutes();  // Add captive portal handlers FIRST
    setupAPIRoutes();
    setupPageRoutes();
    
    // Redirect unknown requests to portal (captive portal behavior)
    _server.onNotFound([](AsyncWebServerRequest* r) {
        // If it looks like a captive portal check or browser request, redirect to portal
        if (r->host() != "192.168.4.1" && r->host() != "sumi.local") {
            r->redirect("http://192.168.4.1/");
        } else {
            r->send(404, "application/json", "{\"error\":\"Not found\"}");
        }
    });
}

// =============================================================================
// Captive Portal Routes - Makes phones/computers auto-detect the portal
// =============================================================================
void SumiWebServer::setupCaptivePortalRoutes() {
    // Windows connectivity checks
    _server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", "Microsoft Connect Test");
    });
    _server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", "Microsoft NCSI");
    });
    _server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->redirect("http://192.168.4.1/");
    });
    _server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->redirect("http://192.168.4.1/");
    });
    
    // Apple captive portal checks
    _server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    _server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    
    // Android captive portal checks
    _server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(204);
    });
    _server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(204);
    });
    
    // Firefox captive portal checks
    _server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    _server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", "success");
    });
    
    Serial.println("[WEB] Captive portal routes registered");
}

void SumiWebServer::setupAPIRoutes() {
    // Status
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* r) { handleStatus(r); });
    
    // Settings
    _server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGetSettings(r); });
    _server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t len, size_t, size_t) { handleSaveSettings(r, d, len); });
    
    // Setup Complete
    _server.on("/api/setup/complete", HTTP_POST, [this](AsyncWebServerRequest* r) { handleSetupComplete(r); });
    
    // WiFi - Essential endpoints only
    _server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* r) { handleWiFiScan(r); });
    _server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t len, size_t, size_t) { handleWiFiConnect(r, d, len); });
    _server.on("/api/wifi/disconnect", HTTP_POST, [this](AsyncWebServerRequest* r) { handleWiFiDisconnect(r); });
    
    // Files - Basic only
    _server.on("/api/files", HTTP_GET, [this](AsyncWebServerRequest* r) { handleFileList(r); });
    _server.on("/api/files/upload", HTTP_POST,
        [](AsyncWebServerRequest* r) { r->send(200, "application/json", "{\"status\":\"ok\"}"); },
        [this](AsyncWebServerRequest* r, const String& fn, size_t idx, uint8_t* d, size_t len, bool fin) {
            handleFileUpload(r, fn, idx, d, len, fin);
        });
    
    // System
    _server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* r) { handleReboot(r); });
    
    // Display refresh
    _server.on("/api/refresh", HTTP_POST, [this](AsyncWebServerRequest* r) {
        Serial.println("[WEB] Display refresh requested");
        extern volatile bool g_settingsDeployed;
        g_settingsDeployed = true;  // Triggers refresh in main loop
        sendSuccess(r, "refresh_triggered");
    });
    
    // File delete
    _server.on("/api/files/delete", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            const char* path = doc["path"];
            if (!path || strlen(path) == 0) {
                sendError(r, "Path required");
                return;
            }
            
            // Security: Prevent directory traversal
            if (strstr(path, "..") != nullptr) {
                sendError(r, "Invalid path");
                return;
            }
            
            if (!SD.exists(path)) {
                sendError(r, "File not found");
                return;
            }
            
            if (SD.remove(path)) {
                Serial.printf("[WEB] Deleted: %s\n", path);
                sendSuccess(r, "deleted");
            } else {
                sendError(r, "Delete failed");
            }
        });
    
    // Factory reset
    _server.on("/api/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* r) {
        Serial.println("[WEB] Factory reset requested");
        settingsManager.reset();
        settingsManager.setSetupComplete(false);
        settingsManager.save();
        sendSuccess(r, "Factory reset complete. Rebooting...");
        delay(500);
        ESP.restart();
    });
    
    // Weather location API
    _server.on("/api/weather/location", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            handleWeatherLocation(r, data, len);
        });
    
    // Weather unit API
    _server.on("/api/weather/unit", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            handleWeatherUnit(r, data, len);
        });
    
    // Timezone API
    _server.on("/api/weather/timezone", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            handleTimezone(r, data, len);
        });
    
    // Debug heap
    _server.on("/api/debug/heap", HTTP_GET, [](AsyncWebServerRequest* r) {
        String json = "{\"freeHeap\":" + String(ESP.getFreeHeap()) + 
                      ",\"minFreeHeap\":" + String(ESP.getMinFreeHeap()) + "}";
        r->send(200, "application/json", json);
    });
    
    // =========================================================================
    // Backup/Restore Settings API
    // =========================================================================
    
    // GET /api/backup - Download all settings as JSON
    _server.on("/api/backup", HTTP_GET, [this](AsyncWebServerRequest* r) {
        Serial.println("[WEB] Backup settings requested");
        
        JsonDocument doc;
        settingsManager.toJSON(doc.to<JsonObject>());
        doc["backupVersion"] = 1;
        doc["backupDate"] = millis();
        doc["firmwareVersion"] = SUMI_VERSION;
        
        String response;
        serializeJsonPretty(doc, response);
        
        AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        resp->addHeader("Content-Disposition", "attachment; filename=\"sumi-backup.json\"");
        r->send(resp);
        
        Serial.printf("[WEB] Backup sent: %d bytes\n", response.length());
    });
    
    // POST /api/restore - Upload and restore settings from JSON
    _server.on("/api/restore", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            Serial.println("[WEB] Restore settings requested");
            
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                Serial.printf("[WEB] Restore JSON parse error: %s\n", err.c_str());
                sendError(r, "Invalid JSON");
                return;
            }
            
            // Validate it's a backup file
            if (!doc["backupVersion"].is<int>()) {
                sendError(r, "Not a valid backup file");
                return;
            }
            
            // Restore settings
            if (settingsManager.fromJSON(doc.as<JsonObjectConst>())) {
                settingsManager.save();
                Serial.println("[WEB] Settings restored successfully");
                sendSuccess(r, "Settings restored");
            } else {
                sendError(r, "Restore failed");
            }
        });
    
    // =========================================================================
    // KOReader Sync API
    // =========================================================================
    
    // GET /api/sync/settings - Get sync settings
    _server.on("/api/sync/settings", HTTP_GET, [this](AsyncWebServerRequest* r) {
        JsonDocument doc;
        doc["url"] = settingsManager.sync.kosyncUrl;
        doc["username"] = settingsManager.sync.kosyncUser;
        // Don't send password
        doc["enabled"] = settingsManager.sync.kosyncEnabled;
        
        String response;
        serializeJson(doc, response);
        AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        r->send(resp);
    });
    
    // POST /api/sync/settings - Update sync settings
    _server.on("/api/sync/settings", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            if (doc["url"].is<const char*>()) {
                strncpy(settingsManager.sync.kosyncUrl, doc["url"], sizeof(settingsManager.sync.kosyncUrl) - 1);
            }
            if (doc["username"].is<const char*>()) {
                strncpy(settingsManager.sync.kosyncUser, doc["username"], sizeof(settingsManager.sync.kosyncUser) - 1);
            }
            if (doc["password"].is<const char*>()) {
                strncpy(settingsManager.sync.kosyncPass, doc["password"], sizeof(settingsManager.sync.kosyncPass) - 1);
            }
            if (doc["enabled"].is<bool>()) {
                settingsManager.sync.kosyncEnabled = doc["enabled"];
            }
            
            settingsManager.save();
            sendSuccess(r, "sync_settings_saved");
        });
    
    // GET /api/sync/test - Test sync connection
    _server.on("/api/sync/test", HTTP_GET, [this](AsyncWebServerRequest* r) {
        #if FEATURE_WIFI
        if (!settingsManager.sync.kosyncEnabled || strlen(settingsManager.sync.kosyncUrl) == 0) {
            sendError(r, "Sync not configured");
            return;
        }
        
        // Try to connect to sync server
        HTTPClient http;
        String url = String(settingsManager.sync.kosyncUrl) + "/healthcheck";
        http.begin(url);
        int code = http.GET();
        http.end();
        
        JsonDocument doc;
        if (code == 200) {
            doc["success"] = true;
            doc["message"] = "Connected to sync server";
        } else {
            doc["success"] = false;
            doc["error"] = "Server returned " + String(code);
        }
        
        String response;
        serializeJson(doc, response);
        AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        r->send(resp);
        #else
        sendError(r, "WiFi not available");
        #endif
    });
    
    // =========================================================================
    // Reading Statistics API
    // =========================================================================
    
    // GET /api/stats - Get reading statistics
    _server.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest* r) {
        JsonDocument doc;
        
        // Load stats from file
        File f = SD.open("/.sumi/reading_stats.bin", FILE_READ);
        if (f) {
            uint32_t magic, totalPages, totalMinutes, booksFinished;
            f.read((uint8_t*)&magic, 4);
            f.read((uint8_t*)&totalPages, 4);
            f.read((uint8_t*)&totalMinutes, 4);
            f.read((uint8_t*)&booksFinished, 4);
            f.close();
            
            if (magic == 0x53544154) {  // "STAT"
                doc["totalPagesRead"] = totalPages;
                doc["totalMinutesRead"] = totalMinutes;
                doc["totalHoursRead"] = totalMinutes / 60;
                doc["booksFinished"] = booksFinished;
            } else {
                doc["totalPagesRead"] = 0;
                doc["totalMinutesRead"] = 0;
                doc["totalHoursRead"] = 0;
                doc["booksFinished"] = 0;
            }
        } else {
            doc["totalPagesRead"] = 0;
            doc["totalMinutesRead"] = 0;
            doc["totalHoursRead"] = 0;
            doc["booksFinished"] = 0;
        }
        
        String response;
        serializeJson(doc, response);
        AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        r->send(resp);
    });
    
    // DELETE /api/stats - Reset reading statistics
    _server.on("/api/stats", HTTP_DELETE, [this](AsyncWebServerRequest* r) {
        SD.remove("/.sumi/reading_stats.bin");
        sendSuccess(r, "Statistics reset");
    });
    
    // =========================================================================
    // Reader Settings API
    // =========================================================================
    _server.on("/api/reader/settings", HTTP_GET, [this](AsyncWebServerRequest* r) {
        JsonDocument doc;
        doc["fontSize"] = settingsManager.reader.fontSize;
        doc["lineHeight"] = settingsManager.reader.lineHeight;
        doc["margins"] = settingsManager.reader.margins;
        doc["paraSpacing"] = settingsManager.reader.paraSpacing;
        doc["sceneBreakSpacing"] = settingsManager.reader.sceneBreakSpacing;
        doc["textAlign"] = settingsManager.reader.textAlign;
        doc["hyphenation"] = settingsManager.reader.hyphenation;
        doc["showProgress"] = settingsManager.reader.showProgress;
        doc["showChapter"] = settingsManager.reader.showChapter;
        doc["showPages"] = settingsManager.reader.showPages;
        doc["pageTurn"] = settingsManager.reader.pageTurn;
        doc["tapZones"] = settingsManager.reader.tapZones;
        
        String response;
        serializeJson(doc, response);
        AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        r->send(resp);
    });
    
    _server.on("/api/reader/settings", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            // Apply reader settings directly
            if (doc["fontSize"].is<int>()) settingsManager.reader.fontSize = doc["fontSize"];
            if (doc["lineHeight"].is<int>()) settingsManager.reader.lineHeight = doc["lineHeight"];
            if (doc["margins"].is<int>()) settingsManager.reader.margins = doc["margins"];
            if (doc["paraSpacing"].is<int>()) settingsManager.reader.paraSpacing = doc["paraSpacing"];
            if (doc["sceneBreakSpacing"].is<int>()) settingsManager.reader.sceneBreakSpacing = doc["sceneBreakSpacing"];
            if (doc["textAlign"].is<int>()) settingsManager.reader.textAlign = doc["textAlign"];
            if (doc["hyphenation"].is<bool>()) settingsManager.reader.hyphenation = doc["hyphenation"];
            if (doc["showProgress"].is<bool>()) settingsManager.reader.showProgress = doc["showProgress"];
            if (doc["showChapter"].is<bool>()) settingsManager.reader.showChapter = doc["showChapter"];
            if (doc["showPages"].is<bool>()) settingsManager.reader.showPages = doc["showPages"];
            if (doc["pageTurn"].is<int>()) settingsManager.reader.pageTurn = doc["pageTurn"];
            if (doc["tapZones"].is<int>()) settingsManager.reader.tapZones = doc["tapZones"];
            
            settingsManager.save();
            Serial.println("[WEB] Reader settings saved");
            sendSuccess(r, "reader_settings_saved");
        });
    
    // =========================================================================
    // Image Settings API
    // =========================================================================
    _server.on("/api/images/settings", HTTP_GET, [this](AsyncWebServerRequest* r) {
        JsonDocument doc;
        doc["grayscale"] = settingsManager.images.grayscale;
        doc["dither"] = settingsManager.images.dither;
        doc["contrast"] = settingsManager.images.contrast;
        doc["brightness"] = settingsManager.images.brightness;
        
        String response;
        serializeJson(doc, response);
        AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        r->send(resp);
    });
    
    _server.on("/api/images/settings", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            if (doc["grayscale"].is<bool>()) settingsManager.images.grayscale = doc["grayscale"];
            if (doc["dither"].is<bool>()) settingsManager.images.dither = doc["dither"];
            if (doc["contrast"].is<int>()) settingsManager.images.contrast = doc["contrast"];
            if (doc["brightness"].is<int>()) settingsManager.images.brightness = doc["brightness"];
            
            settingsManager.save();
            Serial.println("[WEB] Image settings saved");
            sendSuccess(r, "images_settings_saved");
        });
    
    // =========================================================================
    // Flashcard Management API
    // =========================================================================
    _server.on("/api/flashcards/settings", HTTP_GET, [this](AsyncWebServerRequest* r) {
        JsonDocument doc;
        doc["newPerDay"] = settingsManager.flashcards.newPerDay;
        doc["reviewLimit"] = settingsManager.flashcards.reviewLimit;
        doc["retention"] = settingsManager.flashcards.retention;
        doc["useFsrs"] = settingsManager.flashcards.useFsrs;
        doc["showTimer"] = settingsManager.flashcards.showTimer;
        doc["autoFlip"] = settingsManager.flashcards.autoFlip;
        doc["shuffle"] = settingsManager.flashcards.shuffle;
        
        String response;
        serializeJson(doc, response);
        AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        r->send(resp);
    });
    
    _server.on("/api/flashcards/settings", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            if (doc["newPerDay"].is<int>()) settingsManager.flashcards.newPerDay = doc["newPerDay"];
            if (doc["reviewLimit"].is<int>()) settingsManager.flashcards.reviewLimit = doc["reviewLimit"];
            if (doc["retention"].is<int>()) settingsManager.flashcards.retention = doc["retention"];
            if (doc["useFsrs"].is<bool>()) settingsManager.flashcards.useFsrs = doc["useFsrs"];
            if (doc["showTimer"].is<bool>()) settingsManager.flashcards.showTimer = doc["showTimer"];
            if (doc["autoFlip"].is<bool>()) settingsManager.flashcards.autoFlip = doc["autoFlip"];
            if (doc["shuffle"].is<bool>()) settingsManager.flashcards.shuffle = doc["shuffle"];
            
            settingsManager.save();
            Serial.println("[WEB] Flashcard settings saved");
            sendSuccess(r, "flashcards_settings_saved");
        });
    
    // Create new flashcard deck
    _server.on("/api/flashcards/create", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            const char* name = doc["name"];
            if (!name || strlen(name) == 0) {
                sendError(r, "Deck name required");
                return;
            }
            
            // Security: Prevent directory traversal
            if (strstr(name, "..") != nullptr || strstr(name, "/") != nullptr) {
                sendError(r, "Invalid deck name");
                return;
            }
            
            String path = String("/flashcards/") + name + ".json";
            
            // Check if already exists
            if (SD.exists(path)) {
                sendError(r, "Deck already exists");
                return;
            }
            
            File file = SD.open(path, FILE_WRITE);
            if (!file) {
                sendError(r, "Failed to create deck");
                return;
            }
            
            file.println("{\"name\":\"" + String(name) + "\",\"cards\":[],\"created\":" + String(millis()/1000) + ",\"modified\":" + String(millis()/1000) + "}");
            file.close();
            
            Serial.printf("[WEB] Created flashcard deck: %s\n", path.c_str());
            sendSuccess(r, "deck_created");
        });
    
    // Export flashcard deck (returns tab-separated text format)
    _server.on("/api/flashcards/export", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!r->hasParam("deck")) {
            sendError(r, "Deck name required");
            return;
        }
        
        String deckName = r->getParam("deck")->value();
        String path = "/flashcards/" + deckName;
        
        // Add .json extension if missing
        if (!path.endsWith(".json")) {
            path += ".json";
        }
        
        if (!SD.exists(path)) {
            sendError(r, "Deck not found");
            return;
        }
        
        File file = SD.open(path, FILE_READ);
        if (!file) {
            sendError(r, "Failed to open deck");
            return;
        }
        
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();
        
        if (err) {
            sendError(r, "Invalid deck format");
            return;
        }
        
        String output;
        JsonArray cards = doc["cards"].as<JsonArray>();
        for (JsonObject card : cards) {
            output += card["front"].as<String>() + "\t" + card["back"].as<String>() + "\n";
        }
        
        // Remove .json for download filename
        String downloadName = deckName;
        downloadName.replace(".json", "");
        
        AsyncWebServerResponse* resp = r->beginResponse(200, "text/plain", output);
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + downloadName + ".txt\"");
        addCORSHeaders(resp);
        r->send(resp);
    });
    
    // Import flashcard deck (Anki/text format - tab separated)
    _server.on("/api/flashcards/import", HTTP_POST,
        [this](AsyncWebServerRequest* r) { 
            sendSuccess(r, "import_complete");
        },
        [this](AsyncWebServerRequest* r, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
            static String importBuffer;
            static String importFilename;
            
            if (index == 0) {
                importBuffer = "";
                importFilename = filename;
                Serial.printf("[WEB] Importing flashcards from: %s\n", filename.c_str());
            }
            
            // Accumulate data
            for (size_t i = 0; i < len; i++) {
                importBuffer += (char)data[i];
            }
            
            if (final) {
                JsonDocument doc;
                JsonArray cards = doc["cards"].to<JsonArray>();
                
                // Parse text format (tab-separated: front\tback)
                int start = 0;
                int cardCount = 0;
                for (size_t i = 0; i <= importBuffer.length(); i++) {
                    if (i == importBuffer.length() || importBuffer[i] == '\n') {
                        String line = importBuffer.substring(start, i);
                        line.trim();
                        
                        int tabPos = line.indexOf('\t');
                        if (tabPos > 0) {
                            JsonObject card = cards.add<JsonObject>();
                            card["front"] = line.substring(0, tabPos);
                            card["back"] = line.substring(tabPos + 1);
                            card["due"] = 0;
                            card["interval"] = 0;
                            card["ease"] = 2.5;
                            cardCount++;
                        }
                        start = i + 1;
                    }
                }
                
                doc["created"] = millis() / 1000;
                doc["modified"] = millis() / 1000;
                
                // Generate deck path from filename
                String deckPath = "/flashcards/" + importFilename;
                deckPath.replace(".txt", ".json");
                deckPath.replace(".csv", ".json");
                deckPath.replace(".apkg", ".json");
                if (!deckPath.endsWith(".json")) {
                    deckPath += ".json";
                }
                
                // Extract deck name from filename
                String deckName = importFilename;
                deckName.replace(".txt", "");
                deckName.replace(".csv", "");
                deckName.replace(".apkg", "");
                deckName.replace(".json", "");
                doc["name"] = deckName;
                
                File file = SD.open(deckPath, FILE_WRITE);
                if (file) {
                    serializeJson(doc, file);
                    file.close();
                    Serial.printf("[WEB] Imported %d cards to %s\n", cardCount, deckPath.c_str());
                } else {
                    Serial.println("[WEB] Failed to save imported deck");
                }
                
                importBuffer = "";
            }
        });
    
    // =========================================================================
    // Notes API
    // =========================================================================
    _server.on("/api/notes/create", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            const char* name = doc["name"];
            if (!name || strlen(name) == 0) {
                sendError(r, "Note name required");
                return;
            }
            
            // Security: Prevent directory traversal
            if (strstr(name, "..") != nullptr || strstr(name, "/") != nullptr) {
                sendError(r, "Invalid note name");
                return;
            }
            
            String path = String("/notes/") + name + ".txt";
            
            if (SD.exists(path)) {
                sendError(r, "Note already exists");
                return;
            }
            
            File file = SD.open(path, FILE_WRITE);
            if (!file) {
                sendError(r, "Failed to create note");
                return;
            }
            
            file.print("");  // Empty note
            file.close();
            
            Serial.printf("[WEB] Created note: %s\n", path.c_str());
            sendSuccess(r, "note_created");
        });
    
    // =========================================================================
    // Bluetooth API
    // =========================================================================
    _server.on("/api/bluetooth/enable", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            bool enabled = doc["enabled"] | false;
            
            #if FEATURE_BLUETOOTH
                bluetoothManager.setEnabled(enabled);
                settingsManager.bluetooth.enabled = enabled;
                settingsManager.save();
                Serial.printf("[WEB] Bluetooth %s\n", enabled ? "enabled" : "disabled");
                sendSuccess(r, enabled ? "bluetooth_enabled" : "bluetooth_disabled");
            #else
                // Bluetooth not compiled in - still save the setting
                settingsManager.bluetooth.enabled = enabled;
                settingsManager.save();
                sendSuccess(r, enabled ? "bluetooth_enabled" : "bluetooth_disabled");
            #endif
        });
    
    _server.on("/api/bluetooth/devices", HTTP_GET, [this](AsyncWebServerRequest* r) {
        JsonDocument doc;
        JsonArray paired = doc["paired"].to<JsonArray>();
        JsonArray discovered = doc["discovered"].to<JsonArray>();
        
        #if FEATURE_BLUETOOTH
            bluetoothManager.getPairedDevices(paired);
            bluetoothManager.getDiscoveredDevices(discovered);
            doc["enabled"] = bluetoothManager.isEnabled();
            doc["scanning"] = bluetoothManager.isScanning();
            doc["connected"] = bluetoothManager.hasConnectedKeyboard();
            doc["available"] = true;
        #else
            doc["enabled"] = settingsManager.bluetooth.enabled;
            doc["scanning"] = false;
            doc["connected"] = false;
            doc["available"] = false;
        #endif
        
        String response;
        serializeJson(doc, response);
        AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        r->send(resp);
    });
    
    _server.on("/api/bluetooth/scan", HTTP_POST, [this](AsyncWebServerRequest* r) {
        #if FEATURE_BLUETOOTH
            if (!bluetoothManager.isEnabled()) {
                sendError(r, "Bluetooth not enabled");
                return;
            }
            if (bluetoothManager.isScanning()) {
                sendSuccess(r, "scan_already_running");
                return;
            }
            bluetoothManager.startScan(10000);  // 10 second scan
            sendSuccess(r, "scan_started");
        #else
            sendError(r, "Bluetooth not available");
        #endif
    });
    
    _server.on("/api/bluetooth/pair", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            const char* address = doc["address"];
            if (!address) {
                sendError(r, "Address required");
                return;
            }
            
            #if FEATURE_BLUETOOTH
                if (!bluetoothManager.isEnabled()) {
                    sendError(r, "Bluetooth not enabled");
                    return;
                }
                bool success = bluetoothManager.pair(address);
                if (success) {
                    sendSuccess(r, "paired");
                } else {
                    sendError(r, "Pairing failed");
                }
            #else
                sendError(r, "Bluetooth not available");
            #endif
        });
    
    _server.on("/api/bluetooth/connect", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            
            const char* address = doc["address"];
            if (!address) {
                sendError(r, "Address required");
                return;
            }
            
            #if FEATURE_BLUETOOTH
                if (!bluetoothManager.isEnabled()) {
                    sendError(r, "Bluetooth not enabled");
                    return;
                }
                bool success = bluetoothManager.connect(address);
                if (success) {
                    sendSuccess(r, "connected");
                } else {
                    sendError(r, "Connection failed");
                }
            #else
                sendError(r, "Bluetooth not available");
            #endif
        });
    
    _server.on("/api/bluetooth/disconnect", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                // No body is OK for disconnect
            }
            
            const char* address = doc["address"];
            
            #if FEATURE_BLUETOOTH
                if (address) {
                    bluetoothManager.disconnect(address);
                } else if (bluetoothManager.hasConnectedKeyboard()) {
                    // Disconnect current connected device
                    bluetoothManager.disconnect(bluetoothManager.getConnectedKeyboardName());
                }
                sendSuccess(r, "disconnected");
            #else
                sendSuccess(r, "disconnected");
            #endif
        });
    
    // Keyboard layout setting
    _server.on("/api/bluetooth/layout", HTTP_POST, [](AsyncWebServerRequest* r){}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                sendError(r, "Invalid JSON");
                return;
            }
            const char* layout = doc["layout"];
            if (layout) {
                if (strcmp(layout, "us") == 0) settingsManager.bluetooth.keyboardLayout = 0;
                else if (strcmp(layout, "uk") == 0) settingsManager.bluetooth.keyboardLayout = 1;
                else if (strcmp(layout, "de") == 0) settingsManager.bluetooth.keyboardLayout = 2;
                else if (strcmp(layout, "fr") == 0) settingsManager.bluetooth.keyboardLayout = 3;
                else if (strcmp(layout, "es") == 0) settingsManager.bluetooth.keyboardLayout = 4;
                else if (strcmp(layout, "jp") == 0) settingsManager.bluetooth.keyboardLayout = 5;
                settingsManager.save();
                Serial.printf("[WEB] Keyboard layout set to: %s\n", layout);
            }
            sendSuccess(r, "layout_updated");
        });
}

void SumiWebServer::setupPageRoutes() {
    // Main page - serve embedded portal from flash (no SD needed)
    // Serve portal HTML using chunked response to avoid memory issues
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        // Use chunked response - sends data in small pieces without large RAM allocation
        size_t contentLen = strlen_P(PORTAL_HTML);
        AsyncWebServerResponse* response = r->beginChunkedResponse("text/html", 
            [](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
                // Copy chunk from PROGMEM
                size_t totalLen = strlen_P(PORTAL_HTML);
                if (index >= totalLen) return 0;  // Done
                
                size_t remaining = totalLen - index;
                size_t toSend = (remaining < maxLen) ? remaining : maxLen;
                
                memcpy_P(buffer, PORTAL_HTML + index, toSend);
                return toSend;
            });
        response->setContentLength(contentLen);
        r->send(response);
    });
}

// =============================================================================
// Status API
// =============================================================================
void SumiWebServer::handleStatus(AsyncWebServerRequest* request) {
    // ArduinoJson v7 compatible
    JsonDocument doc;
    
    doc["device"] = "Sumi";
    doc["firmware"] = SUMI_VERSION;
    doc["variant"] = SUMI_VARIANT;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["setupComplete"] = settingsManager.isSetupComplete();
    
    JsonObject bat = doc["battery"].to<JsonObject>();
    bat["percent"] = batteryMonitor.getPercent();
    bat["voltage"] = batteryMonitor.getVoltage();
    
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = wifiManager.isConnected();
    wifi["ssid"] = wifiManager.getSSID();
    wifi["ip"] = wifiManager.getIP();
    wifi["ap_mode"] = wifiManager.isAPMode();
    
    JsonObject stor = doc["storage"].to<JsonObject>();
    stor["sd_present"] = SD.cardSize() > 0;
    if (SD.cardSize() > 0) {
        stor["sd_total_mb"] = SD.cardSize() / (1024 * 1024);
    }
    
    // Add feature flags so portal can hide disabled features
    JsonObject features = doc["features"].to<JsonObject>();
    features["reader"] = FEATURE_READER;
    features["flashcards"] = FEATURE_FLASHCARDS;
    features["weather"] = FEATURE_WEATHER;
    features["lockscreen"] = FEATURE_LOCKSCREEN;
    features["bluetooth"] = FEATURE_BLUETOOTH;
    features["games"] = FEATURE_GAMES;
    
    String response;
    serializeJson(doc, response);
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    addCORSHeaders(resp);
    request->send(resp);
}

// =============================================================================
// Settings API
// =============================================================================
void SumiWebServer::handleGetSettings(AsyncWebServerRequest* request) {
    JsonDocument doc;
    settingsManager.toJSON(doc.to<JsonObject>());
    
    String response;
    serializeJson(doc, response);
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    addCORSHeaders(resp);
    request->send(resp);
}

void SumiWebServer::handleSaveSettings(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        sendError(request, "Invalid JSON");
        return;
    }
    settingsManager.fromJSON(doc.as<JsonObjectConst>());
    
    // Mark setup as complete and save
    settingsManager.setSetupComplete(true);
    settingsManager.save();
    
    // Signal main loop to exit setup mode and refresh display
    g_settingsDeployed = true;
    Serial.println("[WEB] Settings deployed - exiting setup mode");
    
    sendSuccess(request, "deployed");
}

// =============================================================================
// Setup Complete
// =============================================================================
void SumiWebServer::handleSetupComplete(AsyncWebServerRequest* request) {
    settingsManager.setSetupComplete(true);
    settingsManager.save();
    Serial.println("[WEB] Setup marked complete");
    sendSuccess(request, "setup_complete");
}

// =============================================================================
// WiFi API
// =============================================================================
void SumiWebServer::handleWiFiScan(AsyncWebServerRequest* request) {
    Serial.println("[WEB] Starting WiFi scan...");
    int n = WiFi.scanNetworks();
    
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    for (int i = 0; i < n && i < 15; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    
    doc["count"] = n;
    
    String response;
    serializeJson(doc, response);
    WiFi.scanDelete();
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    addCORSHeaders(resp);
    request->send(resp);
}

void SumiWebServer::handleWiFiConnect(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        sendError(request, "Invalid JSON");
        return;
    }
    
    const char* ssid = doc["ssid"];
    const char* password = doc["password"];
    
    if (!ssid) {
        sendError(request, "Missing SSID");
        return;
    }
    
    Serial.printf("[WEB] Saving credentials for: %s\n", ssid);
    
    // Save credentials to both settings and WiFiManager (for NVS persistence)
    settingsManager.addWiFiNetwork(ssid, password ? password : "");
    settingsManager.save();
    wifiManager.saveCredentials(ssid, password ? password : "");
    
    // Respond IMMEDIATELY with "credentials saved" - don't wait for connection
    // This prevents browser timeout issues when switching networks
    JsonDocument respDoc;
    respDoc["status"] = "credentials_saved";
    respDoc["message"] = "WiFi credentials saved! Device will connect automatically.";
    respDoc["ssid"] = ssid;
    String response;
    serializeJson(respDoc, response);
    AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
    addCORSHeaders(r);
    request->send(r);
    
    Serial.println("[WEB] Credentials saved, attempting connection...");
    
    // Try to connect AFTER sending response
    delay(100);  // Let response send
    bool success = wifiManager.connect(ssid, password ? password : "");
    
    if (success) {
        // Fetch timezone from IP location API
        Serial.println("[WEB] Fetching timezone from IP...");
        WiFiClient client;
        if (client.connect("ip-api.com", 80)) {
            client.print("GET /json/?fields=status,offset HTTP/1.1\r\n");
            client.print("Host: ip-api.com\r\n");
            client.print("Connection: close\r\n\r\n");
            
            unsigned long timeout = millis();
            while (client.available() == 0 && millis() - timeout < 5000) {
                delay(10);
            }
            
            // Skip headers
            while (client.available()) {
                String line = client.readStringUntil('\n');
                if (line == "\r") break;
            }
            
            String payload = client.readString();
            client.stop();
            
            JsonDocument tzDoc;
            if (!deserializeJson(tzDoc, payload)) {
                const char* status = tzDoc["status"] | "fail";
                if (strcmp(status, "success") == 0) {
                    int32_t tzOffset = tzDoc["offset"] | 0;
                    settingsManager.weather.timezoneOffset = tzOffset;
                    settingsManager.markDirty();
                    Serial.printf("[WEB] Timezone offset: %d seconds\n", tzOffset);
                }
            }
        }
        
        // Sync time via NTP with timezone offset
        Serial.println("[WEB] Syncing time via NTP...");
        int32_t tzOffset = settingsManager.weather.timezoneOffset;
        configTime(tzOffset, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000)) {
            wifiManager.setTimeSynced(true);
            char timeStr[32];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            Serial.printf("[WEB] Time synced (local): %s\n", timeStr);
        }
        
        // Signal main loop to exit setup mode
        g_wifiJustConnected = true;
        Serial.println("[WEB] WiFi connected successfully!");
    } else {
        Serial.println("[WEB] WiFi connection failed - credentials still saved for retry");
    }
}

void SumiWebServer::handleWiFiDisconnect(AsyncWebServerRequest* request) {
    wifiManager.disconnect();
    sendSuccess(request, "disconnected");
}

// =============================================================================
// File API - Basic
// =============================================================================
void SumiWebServer::handleFileList(AsyncWebServerRequest* request) {
    String path = "/";
    if (request->hasParam("path")) {
        path = request->getParam("path")->value();
    }
    
    JsonDocument doc;
    JsonArray files = doc["files"].to<JsonArray>();
    
    File root = SD.open(path);
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            JsonObject f = files.add<JsonObject>();
            f["name"] = String(file.name());
            f["size"] = file.size();
            f["dir"] = file.isDirectory();
            file = root.openNextFile();
        }
    }
    
    doc["path"] = path;
    
    String response;
    serializeJson(doc, response);
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    addCORSHeaders(resp);
    request->send(resp);
}

void SumiWebServer::handleFileUpload(AsyncWebServerRequest* request, const String& filename,
                                      size_t index, uint8_t* data, size_t len, bool final) {
    static File uploadFile;
    String path = "/";
    
    if (request->hasParam("path", true)) {
        path = request->getParam("path", true)->value();
    }
    
    String fullPath = path + "/" + filename;
    
    if (index == 0) {
        Serial.printf("[WEB] Upload start: %s\n", fullPath.c_str());
        uploadFile = SD.open(fullPath, FILE_WRITE);
    }
    
    if (uploadFile) {
        uploadFile.write(data, len);
    }
    
    if (final) {
        if (uploadFile) {
            uploadFile.close();
        }
        Serial.printf("[WEB] Upload complete: %d bytes\n", index + len);
    }
}

// =============================================================================
// System API
// =============================================================================
void SumiWebServer::handleReboot(AsyncWebServerRequest* request) {
    sendSuccess(request, "rebooting");
    delay(500);
    ESP.restart();
}

// =============================================================================
// Helpers
// =============================================================================
// Weather API
// =============================================================================
void SumiWebServer::handleWeatherLocation(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        sendError(request, "Invalid JSON");
        return;
    }
    
    // Check if clearing location
    if (doc["clear"].as<bool>()) {
        settingsManager.weather.latitude = 0;
        settingsManager.weather.longitude = 0;
        settingsManager.weather.location[0] = '\0';
        settingsManager.weather.zipCode[0] = '\0';
        settingsManager.markDirty();
        settingsManager.save();
        
        JsonDocument resp;
        resp["success"] = true;
        String response;
        serializeJson(resp, response);
        AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
        addCORSHeaders(r);
        request->send(r);
        return;
    }
    
    const char* zipCode = doc["zipCode"];
    if (!zipCode || strlen(zipCode) != 5) {
        sendError(request, "Invalid ZIP code");
        return;
    }
    
    Serial.printf("[WEB] Looking up ZIP: %s\n", zipCode);
    
    // Lookup ZIP code via zippopotam.us API
    WiFiClient client;
    if (!client.connect("api.zippopotam.us", 80)) {
        sendError(request, "Could not connect to location service");
        return;
    }
    
    char url[64];
    snprintf(url, sizeof(url), "GET /us/%s HTTP/1.0", zipCode);
    client.println(url);
    client.println("Host: api.zippopotam.us");
    client.println("Connection: close");
    client.println();
    
    unsigned long timeout = millis() + 5000;
    while (!client.available() && millis() < timeout) {
        delay(10);
    }
    
    if (!client.available()) {
        client.stop();
        sendError(request, "Location service timeout");
        return;
    }
    
    // Skip HTTP headers
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }
    
    String payload = client.readString();
    client.stop();
    
    JsonDocument zipDoc;
    if (deserializeJson(zipDoc, payload)) {
        sendError(request, "Invalid ZIP code or not found");
        return;
    }
    
    // Extract location data
    JsonArray places = zipDoc["places"].as<JsonArray>();
    if (places.size() == 0) {
        sendError(request, "ZIP code not found");
        return;
    }
    
    JsonObject place = places[0];
    float lat = place["latitude"].as<String>().toFloat();
    float lon = place["longitude"].as<String>().toFloat();
    const char* city = place["place name"];
    const char* state = place["state abbreviation"];
    
    // Save to settings
    settingsManager.weather.latitude = lat;
    settingsManager.weather.longitude = lon;
    snprintf(settingsManager.weather.location, sizeof(settingsManager.weather.location), 
             "%s, %s", city, state);
    strncpy(settingsManager.weather.zipCode, zipCode, sizeof(settingsManager.weather.zipCode) - 1);
    settingsManager.markDirty();
    settingsManager.save();
    
    Serial.printf("[WEB] Location saved: %s (%.4f, %.4f)\n", 
                  settingsManager.weather.location, lat, lon);
    
    // Send response
    JsonDocument resp;
    resp["success"] = true;
    resp["location"] = settingsManager.weather.location;
    resp["latitude"] = lat;
    resp["longitude"] = lon;
    String response;
    serializeJson(resp, response);
    AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
    addCORSHeaders(r);
    request->send(r);
}

void SumiWebServer::handleWeatherUnit(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        sendError(request, "Invalid JSON");
        return;
    }
    
    bool celsius = doc["celsius"].as<bool>();
    settingsManager.weather.celsius = celsius;
    settingsManager.markDirty();
    settingsManager.save();
    
    Serial.printf("[WEB] Weather unit set to: %s\n", celsius ? "Celsius" : "Fahrenheit");
    
    JsonDocument resp;
    resp["success"] = true;
    String response;
    serializeJson(resp, response);
    AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
    addCORSHeaders(r);
    request->send(r);
}

void SumiWebServer::handleTimezone(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        sendError(request, "Invalid JSON");
        return;
    }
    
    if (doc["auto"].as<bool>()) {
        // Clear manual timezone - will auto-detect on next sync
        settingsManager.weather.timezoneOffset = 0;
        Serial.println("[WEB] Timezone set to auto-detect");
    } else if (doc["offset"].is<int>()) {
        int32_t offset = doc["offset"].as<int32_t>();
        settingsManager.weather.timezoneOffset = offset;
        
        // Apply immediately
        configTime(offset, 0, "pool.ntp.org", "time.nist.gov");
        
        Serial.printf("[WEB] Timezone offset set to: %d seconds\n", offset);
    }
    
    settingsManager.markDirty();
    settingsManager.save();
    
    JsonDocument resp;
    resp["success"] = true;
    String response;
    serializeJson(resp, response);
    AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
    addCORSHeaders(r);
    request->send(r);
}

// =============================================================================
// Helper Functions
// =============================================================================
void SumiWebServer::sendError(AsyncWebServerRequest* request, const char* message, int code) {
    JsonDocument doc;
    doc["status"] = "error";
    doc["message"] = message;
    String response;
    serializeJson(doc, response);
    AsyncWebServerResponse* resp = request->beginResponse(code, "application/json", response);
    addCORSHeaders(resp);
    request->send(resp);
}

void SumiWebServer::sendSuccess(AsyncWebServerRequest* request, const char* message) {
    JsonDocument doc;
    doc["status"] = message;
    String response;
    serializeJson(doc, response);
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    addCORSHeaders(resp);
    request->send(resp);
}

void SumiWebServer::addCORSHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

#endif // FEATURE_WEBSERVER
