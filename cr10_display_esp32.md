# Creality CR-10 LCD Display — ESP32-S3 Repurposing Guide

## Overview

This documents the pinout and wiring for repurposing a Creality CR-10 control screen (ST7920 128x64 LCD, rotary encoder with push button, and piezo buzzer) with an ESP32-S3 microcontroller.

The display board has a single **EXP3** 2x5 pin header (10 pins) with a keying notch. The EXP1 and EXP2 headers are unpopulated but have silkscreen labels that were used to trace the EXP3 pinout.

## Display Specs

- **LCD Controller:** ST7920
- **Resolution:** 128 x 64 pixels
- **Interface:** SPI (active when PSB pin is low)
- **Power:** 5V
- **Encoder:** Quadrature rotary encoder with push button (5 pins: 2 quadrature, 1 common/GND, 2 push button)
- **Buzzer:** Passive piezo, driven through a transistor (active high)

## EXP3 Pinout

Orientation: **notch/tab facing down**, square pad (pin 1) at lower left.

```
        EXP3 (notch down)
┌──────────────────────────────────────────┐
│  2=BTN   4=NC   6=SCLK   8=MOSI  10=5V  │
│ ■1=BZR   3=ENC_A 5=ENC_B  7=CS    9=GND │
└───┘┘─────────────────────────────────────┘
     notch
```

| EXP3 Pin | Signal         | Notes                                        |
|----------|----------------|----------------------------------------------|
| 1        | Beeper         | Active high, driven through base resistor → transistor → buzzer → 5V |
| 2        | Encoder Button | Push button (active low, needs pullup)        |
| 3        | Encoder A      | Quadrature channel A (needs pullup)           |
| 4        | NC             | Not connected                                 |
| 5        | Encoder B      | Quadrature channel B (needs pullup)           |
| 6        | SCLK           | ST7920 E pin — SPI clock in serial mode       |
| 7        | CS             | ST7920 RS pin — chip select in serial mode    |
| 8        | MOSI / SID     | ST7920 R/W pin — serial data in serial mode   |
| 9        | GND            | Ground                                        |
| 10       | 5V             | Power (also tied to LCD RST, held high)       |

### ST7920 SPI Pin Mapping

The ST7920 pin names differ from standard SPI terminology:

| ST7920 Pin | SPI Function | EXP3 Pin |
|------------|-------------|----------|
| E          | SCLK        | 6        |
| RS         | CS          | 7        |
| R/W        | MOSI / SID  | 8        |

## Buzzer Circuit

The buzzer is **not** directly driven from the logic pin. The signal path is:

```
EXP3 Pin 1 → Resistor (base) → NPN Transistor (collector) → Buzzer → 5V
                                              (emitter) → GND
```

Driving pin 1 **HIGH** turns on the transistor, sinking current through the buzzer. A 3.3V signal from the ESP32 is sufficient to switch the transistor.

## ESP32-S3 Wiring

| EXP3 Pin | Signal     | ESP32-S3 GPIO |
|----------|-----------|---------------|
| 1        | Beeper    | GPIO 15       |
| 2        | ENC BTN   | GPIO 7        |
| 3        | ENC A     | GPIO 5        |
| 4        | NC        | —             |
| 5        | ENC B     | GPIO 6        |
| 6        | SCLK      | GPIO 12       |
| 7        | CS        | GPIO 10       |
| 8        | MOSI      | GPIO 11       |
| 9        | GND       | GND           |
| 10       | 5V        | 5V (VBUS)     |

### Logic Level Notes

- The ST7920 is a 5V device. The datasheet specifies a high input threshold of 0.7×Vcc (3.5V), making 3.3V technically out of spec.
- **In practice, 3.3V logic from the ESP32-S3 works reliably** with no level shifting required.
- If issues occur, slow down the SPI clock speed in the U8g2 configuration.
- The encoder and buzzer transistor circuit work fine at 3.3V logic levels.

## Software

### Dependencies

- **Library:** [U8g2](https://github.com/olikraus/u8g2) — for ST7920 SPI display
- **Framework:** Arduino (ESP32-S3 board support)

### Working Test Sketch

```cpp
#include <U8g2lib.h>
#include <SPI.h>

// LCD - Software SPI
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/ 12, /* data=*/ 11, /* cs=*/ 10);

// Pins
#define ENC_A 5
#define ENC_B 6
#define ENC_BTN 7
#define BUZZER 15

// Encoder state
volatile int encoderPos = 0;
volatile uint8_t lastState = 0;

void IRAM_ATTR encoderISR() {
  uint8_t a = digitalRead(ENC_A);
  uint8_t b = digitalRead(ENC_B);
  uint8_t state = (a << 1) | b;

  static const int8_t transition[4][4] = {
    { 0, -1,  1,  0},
    { 1,  0,  0, -1},
    {-1,  0,  0,  1},
    { 0,  1, -1,  0}
  };

  encoderPos += transition[lastState][state];
  lastState = state;
}

void setup() {
  u8g2.begin();

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  lastState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);

  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), encoderISR, CHANGE);
}

void loop() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(0, 20);
  u8g2.print("Encoder: ");
  u8g2.print(encoderPos / 4);  // 4 state changes per detent

  if (!digitalRead(ENC_BTN)) {
    u8g2.setCursor(0, 40);
    u8g2.print("PRESSED");
    digitalWrite(BUZZER, HIGH);
  } else {
    digitalWrite(BUZZER, LOW);
  }

  u8g2.sendBuffer();
}
```

### Encoder Notes

- This encoder produces **4 state transitions per detent**, so divide `encoderPos` by 4 for clean ±1 per click.
- Uses a full gray code state machine with interrupts on **both** pins (CHANGE mode) for reliable decoding.
- If direction is inverted, swap `ENC_A` and `ENC_B` pin definitions.
- The encoder button is active low (reads LOW when pressed) using internal pullup.

## Pinout Discovery Method

The EXP3 pinout was mapped using a multimeter in continuity mode by tracing from the silkscreened EXP1/EXP2 pads (unpopulated) to the EXP3 header pins. The buzzer required diode mode to trace through the transistor in the drive circuit.

**Pin 1 identification:** Square solder pad on the PCB (standard convention), visible on EXP1/EXP2 headers — lower left when tab/notch faces up.

## Hardware

- **Display Board:** Creality CR-10 stock LCD (ST7920-based 12864 RepRap Smart Controller clone)
- **MCU:** ESP32-S3 DevKit
- **Connection:** Direct wiring from EXP3 header, no level shifters or additional components required
