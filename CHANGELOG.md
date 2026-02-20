# SUMI Changelog

Built on [Papyrix](https://github.com/pliashkou/papyrix) by Pavel Liashkov (@bigbag), itself based on CrossPoint Reader by Dave Allie.

## [0.4.1] — 2026-02-19

**App Bug Fixes** — Fixed multiple bugs across Chess, Sudoku, and Flashcards:
- Chess: castling rights are now correctly revoked when a rook is *captured* (not just when it moves). Previously allowed illegal castling after an opponent took your rook on its home square. Also added bounds checking to `pathClear()` to prevent potential out-of-bounds access.
- Sudoku: "New personal best!" no longer shows on every win. The record flag was being checked after the best time was already overwritten, so it always matched. Also implemented pencil mode toggle (Power button while on grid) — the feature existed in UI and code paths but had no way to activate it.
- Flashcards: streak tracking now works correctly across month boundaries. Date arithmetic was using simple integer subtraction on YYYYMMDD format (Feb 1 - 1 = 20260200, not Jan 31). Replaced with proper Rata Die day counting.

**Library Carousel Expansion** — Home screen carousel now shows up to 10 recently opened books (was limited to 3). `RecentBooks` storage expanded from 5 to 10 entries. Cover art loads correctly for each book as you scroll through, with full progress info from LibraryIndex.

**Carousel Thumbnail Fix** — Removed the three-tier thumbnail caching system (in-memory G5 → flash → SD card) that was causing corrupted thumbnails with horizontal banding when scrolling between books. Cover art now re-renders from SD card on every navigation. Simpler, always correct, and still fast.

**BMP File Support** — BMP files are now visible in the file browser and open in the Images plugin. Previously `.bmp` was missing from `isSupportedFile()`, making BMP files invisible. Added routing so selecting a BMP launches the Images app instead of the reader.

**Sleep Screen Version** — Version number now displays on the sleep screen, matching the boot screen layout. Previously only the boot screen showed the version.

**Reader Power Button Fix** — Removed redundant `markDirty()` calls after screen refresh action. The display was being flagged for a second refresh unnecessarily.

**Minimum-Raggedness Line Breaking** — Text layout now uses a DP algorithm that minimizes the variance of line lengths across the paragraph, producing visually balanced justified text. DP arrays are allocated from the arena bump allocator to avoid heap fragmentation.

**Liang Hyphenation** — Ported the Liang/Knuth hyphenation system from CrossPoint. Supports 28 languages with compact trie-based pattern files. Language is auto-detected from EPUB metadata (`dc:language`). Hyphenation points are integrated into the line-breaking DP.

**Arena Bump Allocator** — The 80KB memory arena now doubles as a scratch pool via `scratchAlloc()` and `ArenaScratch` RAII guard. Text layout DP arrays, hyphenation vectors, and other temporary allocations come from the arena instead of the heap. Falls back to `std::vector` if the arena is unavailable.

**Audit Fixes** — Fixed 8 issues from static analysis: operator precedence in glyph rendering (`GfxRenderer.cpp`), null checks on ditherer calloc (`BitmapHelpers.h`), row buffer sizing for 32bpp (`GfxRenderer.h`), silent data corruption in BMP header parsing (`Bitmap.cpp`), path normalization for `.` components (`FsHelpers.cpp`), EOF handling in HTML parser (`ChapterHtmlSlimParser.cpp`), PNG zero-dimension check and integer overflow guard (`PngToBmpConverter.cpp`).

**Site: Universal Converter** — Added TIFF support via UTIF.js decoder (works in all browsers, not just Safari). Removed GIF from convertible formats. Fixed misleading size percentage display for small source files.

**Site: Image Converter** — Updated drop zone text to include TIFF format.

## [0.4.0] — 2026-02-18

**Memory Arena System** — Pre-allocated 80KB buffer pool eliminates heap fragmentation from image processing. The arena is a single contiguous allocation split into a 32KB primary buffer (time-shared between image decode and ZIP decompression) and a 48KB work buffer (row I/O, dithering, scaling, scratch). No more malloc/free cycles during cover art processing means no more "CORRUPT HEAP" crashes from fragmented memory.

**Flash Thumbnail Cache** — Cover thumbnails now persist in LittleFS (`/thumbs/`). First time you open a book, the thumbnail is saved to flash. Subsequent home screen loads pull directly from flash (~10ms) instead of decoding from SD card (~500ms). Recent books are cached, regenerated automatically if missing.

**Arena Buffer Layout (80KB total):**
- `primaryBuffer` (32KB) — Shared image decode + ZIP decompression (time-shared)
- `rowBuffer` (4KB) — Bitmap row I/O
- `ditherBuffer` (32KB) — Error diffusion for JPEG dithering
- `imageBuffer2` (4KB) — Scaling accumulators
- `scratchBuffer` (8KB) — Thumbnails, Group5 compression

Technical: `MemoryArena` class in `src/core/`, `ThumbnailCache` in `src/content/`. Arena initialized in `earlyInit()` after LittleFS mount. JPEG/PNG converters and HomeState all use arena buffers instead of per-operation malloc.

## [0.3.8] — 2026-02-18

**Image Dithering Buffer Fix** — Fixed heap corruption in cover art processing. The Atkinson and Floyd-Steinberg ditherers were overflowing their error buffers on wide images. Root cause: insufficient padding for error diffusion (needed `width + 16`, had `width + 8`) and uninitialized buffer contents. Fixed across `BitmapHelpers.h/cpp`, `JpegToBmpConverter.cpp`, `PngToBmpConverter.cpp`. Changed `new[]` to `calloc()` for zero-initialization.

## [0.3.7] — 2026-02-18

**Show Tables Toggle** — Settings → Display now has a "Show Tables" option. When enabled, tables render as column-aligned grids with borders. When disabled, tables are skipped entirely (useful for cleaner reading of table-heavy documents). Default: enabled.

**Stack Overflow Fixes** — Reduced stack usage in carousel rendering and theme loading. Moved large arrays from stack to heap, fixed recursive font loading that was blowing the stack on deeply nested theme configurations.

## [0.3.6] — 2026-02-17

**Theme Persistence** — Selected home art theme now persists across reboots. Stored in `SumiSettings` alongside other preferences.

**Carousel Memory Optimization** — Rewrote home screen carousel to use fixed-size buffers instead of dynamic allocation. Reduced heap usage by ~8KB during home screen rendering.

## [0.3.5] — 2026-02-17

**Home Art Themes** — Swappable home screen backgrounds. Settings → Home Art lets you browse themes in a 2×2 visual preview grid. Ships with ten styles (sumi-e, art nouveau, celtic, botanical, woodcut, geometric, retro 70s, doodle, art deco, nautical). Add your own as `/config/themes/yourtheme.bmp` — 800×480 1-bit BMPs in native display orientation. The theme system loads art from SD card on-the-fly, falling back to the built-in PROGMEM default.

**Library Carousel** — The home screen now tracks your recently read books. Use Up/Down to scroll through your reading history — see the title and progress of each book, then press OK to open it directly. No more digging through the file browser to find what you were reading. Recent books are stored in `/.sumi/recent.bin` and persist across reboots.

**BLE File Transfer** — Upload files wirelessly from sumi.page tools. The site's BLE transfer module connects to SUMI over Web Bluetooth (service `19B10000-...`), streams files in 512-byte chunks with sequence numbers and CRC32 verification, and writes directly to SD card. Works with Convert, Newspaper, Clipper, Flashcards, Plugins, Fonts, Images, and Optimize tools. No USB cable needed for getting content onto the device.

## [0.3.4] — 2026-02-17

Credits correction and Papyrix optimizations. Fixed README lineage (CrossPoint → Papyrix → SUMI). Adopted performance patterns from Papyrix v1.7.4: `clearWidthCache()` swap idiom, idle loop power saving, skip background caching for XTC, fragmented heap allocations in ZipFile.

## [0.3.3] — 2026-02-17

Boot loop protection and SumiBoy flash fix. The web flasher manifest had SumiBoy's offset at 0xC5000 (inside app0) instead of 0x650000 (the actual app1 partition). Anyone who flashed the emulator through the site was either corrupting SUMI or booting into garbage — causing an unrecoverable boot loop.

**Boot loop guard** — RTC memory counter tracks rapid reboots. Four restarts within 15 seconds triggers recovery: forces OTA boot partition back to app0, clears pending transitions, and resumes normal SUMI startup. Counter resets after 10 seconds of stable operation. On every SUMI boot, `ensureSumiBootPartition()` also verifies the boot partition points at app0, catching stale OTA configs from crashed emulator sessions.

**SumiBoy validation hardened** — replaced the single magic byte check (0xE9) with full ESP32 image header validation: segment count (1–32), entry point in valid ESP32-C3 address ranges (IRAM/DRAM/flash-mapped), and not-all-0xFF (erased flash). Re-validates immediately before boot. If validation fails, shows error and stays in SUMI.

**ErrorState recovery** — ErrorState previously tried to transition to FileList, which doesn't exist in READER boot mode. Now restarts into full UI mode instead of getting stuck.

**Flashcard decks visible in file browser** — `.tsv` and `.csv` files now appear in the file browser. Selecting one launches the Flashcards app directly instead of trying to open it in the reader (which would fail and trigger a restart loop through ErrorState).

**Web flasher fixes** — corrected SumiBoy manifest offset to 0x650000 (6619136 decimal). Fixed partition diagram (was showing 3MB partitions at 0x310000, actual layout is 6.25MB at 0x650000). Removed non-functional erase checkbox, added clear guidance to use "Erase device" for SUMI installs and skip it for SumiBoy. Fixed stale version/size tags.

## [0.3.2] — 2026-02-15

Table rendering and content hint pipeline. Tables now render as actual column-aligned layouts with borders instead of `[Table omitted]` — the parser collects cell data, calculates proportional column widths, and positions cell text with `+--+` grid borders. Header rows render in bold with a separator line. Cells truncate gracefully when the table is wider than the screen.

Also added content hints: EPUBs from sumi.page now embed `dc:subject` tags that tell the firmware what kind of content it's looking at. The OPF parser picks them up, stores them in the book cache (v6) and library index (v2), and the file browser renders labels like `MNGA 42%` or `NEWS` next to your reading progress. The converter, newspaper builder, and web clipper all tag their output automatically. EPUB passthrough injects `sumi:book` if nothing's there.

## [0.3.1] — 2026-02-14

Big sumi.page expansion — font converter (TTF/OTF → .bin with a live e-ink preview canvas), web clipper (URL → EPUB with Readability-style extraction), EPUB optimizer for fixing up messy third-party files. Flashcard creator now ships 37 premade language decks and an AI generator. Plugin creator has 11 templates and keyboard controls.

## [0.3.0] — 2026-02-12

File browser overhaul. New `LibraryIndex` tracks per-book progress in `/.sumi/library.bin` so the file list shows reading percentages without having to open each book. Unsupported formats get a "convert" badge pointing to sumi.page. Directories sort first, then EPUBs, then convertible files.

## [0.2.4] — 2026-02-11

BLE keyboards working. Pair in Settings, type in Notes. Spent way too long on a NimBLE issue where modifier keys were getting dropped — `onNotify` fires per-characteristic, not per-report, so the modifier byte was getting lost on multi-characteristic HID devices. Buffering the full 8-byte report before parsing fixed it. Page turners work too — PAGE_NEXT/PAGE_PREV are mapped and wired into ReaderState.

## [0.2.3] — 2026-02-10

Pulled over SUMI settings that Papyrix didn't have — show images toggle, hyphenation, paragraph indent levels, more font size steps (12 instead of 8). Added settings pages for Plugins and Bluetooth. Binary format stays compatible with old settings.bin.

## [0.2.2] — 2026-02-09

Second batch of plugins. Solitaire, Minesweeper, Checkers, TodoList, ToolSuite, Cube3D. Solitaire card overlap was leaving partial refresh artifacts — ended up tracking dirty rects per stack instead of per card. ToolSuite shows heap stats and SD benchmark which has been useful for catching memory issues.

## [0.2.1] — 2026-02-07

First plugins running — Chess, Sudoku, Flashcards, Notes, Images, Maps. Chess legal move validation is ~800 lines on its own thanks to castling-through-check and en passant edge cases. Sudoku's cell-first navigation with a valid-number popup actually works really well on the 5-way d-pad.

## [0.2.0] — 2026-02-06

Plugin system. `PluginInterface` base class, `PluginRenderer` with GxEPD2-compatible drawing API, `PluginHostState` for sandboxed execution. Any `.h` dropped in `src/plugins/` and registered in main.cpp becomes a launchable app. No plugins yet at this point, just proving the architecture compiles and the host state doesn't break the reader.

## [0.1.2] — 2026-02-04

Home screen and branding. Replaced the Papyrix home with sumi-e ink art — Atkinson-dithered down to 1-bit, stored as 48KB PROGMEM. Cover art, title, progress, and battery render as overlays on top. New boot splash. Updated file entry styling and menu spacing.

## [0.1.1] — 2026-02-03

Ripped out WiFi. Removed the network driver, web server, credential store, Calibre sync, and all the network/sync states and views — 21 files, ~4,400 lines plus the Calibre lib. Clean compile, no WiFi references left. Frees up ~100KB of heap for the plugin system and BLE.

## [0.1.0] — 2026-02-01

Fresh start on Papyrix 1.6.5. Renamed `namespace papyrix` → `sumi`, all defines and paths updated, `PapyrixSettings` → `SumiSettings` with binary compat. Cache directory migrates from `.papyrix/` to `.sumi/` on first boot. Compiles and runs on X4 hardware out of the box.
