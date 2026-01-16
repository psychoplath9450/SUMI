# Hardware Reference

## The Basics

- **MCU:** ESP32-C3 (RISC-V, 160MHz, single core)
- **Flash:** 16MB
- **RAM:** 400KB (no PSRAM)
- **Display:** GDEQ0426T82 4.2" e-ink (800×480, black/white)

## Pin Assignments

### Display (SPI)

| Signal | GPIO | Notes |
|--------|------|-------|
| SCLK | 8 | shared with SD |
| MOSI | 10 | shared with SD |
| CS | 21 | |
| DC | 4 | data/command |
| RST | 5 | reset |
| BUSY | 6 | |

### SD Card (SPI)

| Signal | GPIO | Notes |
|--------|------|-------|
| SCK | 8 | shared with display |
| MOSI | 10 | shared with display |
| MISO | 7 | |
| CS | 12 | |

### Buttons

The buttons use a resistor ladder, so multiple buttons share ADC pins:

| GPIO | Buttons |
|------|---------|
| 1 | RIGHT, LEFT, CONFIRM, BACK |
| 2 | DOWN, UP |
| 3 | Power (digital) |

ADC thresholds are in `config.h`. If your hardware uses different resistor values, hold the power button for 10 seconds during boot to enter calibration mode.

### Power

| Signal | GPIO |
|--------|------|
| Battery ADC | 0 |
| Power hold | 9 |
| Power button | 20 |

## Display Details

The GDEQ0426T82 is a 4.2" Good Display panel with an SSD1683 controller.

- Full refresh: ~1.5 seconds
- Partial refresh: ~200ms
- SPI clock: 40MHz

## Battery

Battery voltage is read through a 2:1 voltage divider on GPIO0.

Rough percentage mapping for a typical LiPo:
- 4.2V = 100%
- 3.7V = 50%
- 3.4V = 10%
- 3.2V = dead

## SPI Bus Sharing

The display and SD card share SCLK and MOSI. They have separate chip select lines, so only one can be active at a time. The firmware handles this automatically.

## Power Consumption

- Active: ~80mA
- Deep sleep: ~10µA

The power button triggers deep sleep on long press (~800ms). Another long press wakes it back up.

## Button Calibration

If buttons aren't working right (wrong resistor values, etc.), you can recalibrate:

1. Power off the device
2. Hold the power button for 10+ seconds while powering on
3. Follow the on-screen wizard
4. Values get saved to NVS

## Expansion

A few GPIOs are free if you want to add stuff:

| GPIO | Notes |
|------|-------|
| 11 | good for an LED |
| 13 | general purpose |
| 18/19 | USB D-/D+ if USB is disabled |

I2C is tricky since the default pins conflict with SPI and power hold. Use software I2C on the free GPIOs if you need it.
