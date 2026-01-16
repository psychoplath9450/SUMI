# Settings Reference

Settings are stored in two places:

- **NVS (flash):** WiFi credentials and setup-complete flag - survives factory reset
- **SD card:** Everything else, in `/.config/settings.json`

## Display Settings

| Setting | Default | Description |
|---------|---------|-------------|
| orientation | 0 | 0=landscape, 1=portrait |
| sleepMinutes | 5 | Auto-sleep timeout (0=never) |
| fullRefreshPages | 15 | Pages between full e-ink refresh |
| showBatteryHome | true | Show battery on home screen |
| showClockHome | true | Show time on home screen |

## Reader Settings

These sync between the on-device menu and the web portal.

| Setting | Default | Options |
|---------|---------|---------|
| fontSize | Medium | Small, Medium, Large |
| margins | Normal | Narrow, Normal, Wide |
| lineSpacing | Normal | Compact, Normal, Relaxed |
| justifyText | true | true/false |

The actual pixel values:

| Font Size | Height |
|-----------|--------|
| Small | 18px |
| Medium | 22px |
| Large | 28px |

| Margins | Pixels |
|---------|--------|
| Narrow | 10px |
| Normal | 20px |
| Wide | 35px |

## Weather Settings

| Setting | Default | Description |
|---------|---------|-------------|
| latitude | 0.0 | Auto-detected on first use |
| longitude | 0.0 | Auto-detected on first use |
| location | "" | City name (auto-detected) |
| celsius | false | Temperature unit |
| updateHours | 3 | How often to refresh |

## Home Screen

The home screen shows a customizable set of apps. In the portal, you can drag to reorder and toggle which apps appear.

Default apps: Library, Flashcards, Chess, Sudoku, Weather, Settings

Settings is always shown and can't be removed.

## Auto-Save

Settings auto-save 5 seconds after any change. You don't need to manually save unless you want changes to apply immediately.

## Factory Reset

From the Settings app or the web portal, you can reset to defaults. This wipes `/.config/settings.json` but keeps your books, flashcards, etc.

WiFi credentials in NVS are preserved - you'll need to re-run setup wizard to clear those.
