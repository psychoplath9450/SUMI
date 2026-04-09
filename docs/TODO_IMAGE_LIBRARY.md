# SUMI Future Improvements - Image & Library System

## Status: v0.4.1 Complete

What we implemented:
- ✅ Memory Arena (76KB: 32+20+24, releasable for BLE)
- ✅ Flash Thumbnail Cache (instant home screen for opened books)
- ✅ Arena migration for JPEG/PNG converters
- ✅ HomeState uses arena scratchBuffer
- ✅ **JPEGDEC Integration** - Replaced picojpeg with modern decoder
- ✅ **ZipFile Arena Migration** - zipBuffer alias provides LZ77 dictionary

---

## ✅ JPEGDEC Integration (DONE — v0.4.1)

Replaced picojpeg (2011 vintage) with JPEGDEC by Larry Bank (bitbank2).

### Benefits
- Built-in Floyd-Steinberg dithering for 1/2/4-bit output
- 2x faster decoding (ESP32 benchmarks)
- Actively maintained (latest release Nov 2025)
- Simpler API - no manual dithering classes needed for JPEG

### Implementation
- Added `bitbank2/JPEGDEC @ ^1.6.2` to platformio.ini lib_deps
- Rewrote JpegToBmpConverter.cpp to use JPEGDEC API
- Old picojpeg code backed up as `.picojpeg.bak`

### Dithering
- `setPixelType(TWO_BIT_DITHERED)` for 4 gray levels
- `decodeDither()` handles Floyd-Steinberg internally
- No more manual ditherer allocation for JPEG

---

## ✅ ZipFile Arena Migration (DONE)

`zipBuffer` is now an alias for `primaryBuffer` in the arena. `Epub::readItemContentsToStream()` passes `MemoryArena::zipBuffer` as the `dictBuffer` parameter to `ZipFile::readFileToStream()`. This provides the 32KB LZ77 dictionary from arena memory instead of `malloc(32KB)`. Falls back to heap malloc when arena is unavailable.

---

## TODO: Library Cover Scanning

**Goal:** Extract covers from all EPUBs without opening each book manually.

### Approach A: Background Scan on Boot
```
Boot → Home screen loads
         ↓
Background task starts
         ↓
For each EPUB in library:
  - Check if cover.bmp exists in /.sumi/epub_XXXX/
  - If not: extract cover.jpg, convert to BMP, generate thumbnail
  - Save to flash cache
         ↓
Home screen refreshes with new covers
```

### Approach B: Manual "Scan Library" in Settings
- Settings → Library → Scan for Covers
- Shows progress bar
- User-initiated, doesn't slow boot

### Approach C: Lazy Scan on File Browser
- When browsing a folder, extract covers for visible EPUBs
- Covers appear as you scroll through library
- No full scan, just what you see

### Implementation Notes
- Need to extract cover without fully parsing EPUB (just grab cover.jpg from ZIP)
- Could use arena buffers (primaryBuffer for decode, scratchBuffer for thumbnail)
- Flash cache already handles storage
- Consider: limit to N covers per session to avoid blocking UI

---

## TODO: Home Screen as Library Carousel

**Goal:** Home screen shows carousel of ALL books with covers, not just 10 recent.

### Current State
```
Home Screen:
┌─────────────────────┐
│  [Current Book]     │  ← Large card with cover
│  Title / Author     │
│  Progress bar       │
├─────────────────────┤
│  Up/Down to scroll  │  ← 10 recent books
└─────────────────────┘
```

### Future Vision
```
Library Carousel:
┌─────────────────────┐
│      ← PREV         │
│  ┌───────────────┐  │
│  │   [COVER]     │  │  ← Full cover image
│  │               │  │
│  │               │  │
│  └───────────────┘  │
│  Title              │
│  Author             │
│  Progress: 45%      │
│      NEXT →         │
└─────────────────────┘

Up/Down: Scroll through ALL books
OK: Open book
Left: Jump to A-M
Right: Jump to N-Z
```

### Implementation Steps
1. LibraryIndex already tracks all books with metadata
2. Add cover path to LibraryIndex entries
3. Modify HomeState to paginate through full library
4. Load/display covers using flash cache
5. Pre-fetch adjacent covers for smooth scrolling

### Dependencies
- Library Cover Scanning (need covers for all books)
- May need larger flash cache (currently limited to recent books)
- Consider: virtual scrolling, only load visible covers

---

## Priority Order

1. **Library Cover Scanning** - Required for full carousel
2. **Home as Library Carousel** - Big UX improvement

---

## Notes

- All TODO items use the existing 76KB arena (32+20+24)
- Flash cache may need expansion for full library
- Consider moving flash cache limit to config.h
- LibraryIndex v2 already has most metadata needed for carousel
