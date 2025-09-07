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

// ========================================
// PROGRAM CONSTANTS
// ========================================
const unsigned long DISPLAY_INTERVAL = 200;  // ms
const unsigned long DEBOUNCE_TIME = 1;       // ms
const int WIPER_MIN_VALUE = 1;
const int WIPER_MAX_VALUE = 127;
const int SERIAL_BAUD_RATE = 9600;

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
volatile int temperature = 0;
volatile int wipervalue = 0;
volatile unsigned long last_time = 0;
int prev_temperature = 0;
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

void loop() {
  button.loop();  // check button


  // Only update display every DISPLAY_INTERVAL ms
  if (millis() - lastDisplayUpdate > DISPLAY_INTERVAL || button.isPressed()) {
    lastDisplayUpdate = millis();

    if (prev_temperature != temperature) {
      prev_temperature = temperature;
    }

      uint8_t default_value = ds3502.getWiper();
      Serial.print("Default wiper value: ");
      Serial.println(default_value);

      wipervalue++;
      if (wipervalue > WIPER_MAX_VALUE) {
        wipervalue = WIPER_MIN_VALUE;
      }
  
      ds3502.setWiper(wipervalue);
      Serial.print("Actual wiper Value: ");
      Serial.println(wipervalue);

    // Get current time from RTC
    DateTime now = rtc.now();
    
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    if (temperature == 0){
      temperature = (int)tempC;
    }

    float tempDelta = tempC - temperature;

    // Actual Temp
    char tempStr[30];
    dtostrf(tempC, 3, 1, tempStr);
    char displayStr[40];
    sprintf(displayStr, "%sC", tempStr);

    // Target Temp
    char tempStrTarget[30];
    dtostrf(temperature, 3, 1, tempStrTarget);
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
    temperature--;
  } else {
    temperature++;
  }

  last_time = millis();
}
