# Writing Plugins for Sumi

Plugins are self-contained apps that run from the home screen. Each one handles its own drawing, button input, and state.

## Basic Structure

You need two files:

```
include/plugins/MyPlugin.h   # the class definition
src/plugins/MyPlugin.cpp     # just instantiates it
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
    
    void init(int w, int h) {
        screenW = w;
        screenH = h;
    }
    
    bool handleButton(Button btn) {
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

extern MyPlugin myPlugin;

#endif
```

```cpp
// src/plugins/MyPlugin.cpp
#include "plugins/MyPlugin.h"
MyPlugin myPlugin;
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

// in openApp():
case HOME_ITEM_MYPLUGIN:
    runPluginSimple(myPlugin, "My Plugin");
    break;
```

### 3. Include in main.cpp

```cpp
#include "plugins/MyPlugin.h"
```

## Saving State

Most plugins want to remember things between runs:

```cpp
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
#include "config.h"
#if FEATURE_GAMES

class MyPlugin { ... };
extern MyPlugin myPlugin;

#endif
```

And in AppLauncher:

```cpp
#if FEATURE_GAMES
case HOME_ITEM_MYPLUGIN:
    runPluginSimple(myPlugin, "My Plugin");
    break;
#endif
```

## Good Examples

Check out these for reference:

- `Sudoku.h` - grid-based game with partial refresh
- `Flashcards.h` - file loading, multiple formats
- `Weather.h` - network requests, JSON parsing
