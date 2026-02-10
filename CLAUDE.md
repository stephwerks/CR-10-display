# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Hardware documentation and reference implementation for repurposing a Creality CR-10 3D printer control screen (ST7920 128x64 LCD, rotary encoder, piezo buzzer) with an ESP32-S3 microcontroller. The repository is documentation-only — no build system, tests, or compiled source tree.

## Repository Structure

- `cr10_display_esp32.md` — The sole content file. Contains complete EXP3 pinout mapping, ESP32-S3 GPIO wiring table, buzzer transistor circuit description, logic level notes, and a working Arduino test sketch using U8g2 software SPI.

## Key Technical Details

- **Display:** ST7920 in SPI mode (PSB low), 128x64, 5V powered. ST7920 pin names differ from standard SPI (E=SCLK, RS=CS, R/W=MOSI).
- **Encoder:** Quadrature with 4 state transitions per detent. Uses gray code state machine with dual CHANGE-mode interrupts. Active-low button with internal pullup.
- **Buzzer:** Transistor-driven (active high from MCU, NPN sinks current through buzzer to 5V). 3.3V logic sufficient to switch.
- **Voltage:** 3.3V ESP32 GPIO drives 5V ST7920 reliably without level shifting despite being technically out-of-spec (V_IH = 3.5V).
- **Software deps:** U8g2 library, Arduino framework with ESP32-S3 board support.

## When Editing

- The embedded C++ sketch is reference code — keep it minimal and well-commented.
- Pin assignments and wiring tables must stay consistent across all sections (pinout table, wiring table, and code `#define` lines).
- EXP3 pinout orientation convention: notch/tab facing down, pin 1 (square pad) at lower left.
