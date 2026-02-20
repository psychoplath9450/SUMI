# SUMI Future Improvements - Image & Library System

## Status: v0.4.0 Complete

What we implemented:
- ✅ Memory Arena (64KB pre-allocated, releasable for BLE)
- ✅ Flash Thumbnail Cache (instant home screen for opened books)
- ✅ Arena migration for JPEG/PNG converters
- ✅ HomeState uses arena scratchBuffer
- ✅ **JPEGDEC Integration** - Replaced picojpeg with modern decoder

---

## ✅ JPEGDEC Integration (DONE)

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
- Could use arena buffers (imageBuffer for decode, scratchBuffer for thumbnail)
- Flash cache already handles storage
- Consider: limit to N covers per session to avoid blocking UI

---

## TODO: JPEGDEC Integration

**Goal:** Replace picojpeg with modern, maintained decoder that has built-in e-ink dithering.

### Why
- picojpeg is from 2011, designed for 8-bit microcontrollers
- JPEGDEC has built-in Floyd-Steinberg for 1/2/4-bit output
- JPEGDEC is fuzz-tested, actively maintained
- Would delete 4 ditherer classes (AtkinsonDitherer, FloydSteinbergDitherer, Atkinson1BitDitherer, RawAtkinson1BitDitherer)

### Steps
1. Add JPEGDEC library to lib/
2. Create new JpegToBmpConverter implementation
3. Test with various JPEG sizes and types
4. Keep picojpeg as fallback initially
5. Once stable, remove picojpeg and old ditherers

### Blockers
- Need to test with real cover images
- JPEGDEC uses different callback pattern than picojpeg
- PNG still needs ditherers (until PNGdec integration)

---

## TODO: Home Screen as Library Carousel

**Goal:** Home screen shows carousel of ALL books with covers, not just 3 recent.

### Current State
```
Home Screen:
┌─────────────────────┐
│  [Current Book]     │  ← Large card with cover
│  Title / Author     │
│  Progress bar       │
├─────────────────────┤
│  Recent 1 │ Recent 2│  ← Small cards, just titles
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
- May need larger flash cache (currently 3 books)
- Consider: virtual scrolling, only load visible covers

---

## TODO: ZipFile Arena Migration

**Goal:** Move ZipFile malloc/free to use arena zipBuffer.

### Current State
- ZipFile has 8 malloc calls
- Uses TINFL_LZ_DICT_SIZE (32KB) for inflate dictionary
- Arena has zipBuffer (32KB) ready but unused

### Why Deferred
- ZipFile is complex with multiple code paths
- Current implementation works fine
- Lower risk than image processing (less fragmentation)

### Steps
1. Audit all ZipFile malloc/free patterns
2. Replace dictionary malloc with MemoryArena::zipBuffer
3. Put tinfl_decompressor on stack (small, ~11KB)
4. Test EPUB loading thoroughly

---

## Priority Order

1. **JPEGDEC Integration** - Eliminates ditherer complexity, more robust
2. **Library Cover Scanning** - Required for full carousel
3. **Home as Library Carousel** - Big UX improvement
4. **ZipFile Arena** - Nice to have, low priority

---

## Notes

- All TODO items use the existing 120KB arena
- Flash cache may need expansion for full library (currently ~8KB for 3 books)
- Consider moving flash cache limit to config.h
- LibraryIndex v2 already has most metadata needed for carousel
