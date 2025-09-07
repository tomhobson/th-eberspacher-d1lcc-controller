# Eberspacher D1LC Controller - Temperature Display

Arduino-based temperature controller for Eberspacher D1LC heater with display and rotary encoder control.

## Hardware Components

- **Arduino Uno** (or compatible)
- **SSD1309 OLED Display** (128x64, I2C)
- **DS18B20 Temperature Sensor** (OneWire)
- **Rotary Encoder** with push button
- **DS3502 Digital Potentiometer** (I2C)
- **DS3231 Real-Time Clock** with AT24C32 EEPROM (I2C)

## Pin Configuration

| Component | Pin | Description |
|-----------|-----|-------------|
| **Temperature Sensor** | Pin 2 | OneWire data line (ONE_WIRE_BUS) |
| **Rotary Encoder** | Pin 3 | CLK (Clock) |
| **Rotary Encoder** | Pin 4 | DT (Data) |
| **Rotary Encoder** | Pin 5 | SW (Switch/Button) |
| **Heater Control** | Pin 6 | Yellow wire to D1LC ECU |
| **OLED Display** | I2C | SDA/SCL (A4/A5 on Uno) |
| **DS3502 Potentiometer** | I2C | SDA/SCL (A4/A5 on Uno) |
| **DS3231 RTC Module** | I2C | SDA/SCL (A4/A5 on Uno) |

## Features

- Real-time cabin temperature monitoring
- Target temperature adjustment via rotary encoder
- Automatic D1LC heater power control based on temperature difference:
  - **High Power**: ≥3°C below target (wiper ~28, ~2.2kΩ)
  - **Medium Power**: 1-3°C below target (wiper ~25, ~2.1kΩ)  
  - **Low Power**: 0-1°C below target (wiper ~22, ~2.0kΩ)
  - **Off**: At or above target temperature
- Digital rheostat control via DS3502 (brown/white ↔ green/red wires)
- Real-time clock with date/time display
- OLED display with status icons and heater status
- Automatic time setting on first boot or after power loss

## Dependencies

The following libraries are required:

- `OneWire` - For DS18B20 temperature sensor
- `DallasTemperature` - Dallas temperature sensor library
- `U8g2lib` - OLED display driver
- `ezButton` - Button debouncing library
- `Adafruit_DS3502` - Digital potentiometer control
- `Adafruit_BusIO` - I2C/SPI bus abstraction
- `RTClib` - DS3231 real-time clock library

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
- **Heater Control**: Automatic based on cabin vs target temperature
- **Power Levels**: DS3502 wiper values 20-28 provide ~1.8-2.2kΩ resistance

## Display Layout

- **Time**: Current time (HH:MM) at top of display
- **Current Temperature**: Real-time sensor reading
- **Desired Temperature**: Target set via encoder
- **Delta**: Difference between current and target
- **Date**: Current date (DD/MM/YYYY) at bottom left
- **Status Icons**: Network, Bluetooth, and Temperature indicators

## I2C Device Addresses

- **SSD1309 OLED Display**: 0x3C (default)
- **DS3502 Digital Potentiometer**: 0x28 (default)
- **DS3231 Real-Time Clock**: 0x68 (fixed)

## D1LC Heater Wiring

- **DS3502 RW ↔ RL**: Connected across brown/white ↔ green/red wires
- **Yellow Wire**: Connected to Pin 6 for heater enable/disable
- **ECU expects**: ~1.8–2.2kΩ resistance for power level control
- **Valid wiper range**: 20-30 (corresponding to required resistance values)