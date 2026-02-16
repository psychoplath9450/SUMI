# SUMI

Custom e-ink firmware for the **Xteink X4** — a $70 ESP32-C3 e-reader with 380KB of RAM, a 480×800 display, and five buttons.

SUMI turns it into an offline-first reader with apps, Bluetooth keyboards, and a Game Boy emulator. No WiFi, no cloud, no accounts. Books and tools on paper-like glass.

<p align="center">
  <img src="images/home-screen.jpg" width="300" alt="SUMI home screen showing The Wonderful Wizard of Oz with sumi-e ink art border" />
</p>

Built on [Papyrix](https://github.com/crosspoint-reader/crosspoint-reader) by Dave Allie — an excellent open-source reader firmware for the CrossPoint Reader. SUMI strips out WiFi, adds a plugin system, games, productivity apps, Bluetooth, and a visual rebrand. The reading engine is almost entirely Dave's work and it's really good.

Companion tools at **[sumi.page](https://sumi.page)** — browser-based converters, a newspaper builder, flashcard creator, font converter, and more.

---

## What it does

**Reads books.** EPUB, TXT, Markdown, XTC. Adjustable fonts, sizes, margins, line spacing, alignment. Chapter navigation, progress tracking, cover art, thumbnails, CSS styling, inline images, RTL for Arabic and Hebrew, hyphenation, table rendering. The reading experience is Papyrix and it's solid.

**Runs apps.** 13 plugins and counting — a sandboxed plugin system where any `.h` file dropped into `src/plugins/` becomes a launchable app.

<p align="center">
  <img src="images/apps.jpg" width="300" alt="SUMI apps list: Chess, Sudoku, Minesweeper, Checkers, Solitaire, Benchmark, SumiBoy, Notes, Todo List, Tools" />
</p>

**Types.** Pair any BLE keyboard and type in the Notes editor. Page turners work in the reader too.

<p align="center">
  <img src="images/ble-notes.jpg" width="500" alt="SUMI Notes app with a Bluetooth folding keyboard" />
</p>

**Plays Game Boy.** SumiBoy is a dual-boot launcher — it checks the app1 partition for emulator firmware and OTA-swaps to it. The emulator swaps back the same way. Pokemon on an e-ink screen runs at about 3 FPS and it's still somehow fun.

<p align="center">
  <img src="images/sumiboy.jpg" width="300" alt="Pokemon running on the e-ink display through SumiBoy" />
  &nbsp;&nbsp;
  <img src="images/sumiboy-launch.jpg" width="300" alt="SumiBoy launch screen" />
</p>

---

## What comes from where

SUMI wouldn't exist without Papyrix. Here's an honest accounting.

### Papyrix: the reader foundation (~85,600 lines, 83%)

The EPUB engine (streaming XML parser, ZIP decompression, OPF metadata, TOC parsing, CSS, HTML-to-pages renderer, inline images, text layout, hyphenation, justification, RTL). ReaderState (pagination, background caching, spine navigation, progress). The rendering stack (1-bit graphics, bitmap fonts, dithering, fax compression). Text shaping (Arabic, Thai, script detection). The state machine, content providers, all the libraries. This is the hard code and Dave wrote it.

### SUMI removed WiFi (~4,400 lines)

The ESP32-C3 has 380KB of RAM and WiFi eats ~100KB while fragmenting the heap. Removing it buys enough memory for plugins and BLE. Gone: WiFi driver, web server, credential store, Calibre OPDS sync, network states.

### SUMI added plugins, BLE, and content awareness (~17,700 lines)

The plugin framework (~1,000 lines), 13 plugins (~10,300 lines), BLE HID input (477 lines), content hint pipeline (~100 lines), library index (202 lines), home screen art, expanded settings, and file browser upgrades.

### Numbers

```
                          Papyrix 1.6.5       SUMI
                          ─────────────       ────
Application code (src/)    14,630 lines     28,365 lines
Libraries (lib/)           75,873 lines     74,977 lines
Total                      90,503 lines    103,342 lines

From Papyrix:             ~85,600  (83%)
From SUMI:                ~17,700  (17%)
```

In the application layer (`src/`), SUMI wrote ~58%. But Papyrix's 42% is the reader engine, content providers, state machine, and rendering pipeline — the code that makes the device actually work as an e-reader.

---

## Hardware

| | |
|---|---|
| **MCU** | ESP32-C3 (RISC-V, single core, 160MHz) |
| **RAM** | 380KB usable (WiFi disabled) |
| **Display** | 480×800 e-ink, 1-bit, SSD1677 controller |
| **Storage** | SD card (FAT32) |
| **Input** | 5-way d-pad + power button |
| **Connectivity** | Bluetooth Low Energy 5.0 |

## SD Card

Drop your EPUBs anywhere — the file browser walks everything from root. These folders are either auto-created or optional:

```
/
├── (your books, anywhere)
├── flashcards/         ← .tsv decks (auto-created by plugin)
├── images/             ← .bmp files for Image Viewer (auto-created)
├── sleep/              ← .bmp files for random sleep screens (480×800)
├── maps/               ← Map tile BMPs (auto-created)
├── notes/              ← Notes plugin saves here (auto-created)
├── config/fonts/       ← Custom .bin fonts from sumi.page/fonts
└── .sumi/              ← System, don't touch
```

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
pio run                    # Build
pio run -t upload          # Flash via USB
pio device monitor         # Serial console
```

Or flash from Chrome: **[sumi.page/flasher](https://sumi.page/flasher/)**

## Plugin development

```cpp
class MyPlugin : public PluginInterface {
public:
    const char* name() const override { return "My Plugin"; }
    void init(PluginRenderer& r) override { /* setup */ }
    bool update(PluginRenderer& r, PluginButton btn) override {
        r.fillScreen(1);
        r.setCursor(10, 30);
        r.print("Hello from my plugin!");
        r.display();
        return true;
    }
};
```

Register in `main.cpp`, or scaffold from templates at **[sumi.page/plugins](https://sumi.page/plugins/)**.

## Memory

380KB. Everything streams — EPUB parsed via streaming XML, never loaded fully. Page layouts cached to SD. Thumbnails fax-compressed (2-4KB vs 48KB). Library index at 9 bytes per book. One content provider allocated at a time. No WiFi saves ~100KB of heap.

## Credits

SUMI is built on **Papyrix** by **Dave Allie**. The reader engine, EPUB parser, rendering pipeline, font system, text shaping, and state machine are all his work. Outstanding open-source software.

- [Papyrix / CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) (MIT License)
- **@ngxson** — Power management and button remapping
- **sumi.page** — Companion web tools

## License

MIT — see [LICENSE](LICENSE). Original Papyrix code copyright Dave Allie. SUMI additions copyright SUMI Contributors.
