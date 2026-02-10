# CR-10 Display — ESP32-S3 Repurposing Guide

Technical reference for repurposing a Creality CR-10 3D printer control screen with an ESP32-S3 microcontroller.

## What's Covered

- **EXP3 pinout** — Full 2x5 header pin mapping with orientation diagram
- **ESP32-S3 wiring** — GPIO assignments for LCD (SPI), rotary encoder, and buzzer
- **ST7920 SPI interface** — Pin name translation (ST7920 uses non-standard naming: E=SCLK, RS=CS, R/W=MOSI)
- **Buzzer circuit** — Transistor drive circuit explanation
- **Logic levels** — 3.3V ESP32 driving 5V ST7920 without level shifting
- **Working Arduino sketch** — Complete test code using U8g2 with encoder state machine and buzzer control

## Hardware

| Component | Detail |
|-----------|--------|
| Display | Creality CR-10 stock LCD (ST7920 128x64, RepRap Smart Controller clone) |
| MCU | ESP32-S3 DevKit |
| Connection | Direct wiring from EXP3 header — no level shifters needed |

## Software Dependencies

- [U8g2](https://github.com/olikraus/u8g2) — ST7920 display library
- Arduino framework with ESP32-S3 board support

## Documentation

All technical details are in [`cr10_display_esp32.md`](cr10_display_esp32.md).
