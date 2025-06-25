# Monoscope TV Test Pattern Display

A retro TV test pattern display project for ESP32 with ILI9341 SPI display, simulating classic monoscope broadcast test 
patterns with old television effects.

## Overview

This project draws ESP32 and ILI9341 display into a nostalgic TV test pattern generator, complete with:
- Classic monoscope test pattern elements
- Simulated TV static and noise

## Hardware Requirements

### ESP32 Module

- Any ESP32 development board (ESP32-C6, ESP32-C3, etc.)

### Display

- ILI9341 320x240 TFT LCD with SPI interface

### Connections

| ESP32 Pin | ILI9341 Pin | Function |
|-----------|-------------|----------|
| GPIO 6    | SCK         | SPI Clock |
| GPIO 7    | SDA/MOSI    | SPI Data |
| GPIO 20   | CS          | Chip Select |
| GPIO 21   | DC          | Data/Command |
| GPIO 3    | RST         | Reset |
| 3.3V      | VCC         | Power |
| GND       | GND         | Ground |

## Software Requirements

- **ESP-IDF**: Version 4.4 or later
- **Components Used**:
    - FreeRTOS
    - ESP LCD Panel
    - SPI Master Driver
    - Random Number Generator

## Building and Flashing

- Set up ESP-IDF environment:
   ```bash
   . /esp-idf/export.sh
   ```

- Configure the project:
   ```bash
   idf.py set-target esp32-c6
   ```

4. Build the project:
   ```bash
   idf.py build flash monitor
   ```

## Configuration

### Pin Configuration
Modify the pin definitions in `monoscope.c` if your wiring differs:

```c
#define DISPLAY_SCLK_GPIO       6
#define DISPLAY_MOSI_GPIO       7
#define DISPLAY_CS_GPIO         20
#define DISPLAY_DC_GPIO         21
#define DISPLAY_RST_GPIO        3
```
