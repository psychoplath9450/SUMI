# Settings Reference

*Last updated: January 22, 2026*

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
| bootToLastBook | false | Skip home screen, open last-read book |

## Reader Settings

These sync between the on-device menu and the web portal.

| Setting | Default | Options |
|---------|---------|---------|
| fontSize | Medium | Small, Medium, Large |
| margins | Normal | Narrow, Normal, Wide |
| lineSpacing | Normal | Compact, Normal, Relaxed |
| justifyText | true | true/false |
| sceneBreakSpacing | 30px | 0-60px spacing for `<hr>` tags |

The actual pixel values:

| Font Size | Height |
|-----------|--------|
| Small | 18px |
| Medium | 22px |
| Large | 28px |

| Margins | Pixels |
|---------|--------|
| Narrow | 5px |
| Normal | 10px |
| Wide | 20px |

## Bookmarks

Up to 20 bookmarks can be saved per book. Bookmarks are stored in the book's cache directory:
- Location: `/.sumi/books/{hash}/bookmarks.bin`
- Access from: Settings menu while reading → View Bookmarks
- Add: Settings menu → Add Bookmark Here

## Reading Statistics

Track your reading progress across all books:
- **Lifetime stats:** Total pages read, hours spent reading, books finished
- **Session stats:** Pages and time for current session
- **Location:** `/.sumi/reading_stats.bin`
- **Reset:** Available in web portal under Reader Settings

## KOReader Sync Settings

Sync reading progress with KOReader devices and apps.

| Setting | Default | Description |
|---------|---------|-------------|
| kosyncEnabled | false | Enable progress sync |
| kosyncUrl | "" | Sync server URL (e.g., https://sync.koreader.rocks) |
| kosyncUser | "" | Username for sync server |
| kosyncPass | "" | Password (stored locally only) |

## Bluetooth Settings

| Setting | Default | Description |
|---------|---------|-------------|
| enabled | false | Enable Bluetooth |
| autoConnect | true | Auto-connect to paired devices on boot |
| keyboardLayout | US | Keyboard layout (US, UK, DE, FR, ES, IT) |

Page turner key mapping:
- Next page: Right Arrow, Space
- Previous page: Left Arrow, Backspace

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

## Backup & Restore

Export all settings to a JSON file from the portal About page. Includes:
- Display settings
- Reader settings
- Weather location
- Sync configuration
- WiFi networks (passwords excluded)
- Home screen layout

Restore by uploading a backup file. Settings take effect after page reload.

## Auto-Save

Settings auto-save 5 seconds after any change. You don't need to manually save unless you want changes to apply immediately.

## Factory Reset

From the Settings app or the web portal, you can reset to defaults. This wipes `/.config/settings.json` but keeps your books, flashcards, etc.

WiFi credentials in NVS are preserved - you'll need to re-run setup wizard to clear those.
