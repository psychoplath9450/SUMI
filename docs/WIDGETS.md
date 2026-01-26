# Home Screen Widgets

*Last updated: January 23, 2026*

Widgets are small information displays on the home screen that show data at a glance without opening an app.

## Current Widgets

### Book Widget
Shows your current book's cover for quick access to resume reading.

**Display:**
- Large book cover image (scaled and dithered for e-ink)
- Clean design - just the cover, no borders or progress bars
- When selected, shows subtle highlight border

**Activation:** Tap to continue reading at your last position

**Data source:** `/.sumi/lastbook.bin` - Updated automatically when you read

### Weather Widget
Shows current weather conditions and date at a glance.

**Display (Portrait - All 3 widgets shown):**
- Location name (e.g., "Breckenridge, CO")
- Weather condition icon (sun, cloud, rain, snow, storm)
- Current temperature with unit (°F or °C)
- Humidity percentage
- Full day of week (e.g., "Wednesday")
- Date (e.g., "January 22")

**Display (Landscape):**
- Weather icon
- Temperature
- Location name (abbreviated)

**Activation:** Tap to open full Weather app with 7-day forecast

**Data source:** `/.sumi/weather_cache.bin` - Updated by Weather app

**Note:** Date/time display requires the device to have synced time via WiFi at least once.

### Orientation Widget
Quick toggle between portrait and landscape modes.

**Display:**
- Slim toggle switch
- Knob position indicates current mode (left = landscape, right = portrait)
- Minimal, clean design

**Activation:** Tap to instantly rotate the display

**Note:** This saves your preference and the display rotates immediately.

## Widget Layout

The widget layout uses a two-column design with the book cover taking the left half and weather/orientation stacked on the right.

### Portrait Mode
```
┌─────────────────────────────┐
│                       100%  │  Status bar
├─────────────┬───────────────┤
│             │ Breckenridge  │  Location
│             │ ☀️  72°F      │  Icon + Temp
│   ┌───────┐ │ 45% humidity  │  Humidity
│   │ BOOK  │ │ Wednesday     │  Day of week
│   │ COVER │ │ January 22    │  Date
│   │ (big) │ ├───────────────┤
│   └───────┘ │    ○━━━●     │  Orientation toggle
│             │               │
├─────────────┴───────────────┤
│  Library    │   Flashcards  │
├─────────────┼───────────────┤
│    ...      │      ...      │
```

- **Left column:** Large book cover (tap to resume reading)
- **Right column (stacked):**
  - Weather: Location, icon, temp, humidity, day, date (70% height)
  - Orientation: Slim toggle switch (30% height)

### Landscape Mode
```
┌──────────────────────────────────────────────┐
│                                        100%  │
├────────────────┬─────────────────────────────┤
│                │                             │
│   BOOK COVER   │  Library │ Flashcards│ ... │
│    (large)     ├──────────┼───────────┼─────┤
│                │  Sudoku  │   Demo    │ ... │
├────────┬───────┼──────────┴───────────┴─────┤
│☀️ 72°F │ ●━━○  │                             │
│ City   │       │                             │
└────────┴───────┴─────────────────────────────┘
   Weather  Orient
```

- **Top left:** Large book cover
- **Bottom left:** Weather (icon + temp + location) + Orientation toggle side by side
- **Right:** App grid

## Widget Visibility

All widgets can be individually enabled/disabled in the Portal:
1. Go to **Home Screen** tab
2. Click **Widgets** sub-tab
3. Toggle each widget on/off

All widgets are enabled by default.

## Widget Navigation

Navigation depends on screen orientation:

### Portrait Mode
From the home screen:
1. Press **UP** from the top row of apps to enter widget selection
2. Use **LEFT/RIGHT** to navigate between widgets horizontally
3. Press **CONFIRM** to activate the selected widget
4. Press **BACK** or **DOWN** to return to app grid

### Landscape Mode
From the home screen:
1. Press **UP** from the top row of apps to enter widget selection
2. Use **UP/DOWN** to navigate between widgets vertically
3. Press **CONFIRM** to activate the selected widget
4. Press **BACK** or **DOWN** (from bottom widget) to return to app grid

Selected widgets show with inverted colors (white on black) or a highlight border.

## Widget Data Structures

### LastBookWidget (HomeScreen.cpp)
```cpp
struct LastBookWidget {
    uint32_t magic = 0x4C415354;  // "LAST"
    char title[64];               // Book title
    char coverPath[96];           // Path to cover image
    float progress;               // 0.0-1.0 percent
    bool valid;
};
```

### WeatherWidget (HomeScreen.cpp)
```cpp
struct WeatherWidget {
    uint32_t magic = 0x57545852;  // "WTXR"
    float temperature;            // Current temp (in user's preferred unit)
    int weatherCode;              // WMO weather code
    int humidity;                 // Percentage
    float wind;                   // Wind speed
    char location[32];            // City name
    bool useCelsius;              // Temperature unit preference
    uint32_t timestamp;           // When data was fetched
};
```

## Creating a New Widget

### 1. Define the data structure

```cpp
struct MyWidget {
    uint32_t magic = 0x4D595744;  // Unique 4-char identifier
    // Your data fields...
    bool valid;
};
```

### 2. Add cache file path

```cpp
#define MY_WIDGET_CACHE "/.sumi/mywidget_cache.bin"
```

### 3. Add load/save functions

```cpp
void loadMyWidget() {
    File f = SD.open(MY_WIDGET_CACHE, FILE_READ);
    if (f) {
        f.read((uint8_t*)&myWidget, sizeof(MyWidget));
        f.close();
        if (myWidget.magic != 0x4D595744) {
            myWidget.valid = false;
        }
    }
}

void saveMyWidget() {
    File f = SD.open(MY_WIDGET_CACHE, FILE_WRITE);
    if (f) {
        f.write((uint8_t*)&myWidget, sizeof(MyWidget));
        f.close();
    }
}
```

### 4. Add to drawWidgets() in HomeScreen.cpp

The widget drawing code is in `drawWidgets()`. Follow the two-column layout pattern:
- Portrait: Book on left, others stacked on right
- Landscape: Book on left within 160px widget area, others stacked beside it

### 5. Update widget from your plugin

When your plugin has new data, save it to the cache file. The home screen will read it on next display.

## API Widget Considerations

For widgets that fetch data from the internet:

### Good API characteristics:
- **Free tier** with no API key (or easy signup)
- **Low rate limits OK** - widgets refresh infrequently
- **Returns small JSON** - ESP32 has limited RAM
- **HTTPS support** - most APIs require it
- **Stable endpoints** - won't break frequently

### Timing considerations:
- E-ink updates are slow (~1-2 seconds)
- Widgets should cache data, not fetch on every draw
- Fetch in background or when app is opened
- Show cached data immediately, update async

### Example free APIs suitable for widgets:

| API | Data | URL | Auth |
|-----|------|-----|------|
| ZenQuotes | Quote of the day | zenquotes.io/api/today | None |
| icanhazdadjoke | Random joke | icanhazdadjoke.com/api | None |
| Sunrise-Sunset | Sun times | sunrise-sunset.org/api | None |
| Open-Meteo | Weather | open-meteo.com/api | None |
| Numbers API | Math facts | numbersapi.com | None |
| Wikipedia OTDH | This day in history | api.wikimedia.org | None |

### Network code pattern:

```cpp
void fetchWidgetData() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    HTTPClient http;
    http.begin("https://api.example.com/data");
    http.addHeader("Accept", "application/json");
    
    int code = http.GET();
    if (code == 200) {
        String json = http.getString();
        // Parse and save to cache...
    }
    http.end();
}
```

## Widget Best Practices

1. **Cache aggressively** - Don't fetch on every home screen draw
2. **Fail gracefully** - Show placeholder if data unavailable
3. **Keep it simple** - Widgets are glanceable, not interactive
4. **Respect the display** - High contrast, clear icons, minimal text
5. **Update sensibly** - Once per app open or on manual refresh
6. **Handle offline** - Always show something even without network

## Files Involved

| File | Purpose |
|------|---------|
| `src/core/HomeScreen.cpp` | Widget rendering and navigation |
| `include/core/HomeScreen.h` | Widget exports and function declarations |
| `/.sumi/*.bin` | Widget cache files on SD card |
