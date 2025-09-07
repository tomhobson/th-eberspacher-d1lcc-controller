#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// PIN DEFINITIONS
#define TEMP_SENSOR_PIN 2
#define ENCODER_CLK_PIN 3
#define ENCODER_DT_PIN 4
#define ENCODER_SW_PIN 5
#define HEATER_CONTROL_PIN 6

// HARDWARE CONFIGURATION
const int SERIAL_BAUD_RATE = 9600;
const unsigned long DISPLAY_INTERVAL = 200;  // ms
const unsigned long DEBOUNCE_TIME = 1;       // ms

// HEATER CONTROL

// DS3502 Wiper Values
const uint8_t WIPER_MIN_SAFE = 20;   // ~1.8kΩ
const uint8_t WIPER_LOW_SAFE = 22;   // ~2.0kΩ
const uint8_t WIPER_MED_SAFE = 25;   // ~2.1kΩ  
const uint8_t WIPER_HIGH_SAFE = 28;  // ~2.2kΩ
const uint8_t WIPER_MAX_SAFE = 30;   // Maximum safe value

// Thermostat Behavior
const float DIFF_HIGH = 3.0;  // target - cabin ≥ 3 → HIGH
const float DIFF_MED = 1.0;   // target - cabin ≥ 1 → MED
const float HYS_ON = 1.5;     // turn ON if below target by ≥ 1.5
const float HYS_OFF = 0.5;    // allow OFF only if above target by ≥ 0.5

// Timings
const unsigned long MIN_ON_MS = 10UL * 60UL * 1000UL;  // ≥10 min on
const unsigned long MIN_OFF_MS = 5UL * 60UL * 1000UL;  // ≥5 min off
const unsigned long WIPER_STEP_DELAY_MS = 120UL;       // smooth ramp

// DISPLAY CONFIG
const unsigned long POWER_SAVE_TIMEOUT = 30000;
const unsigned long BUTTON_LONG_PRESS_TIME = 1000;
const unsigned long BUTTON_DOUBLE_CLICK_TIME = 300;
const unsigned long DISPLAY_UPDATE_INTERVAL = 200;

// Screen dimensions
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

// Fonts
#define FONT_SMALL u8g2_font_6x10_tf
#define FONT_MEDIUM u8g2_font_7x13_tf
#define FONT_LARGE u8g2_font_10x20_tf

// Menu Navigation
const int MAX_MENU_ITEMS = 15;        // Maximum total menu items
const int MAX_VISIBLE_MENU_ITEMS = 4; // Maximum items visible on screen at once
const int MENU_TIMEOUT = 15000;       // 15 seconds auto-exit

// RTC CONFIG
const int RTC_VALID_YEAR_MIN = 2020;
const int RTC_VALID_YEAR_MAX = 2099;
const int RTC_TIME_JUMP_THRESHOLD = 300;  // 5 minutes in seconds

// WAKEUP CONFIG
const int MAX_WAKEUP_TIMERS = 3;  // Maximum number of wake-up timers
const unsigned long WAKEUP_PREHEAT_MINUTES = 30;  // Start heating 30 min before target time
const int MIN_WAKEUP_TEMP = 15;   // Minimum wake-up target temperature
const int MAX_WAKEUP_TEMP = 30;   // Maximum wake-up target temperature

// ENUMS
enum HeatState { HS_OFF, HS_LOW, HS_MED, HS_HIGH };

// Wake-up timer states
enum WakeupState { 
  WAKEUP_DISABLED,    // Timer is off
  WAKEUP_ARMED,       // Timer is set and waiting
  WAKEUP_PREHEATING,  // Currently pre-heating
  WAKEUP_READY,       // Target temperature reached
  WAKEUP_EXPIRED      // Timer has finished
};

// Days of week for wake-up timer
enum WakeupDay {
  DAY_SUNDAY = 0,
  DAY_MONDAY = 1,
  DAY_TUESDAY = 2,
  DAY_WEDNESDAY = 3,
  DAY_THURSDAY = 4,
  DAY_FRIDAY = 5,
  DAY_SATURDAY = 6
};

// DEBUG CONFIG
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINT_F(x) Serial.print(F(x))
  #define DEBUG_PRINTLN_F(x) Serial.println(F(x))
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT_F(x)
  #define DEBUG_PRINTLN_F(x)
#endif

// LIMITS
const int MIN_TARGET_TEMP = 5;   // Minimum target temperature (°C)
const int MAX_TARGET_TEMP = 40;  // Maximum target temperature (°C)
const int DEFAULT_TARGET_TEMP = 20;  // Default target temperature (°C)

// COMPATIBILITY
// Watchdog timer constants (if not defined by AVR library)
#ifndef WDTO_8S
#define WDTO_8S 0x21
#endif

// Dallas Temperature library constants (if not defined)
#ifndef DEVICE_DISCONNECTED_C
#define DEVICE_DISCONNECTED_C -127
#endif

#endif // CONFIG_H