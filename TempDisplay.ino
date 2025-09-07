#include <DallasTemperature.h>
#include <U8g2lib.h>
#include "OneWire.h"
#include "DallasTemperature.h"
#include <ezButton.h> 
#include <Adafruit_DS3502.h>
#include <RTClib.h>

// ========================================
// PIN DEFINITIONS
// ========================================
#define TEMP_SENSOR_PIN 2
#define ENCODER_CLK_PIN 3
#define ENCODER_DT_PIN 4
#define ENCODER_SW_PIN 5
#define HEATER_CONTROL_PIN 6

// ========================================
// PROGRAM CONSTANTS
// ========================================
const unsigned long DISPLAY_INTERVAL = 200;  // ms
const unsigned long DEBOUNCE_TIME = 1;       // ms
const int SERIAL_BAUD_RATE = 9600;

// ===== Safer DS3502 window (≈1.8–2.2 kΩ) and control bands
const uint8_t WIPER_MIN_SAFE = 20;
const uint8_t WIPER_LOW_SAFE = 22;
const uint8_t WIPER_MED_SAFE = 25;
const uint8_t WIPER_HIGH_SAFE = 28;
const uint8_t WIPER_MAX_SAFE = 30;

// Thermostat behaviour (°C)
const float DIFF_HIGH  = 3.0;  // target - cabin ≥ 3  → HIGH
const float DIFF_MED   = 1.0;  // target - cabin ≥ 1  → MED
const float HYS_ON     = 1.5;  // turn ON if below target by ≥ 1.5
const float HYS_OFF    = 0.5;  // allow OFF only if above target by ≥ 0.5

// Anti-chatter / battery-friendly timings
const unsigned long MIN_ON_MS   = 10UL * 60UL * 1000UL; // ≥10 min on
const unsigned long MIN_OFF_MS  =  5UL * 60UL * 1000UL; // ≥5  min off
const unsigned long WIPER_STEP_DELAY_MS = 120UL;        // smooth ramp

// ========================================
// HARDWARE OBJECTS
// ========================================
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_DS3502 ds3502 = Adafruit_DS3502();
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0);
ezButton button(ENCODER_SW_PIN);
RTC_DS3231 rtc;

// ===== State for control + smoothing
enum HeatState { HS_OFF, HS_LOW, HS_MED, HS_HIGH };
static HeatState hs = HS_OFF;
static uint8_t   wiper_now = WIPER_LOW_SAFE;
static unsigned long lastOnMs = 0, lastOffMs = 0, lastWiperStepMs = 0;

// Initialize timing to allow immediate startup
void initHeaterTiming() {
  lastOffMs = millis() - MIN_OFF_MS - 1000;  // Allow immediate turn on
}

// Helpers
inline uint8_t clampWiper(uint8_t v){
  if (v < WIPER_MIN_SAFE) return WIPER_MIN_SAFE;
  if (v > WIPER_MAX_SAFE) return WIPER_MAX_SAFE;
  return v;
}
inline bool canTurnOn()  { return (millis() - lastOffMs) > MIN_OFF_MS; }
inline bool canTurnOff() { return (millis() - lastOnMs)  > MIN_ON_MS;  }

// Smoothly step the DS3502 by 1 code toward target (no big jumps)
void setWiperSmooth(uint8_t target){
  target = clampWiper(target);
  const unsigned long now = millis();
  if (now - lastWiperStepMs < WIPER_STEP_DELAY_MS) return;

  if (wiper_now < target) wiper_now++;
  else if (wiper_now > target) wiper_now--;
  else return;

  ds3502.setWiper(wiper_now);
  lastWiperStepMs = now;
  Serial.print("Wiper → "); Serial.println(wiper_now);
}

// ========================================
// PROGRAM VARIABLES
// ========================================
volatile int target_temperature = 0;
volatile unsigned long last_time = 0;
int prev_temperature = 0;
int current_wiper_value = WIPER_LOW_SAFE;
bool heater_enabled = false;
unsigned long lastDisplayUpdate = 0;

// Time display variables
static bool rtc_working = true;
static bool rtc_initialized = false;
static uint16_t last_good_year = 2024;
static uint8_t last_good_month = 1;
static uint8_t last_good_day = 1;
static uint8_t last_good_hour = 12;
static uint8_t last_good_minute = 0;
static unsigned long last_rtc_read = 0;

// ========================================
// DISPLAY ICONS (PROGMEM) - 16x16 pixels - XBM FORMAT
// ========================================

// Simple thermometer - PROPER design
static const unsigned char PROGMEM icon_temp[] = {
  0x00, 0x00, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
  0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0xC0, 0x03, 0xE0, 0x07,
  0xE0, 0x07, 0xE0, 0x07, 0xC0, 0x03, 0x00, 0x00
};

// Clock icon for time display
static const unsigned char PROGMEM icon_clock[] = {
  0x00, 0x00, 0xF0, 0x0F, 0x08, 0x10, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20,
  0x04, 0x20, 0x84, 0x21, 0x44, 0x22, 0x24, 0x24, 0x04, 0x20, 0x04, 0x20,
  0x04, 0x20, 0x08, 0x10, 0xF0, 0x0F, 0x00, 0x00
};

// Heater OFF - simple circle with X
static const unsigned char PROGMEM icon_heater_off[] = {
  0x00, 0x00, 0xF0, 0x0F, 0x08, 0x10, 0x04, 0x20, 0x82, 0x41, 0x41, 0x82,
  0x21, 0x84, 0x11, 0x88, 0x11, 0x88, 0x21, 0x84, 0x41, 0x82, 0x82, 0x41,
  0x04, 0x20, 0x08, 0x10, 0xF0, 0x0F, 0x00, 0x00
};

// Heater LOW - small flame
static const unsigned char PROGMEM icon_heater_low[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01,
  0x80, 0x01, 0xC0, 0x03, 0xC0, 0x03, 0xE0, 0x07, 0xE0, 0x07, 0xC0, 0x03,
  0xC0, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00
};

// Heater MED - medium flame
static const unsigned char PROGMEM icon_heater_med[] = {
  0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x80, 0x01, 0xC0, 0x03, 0xC0, 0x03,
  0xE0, 0x07, 0xE0, 0x07, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xE0, 0x07,
  0xE0, 0x07, 0xC0, 0x03, 0x80, 0x01, 0x00, 0x00
};

// Heater HIGH - large flame
static const unsigned char PROGMEM icon_heater_high[] = {
  0x80, 0x01, 0x80, 0x01, 0xC0, 0x03, 0xC0, 0x03, 0xE0, 0x07, 0xE0, 0x07,
  0xF0, 0x0F, 0xF0, 0x0F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F,
  0xF0, 0x0F, 0xF0, 0x0F, 0xE0, 0x07, 0xC0, 0x03
};

void setup(void) {
  Serial.begin(SERIAL_BAUD_RATE);
  u8g2.begin();
  sensors.begin();
  pinMode(ENCODER_SW_PIN, INPUT);
  pinMode(ENCODER_DT_PIN, INPUT);
  pinMode(HEATER_CONTROL_PIN, OUTPUT);
  digitalWrite(HEATER_CONTROL_PIN, LOW);  // Start with heater off
  button.setDebounceTime(DEBOUNCE_TIME);

  if (!ds3502.begin()) {
    Serial.println("WARNING: Couldn't find DS3502 chip - continuing anyway");
    delay(1000);  // Give time to read message
  } else {
    Serial.println("Found DS3502 chip");
  }

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("WARNING: Couldn't find RTC - using fallback time");
    rtc_working = false;
    delay(1000);  // Give time to read message
  } else {
    rtc_initialized = true;
  }

  if (rtc_initialized && rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  // Initialize heater timing to allow immediate operation
  initHeaterTiming();
  
  // Wait a moment for hardware to stabilize
  delay(100);
  Serial.println("Setup complete - starting main loop");

  // use interrupt for CLK pin is enough
  // call ISR_encoderChange() when CLK pin changes from LOW to HIGH
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), ISR_encoderChange, RISING);
}

// Validate RTC time data
bool isValidTime(DateTime dt) {
  // Check if year is reasonable (between 2020 and 2099)
  if (dt.year() < 2020 || dt.year() > 2099) return false;
  
  // Check month (1-12)
  if (dt.month() < 1 || dt.month() > 12) return false;
  
  // Check day (1-31)
  if (dt.day() < 1 || dt.day() > 31) return false;
  
  // Check hour (0-23)
  if (dt.hour() > 23) return false;
  
  // Check minute (0-59)
  if (dt.minute() > 59) return false;
  
  return true;
}

// Check if time change is reasonable (not jumping wildly)
bool isReasonableTimeChange(DateTime newTime) {
  // If we don't have a reference, accept first valid time
  if (last_rtc_read == 0) return true;
  
  // Calculate expected time progression
  unsigned long elapsed_ms = millis() - last_rtc_read;
  unsigned long expected_minute_change = elapsed_ms / 60000; // minutes
  
  // Create expected time based on last good time + elapsed
  DateTime expected(last_good_year, last_good_month, last_good_day, 
                    last_good_hour, last_good_minute, 0);
  expected = expected + TimeSpan(0, 0, expected_minute_change, 0);
  
  // Check if new time is within reasonable range (±5 minutes)
  long time_diff = abs((long)newTime.unixtime() - (long)expected.unixtime());
  return (time_diff < 300); // 5 minutes in seconds
}

// Get stable time with validation and persistence
DateTime getStableTime() {
  DateTime now;
  
  if (rtc_initialized) {
    now = rtc.now();
    
    if (isValidTime(now) && isReasonableTimeChange(now)) {
      // Time is valid and reasonable - store it
      last_good_year = now.year();
      last_good_month = now.month();  
      last_good_day = now.day();
      last_good_hour = now.hour();
      last_good_minute = now.minute();
      last_rtc_read = millis();
      rtc_working = true;
      return now;
    } else {
      // Time is invalid or jumping - use stored time
      rtc_working = false;
    }
  }
  
  // Use last known good time (fallback)
  return DateTime(last_good_year, last_good_month, last_good_day, 
                  last_good_hour, last_good_minute, 0);
}

void controlHeater(float cabin_temp, int target_temp) {
  const float diff = target_temp - cabin_temp;   // >0 means too cold
  const unsigned long now = millis();
  HeatState desired = hs;
  
  Serial.print("[CONTROL] Current state: ");
  Serial.print(hs);
  Serial.print(" Diff: ");
  Serial.print(diff);
  Serial.print(" HYS_ON: ");
  Serial.print(HYS_ON);
  Serial.print(" canTurnOn: ");
  Serial.println(canTurnOn());

  if (hs == HS_OFF) {
    // Stay OFF unless we’re sufficiently below target and allowed to start
    if (diff >= HYS_ON && canTurnOn()) {
      if      (diff >= DIFF_HIGH) desired = HS_HIGH;
      else if (diff >= DIFF_MED)  desired = HS_MED;
      else                        desired = HS_LOW;
    }
  } else {
    // Heater is ON: only consider OFF if comfortably above target and min-on met
    if (cabin_temp >= (target_temp + HYS_OFF) && canTurnOff()) {
      desired = HS_OFF;
    } else {
      // Otherwise choose power based on how far below target we are
      if      (diff >= DIFF_HIGH) desired = HS_HIGH;
      else if (diff >= DIFF_MED)  desired = HS_MED;
      else                        desired = HS_LOW;  // near setpoint: hold LOW
    }
  }

  // Apply transition edge effects
  if (desired != hs) {
    hs = desired;
    switch (hs) {
      case HS_OFF:
        digitalWrite(HEATER_CONTROL_PIN, LOW);     // disable yellow ON line
        heater_enabled = false;  // Update display variable
        lastOffMs = now;
        Serial.println("Heater: OFF");
        // Park wiper at safe in-range value (some ECUs read on wake)
        wiper_now = clampWiper(WIPER_LOW_SAFE);
        ds3502.setWiper(wiper_now);
        break;

      case HS_LOW:
        digitalWrite(HEATER_CONTROL_PIN, HIGH);
        heater_enabled = true;  // Update display variable
        lastOnMs = now;
        Serial.println("Heater: LOW");
        break;

      case HS_MED:
        digitalWrite(HEATER_CONTROL_PIN, HIGH);
        heater_enabled = true;  // Update display variable
        lastOnMs = now;
        Serial.println("Heater: MEDIUM");
        break;

      case HS_HIGH:
        digitalWrite(HEATER_CONTROL_PIN, HIGH);
        heater_enabled = true;  // Update display variable
        lastOnMs = now;
        Serial.println("Heater: HIGH");
        break;
    }
  }

  // Drive wiper smoothly toward the target for current state
  uint8_t targetWiper = wiper_now;
  switch (hs) {
    case HS_LOW:  targetWiper = WIPER_LOW_SAFE;  break;
    case HS_MED:  targetWiper = WIPER_MED_SAFE;  break;
    case HS_HIGH: targetWiper = WIPER_HIGH_SAFE; break;
    case HS_OFF:  targetWiper = WIPER_LOW_SAFE;  break;  // safe park
  }
  setWiperSmooth(targetWiper);
}

void loop() {
  button.loop();  // check button


  // Only update display every DISPLAY_INTERVAL ms
  if (millis() - lastDisplayUpdate > DISPLAY_INTERVAL || button.isPressed()) {
    lastDisplayUpdate = millis();

    if (prev_temperature != target_temperature) {
      prev_temperature = target_temperature;
    }

    // Get stable time with validation and anti-jump protection
    DateTime now = getStableTime();
    
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    if (target_temperature == 0){
      target_temperature = (int)tempC;
    }

    // Control the D1LC heater based on temperature difference
    controlHeater(tempC, target_temperature);
    
    // Debug output
    Serial.print("Temp: "); Serial.print(tempC);
    Serial.print(" Target: "); Serial.print(target_temperature);
    Serial.print(" Heater: "); Serial.print(heater_enabled ? "ON" : "OFF");
    Serial.print(" Level: ");
    switch(hs) {
      case HS_HIGH: Serial.print("HIGH"); break;
      case HS_MED: Serial.print("MED"); break;
      case HS_LOW: Serial.print("LOW"); break;
      default: Serial.print("OFF"); break;
    }
    Serial.print(" Wiper: "); Serial.println(wiper_now);

    float tempDelta = tempC - target_temperature;

    // Actual Temp
    char tempStr[30];
    dtostrf(tempC, 3, 1, tempStr);
    char displayStr[40];
    sprintf(displayStr, "%sC", tempStr);

    // Target Temp
    char tempStrTarget[30];
    dtostrf(target_temperature, 3, 1, tempStrTarget);
    char displayStrTarget[40];
    sprintf(displayStrTarget, "%sC", tempStrTarget);

    // Temp delta
    char tempStrDelta[30];
    dtostrf(tempDelta, 3, 1, tempStrDelta);
    char displayStrDelta[40];
    sprintf(displayStrDelta, "%sC", tempStrDelta);

    // Format time for display (with RTC status indicator)
    char timeStr[12];
    if (rtc_working) {
      sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
    } else {
      sprintf(timeStr, "%02d:%02d?", now.hour(), now.minute());  // ? indicates RTC issue
    }
    
    // Format date for display  
    char dateStr[15];
    sprintf(dateStr, "%02d/%02d/%04d", now.day(), now.month(), now.year());

    //u8g2.clearBuffer();
    //u8g2.setFont(u8g2_font_6x10_tr);
    //u8g2.drawStr(0, 10, displayStr);
    //u8g2.drawStr(0, 22, displayStrTarget);
    //u8g2.sendBuffer();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);

    // Time with clock icon
    u8g2.drawXBMP(1, 1, 16, 16, icon_clock);
    u8g2.drawStr(20, 12, timeStr);
    
    // Temperature with thermometer icon
    u8g2.drawXBMP(1, 18, 16, 16, icon_temp);
    u8g2.drawStr(20, 29, displayStr);
    u8g2.drawStr(70, 29, displayStrTarget);

    // Heater status with DYNAMIC ICONS and delay info
    u8g2.drawStr(20, 41, "Heat:");
    
    // Draw dynamic heater icon and status
    const unsigned char* heater_icon;
    switch(hs) {
      case HS_HIGH:
        heater_icon = icon_heater_high;
        u8g2.drawStr(50, 41, "HIGH");
        break;
      case HS_MED:
        heater_icon = icon_heater_med;
        u8g2.drawStr(50, 41, "MED");
        break;
      case HS_LOW:
        heater_icon = icon_heater_low;
        u8g2.drawStr(50, 41, "LOW");
        break;
      default:
        heater_icon = icon_heater_off;
        // Show delay countdown if waiting to turn on
        float temp_diff_display = target_temperature - tempC;
        
        // Debug: print delay calculation
        Serial.print("[DELAY DEBUG] temp_diff: "); Serial.print(temp_diff_display);
        Serial.print(" HYS_ON: "); Serial.print(HYS_ON);
        Serial.print(" canTurnOn: "); Serial.print(canTurnOn());
        Serial.print(" lastOffMs: "); Serial.print(lastOffMs);
        Serial.print(" millis: "); Serial.print(millis());
        
        if (temp_diff_display >= HYS_ON && !canTurnOn()) {
          unsigned long time_left = MIN_OFF_MS - (millis() - lastOffMs);
          char delayStr[10];
          sprintf(delayStr, "%lum", time_left / (60UL * 1000UL));
          u8g2.drawStr(50, 41, delayStr);  // Show minutes left
          Serial.print(" DELAY: "); Serial.println(delayStr);
        } else {
          u8g2.drawStr(50, 41, "OFF");
          Serial.println(" OFF");
        }
        break;
    }
    u8g2.drawXBMP(1, 35, 16, 16, heater_icon);

    // Wiper debug info - smaller font
    u8g2.setFont(u8g2_font_5x7_tr);
    char wiperStr[12];
    sprintf(wiperStr, "W:%d", wiper_now);
    u8g2.drawStr(85, 41, wiperStr);
    
    // Date and status at bottom
    u8g2.drawStr(1, 58, dateStr);
    if (!rtc_working) {
      u8g2.drawStr(80, 58, "RTC?");
    }

    u8g2.sendBuffer();
  }
}


void ISR_encoderChange() {
  if ((millis() - last_time) < DEBOUNCE_TIME)
    return;

  if (digitalRead(ENCODER_DT_PIN) == HIGH) {
    target_temperature--;
  } else {
    target_temperature++;
  }

  last_time = millis();
}
