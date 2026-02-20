# JPEGDEC Migration

## Summary

Replaced picojpeg (2011 vintage, unmaintained) with JPEGDEC by Larry Bank (bitbank2).

## Why JPEGDEC?

1. **Actively maintained** - Latest release Nov 2025 (v1.8.4)
2. **Built-in Floyd-Steinberg dithering** - Perfect for e-ink displays
3. **Faster** - 2x faster than tjpgd according to ESP32 forum benchmarks
4. **Smaller memory footprint** - Uses efficient MCU row buffer approach
5. **Better format support** - Handles more JPEG variants

## Changes Made

### Added
- `lib_deps` entry for `bitbank2/JPEGDEC @ ^1.6.2` in platformio.ini
- New `JpegToBmpConverter.cpp` using JPEGDEC API

### Removed (kept as backup)
- `JpegToBmpConverter.cpp.picojpeg.bak` - old picojpeg-based converter

### Kept (still used by other components)
- `lib/picojpeg/` - Not included anymore, kept for reference
- Ditherer classes in BitmapHelpers.h - Still used by PNG converter and BMP loader

## JPEGDEC API Usage

```cpp
JPEGDEC jpeg;

// Open from RAM
jpeg.openRAM(data, size, drawCallback);

// Set pixel type for dithering (MUST be after open())
jpeg.setPixelType(TWO_BIT_DITHERED);  // 4 gray levels
// or: ONE_BIT_DITHERED, FOUR_BIT_DITHERED

// Decode with dithering
uint8_t ditherBuffer[width * 16];  // Error diffusion buffer
int scale = 0;  // 0=full, 1=1/2, 2=1/4, 3=1/8
jpeg.decodeDither(ditherBuffer, scale);

jpeg.close();
```

## Dither Output Format

- `TWO_BIT_DITHERED`: 4 pixels per byte, MSB first (matches BMP format)
- `ONE_BIT_DITHERED`: 8 pixels per byte, MSB first
- `FOUR_BIT_DITHERED`: 2 pixels per byte

## Performance

Expected improvements:
- Cover image conversion: ~500ms → ~250ms
- In-book image conversion: Proportional speedup
- Memory usage: Similar (uses arena buffers)

## Rollback

If issues arise:
1. Remove JPEGDEC from lib_deps
2. Rename `.picojpeg.bak` back to `.cpp`
3. The old code will work unchanged
