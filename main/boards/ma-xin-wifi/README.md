# MaXin WiFi ESP32-S3 Board

Custom board for ESP32-S3 N16R8 based XiaoZhi AI chatbot with 2.0 ST7789 LCD display.

## Hardware Configuration

- MCU: ESP32-S3 N16R8 (QFN56)
- Flash: 16MB | PSRAM: 8MB
- Display: 2.0 ST7789 SPI LCD (240x320)
- Audio: ES8311 codec

## Building

This board is automatically built via GitHub Actions when changes are pushed to the main branch.

To build manually:
`ash
python scripts/release.py ma-xin-wifi --name ma-xin-wifi
`

## Flashing

1. Hold BOOT + press RESET to enter download mode
2. Flash at 921600 baud to address 0x0

## Display Views

- [Home]: Main chat/voice screen
- [Weather]: Weather (icon, temp, condition, humidity)  
- [Calendar]: Date, weekday, month/year

## Board Files

- config.h - GPIO pin configuration
- ma_xin_wifi_ESP32S3.cc - Board initialization
- ma_xin_wifi_lcd_display.cc/h - Custom weather & calendar UI

MIT License
