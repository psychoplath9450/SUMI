# Writing Plugins for Sumi

Plugins are self-contained apps that run from the home screen. Each one handles its own drawing, button input, and state. Plugins are allocated when opened and freed when closed to save RAM.

## Memory Model

Plugins are **not** global instances. They're allocated on-demand:

```
User taps "Chess" on home screen
    ↓
AppLauncher calls: runPluginAllocSimple<ChessGame>("Chess")
    ↓
ChessGame* plugin = new ChessGame()  // Allocated
plugin->init(w, h)
plugin->draw()
// ... user plays ...
delete plugin                         // Freed when user exits
    ↓
Back to home screen (plugin memory reclaimed)
```

This means your plugin class must have a working default constructor and destructor.

## Basic Structure

You need two files:

```
include/plugins/MyPlugin.h   # the class definition
src/plugins/MyPlugin.cpp     # can be minimal (just includes header)
```

### Minimal Example

```cpp
// include/plugins/MyPlugin.h
#ifndef SUMI_PLUGIN_MYPLUGIN_H
#define SUMI_PLUGIN_MYPLUGIN_H

#include "config.h"
#include <Arduino.h>
#include <GxEPD2_BW.h>

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class MyPlugin {
public:
    MyPlugin() : screenW(800), screenH(480) {}
    ~MyPlugin() {}  // Clean up any resources here
    
    void init(int w, int h) {
        screenW = w;
        screenH = h;
    }
    
    bool handleInput(Button btn) {
        if (btn == BTN_BACK) {
            return false;  // exit
        }
        // handle other buttons...
        return true;  // keep running
    }
    
    void draw() {
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            display.setCursor(20, 40);
            display.print("Hello!");
        } while (display.nextPage());
    }
    
private:
    int screenW, screenH;
};

#endif
```

```cpp
// src/plugins/MyPlugin.cpp
#include "plugins/MyPlugin.h"
#if FEATURE_GAMES  // Or whatever feature flag applies
// No global instance needed - plugin is allocated on demand
#endif
```

## Hooking It Up

### 1. Add to HomeItems.h

```cpp
#define HOME_ITEM_MYPLUGIN 13

static const HomeItemInfo HOME_ITEMS[] = {
    // ...existing stuff...
    { HOME_ITEM_MYPLUGIN, "myplugin", "My Plugin", "M", "tools" },
};
```

### 2. Add to AppLauncher.cpp

```cpp
#include "plugins/MyPlugin.h"

// in openAppByItemIndex():
case HOME_ITEM_MYPLUGIN:
    runPluginAllocSimple<MyPlugin>("My Plugin");  // Allocated on open, freed on close
    return;
```

Note: Use the appropriate runner template:
- `runPluginAllocSimple<T>()` - Standard plugins
- `runPluginAllocSelfRefresh<T>()` - Plugins with partial refresh (needs `needsFullRedraw`, `drawPartial()`, `update()`)
- `runPluginAllocAnimation<T>()` - Animation plugins (needs `isRunning()`, `drawFullScreen()`)

## Saving State

Since plugins are deleted when closed, save state in your destructor:

```cpp
class MyPlugin {
public:
    MyPlugin() { loadState(); }   // Load on create
    ~MyPlugin() { saveState(); }  // Save on destroy
    
private:
    void saveState() {
        File f = SD.open("/data/myplugin.json", FILE_WRITE);
        if (f) {
            JsonDocument doc;
            doc["score"] = score;
            doc["level"] = level;
            serializeJson(doc, f);
            f.close();
        }
    }
    
    void loadState() {
        File f = SD.open("/data/myplugin.json");
        if (f) {
            JsonDocument doc;
            if (!deserializeJson(doc, f)) {
                score = doc["score"] | 0;
                level = doc["level"] | 1;
            }
            f.close();
        }
    }
};
```

## E-Ink Tips

E-ink is slow and ghosty. A few things help:

**Use partial refresh for small changes:**
```cpp
display.setPartialWindow(x, y, width, height);
display.firstPage();
do {
    // only draw the changed area
} while (display.nextPage());
```

**Do a full refresh every so often:**
```cpp
int partialCount = 0;
if (++partialCount >= 15) {
    display.setFullWindow();
    partialCount = 0;
}
```

**Don't update constantly.** Only redraw when something actually changes - after a button press, not in a loop.

## Feature Flags

Wrap your plugin so it can be compiled out:

```cpp
// In header
#include "config.h"
#if FEATURE_GAMES

class MyPlugin { ... };

#endif
```

```cpp
// In AppLauncher.cpp
#if FEATURE_GAMES
case HOME_ITEM_MYPLUGIN:
    runPluginAllocSimple<MyPlugin>("My Plugin");
    return;
#endif
```

## Good Examples

Check out these for reference:

- `Sudoku.h` - Multi-screen app with menus, settings, statistics
- `ChessGame.h` - Game with difficulty levels, captured pieces, move history
- `Weather.h` - Network requests, multi-screen navigation, settings
- `Flashcards.h` - File loading, multiple formats, destructor cleanup
- `Cube3D.h` - Animation plugin with menu system
- `Library.h` - Complex plugin that allocates subsystems (TextLayout, PageCache)

## UI Design Patterns

Our apps follow consistent design patterns for a cohesive experience:

### Screen Structure

**Header Bar (48px, black background):**
```cpp
display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
display.setTextColor(GxEPD_WHITE);
display.setFont(&FreeSansBold12pt7b);
centerText("App Title", screenW / 2, 34);
```

**Content Area:**
- Use cards with rounded corners: `display.drawRoundRect(16, y, screenW - 32, height, 8, GxEPD_BLACK)`
- 16-20px padding from screen edges
- 8-12px gaps between elements

**Footer Bar (hints/navigation):**
```cpp
display.setFont(&FreeSans9pt7b);
centerText("Up/Down: Select  •  OK: Confirm", screenW / 2, screenH - 12);
```

### Menu Items

**Selected item (inverted):**
```cpp
display.fillRoundRect(16, y, screenW - 32, 52, 8, GxEPD_BLACK);
display.setTextColor(GxEPD_WHITE);
```

**Unselected item (outline):**
```cpp
display.drawRoundRect(16, y, screenW - 32, 52, 8, GxEPD_BLACK);
display.setTextColor(GxEPD_BLACK);
```

### Toggle Switches

```cpp
int sw = 44, sh = 24;  // Switch dimensions
int sx = screenW - 70;  // Right-aligned

if (enabled) {
    display.fillRoundRect(sx, sy, sw, sh, sh / 2, GxEPD_BLACK);
    display.fillCircle(sx + sw - sh / 2, sy + sh / 2, 8, GxEPD_WHITE);
} else {
    display.drawRoundRect(sx, sy, sw, sh, sh / 2, GxEPD_BLACK);
    display.fillCircle(sx + sh / 2, sy + sh / 2, 8, GxEPD_BLACK);
}
```

### Multi-Screen Navigation

Use an enum for screens and a switch in handleInput/draw:

```cpp
enum AppScreen : uint8_t {
    SCREEN_MAIN_MENU,
    SCREEN_PLAYING,
    SCREEN_SETTINGS,
    SCREEN_VICTORY
};

AppScreen currentScreen;
int menuCursor;

bool handleInput(Button btn) {
    switch (currentScreen) {
        case SCREEN_MAIN_MENU: return handleMenuInput(btn);
        case SCREEN_PLAYING: return handlePlayingInput(btn);
        // ...
    }
}

void draw() {
    switch (currentScreen) {
        case SCREEN_MAIN_MENU: drawMainMenu(); break;
        case SCREEN_PLAYING: drawPlayingScreen(); break;
        // ...
    }
}
```

### Statistics & Persistence

Store stats in binary files with magic number validation:

```cpp
struct AppStats {
    uint32_t magic = 0x41505053;  // "APPS"
    uint16_t gamesPlayed = 0;
    uint16_t wins = 0;
    uint16_t currentStreak = 0;
    uint8_t reserved[16];
    
    bool isValid() const { return magic == 0x41505053; }
};

void saveStats() {
    SD.mkdir("/.sumi");
    File f = SD.open("/.sumi/app_stats.bin", FILE_WRITE);
    if (f) {
        f.write((uint8_t*)&stats, sizeof(AppStats));
        f.close();
    }
}
```

### Common UI Elements

**Section Headers:**
```cpp
display.setFont(&FreeSans9pt7b);
display.setTextColor(GxEPD_BLACK);
display.setCursor(20, y);
display.print("SECTION TITLE");
y += 20;
```

**Stats Grid:**
```cpp
int boxW = (screenW - 60) / 3;
for (int i = 0; i < 3; i++) {
    int bx = 20 + i * (boxW + 10);
    display.drawRoundRect(bx, y, boxW, 56, 6, GxEPD_BLACK);
    // Centered value and label
}
```

**Progress Bar:**
```cpp
display.drawRect(16, y, screenW - 32, 8, GxEPD_BLACK);
int fillW = ((screenW - 34) * progress) / maxProgress;
display.fillRect(17, y + 1, fillW, 6, GxEPD_BLACK);
```

### Helper Functions

Add a centerText helper to your class:

```cpp
void centerText(const char* text, int x, int y) {
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(x - tw / 2, y);
    display.print(text);
}
```

## Widgets

Widgets are different from plugins - they're small data displays on the home screen that show information at a glance.

See [WIDGETS.md](WIDGETS.md) for:
- How widgets work
- Creating new widgets
- API considerations for data-fetching widgets
- Widget navigation and activation
