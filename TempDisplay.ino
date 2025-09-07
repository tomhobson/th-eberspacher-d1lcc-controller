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

// ===== State for control + smoothing
enum HeatState { HS_OFF, HS_LOW, HS_MED, HS_HIGH };
static HeatState hs = HS_OFF;
static uint8_t   wiper_now = WIPER_LOW_SAFE;
static unsigned long lastOnMs = 0, lastOffMs = 0, lastWiperStepMs = 0;

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

// D1LC Heater Control Constants
const int WIPER_OFF = 20;          // ~1.8kΩ - minimum power
const int WIPER_LOW = 22;          // ~2.0kΩ - low power  
const int WIPER_MEDIUM = 25;       // ~2.1kΩ - medium power
const int WIPER_HIGH = 28;         // ~2.2kΩ - high power
const int TEMP_THRESHOLD_HIGH = 3;  // degrees below target for high power
const int TEMP_THRESHOLD_MED = 1;   // degrees below target for medium power

// ========================================
// HARDWARE OBJECTS
// ========================================
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_DS3502 ds3502 = Adafruit_DS3502();
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0);
ezButton button(ENCODER_SW_PIN);
RTC_DS3231 rtc;

// ========================================
// PROGRAM VARIABLES
// ========================================
volatile int target_temperature = 0;
volatile unsigned long last_time = 0;
int prev_temperature = 0;
int current_wiper_value = WIPER_OFF;
bool heater_enabled = false;
unsigned long lastDisplayUpdate = 0;

// ========================================
// DISPLAY ICONS (PROGMEM)
// ========================================
static const unsigned char PROGMEM image_bluetooth_bits[] = {
  0x01,0x00,0x02,0x80,0x02,0x40,0x22,0x20,0x12,0x20,0x0a,0x40,0x06,0x80,0x03,0x00,
  0x06,0x80,0x0a,0x40,0x12,0x20,0x22,0x20,0x02,0x40,0x02,0x80,0x01,0x00,0x00,0x00
};

static const unsigned char PROGMEM image_network_3_bars_bits[] = {
  0x00,0x0e,0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0xea,0x00,0xea,0x00,0xea,0x00,0xea,
  0x0e,0xea,0x0e,0xea,0x0e,0xea,0x0e,0xea,0xee,0xea,0xee,0xea,0xee,0xee,0x00,0x00
};

static const unsigned char PROGMEM image_weather_temperature_bits[] = {
  0x1c,0x00,0x22,0x02,0x2b,0x05,0x2a,0x02,0x2b,0x38,0x2a,0x60,0x2b,0x40,0x2a,0x40,
  0x2a,0x60,0x49,0x38,0x9c,0x80,0xae,0x80,0xbe,0x80,0x9c,0x80,0x41,0x00,0x3e,0x00
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
    Serial.println("Couldn't find DS3502 chip");
    while (1);
  }
  Serial.println("Found DS3502 chip");

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // use interrupt for CLK pin is enough
  // call ISR_encoderChange() when CLK pin changes from LOW to HIGH
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), ISR_encoderChange, RISING);
}

void controlHeater(float cabin_temp, int target_temp) {
  const float diff = target_temp - cabin_temp;   // >0 means too cold
  const unsigned long now = millis();
  HeatState desired = hs;

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
        lastOffMs = now;
        Serial.println("Heater: OFF");
        // Park wiper at safe in-range value (some ECUs read on wake)
        wiper_now = clampWiper(WIPER_LOW_SAFE);
        ds3502.setWiper(wiper_now);
        break;

      case HS_LOW:
        digitalWrite(HEATER_CONTROL_PIN, HIGH);
        lastOnMs = now;
        Serial.println("Heater: LOW");
        break;

      case HS_MED:
        digitalWrite(HEATER_CONTROL_PIN, HIGH);
        lastOnMs = now;
        Serial.println("Heater: MEDIUM");
        break;

      case HS_HIGH:
        digitalWrite(HEATER_CONTROL_PIN, HIGH);
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

    // Get current time from RTC
    DateTime now = rtc.now();
    
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    if (target_temperature == 0){
      target_temperature = (int)tempC;
    }

    // Control the D1LC heater based on temperature difference
    controlHeater(tempC, target_temperature);

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

    // Format time for display
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
    
    // Format date for display  
    char dateStr[12];
    sprintf(dateStr, "%02d/%02d/%04d", now.day(), now.month(), now.year());

    //u8g2.clearBuffer();
    //u8g2.setFont(u8g2_font_6x10_tr);
    //u8g2.drawStr(0, 10, displayStr);
    //u8g2.drawStr(0, 22, displayStrTarget);
    //u8g2.sendBuffer();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);

    // Display time at top
    u8g2.drawStr(1, 10, timeStr);
    
    // Temperature readings
    u8g2.drawStr(1, 22, "Current:");
    u8g2.drawStr(50, 22, displayStr);

    u8g2.drawStr(1, 34, "Desired:");
    u8g2.drawStr(50, 34, displayStrTarget);

    u8g2.drawStr(1, 46, "Delta:");
    u8g2.drawStr(50, 46, displayStrDelta);

    // Display date at bottom left
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(1, 62, dateStr);

    // Status icons
    u8g2.drawXBMP(95, 48, 15, 16, image_network_3_bars_bits);
    u8g2.drawXBMP(92, 1, 16, 16, image_weather_temperature_bits);
    u8g2.drawXBMP(113, 48, 14, 16, image_bluetooth_bits);

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
