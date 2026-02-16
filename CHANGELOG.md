# SUMI Changelog

Built on [Papyrix](https://github.com/crosspoint-reader/crosspoint-reader) 1.6.5 by Dave Allie.

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
