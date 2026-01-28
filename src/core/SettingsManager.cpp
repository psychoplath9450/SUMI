#include "core/SettingsManager.h"
#include "core/ReaderSettings.h"
#include <SD.h>

// External reader settings manager (used by Library)
extern ReaderSettingsManager readerSettings;

SettingsManager settingsManager;

// =============================================================================
// Helper: Clamp value to range
// =============================================================================
template<typename T>
T clampValue(T val, T minVal, T maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

// =============================================================================
// Constructor
// =============================================================================
SettingsManager::SettingsManager() : _dirty(false), _lastChange(0), setupComplete(false), themeIndex(0) {
    loadDefaults();
}

// =============================================================================
// Lifecycle
// =============================================================================
void SettingsManager::begin() {
    Serial.println("[SETTINGS] ===== SettingsManager::begin() =====");
    Serial.printf("[SETTINGS] Expected SETTINGS_VERSION: %d\n", SETTINGS_VERSION);
    load();
    Serial.printf("[SETTINGS] After load: setupComplete=%s\n", setupComplete ? "true" : "false");
}

void SettingsManager::loadDefaults() {
    Serial.println("[SETTINGS] ===== loadDefaults() - RESETTING ALL SETTINGS =====");
    
    // Force setup on defaults
    setupComplete = false;
    Serial.println("[SETTINGS] setupComplete set to FALSE");
    
    // Display defaults
    display.rotation = 0;
    display.sleepMinutes = 15;
    display.fullRefreshPages = 10;
    display.deepSleep = true;
    display.showBatteryHome = true;
    display.showBatterySleep = true;
    display.showClockHome = true;
    display.showDate = true;
    display.showWifi = false;
    display.sleepStyle = 0;  // 0=Book Cover (current), 1=Shuffle Images, 2=Wake Me Up
    display.clockStyle = 0;  // Digital
    display.homeLayout = 0;  // Grid
    display.invertColors = false;
    
    // Lock screen defaults
    display.lockStyle = 0;        // Clock
    display.lockPhotoSource = 0;  // Shuffle
    display.showBatteryLock = true;
    display.showWeatherLock = false;
    
    // Sleep screen settings
    display.sleepPhotoSource = 0; // Shuffle (legacy)
    memset(display.sleepSelectedImage, 0, sizeof(display.sleepSelectedImage));
    
    // === PORTAL CUSTOMIZATION - DEFAULT TO VERTICAL ===
    display.orientation = 1;     // 0=horizontal, 1=vertical (DEFAULT VERTICAL)
    display.buttonShape = 0;     // 0=rounded, 1=pill, 2=square
    display.fontStyle = 0;       // 0=sans, 1=serif, 2=mono
    display.bgTheme = 0;         // 0=light, 1=gray, 2=sepia, 3=dark
    display.accentColor = 0;     // 0=orange, 1=blue, 2=green, 3=purple, 4=red, 5=teal
    display.hItemsPerRow = 4;    // Horizontal mode: 3-5 items per row
    display.vItemsPerRow = 2;    // Vertical mode: 2-3 items per row
    
    // Display performance defaults
    display.refreshMode = 0;        // Auto
    display.transitionStyle = 0;    // None
    display.ghostingThreshold = 20;
    display.partialQuality = 2;
    display.enableDirtyRects = true;
    display.enableTransitions = true;
    display.readingModeQuality = true;
    
    // Additional portal settings
    display.showStatusBar = true;
    display.fontSize = 12;
    display.sleepRefresh = false;
    display.wakeButton = 0;  // Any button
    display.bootToLastBook = false;  // Default to showing home screen
    
    // Widget visibility defaults - book and weather enabled, orient disabled
    display.showBookWidget = true;
    display.showWeatherWidget = true;
    display.showOrientWidget = false;
    
    // Reader defaults
    reader.fontSize = 18;
    reader.lineHeight = 150;
    reader.margins = 20;
    reader.paraSpacing = 10;
    reader.sceneBreakSpacing = 30;  // Extra spacing for scene breaks
    reader.textAlign = 1;
    reader.hyphenation = true;
    reader.showProgress = true;
    reader.showChapter = true;
    reader.showPages = true;
    reader.pageTurn = 0;
    reader.tapZones = 0;
    
    // Flashcard defaults
    flashcards.newPerDay = 20;
    flashcards.reviewLimit = 200;
    flashcards.retention = 90;
    flashcards.useFsrs = true;
    flashcards.showTimer = false;
    flashcards.autoFlip = false;
    flashcards.shuffle = true;
    flashcards.fontSize = 1;          // Medium
    flashcards.centerText = true;     // Center by default
    flashcards.showProgressBar = true;
    flashcards.showStats = true;
    
    // Weather defaults
    weather.latitude = 0;
    weather.longitude = 0;
    memset(weather.location, 0, sizeof(weather.location));
    strcpy(weather.location, "New York");
    weather.celsius = false;
    weather.updateHours = 1;
    weather.timezoneOffset = 0;  // UTC default
    
    // Sync defaults
    memset(sync.kosyncUrl, 0, sizeof(sync.kosyncUrl));
    memset(sync.kosyncUser, 0, sizeof(sync.kosyncUser));
    memset(sync.kosyncPass, 0, sizeof(sync.kosyncPass));
    sync.kosyncEnabled = false;
    
    // Bluetooth defaults
    bluetooth.enabled = false;
    bluetooth.autoConnect = true;
    bluetooth.keyboardLayout = 0;
    bluetooth.showInStatusBar = true;
    
    // Image settings defaults
    images.grayscale = true;
    images.dither = true;
    images.contrast = 128;
    images.brightness = 128;
    
    // API defaults
    memset(api.stockApiKey, 0, sizeof(api.stockApiKey));
    memset(api.stockSymbols, 0, sizeof(api.stockSymbols));
    memset(api.newsApiKey, 0, sizeof(api.newsApiKey));
    api.newsCategory = 0;
    
    enabledPlugins = PLUGINS_DEFAULT;
    setupComplete = false;
    themeIndex = 0;
    pluginOrderCount = 0;
    memset(pluginOrder, 0, sizeof(pluginOrder));
    getDefaultHomeItems(homeScreenEnabled);
    loadWiFiDefaults();
}

void SettingsManager::loadWiFiDefaults() {
    memset(&wifi, 0, sizeof(wifi));
    wifi.preferredIndex = 0;
    wifi.savedCount = 0;
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        wifi.networks[i].isActive = false;
        wifi.networks[i].lastRSSI = 0;
    }
}

void SettingsManager::reset() {
    loadDefaults();
    save();
}

// =============================================================================
// Save
// =============================================================================
void SettingsManager::save() {
    _prefs.begin("sumi", false);
    
    _prefs.putUChar("version", SETTINGS_VERSION);
    _prefs.putString("fw_version", SUMI_VERSION);  // Store firmware version
    _prefs.putBool("setup_done", setupComplete);
    _prefs.putUChar("theme", themeIndex);
    _prefs.putBytes("home_items", homeScreenEnabled, HOME_ITEMS_BYTES);
    
    // Display - Basic
    _prefs.putUChar("d_rot", display.rotation);
    _prefs.putUChar("d_sleep", display.sleepMinutes);
    _prefs.putUChar("d_refr", display.fullRefreshPages);
    _prefs.putBool("d_deep", display.deepSleep);
    _prefs.putBool("d_bath", display.showBatteryHome);
    _prefs.putBool("d_bats", display.showBatterySleep);
    _prefs.putBool("d_clkh", display.showClockHome);
    _prefs.putBool("d_date", display.showDate);
    _prefs.putBool("d_wifi", display.showWifi);
    _prefs.putUChar("d_slps", display.sleepStyle);
    _prefs.putUChar("d_clks", display.clockStyle);
    _prefs.putUChar("d_home", display.homeLayout);
    _prefs.putBool("d_inv", display.invertColors);
    
    // === LOCK SCREEN ===
    _prefs.putUChar("d_lcks", display.lockStyle);
    _prefs.putUChar("d_lckp", display.lockPhotoSource);
    _prefs.putBool("d_batl", display.showBatteryLock);
    _prefs.putBool("d_wthl", display.showWeatherLock);
    _prefs.putUChar("d_slpp", display.sleepPhotoSource);
    
    // === PORTAL CUSTOMIZATION ===
    _prefs.putUChar("d_orient", display.orientation);
    _prefs.putUChar("d_btnshp", display.buttonShape);
    _prefs.putUChar("d_font", display.fontStyle);
    _prefs.putUChar("d_bgthm", display.bgTheme);
    _prefs.putUChar("d_accent", display.accentColor);
    _prefs.putUChar("d_hrow", display.hItemsPerRow);
    _prefs.putUChar("d_vrow", display.vItemsPerRow);
    
    // Performance
    _prefs.putUChar("d_rmod", display.refreshMode);
    _prefs.putUChar("d_trans", display.transitionStyle);
    _prefs.putUChar("d_ghost", display.ghostingThreshold);
    _prefs.putUChar("d_qual", display.partialQuality);
    _prefs.putBool("d_dirty", display.enableDirtyRects);
    _prefs.putBool("d_tren", display.enableTransitions);
    _prefs.putBool("d_rqual", display.readingModeQuality);
    
    // Additional portal settings
    _prefs.putBool("d_statbar", display.showStatusBar);
    _prefs.putUChar("d_fsize", display.fontSize);
    _prefs.putBool("d_slpref", display.sleepRefresh);
    _prefs.putUChar("d_wake", display.wakeButton);
    // Widget visibility
    _prefs.putBool("d_wbook", display.showBookWidget);
    _prefs.putBool("d_wwth", display.showWeatherWidget);
    _prefs.putBool("d_wori", display.showOrientWidget);
    
    // Reader
    _prefs.putUChar("r_fs", reader.fontSize);
    _prefs.putUChar("r_lh", reader.lineHeight);
    _prefs.putUChar("r_mg", reader.margins);
    _prefs.putUChar("r_ps", reader.paraSpacing);
    _prefs.putUChar("r_al", reader.textAlign);
    _prefs.putBool("r_hyp", reader.hyphenation);
    _prefs.putBool("r_prog", reader.showProgress);
    _prefs.putBool("r_chap", reader.showChapter);
    _prefs.putBool("r_pgs", reader.showPages);
    _prefs.putUChar("r_pt", reader.pageTurn);
    _prefs.putUChar("r_tap", reader.tapZones);
    
    // Flashcards
    _prefs.putUChar("fc_new", flashcards.newPerDay);
    _prefs.putUShort("fc_rev", flashcards.reviewLimit);
    _prefs.putUChar("fc_ret", flashcards.retention);
    _prefs.putBool("fc_fsrs", flashcards.useFsrs);
    _prefs.putBool("fc_tim", flashcards.showTimer);
    _prefs.putBool("fc_af", flashcards.autoFlip);
    _prefs.putBool("fc_sh", flashcards.shuffle);
    _prefs.putUChar("fc_fsize", flashcards.fontSize);
    _prefs.putBool("fc_cen", flashcards.centerText);
    _prefs.putBool("fc_prog", flashcards.showProgressBar);
    _prefs.putBool("fc_stat", flashcards.showStats);
    
    // Weather
    _prefs.putFloat("w_lat", weather.latitude);
    _prefs.putFloat("w_lon", weather.longitude);
    _prefs.putString("w_loc", weather.location);
    _prefs.putBool("w_c", weather.celsius);
    _prefs.putUChar("w_upd", weather.updateHours);
    _prefs.putLong("w_tz", weather.timezoneOffset);
    
    // Sync
    _prefs.putString("s_url", sync.kosyncUrl);
    _prefs.putString("s_user", sync.kosyncUser);
    _prefs.putString("s_pass", sync.kosyncPass);
    _prefs.putBool("s_en", sync.kosyncEnabled);
    
    // Bluetooth
    _prefs.putBool("bt_en", bluetooth.enabled);
    _prefs.putBool("bt_auto", bluetooth.autoConnect);
    _prefs.putUChar("bt_lay", bluetooth.keyboardLayout);
    _prefs.putBool("bt_stat", bluetooth.showInStatusBar);
    
    // Image settings
    _prefs.putBool("i_gray", images.grayscale);
    _prefs.putBool("i_dith", images.dither);
    _prefs.putUChar("i_cont", images.contrast);
    _prefs.putUChar("i_brit", images.brightness);
    
    // API
    _prefs.putString("a_stk", api.stockApiKey);
    _prefs.putString("a_sym", api.stockSymbols);
    _prefs.putString("a_news", api.newsApiKey);
    _prefs.putUChar("a_cat", api.newsCategory);
    
    _prefs.putUInt("plugins", enabledPlugins);
    _prefs.putUChar("po_cnt", pluginOrderCount);
    _prefs.putBytes("po_ord", pluginOrder, sizeof(pluginOrder));
    
    _prefs.end();
    saveWiFi();
    _dirty = false;
    Serial.println("[SETTINGS] Saved");
}

// =============================================================================
// Dirty Flag & Auto-Save
// =============================================================================
void SettingsManager::markDirty() {
    _dirty = true;
    _lastChange = millis();
}

void SettingsManager::checkAutoSave() {
    // Auto-save after 5 seconds of no changes
    if (_dirty && millis() - _lastChange > 5000) {
        Serial.println("[SETTINGS] Auto-saving after 5s...");
        save();
    }
}

// =============================================================================
// Load
// =============================================================================
void SettingsManager::load() {
    _prefs.begin("sumi", true);
    
    uint8_t ver = _prefs.getUChar("version", 0);
    Serial.printf("[SETTINGS] Stored version: %d, Current version: %d\n", ver, SETTINGS_VERSION);
    
    if (ver != SETTINGS_VERSION) {
        _prefs.end();
        Serial.println("[SETTINGS] *** VERSION MISMATCH - FORCING FULL RESET ***");
        loadDefaults();
        save();
        return;
    }
    
    // Check firmware version - just update it, don't force setup
    String storedFwVersion = _prefs.getString("fw_version", "");
    String currentFwVersion = SUMI_VERSION;
    
    Serial.printf("[SETTINGS] Stored FW: '%s', Current FW: '%s'\n", 
                  storedFwVersion.c_str(), currentFwVersion.c_str());
    
    // Always read setupComplete from NVS - don't reset on firmware update
    setupComplete = _prefs.getBool("setup_done", false);
    Serial.printf("[SETTINGS] setupComplete from NVS: %s\n", setupComplete ? "true" : "false");
    
    // Update stored firmware version if changed (but don't reset setup)
    if (storedFwVersion != currentFwVersion) {
        Serial.println("[SETTINGS] Firmware version updated");
        _prefs.end();
        _prefs.begin("sumi", false);
        _prefs.putString("fw_version", currentFwVersion);
        _prefs.end();
        _prefs.begin("sumi", true);
    }
    
    themeIndex = _prefs.getUChar("theme", 0);
    
    size_t len = _prefs.getBytes("home_items", homeScreenEnabled, HOME_ITEMS_BYTES);
    if (len != HOME_ITEMS_BYTES) {
        getDefaultHomeItems(homeScreenEnabled);
    }
    
    // Display - Basic
    display.rotation = _prefs.getUChar("d_rot", 0);
    display.sleepMinutes = _prefs.getUChar("d_sleep", 15);
    display.fullRefreshPages = _prefs.getUChar("d_refr", 10);
    display.deepSleep = _prefs.getBool("d_deep", true);
    display.showBatteryHome = _prefs.getBool("d_bath", true);
    display.showBatterySleep = _prefs.getBool("d_bats", true);
    display.showClockHome = _prefs.getBool("d_clkh", true);
    display.showDate = _prefs.getBool("d_date", true);
    display.showWifi = _prefs.getBool("d_wifi", false);
    display.sleepStyle = _prefs.getUChar("d_slps", 0);
    display.clockStyle = _prefs.getUChar("d_clks", 0);
    display.homeLayout = _prefs.getUChar("d_home", 0);
    display.invertColors = _prefs.getBool("d_inv", false);
    
    // === LOCK SCREEN ===
    display.lockStyle = _prefs.getUChar("d_lcks", 0);
    display.lockPhotoSource = _prefs.getUChar("d_lckp", 0);
    display.showBatteryLock = _prefs.getBool("d_batl", true);
    display.showWeatherLock = _prefs.getBool("d_wthl", false);
    display.sleepPhotoSource = _prefs.getUChar("d_slpp", 0);
    
    // === PORTAL CUSTOMIZATION ===
    display.orientation = _prefs.getUChar("d_orient", 1);  // Default vertical
    display.buttonShape = _prefs.getUChar("d_btnshp", 0);
    display.fontStyle = _prefs.getUChar("d_font", 0);
    display.bgTheme = _prefs.getUChar("d_bgthm", 0);
    display.accentColor = _prefs.getUChar("d_accent", 0);
    display.hItemsPerRow = _prefs.getUChar("d_hrow", 4);
    display.vItemsPerRow = _prefs.getUChar("d_vrow", 2);
    
    // Performance
    display.refreshMode = _prefs.getUChar("d_rmod", 0);
    display.transitionStyle = _prefs.getUChar("d_trans", 0);
    display.ghostingThreshold = _prefs.getUChar("d_ghost", 20);
    display.partialQuality = _prefs.getUChar("d_qual", 2);
    display.enableDirtyRects = _prefs.getBool("d_dirty", true);
    display.enableTransitions = _prefs.getBool("d_tren", true);
    display.readingModeQuality = _prefs.getBool("d_rqual", true);
    
    // Additional portal settings
    display.showStatusBar = _prefs.getBool("d_statbar", true);
    display.fontSize = _prefs.getUChar("d_fsize", 12);
    display.sleepRefresh = _prefs.getBool("d_slpref", false);
    display.wakeButton = _prefs.getUChar("d_wake", 0);
    // Widget visibility (default all on)
    display.showBookWidget = _prefs.getBool("d_wbook", true);
    display.showWeatherWidget = _prefs.getBool("d_wwth", true);
    display.showOrientWidget = _prefs.getBool("d_wori", false);
    
    // Reader
    reader.fontSize = _prefs.getUChar("r_fs", 18);
    reader.lineHeight = _prefs.getUChar("r_lh", 150);
    reader.margins = _prefs.getUChar("r_mg", 20);
    reader.paraSpacing = _prefs.getUChar("r_ps", 10);
    reader.textAlign = _prefs.getUChar("r_al", 1);
    reader.hyphenation = _prefs.getBool("r_hyp", true);
    reader.showProgress = _prefs.getBool("r_prog", true);
    reader.showChapter = _prefs.getBool("r_chap", true);
    reader.showPages = _prefs.getBool("r_pgs", true);
    reader.pageTurn = _prefs.getUChar("r_pt", 0);
    reader.tapZones = _prefs.getUChar("r_tap", 0);
    
    // Flashcards
    flashcards.newPerDay = _prefs.getUChar("fc_new", 20);
    flashcards.reviewLimit = _prefs.getUShort("fc_rev", 200);
    flashcards.retention = _prefs.getUChar("fc_ret", 90);
    flashcards.useFsrs = _prefs.getBool("fc_fsrs", true);
    flashcards.showTimer = _prefs.getBool("fc_tim", false);
    flashcards.autoFlip = _prefs.getBool("fc_af", false);
    flashcards.shuffle = _prefs.getBool("fc_sh", true);
    flashcards.fontSize = _prefs.getUChar("fc_fsize", 1);
    flashcards.centerText = _prefs.getBool("fc_cen", true);
    flashcards.showProgressBar = _prefs.getBool("fc_prog", true);
    flashcards.showStats = _prefs.getBool("fc_stat", true);
    
    // Weather
    weather.latitude = _prefs.getFloat("w_lat", 0);
    weather.longitude = _prefs.getFloat("w_lon", 0);
    String loc = _prefs.getString("w_loc", "New York");
    strncpy(weather.location, loc.c_str(), sizeof(weather.location) - 1);
    weather.celsius = _prefs.getBool("w_c", false);
    weather.updateHours = _prefs.getUChar("w_upd", 1);
    weather.timezoneOffset = _prefs.getLong("w_tz", 0);
    
    // Sync
    String url = _prefs.getString("s_url", "");
    strncpy(sync.kosyncUrl, url.c_str(), sizeof(sync.kosyncUrl) - 1);
    String user = _prefs.getString("s_user", "");
    strncpy(sync.kosyncUser, user.c_str(), sizeof(sync.kosyncUser) - 1);
    String pass = _prefs.getString("s_pass", "");
    strncpy(sync.kosyncPass, pass.c_str(), sizeof(sync.kosyncPass) - 1);
    sync.kosyncEnabled = _prefs.getBool("s_en", false);
    
    // Bluetooth
    bluetooth.enabled = _prefs.getBool("bt_en", false);
    bluetooth.autoConnect = _prefs.getBool("bt_auto", true);
    bluetooth.keyboardLayout = _prefs.getUChar("bt_lay", 0);
    bluetooth.showInStatusBar = _prefs.getBool("bt_stat", true);
    
    // Image settings
    images.grayscale = _prefs.getBool("i_gray", true);
    images.dither = _prefs.getBool("i_dith", true);
    images.contrast = _prefs.getUChar("i_cont", 128);
    images.brightness = _prefs.getUChar("i_brit", 128);
    
    // API
    String stk = _prefs.getString("a_stk", "");
    strncpy(api.stockApiKey, stk.c_str(), sizeof(api.stockApiKey) - 1);
    String sym = _prefs.getString("a_sym", "");
    strncpy(api.stockSymbols, sym.c_str(), sizeof(api.stockSymbols) - 1);
    String news = _prefs.getString("a_news", "");
    strncpy(api.newsApiKey, news.c_str(), sizeof(api.newsApiKey) - 1);
    api.newsCategory = _prefs.getUChar("a_cat", 0);
    
    enabledPlugins = _prefs.getUInt("plugins", PLUGINS_DEFAULT);
    pluginOrderCount = _prefs.getUChar("po_cnt", 0);
    _prefs.getBytes("po_ord", pluginOrder, sizeof(pluginOrder));
    
    _prefs.end();
    loadWiFi();
    
    Serial.printf("[SETTINGS] Loaded (orient=%d, vItems=%d)\n", display.orientation, display.vItemsPerRow);
}

// =============================================================================
// WiFi
// =============================================================================
void SettingsManager::saveWiFi() {
    Preferences wifiPrefs;
    wifiPrefs.begin("sumi_wifi", false);
    wifiPrefs.putUChar("count", wifi.savedCount);
    wifiPrefs.putUChar("pref", wifi.preferredIndex);
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        String prefix = "n" + String(i) + "_";
        wifiPrefs.putBool((prefix + "act").c_str(), wifi.networks[i].isActive);
        if (wifi.networks[i].isActive) {
            wifiPrefs.putString((prefix + "ssid").c_str(), wifi.networks[i].ssid);
            wifiPrefs.putString((prefix + "pass").c_str(), wifi.networks[i].password);
            wifiPrefs.putChar((prefix + "rssi").c_str(), wifi.networks[i].lastRSSI);
        }
    }
    wifiPrefs.end();
}

void SettingsManager::loadWiFi() {
    Preferences wifiPrefs;
    wifiPrefs.begin("sumi_wifi", true);
    wifi.savedCount = wifiPrefs.getUChar("count", 0);
    wifi.preferredIndex = wifiPrefs.getUChar("pref", 0);
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        String prefix = "n" + String(i) + "_";
        wifi.networks[i].isActive = wifiPrefs.getBool((prefix + "act").c_str(), false);
        if (wifi.networks[i].isActive) {
            String ssid = wifiPrefs.getString((prefix + "ssid").c_str(), "");
            strncpy(wifi.networks[i].ssid, ssid.c_str(), WIFI_SSID_MAX_LEN - 1);
            String pass = wifiPrefs.getString((prefix + "pass").c_str(), "");
            strncpy(wifi.networks[i].password, pass.c_str(), WIFI_PASS_MAX_LEN - 1);
            wifi.networks[i].lastRSSI = wifiPrefs.getChar((prefix + "rssi").c_str(), 0);
        }
    }
    wifiPrefs.end();
}

int SettingsManager::addWiFiNetwork(const char* ssid, const char* password) {
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        if (wifi.networks[i].isActive && strcmp(wifi.networks[i].ssid, ssid) == 0) {
            strncpy(wifi.networks[i].password, password, WIFI_PASS_MAX_LEN - 1);
            return i;
        }
    }
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        if (!wifi.networks[i].isActive) {
            wifi.networks[i].isActive = true;
            strncpy(wifi.networks[i].ssid, ssid, WIFI_SSID_MAX_LEN - 1);
            strncpy(wifi.networks[i].password, password, WIFI_PASS_MAX_LEN - 1);
            wifi.networks[i].lastRSSI = 0;
            wifi.savedCount++;
            return i;
        }
    }
    return -1;
}

bool SettingsManager::removeWiFiNetwork(int index) {
    if (index < 0 || index >= WIFI_MAX_NETWORKS || !wifi.networks[index].isActive) return false;
    wifi.networks[index].isActive = false;
    memset(wifi.networks[index].ssid, 0, WIFI_SSID_MAX_LEN);
    memset(wifi.networks[index].password, 0, WIFI_PASS_MAX_LEN);
    wifi.savedCount--;
    return true;
}

bool SettingsManager::removeWiFiNetworkBySSID(const char* ssid) {
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        if (wifi.networks[i].isActive && strcmp(wifi.networks[i].ssid, ssid) == 0) {
            return removeWiFiNetwork(i);
        }
    }
    return false;
}

int SettingsManager::getWiFiNetworkCount() const { return wifi.savedCount; }

const WiFiCredential* SettingsManager::getWiFiNetwork(int index) const {
    if (index < 0 || index >= WIFI_MAX_NETWORKS || !wifi.networks[index].isActive) return nullptr;
    return &wifi.networks[index];
}

void SettingsManager::setPreferredWifi(int index) {
    if (index >= 0 && index < WIFI_MAX_NETWORKS) wifi.preferredIndex = index;
}

const char* SettingsManager::getWiFiPassword(const char* ssid) const {
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        if (wifi.networks[i].isActive && strcmp(wifi.networks[i].ssid, ssid) == 0) {
            return wifi.networks[i].password;
        }
    }
    return nullptr;
}

void SettingsManager::updateWiFiRSSI(const char* ssid, int8_t rssi) {
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        if (wifi.networks[i].isActive && strcmp(wifi.networks[i].ssid, ssid) == 0) {
            wifi.networks[i].lastRSSI = rssi;
            break;
        }
    }
}

// =============================================================================
// Home Items
// =============================================================================
void SettingsManager::setSetupComplete(bool complete) { setupComplete = complete; _dirty = true; }

bool SettingsManager::isHomeItemEnabled(uint8_t itemIndex) const {
    if (itemIndex >= HOME_ITEMS_MAX) return false;
    return (homeScreenEnabled[itemIndex / 8] & (1 << (itemIndex % 8))) != 0;
}

void SettingsManager::setHomeItemEnabled(uint8_t itemIndex, bool enabled) {
    if (itemIndex >= HOME_ITEMS_MAX) return;
    if (enabled) homeScreenEnabled[itemIndex / 8] |= (1 << (itemIndex % 8));
    else homeScreenEnabled[itemIndex / 8] &= ~(1 << (itemIndex % 8));
    _dirty = true;
}

int SettingsManager::getEnabledHomeItemCount() const {
    int count = 0;
    for (int i = 0; i < HOME_ITEMS_MAX; i++) if (isHomeItemEnabled(i)) count++;
    return count;
}

void SettingsManager::getEnabledHomeItems(uint8_t* outIndices, int* outCount, int maxCount) const {
    int count = 0;
    for (int i = 0; i < HOME_ITEMS_MAX && count < maxCount; i++) {
        // Skip Weather - it's widget-only, accessed via weather widget tap
        if (i == HOME_ITEM_WEATHER) continue;
        
        if (isHomeItemEnabled(i)) outIndices[count++] = i;
    }
    *outCount = count;
}

void SettingsManager::setHomeItemsFromJSON(JsonArrayConst items) {
    memset(homeScreenEnabled, 0, HOME_ITEMS_BYTES);
    int unknownCount = 0;
    
    for (JsonVariantConst item : items) {
        const char* id = item.as<const char*>();
        if (id) {
            int index = getHomeItemIndex(id);
            if (index >= 0) {
                setHomeItemEnabled(index, true);
            } else {
                Serial.printf("[SETTINGS] WARNING: Unknown plugin ID '%s' - ignored\n", id);
                unknownCount++;
            }
        }
    }
    
    if (unknownCount > 0) {
        Serial.printf("[SETTINGS] %d unknown plugin IDs were ignored\n", unknownCount);
    }
    
    _dirty = true;
}

void SettingsManager::homeItemsToJSON(JsonArray items) const {
    for (int i = 0; i < HOME_ITEMS_MAX; i++) {
        if (isHomeItemEnabled(i)) {
            const HomeItemInfo* info = getHomeItemByIndex(i);
            if (info) items.add(info->id);
        }
    }
}

// =============================================================================
// Plugins (Legacy)
// =============================================================================
bool SettingsManager::isPluginEnabled(const char* pluginId) {
    return (enabledPlugins & getPluginBit(pluginId)) != 0;
}

void SettingsManager::setPluginEnabled(const char* pluginId, bool enabled) {
    uint32_t bit = getPluginBit(pluginId);
    if (enabled) enabledPlugins |= bit;
    else enabledPlugins &= ~bit;
    _dirty = true;
}

void SettingsManager::clearPlugins() { enabledPlugins = 0; _dirty = true; }

uint32_t SettingsManager::getPluginBit(const char* pluginId) {
    if (strcmp(pluginId, "reader") == 0) return PLUGIN_READER;
    if (strcmp(pluginId, "flashcards") == 0) return PLUGIN_FLASHCARDS;
    if (strcmp(pluginId, "library") == 0) return PLUGIN_LIBRARY;
    if (strcmp(pluginId, "chess") == 0) return PLUGIN_CHESS;
    if (strcmp(pluginId, "minesweeper") == 0) return PLUGIN_MINESWEEPER;
    if (strcmp(pluginId, "notes") == 0) return PLUGIN_NOTES;
    if (strcmp(pluginId, "weather") == 0) return PLUGIN_WEATHER;
    if (strcmp(pluginId, "checkers") == 0) return PLUGIN_CHECKERS;
    if (strcmp(pluginId, "sudoku") == 0) return PLUGIN_SUDOKU;
    if (strcmp(pluginId, "solitaire") == 0) return PLUGIN_SOLITAIRE;
    if (strcmp(pluginId, "todo") == 0) return PLUGIN_TODO;
    if (strcmp(pluginId, "images") == 0) return PLUGIN_IMAGES;
    if (strcmp(pluginId, "tools") == 0) return PLUGIN_TOOLS;
    return 0;
}

// =============================================================================
// JSON Export - ALL portal customization fields
// =============================================================================
void SettingsManager::toJSON(JsonObject doc) {
    doc["setupComplete"] = setupComplete;
    doc["themeIndex"] = themeIndex;
    
    JsonArray homeItems = doc["homeItems"].to<JsonArray>();
    homeItemsToJSON(homeItems);
    
    // Display - ALL fields
    JsonObject disp = doc["display"].to<JsonObject>();
    disp["rotation"] = display.rotation;
    disp["sleepMinutes"] = display.sleepMinutes;
    disp["fullRefreshPages"] = display.fullRefreshPages;
    disp["deepSleep"] = display.deepSleep;
    disp["showBatteryHome"] = display.showBatteryHome;
    disp["showBatterySleep"] = display.showBatterySleep;
    disp["showClockHome"] = display.showClockHome;
    disp["showDate"] = display.showDate;
    disp["showWifi"] = display.showWifi;
    disp["sleepStyle"] = display.sleepStyle;
    disp["clockStyle"] = display.clockStyle;
    disp["homeLayout"] = display.homeLayout;
    disp["invertColors"] = display.invertColors;
    // Lock screen
    disp["lockStyle"] = display.lockStyle;
    disp["lockPhotoSource"] = display.lockPhotoSource;
    disp["showBatteryLock"] = display.showBatteryLock;
    disp["showWeatherLock"] = display.showWeatherLock;
    disp["sleepPhotoSource"] = display.sleepPhotoSource;
    disp["sleepSelectedImage"] = display.sleepSelectedImage;
    // Portal customization
    disp["orientation"] = display.orientation;
    disp["buttonShape"] = display.buttonShape;
    disp["fontStyle"] = display.fontStyle;
    disp["bgTheme"] = display.bgTheme;
    disp["accentColor"] = display.accentColor;
    disp["hItemsPerRow"] = display.hItemsPerRow;
    disp["vItemsPerRow"] = display.vItemsPerRow;
    // Performance
    disp["refreshMode"] = display.refreshMode;
    disp["transitionStyle"] = display.transitionStyle;
    disp["ghostingThreshold"] = display.ghostingThreshold;
    disp["partialQuality"] = display.partialQuality;
    disp["enableDirtyRects"] = display.enableDirtyRects;
    disp["enableTransitions"] = display.enableTransitions;
    disp["readingModeQuality"] = display.readingModeQuality;
    // Additional portal settings
    disp["showStatusBar"] = display.showStatusBar;
    disp["fontSize"] = display.fontSize;
    disp["sleepRefresh"] = display.sleepRefresh;
    disp["wakeButton"] = display.wakeButton;
    disp["bootToLastBook"] = display.bootToLastBook;
    // Widget visibility
    disp["showBookWidget"] = display.showBookWidget;
    disp["showWeatherWidget"] = display.showWeatherWidget;
    disp["showOrientWidget"] = display.showOrientWidget;
    
    // Reader
    JsonObject rd = doc["reader"].to<JsonObject>();
    rd["fontSize"] = reader.fontSize;
    rd["lineHeight"] = reader.lineHeight;
    rd["margins"] = reader.margins;
    rd["paraSpacing"] = reader.paraSpacing;
    rd["sceneBreakSpacing"] = reader.sceneBreakSpacing;
    rd["textAlign"] = reader.textAlign;
    rd["hyphenation"] = reader.hyphenation;
    rd["showProgress"] = reader.showProgress;
    rd["showChapter"] = reader.showChapter;
    rd["showPages"] = reader.showPages;
    rd["pageTurn"] = reader.pageTurn;
    rd["tapZones"] = reader.tapZones;
    
    // Flashcards
    JsonObject fc = doc["flashcards"].to<JsonObject>();
    fc["newPerDay"] = flashcards.newPerDay;
    fc["reviewLimit"] = flashcards.reviewLimit;
    fc["retention"] = flashcards.retention;
    fc["useFsrs"] = flashcards.useFsrs;
    fc["showTimer"] = flashcards.showTimer;
    fc["autoFlip"] = flashcards.autoFlip;
    fc["shuffle"] = flashcards.shuffle;
    
    // Weather
    JsonObject wt = doc["weather"].to<JsonObject>();
    wt["latitude"] = weather.latitude;
    wt["longitude"] = weather.longitude;
    wt["location"] = weather.location;
    wt["zipCode"] = weather.zipCode;
    wt["celsius"] = weather.celsius;
    wt["updateHours"] = weather.updateHours;
    wt["timezoneOffset"] = weather.timezoneOffset;
    
    // Sync
    JsonObject sy = doc["sync"].to<JsonObject>();
    sy["kosyncUrl"] = sync.kosyncUrl;
    sy["kosyncUser"] = sync.kosyncUser;
    sy["kosyncEnabled"] = sync.kosyncEnabled;
    
    // Bluetooth
    JsonObject bt = doc["bluetooth"].to<JsonObject>();
    bt["enabled"] = bluetooth.enabled;
    bt["autoConnect"] = bluetooth.autoConnect;
    bt["keyboardLayout"] = bluetooth.keyboardLayout;
    bt["showInStatusBar"] = bluetooth.showInStatusBar;
    
    // Image settings
    JsonObject img = doc["images"].to<JsonObject>();
    img["grayscale"] = images.grayscale;
    img["dither"] = images.dither;
    img["contrast"] = images.contrast;
    img["brightness"] = images.brightness;
    
    // API
    JsonObject ap = doc["api"].to<JsonObject>();
    ap["stockSymbols"] = api.stockSymbols;
    ap["newsCategory"] = api.newsCategory;
    
    doc["enabledPlugins"] = enabledPlugins;
    
    // WiFi list
    JsonArray wifiList = doc["wifiNetworks"].to<JsonArray>();
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        if (wifi.networks[i].isActive) {
            JsonObject net = wifiList.add<JsonObject>();
            net["ssid"] = wifi.networks[i].ssid;
            net["rssi"] = wifi.networks[i].lastRSSI;
        }
    }
}

// =============================================================================
// Sync Portal Reader Settings to Library's Internal Format
// =============================================================================
void SettingsManager::syncReaderSettings() {
    Serial.println("[SETTINGS] Syncing portal reader settings to Library...");
    
    // Load current library settings
    readerSettings.load();
    LibReaderSettings& libSettings = readerSettings.get();
    
    // Map portal fontSize (12-32 pixels) to Library FontSize enum
    if (reader.fontSize <= 16) {
        libSettings.fontSize = FontSize::SMALL;
    } else if (reader.fontSize <= 20) {
        libSettings.fontSize = FontSize::MEDIUM;
    } else if (reader.fontSize <= 26) {
        libSettings.fontSize = FontSize::LARGE;
    } else {
        libSettings.fontSize = FontSize::EXTRA_LARGE;
    }
    
    // Map portal lineHeight (100-200%) to Library LineSpacing enum
    if (reader.lineHeight <= 120) {
        libSettings.lineSpacing = LineSpacing::TIGHT;
    } else if (reader.lineHeight <= 160) {
        libSettings.lineSpacing = LineSpacing::NORMAL;
    } else {
        libSettings.lineSpacing = LineSpacing::WIDE;
    }
    
    // Map portal textAlign (0=left, 1=justify) to Library TextAlign enum
    // Note: Library uses opposite mapping (0=justified, 1=left)
    libSettings.textAlign = (reader.textAlign == 1) ? TextAlign::JUSTIFIED : TextAlign::LEFT;
    
    // Map portal margins (5-40) to library screenMargin (0-20)
    // Portal margins are total, library adds to viewable margins
    int adjustedMargin = (reader.margins > 10) ? (reader.margins - 10) : 0;
    if (adjustedMargin > 20) adjustedMargin = 20;
    libSettings.screenMargin = (uint8_t)adjustedMargin;
    
    // Sync display options
    libSettings.showPageNumbers = reader.showPages;
    libSettings.showChapterTitle = reader.showChapter;
    
    // Save updated library settings
    readerSettings.save();
    
    Serial.printf("[SETTINGS] Synced: fontSize=%d, lineSpacing=%d, textAlign=%d, margin=%d\n",
                  (int)libSettings.fontSize, (int)libSettings.lineSpacing, 
                  (int)libSettings.textAlign, libSettings.screenMargin);
}

// =============================================================================
// JSON Import - ALL portal customization fields
// =============================================================================
bool SettingsManager::fromJSON(JsonObjectConst doc) {
    if (doc["setupComplete"].is<bool>()) setupComplete = doc["setupComplete"];
    if (doc["themeIndex"].is<int>()) themeIndex = doc["themeIndex"];
    
    if (doc["homeItems"].is<JsonArrayConst>()) {
        setHomeItemsFromJSON(doc["homeItems"]);
    }
    
    if (doc["display"].is<JsonObjectConst>()) {
        JsonObjectConst d = doc["display"];
        if (d["rotation"].is<int>()) display.rotation = clampValue((int)d["rotation"], 0, 3);
        if (d["sleepMinutes"].is<int>()) display.sleepMinutes = clampValue((int)d["sleepMinutes"], 0, 120);
        if (d["fullRefreshPages"].is<int>()) display.fullRefreshPages = clampValue((int)d["fullRefreshPages"], 0, 50);
        if (d["deepSleep"].is<bool>()) display.deepSleep = d["deepSleep"];
        if (d["showBatteryHome"].is<bool>()) display.showBatteryHome = d["showBatteryHome"];
        if (d["showBatterySleep"].is<bool>()) display.showBatterySleep = d["showBatterySleep"];
        if (d["showClockHome"].is<bool>()) display.showClockHome = d["showClockHome"];
        if (d["showDate"].is<bool>()) display.showDate = d["showDate"];
        if (d["showWifi"].is<bool>()) display.showWifi = d["showWifi"];
        if (d["sleepStyle"].is<int>()) display.sleepStyle = clampValue((int)d["sleepStyle"], 0, 2);
        if (d["clockStyle"].is<int>()) display.clockStyle = clampValue((int)d["clockStyle"], 0, 2);
        if (d["homeLayout"].is<int>()) display.homeLayout = clampValue((int)d["homeLayout"], 0, 1);
        if (d["invertColors"].is<bool>()) display.invertColors = d["invertColors"];
        // Lock screen
        if (d["lockStyle"].is<int>()) display.lockStyle = clampValue((int)d["lockStyle"], 0, 3);
        if (d["lockPhotoSource"].is<int>()) display.lockPhotoSource = clampValue((int)d["lockPhotoSource"], 0, 2);
        if (d["showBatteryLock"].is<bool>()) display.showBatteryLock = d["showBatteryLock"];
        if (d["showWeatherLock"].is<bool>()) display.showWeatherLock = d["showWeatherLock"];
        if (d["sleepPhotoSource"].is<int>()) display.sleepPhotoSource = clampValue((int)d["sleepPhotoSource"], 0, 2);
        if (d["sleepSelectedImage"].is<const char*>()) {
            strncpy(display.sleepSelectedImage, d["sleepSelectedImage"], sizeof(display.sleepSelectedImage) - 1);
            display.sleepSelectedImage[sizeof(display.sleepSelectedImage) - 1] = '\0';
        }
        // Portal customization
        if (d["orientation"].is<int>()) display.orientation = clampValue((int)d["orientation"], 0, 1);
        if (d["buttonShape"].is<int>()) display.buttonShape = clampValue((int)d["buttonShape"], 0, 2);
        if (d["fontStyle"].is<int>()) display.fontStyle = clampValue((int)d["fontStyle"], 0, 2);
        if (d["bgTheme"].is<int>()) display.bgTheme = clampValue((int)d["bgTheme"], 0, 3);
        if (d["accentColor"].is<int>()) display.accentColor = clampValue((int)d["accentColor"], 0, 5);
        if (d["hItemsPerRow"].is<int>()) display.hItemsPerRow = clampValue((int)d["hItemsPerRow"], 3, 5);
        if (d["vItemsPerRow"].is<int>()) display.vItemsPerRow = clampValue((int)d["vItemsPerRow"], 2, 3);
        // Performance
        if (d["refreshMode"].is<int>()) display.refreshMode = clampValue((int)d["refreshMode"], 0, 3);
        if (d["transitionStyle"].is<int>()) display.transitionStyle = clampValue((int)d["transitionStyle"], 0, 3);
        if (d["ghostingThreshold"].is<int>()) display.ghostingThreshold = clampValue((int)d["ghostingThreshold"], 10, 50);
        if (d["partialQuality"].is<int>()) display.partialQuality = clampValue((int)d["partialQuality"], 1, 4);
        if (d["enableDirtyRects"].is<bool>()) display.enableDirtyRects = d["enableDirtyRects"];
        if (d["enableTransitions"].is<bool>()) display.enableTransitions = d["enableTransitions"];
        if (d["readingModeQuality"].is<bool>()) display.readingModeQuality = d["readingModeQuality"];
        // Additional portal settings
        if (d["showStatusBar"].is<bool>()) display.showStatusBar = d["showStatusBar"];
        if (d["fontSize"].is<int>()) display.fontSize = clampValue((int)d["fontSize"], 10, 24);
        if (d["sleepRefresh"].is<bool>()) display.sleepRefresh = d["sleepRefresh"];
        if (d["wakeButton"].is<int>()) display.wakeButton = clampValue((int)d["wakeButton"], 0, 2);
        if (d["bootToLastBook"].is<bool>()) display.bootToLastBook = d["bootToLastBook"];
        // Widget visibility
        if (d["showBookWidget"].is<bool>()) display.showBookWidget = d["showBookWidget"];
        if (d["showWeatherWidget"].is<bool>()) display.showWeatherWidget = d["showWeatherWidget"];
        if (d["showOrientWidget"].is<bool>()) display.showOrientWidget = d["showOrientWidget"];
    }
    
    if (doc["reader"].is<JsonObjectConst>()) {
        JsonObjectConst r = doc["reader"];
        if (r["fontSize"].is<int>()) reader.fontSize = clampValue((int)r["fontSize"], 12, 32);
        if (r["lineHeight"].is<int>()) reader.lineHeight = clampValue((int)r["lineHeight"], 100, 200);
        if (r["margins"].is<int>()) reader.margins = clampValue((int)r["margins"], 5, 50);
        if (r["paraSpacing"].is<int>()) reader.paraSpacing = clampValue((int)r["paraSpacing"], 0, 30);
        if (r["sceneBreakSpacing"].is<int>()) reader.sceneBreakSpacing = clampValue((int)r["sceneBreakSpacing"], 0, 60);
        if (r["textAlign"].is<int>()) reader.textAlign = clampValue((int)r["textAlign"], 0, 1);
        if (r["hyphenation"].is<bool>()) reader.hyphenation = r["hyphenation"];
        if (r["showProgress"].is<bool>()) reader.showProgress = r["showProgress"];
        if (r["showChapter"].is<bool>()) reader.showChapter = r["showChapter"];
        if (r["showPages"].is<bool>()) reader.showPages = r["showPages"];
        if (r["pageTurn"].is<int>()) reader.pageTurn = clampValue((int)r["pageTurn"], 0, 1);
        if (r["tapZones"].is<int>()) reader.tapZones = clampValue((int)r["tapZones"], 0, 2);
        
        // Sync portal reader settings to Library's internal settings file
        syncReaderSettings();
    }
    
    if (doc["flashcards"].is<JsonObjectConst>()) {
        JsonObjectConst f = doc["flashcards"];
        if (f["newPerDay"].is<int>()) flashcards.newPerDay = clampValue((int)f["newPerDay"], 0, 100);
        if (f["reviewLimit"].is<int>()) flashcards.reviewLimit = clampValue((int)f["reviewLimit"], 0, 500);
        if (f["retention"].is<int>()) flashcards.retention = clampValue((int)f["retention"], 70, 99);
        if (f["useFsrs"].is<bool>()) flashcards.useFsrs = f["useFsrs"];
        if (f["showTimer"].is<bool>()) flashcards.showTimer = f["showTimer"];
        if (f["autoFlip"].is<bool>()) flashcards.autoFlip = f["autoFlip"];
        if (f["shuffle"].is<bool>()) flashcards.shuffle = f["shuffle"];
    }
    
    if (doc["weather"].is<JsonObjectConst>()) {
        JsonObjectConst w = doc["weather"];
        if (w["latitude"].is<float>()) weather.latitude = w["latitude"];
        if (w["longitude"].is<float>()) weather.longitude = w["longitude"];
        if (w["location"].is<const char*>()) strncpy(weather.location, w["location"] | "", sizeof(weather.location) - 1);
        if (w["celsius"].is<bool>()) weather.celsius = w["celsius"];
        if (w["updateHours"].is<int>()) weather.updateHours = w["updateHours"];
        if (w["timezoneOffset"].is<int>()) weather.timezoneOffset = w["timezoneOffset"];
    }
    
    if (doc["sync"].is<JsonObjectConst>()) {
        JsonObjectConst s = doc["sync"];
        if (s["kosyncUrl"].is<const char*>()) strncpy(sync.kosyncUrl, s["kosyncUrl"] | "", sizeof(sync.kosyncUrl) - 1);
        if (s["kosyncUser"].is<const char*>()) strncpy(sync.kosyncUser, s["kosyncUser"] | "", sizeof(sync.kosyncUser) - 1);
        if (s["kosyncPass"].is<const char*>()) strncpy(sync.kosyncPass, s["kosyncPass"] | "", sizeof(sync.kosyncPass) - 1);
        if (s["kosyncEnabled"].is<bool>()) sync.kosyncEnabled = s["kosyncEnabled"];
    }
    
    if (doc["bluetooth"].is<JsonObjectConst>()) {
        JsonObjectConst b = doc["bluetooth"];
        if (b["enabled"].is<bool>()) bluetooth.enabled = b["enabled"];
        if (b["autoConnect"].is<bool>()) bluetooth.autoConnect = b["autoConnect"];
        if (b["keyboardLayout"].is<int>()) bluetooth.keyboardLayout = clampValue((int)b["keyboardLayout"], 0, 5);
        if (b["showInStatusBar"].is<bool>()) bluetooth.showInStatusBar = b["showInStatusBar"];
    }
    
    // Image settings
    if (doc["images"].is<JsonObjectConst>()) {
        JsonObjectConst i = doc["images"];
        if (i["grayscale"].is<bool>()) images.grayscale = i["grayscale"];
        if (i["dither"].is<bool>()) images.dither = i["dither"];
        if (i["contrast"].is<int>()) images.contrast = clampValue((int)i["contrast"], 0, 255);
        if (i["brightness"].is<int>()) images.brightness = clampValue((int)i["brightness"], 0, 255);
    }
    
    if (doc["api"].is<JsonObjectConst>()) {
        JsonObjectConst a = doc["api"];
        if (a["stockApiKey"].is<const char*>()) strncpy(api.stockApiKey, a["stockApiKey"] | "", sizeof(api.stockApiKey) - 1);
        if (a["stockSymbols"].is<const char*>()) strncpy(api.stockSymbols, a["stockSymbols"] | "", sizeof(api.stockSymbols) - 1);
        if (a["newsApiKey"].is<const char*>()) strncpy(api.newsApiKey, a["newsApiKey"] | "", sizeof(api.newsApiKey) - 1);
        if (a["newsCategory"].is<int>()) api.newsCategory = a["newsCategory"];
    }
    
    if (doc["enabledPlugins"].is<int>()) enabledPlugins = doc["enabledPlugins"];
    
    _dirty = true;
    return true;
}

// =============================================================================
// Key-Value Access
// =============================================================================
bool SettingsManager::setByKey(const String& key, const String& value) {
    if (key == "setupComplete") { setupComplete = (value == "true" || value == "1"); return true; }
    if (key == "themeIndex") { themeIndex = value.toInt(); return true; }
    if (key == "display.rotation") { display.rotation = value.toInt(); return true; }
    if (key == "display.sleepMinutes") { display.sleepMinutes = value.toInt(); return true; }
    if (key == "display.orientation") { display.orientation = value.toInt(); return true; }
    if (key == "display.buttonShape") { display.buttonShape = value.toInt(); return true; }
    if (key == "display.fontStyle") { display.fontStyle = value.toInt(); return true; }
    if (key == "display.bgTheme") { display.bgTheme = value.toInt(); return true; }
    if (key == "display.accentColor") { display.accentColor = value.toInt(); return true; }
    if (key == "display.hItemsPerRow") { display.hItemsPerRow = value.toInt(); return true; }
    if (key == "display.vItemsPerRow") { display.vItemsPerRow = value.toInt(); return true; }
    if (key == "display.invertColors") { display.invertColors = (value == "true" || value == "1"); return true; }
    if (key == "reader.fontSize") { reader.fontSize = value.toInt(); return true; }
    if (key == "bluetooth.enabled") { bluetooth.enabled = (value == "true" || value == "1"); return true; }
    return false;
}

String SettingsManager::getByKey(const String& key) {
    if (key == "setupComplete") return setupComplete ? "true" : "false";
    if (key == "themeIndex") return String(themeIndex);
    if (key == "display.orientation") return String(display.orientation);
    if (key == "display.buttonShape") return String(display.buttonShape);
    if (key == "display.bgTheme") return String(display.bgTheme);
    if (key == "display.hItemsPerRow") return String(display.hItemsPerRow);
    if (key == "display.vItemsPerRow") return String(display.vItemsPerRow);
    return "";
}

// =============================================================================
// SD Backup
// =============================================================================
bool SettingsManager::backupToSD(const char* path) {
    SD.mkdir("/.config");
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    JsonDocument doc;
    toJSON(doc.to<JsonObject>());
    serializeJsonPretty(doc, file);
    file.close();
    return true;
}

bool SettingsManager::restoreFromSD(const char* path) {
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    JsonDocument doc;
    if (deserializeJson(doc, file)) { file.close(); return false; }
    file.close();
    fromJSON(doc.as<JsonObjectConst>());
    save();
    return true;
}
