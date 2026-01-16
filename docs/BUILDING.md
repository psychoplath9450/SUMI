# Building Sumi Firmware

## What You Need

- **PlatformIO** (6.x) - the build system
- **Python** (3.8+) - PlatformIO needs this
- **Git** - to grab the code

### Installing PlatformIO

**Easy way:** Install VS Code, then grab the PlatformIO extension from the marketplace.

**Or from command line:**
```bash
pip install platformio
```

## Building

### Get the code

```bash
git clone https://github.com/psychoplath9450/SUMI.git
cd sumi
```

### Build it

```bash
pio run
```

Or in VS Code, click the PlatformIO icon in the sidebar and hit "Build".

### Flash to the device

```bash
pio run -t upload
```

Note: It's `-t upload`, not `--target upload` (both work, but `-t` is shorter).

## Feature Flags

You can turn features on/off in `platformio.ini`:

```ini
build_flags =
    -DFEATURE_READER=1      # book reading
    -DFEATURE_FLASHCARDS=1  # study cards
    -DFEATURE_WEATHER=1     # weather widget
    -DFEATURE_GAMES=1       # chess, sudoku, etc
    -DFEATURE_BLUETOOTH=0   # BLE (off by default)
```

For ESP32-C3, keep `SUMI_LOW_MEMORY=1` - it enables memory optimizations that are pretty much required on the C3.

## Common Problems

### "No such file: GxEPD2_BW.h"

Libraries didn't download. Run:
```bash
pio lib install
```

### "region 'dram0_0_seg' overflowed"

Too much stuff enabled for the ESP32-C3's limited RAM. Try disabling some features.

### "Could not open COM4"

Device not connected, or wrong port. Check:
- Windows: Device Manager → Ports
- Linux: `ls /dev/ttyUSB*`
- Mac: `ls /dev/cu.*`

### Upload stuck at 0%

Some boards need you to hold the BOOT button while pressing RST, then release RST first, then BOOT. Then try uploading again.

### Nothing on the display

Double-check your wiring. The pin assignments in `config.h` need to match your actual hardware.

## SD Card Setup

After flashing, set up your SD card (FAT32). The SD card is for user content only:

```
/books/       ← EPUBs, PDFs, and TXT files
/flashcards/  ← JSON study decks (or import Anki .apkg)
/images/      ← BMP, PNG, JPG pictures
/maps/        ← Map images or OSM tile folders
/notes/       ← Text notes (.txt, .md)
/.config/     ← Auto-created by Sumi for settings/state
```

The web portal is embedded in the firmware - no need to copy anything to the SD card for it to work.

## First Boot

1. Power on
2. Connect to the `Sumi-Setup-XXXX` WiFi network
3. Go to `192.168.4.1` in your browser
4. Set up your WiFi and home screen
5. After that, access settings anytime at `sumi.local`

## Serial Monitor

To see debug output:

```bash
pio device monitor -b 115200
```

The firmware logs heap usage every 30 seconds, which is handy for tracking down memory issues.
