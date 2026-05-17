# MaXin WiFi ESP32-S3 Board

Custom board for ESP32-S3 N16R8 based XiaoZhi AI chatbot with 2.0" ST7789 LCD display.

## Hardware Configuration

- **MCU**: ESP32-S3 N16R8 (QFN56)
- **Flash**: 16MB
- **PSRAM**: 8MB
- **Display**: 2.0" ST7789 SPI LCD (240x320)
- **Audio**: ES8311 codec

## Features

- WiFi connectivity
- Voice interaction via MCP protocol
- 2.0" LCD with weather and calendar views
- Battery management with USB charging detection
- SD card support (SPI mode)

## Building

This board is automatically built via GitHub Actions when changes are pushed to the `main` branch.

To build manually:
```bash
python scripts/release.py ma-xin-wifi --name ma-xin-wifi
```

## Firmware Flashing

1. Put device in download mode (hold BOOT, press RESET)
2. Flash using ESP32-S3 tool:
   - Chip: ESP32-S3
   - Mode: UART
   - Baud: 921600
   - Address: 0x0
   - File: merged-binary.bin

## Display Views

- **[Home]**: Main chat/voice interaction screen
- **[Weather]**: Weather display (icon, temperature, condition, humidity)
- **[Calendar]**: Calendar view (date, weekday, month/year)

## License

MIT License