# Weather Icons for SUMI

These 1-bit BMP icons are used by the weather widget on the home screen.

## Installation

Copy the entire `weather` folder to the root of your SD card:
```
SD Card Root/
├── weather/
│   ├── sunny_48.bmp
│   ├── sunny_64.bmp
│   ├── cloudy_48.bmp
│   ├── cloudy_64.bmp
│   ├── partlycloudy_48.bmp
│   ├── partlycloudy_64.bmp
│   ├── rain_48.bmp
│   ├── rain_64.bmp
│   ├── snow_48.bmp
│   ├── snow_64.bmp
│   ├── fog_48.bmp
│   ├── fog_64.bmp
│   ├── showers_48.bmp
│   ├── showers_64.bmp
│   ├── thunderstorm_48.bmp
│   └── thunderstorm_64.bmp
```

## Icon Mapping (WMO Weather Codes)

| Icon | WMO Codes | Description |
|------|-----------|-------------|
| sunny | 0 | Clear sky |
| partlycloudy | 1-2 | Mainly clear, partly cloudy |
| cloudy | 3 | Overcast |
| fog | 45-49 | Fog, depositing rime fog |
| rain | 51-65 | Drizzle, rain |
| snow | 71-77 | Snow, snow grains |
| showers | 80-82 | Rain showers |
| thunderstorm | 95-99 | Thunderstorm, thunderstorm with hail |

## Note

If the icons are not found, the widget will fall back to programmatically-drawn icons.
