cmake_minimum_required(VERSION 3.20)
project(TempDisplay)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

# Arduino-specific settings
set(ARDUINO_BOARD "arduino:avr:uno")
set(ARDUINO_PORT "/dev/cu.usbmodem*")

# Arduino IDE paths (adjust for your system)
set(ARDUINO_IDE_PATH "/Applications/Arduino.app/Contents/Java" CACHE PATH "Arduino IDE installation path")
set(ARDUINO_SKETCHBOOK_PATH "$ENV{HOME}/Documents/Arduino" CACHE PATH "Arduino sketchbook path")

# Arduino core and library paths
set(ARDUINO_CORE_PATH "${ARDUINO_IDE_PATH}/hardware/arduino/avr/cores/arduino")
set(ARDUINO_VARIANT_PATH "${ARDUINO_IDE_PATH}/hardware/arduino/avr/variants/standard")
set(ARDUINO_LIBRARIES_PATH "${ARDUINO_IDE_PATH}/libraries")
set(ARDUINO_USER_LIBRARIES_PATH "${ARDUINO_SKETCHBOOK_PATH}/libraries")

# Include directories
include_directories(
    ${ARDUINO_CORE_PATH}
    ${ARDUINO_VARIANT_PATH}
    ${ARDUINO_LIBRARIES_PATH}
    ${ARDUINO_USER_LIBRARIES_PATH}
    ${ARDUINO_USER_LIBRARIES_PATH}/OneWire
    ${ARDUINO_USER_LIBRARIES_PATH}/DallasTemperature/src
    ${ARDUINO_USER_LIBRARIES_PATH}/U8g2/src
    ${ARDUINO_USER_LIBRARIES_PATH}/ezButton/src
    ${ARDUINO_USER_LIBRARIES_PATH}/Adafruit_DS3502
    ${ARDUINO_USER_LIBRARIES_PATH}/Adafruit_BusIO
    .
)

# Arduino compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR")

# Create the executable
add_executable(TempDisplay
        TempDisplay.ino)
