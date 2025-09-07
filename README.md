# Eberspacher D1LC Controller - Temperature Display

Arduino-based temperature controller for Eberspacher D1LC heater with display and rotary encoder control.

## Hardware Components

- **Arduino Uno** (or compatible)
- **SSD1309 OLED Display** (128x64, I2C)
- **DS18B20 Temperature Sensor** (OneWire)
- **Rotary Encoder** with push button
- **DS3502 Digital Potentiometer** (I2C)

## Pin Configuration

| Component | Pin | Description |
|-----------|-----|-------------|
| **Temperature Sensor** | Pin 2 | OneWire data line (ONE_WIRE_BUS) |
| **Rotary Encoder** | Pin 3 | CLK (Clock) |
| **Rotary Encoder** | Pin 4 | DT (Data) |
| **Rotary Encoder** | Pin 5 | SW (Switch/Button) |
| **OLED Display** | I2C | SDA/SCL (A4/A5 on Uno) |
| **DS3502 Potentiometer** | I2C | SDA/SCL (A4/A5 on Uno) |

## Features

- Real-time temperature monitoring
- Target temperature adjustment via rotary encoder
- Temperature delta calculation and display
- Digital potentiometer control for heater output
- OLED display with status icons (Bluetooth, WiFi, Temperature)

## Dependencies

The following libraries are required:

- `OneWire` - For DS18B20 temperature sensor
- `DallasTemperature` - Dallas temperature sensor library
- `U8g2lib` - OLED display driver
- `ezButton` - Button debouncing library
- `Adafruit_DS3502` - Digital potentiometer control
- `Adafruit_BusIO` - I2C/SPI bus abstraction

## Build Instructions

This project is configured for CLion with CMake. The CMakeLists.txt file includes all necessary Arduino paths and compiler flags.

1. Install required libraries in your Arduino libraries folder
2. Adjust `ARDUINO_IDE_PATH` in CMakeLists.txt if needed
3. Open project in CLion
4. Build and upload to Arduino

## Operation

- **Display Updates**: Every 200ms or on button press
- **Temperature Control**: Use rotary encoder to adjust target temperature
- **Button**: Press to immediately update display
- **Potentiometer**: Automatically cycles through values (1-127) for testing

## Display Layout

- **Current Temperature**: Real-time sensor reading
- **Desired Temperature**: Target set via encoder
- **Delta**: Difference between current and target
- **Status Icons**: Network, Bluetooth, and Temperature indicators