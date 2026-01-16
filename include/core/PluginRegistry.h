/**
 * @file PluginRegistry.h
 * @brief Plugin registration system for Sumi firmware
 * @version 2.1.16
 *
 * Allows plugins to be registered with a single macro, reducing the number
 * of files that need to be modified when adding new plugins.
 * 
 * USAGE:
 * ======
 * 
 * 1. In your plugin header (MyPlugin.h):
 *    
 *    #include "core/PluginRegistry.h"
 *    
 *    class MyPlugin : public IPlugin {
 *    public:
 *        const char* id() const override { return "myplugin"; }
 *        const char* name() const override { return "My Plugin"; }
 *        const char* icon() const override { return "M"; }
 *        PluginCategory category() const override { return PLUGIN_CAT_TOOLS; }
 *        
 *        void init(int w, int h) override;
 *        void draw() override;
 *        bool handleInput(Button btn) override;
 *    };
 * 
 * 2. In your plugin source (MyPlugin.cpp):
 *    
 *    #include "plugins/MyPlugin.h"
 *    
 *    REGISTER_PLUGIN(MyPlugin, FEATURE_GAMES)
 *    
 *    void MyPlugin::init(int w, int h) { ... }
 *    void MyPlugin::draw() { ... }
 *    bool MyPlugin::handleInput(Button btn) { ... }
 * 
 * 3. The plugin will automatically appear in:
 *    - Home screen (if enabled)
 *    - App launcher
 *    - Portal plugin list
 */

#ifndef SUMI_PLUGIN_REGISTRY_H
#define SUMI_PLUGIN_REGISTRY_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Plugin Categories
// =============================================================================
enum PluginCategory {
    PLUGIN_CAT_CORE = 0,      // Library, Flashcards, Notes, Images
    PLUGIN_CAT_GAMES,         // Chess, Checkers, Sudoku, etc.
    PLUGIN_CAT_TOOLS,         // ToolSuite, Todo
    PLUGIN_CAT_WIDGETS,       // Weather
    PLUGIN_CAT_SYSTEM,        // Settings
    PLUGIN_CAT_COUNT
};

inline const char* getCategoryName(PluginCategory cat) {
    switch (cat) {
        case PLUGIN_CAT_CORE:    return "core";
        case PLUGIN_CAT_GAMES:   return "games";
        case PLUGIN_CAT_TOOLS:   return "tools";
        case PLUGIN_CAT_WIDGETS: return "widgets";
        case PLUGIN_CAT_SYSTEM:  return "system";
        default:                 return "unknown";
    }
}

// =============================================================================
// Plugin Runner Type
// =============================================================================
enum PluginRunnerType {
    RUNNER_SIMPLE = 0,        // Standard plugins (most common)
    RUNNER_SELF_REFRESH,      // Plugins that handle their own partial updates
    RUNNER_WITH_UPDATE        // Plugins with periodic update loops
};

// =============================================================================
// Plugin Interface
// =============================================================================
/**
 * @brief Base interface for all plugins
 * 
 * All plugins should inherit from this interface to ensure consistent
 * behavior across the system.
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // === Required Methods ===
    
    /** @brief Unique identifier (lowercase, no spaces) */
    virtual const char* id() const = 0;
    
    /** @brief Display name shown in UI */
    virtual const char* name() const = 0;
    
    /** @brief Single character icon for home screen */
    virtual const char* icon() const = 0;
    
    /** @brief Plugin category for organization */
    virtual PluginCategory category() const = 0;
    
    /** @brief Initialize plugin with screen dimensions */
    virtual void init(int screenW, int screenH) = 0;
    
    /** @brief Draw the plugin UI */
    virtual void draw() = 0;
    
    /** 
     * @brief Handle button input
     * @param btn The button that was pressed
     * @return true to continue running, false to exit plugin
     */
    virtual bool handleInput(Button btn) = 0;
    
    // === Optional Methods (with default implementations) ===
    
    /** @brief Plugin description for portal */
    virtual const char* description() const { return ""; }
    
    /** @brief Version string */
    virtual const char* version() const { return "1.0.0"; }
    
    /** @brief Minimum firmware version required */
    virtual const char* minFirmware() const { return "2.0.0"; }
    
    /** @brief Runner type for this plugin */
    virtual PluginRunnerType runnerType() const { return RUNNER_SIMPLE; }
    
    /** 
     * @brief Periodic update (for RUNNER_WITH_UPDATE or RUNNER_SELF_REFRESH)
     * @return true if display needs refresh
     */
    virtual bool update() { return false; }
    
    /**
     * @brief Draw partial update (for RUNNER_SELF_REFRESH)
     */
    virtual void drawPartial() { draw(); }
    
    /**
     * @brief Check if plugin needs full redraw (for RUNNER_SELF_REFRESH)
     */
    virtual bool needsFullRedraw() const { return true; }
    virtual void setNeedsFullRedraw(bool needs) { (void)needs; }
};

// =============================================================================
// Plugin Registration Entry
// =============================================================================
struct PluginEntry {
    const char* id;
    const char* name;
    const char* icon;
    const char* description;
    PluginCategory category;
    PluginRunnerType runnerType;
    IPlugin* (*factory)();        // Factory function to create instance
    bool (*featureEnabled)();     // Function to check if feature is enabled
    uint8_t homeIndex;            // Index in home screen (0xFF = not on home)
};

// =============================================================================
// Plugin Registry
// =============================================================================
/**
 * @brief Central registry for all plugins
 * 
 * Plugins register themselves at startup using the REGISTER_PLUGIN macro.
 * The registry provides a single point of access for:
 * - Listing all available plugins
 * - Launching plugins by ID
 * - Getting plugin metadata
 */
class PluginRegistry {
public:
    static constexpr int MAX_PLUGINS = 32;
    
    static PluginRegistry& instance() {
        static PluginRegistry registry;
        return registry;
    }
    
    /**
     * @brief Register a plugin with the system
     * @param entry Plugin registration entry
     * @return The assigned home index, or 0xFF if registration failed
     */
    uint8_t registerPlugin(const PluginEntry& entry) {
        if (_count >= MAX_PLUGINS) {
            Serial.println("[REGISTRY] ERROR: Max plugins exceeded");
            return 0xFF;
        }
        
        // Assign home index based on registration order
        PluginEntry e = entry;
        e.homeIndex = _count;
        _plugins[_count] = e;
        
        Serial.printf("[REGISTRY] Registered: %s (index=%d, cat=%s)\n",
                      e.name, e.homeIndex, getCategoryName(e.category));
        
        return _count++;
    }
    
    /** @brief Get number of registered plugins */
    int count() const { return _count; }
    
    /** @brief Get plugin entry by index */
    const PluginEntry* get(int index) const {
        if (index < 0 || index >= _count) return nullptr;
        return &_plugins[index];
    }
    
    /** @brief Get plugin entry by ID */
    const PluginEntry* getById(const char* id) const {
        for (int i = 0; i < _count; i++) {
            if (strcmp(_plugins[i].id, id) == 0) {
                return &_plugins[i];
            }
        }
        return nullptr;
    }
    
    /** @brief Get plugin instance by ID (creates if needed) */
    IPlugin* getPluginInstance(const char* id) {
        const PluginEntry* entry = getById(id);
        if (!entry || !entry->factory) return nullptr;
        if (!entry->featureEnabled || !entry->featureEnabled()) return nullptr;
        return entry->factory();
    }
    
    /** @brief Check if a plugin is available (feature enabled) */
    bool isAvailable(const char* id) const {
        const PluginEntry* entry = getById(id);
        if (!entry) return false;
        if (!entry->featureEnabled) return true;
        return entry->featureEnabled();
    }
    
    /** @brief Get all plugins in a category */
    int getByCategory(PluginCategory cat, const PluginEntry** out, int maxOut) const {
        int found = 0;
        for (int i = 0; i < _count && found < maxOut; i++) {
            if (_plugins[i].category == cat) {
                out[found++] = &_plugins[i];
            }
        }
        return found;
    }
    
    /** @brief Print registry contents (for debugging) */
    void printRegistry() const {
        Serial.println("[REGISTRY] === Plugin Registry ===");
        for (int i = 0; i < _count; i++) {
            const PluginEntry& e = _plugins[i];
            bool enabled = !e.featureEnabled || e.featureEnabled();
            Serial.printf("[REGISTRY]   %d: %s (%s) [%s]\n",
                          i, e.name, e.id, enabled ? "enabled" : "disabled");
        }
        Serial.printf("[REGISTRY] Total: %d plugins\n", _count);
    }
    
private:
    PluginRegistry() : _count(0) {}
    
    PluginEntry _plugins[MAX_PLUGINS];
    int _count;
};

// =============================================================================
// Registration Macros
// =============================================================================

/**
 * @brief Helper to create feature check function
 */
#define _FEATURE_CHECK_FUNC(feature) \
    []() -> bool { return feature != 0; }

/**
 * @brief Helper to create plugin factory function
 */
#define _PLUGIN_FACTORY(ClassName) \
    []() -> IPlugin* { \
        static ClassName instance; \
        return &instance; \
    }

/**
 * @brief Register a plugin with automatic feature gating
 * 
 * Usage:
 *   REGISTER_PLUGIN(MyPlugin, FEATURE_GAMES)
 * 
 * This creates a global registration object that:
 * - Creates a singleton instance of the plugin
 * - Registers it with the PluginRegistry
 * - Gates availability based on the feature flag
 */
#define REGISTER_PLUGIN(ClassName, FeatureFlag) \
    static ClassName _instance_##ClassName; \
    static struct _PluginRegistrar_##ClassName { \
        _PluginRegistrar_##ClassName() { \
            PluginEntry entry; \
            entry.id = _instance_##ClassName.id(); \
            entry.name = _instance_##ClassName.name(); \
            entry.icon = _instance_##ClassName.icon(); \
            entry.description = _instance_##ClassName.description(); \
            entry.category = _instance_##ClassName.category(); \
            entry.runnerType = _instance_##ClassName.runnerType(); \
            entry.factory = _PLUGIN_FACTORY(ClassName); \
            entry.featureEnabled = _FEATURE_CHECK_FUNC(FeatureFlag); \
            entry.homeIndex = 0xFF; \
            PluginRegistry::instance().registerPlugin(entry); \
        } \
    } _registrar_##ClassName

/**
 * @brief Register a plugin that is always available
 * 
 * Usage:
 *   REGISTER_PLUGIN_ALWAYS(SettingsPlugin)
 */
#define REGISTER_PLUGIN_ALWAYS(ClassName) \
    REGISTER_PLUGIN(ClassName, 1)

/**
 * @brief Register a plugin with custom feature check
 * 
 * Usage:
 *   REGISTER_PLUGIN_CUSTOM(MyPlugin, []() { return FEATURE_A && FEATURE_B; })
 */
#define REGISTER_PLUGIN_CUSTOM(ClassName, FeatureCheck) \
    static ClassName _instance_##ClassName; \
    static struct _PluginRegistrar_##ClassName { \
        _PluginRegistrar_##ClassName() { \
            PluginEntry entry; \
            entry.id = _instance_##ClassName.id(); \
            entry.name = _instance_##ClassName.name(); \
            entry.icon = _instance_##ClassName.icon(); \
            entry.description = _instance_##ClassName.description(); \
            entry.category = _instance_##ClassName.category(); \
            entry.runnerType = _instance_##ClassName.runnerType(); \
            entry.factory = _PLUGIN_FACTORY(ClassName); \
            entry.featureEnabled = FeatureCheck; \
            entry.homeIndex = 0xFF; \
            PluginRegistry::instance().registerPlugin(entry); \
        } \
    } _registrar_##ClassName

// =============================================================================
// Convenience Functions
// =============================================================================

/** @brief Get the global plugin registry */
inline PluginRegistry& getPluginRegistry() {
    return PluginRegistry::instance();
}

#endif // SUMI_PLUGIN_REGISTRY_H
