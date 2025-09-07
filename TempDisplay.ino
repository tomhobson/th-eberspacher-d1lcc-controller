#include <DallasTemperature.h>
#include <U8g2lib.h>
#include "OneWire.h"
#include "DallasTemperature.h"
#include <ezButton.h> 
#include <Adafruit_DS3502.h>

#define CLK_PIN 3
#define DT_PIN 4
#define SW_PIN 5

#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_DS3502 ds3502 = Adafruit_DS3502();
volatile int wipervalue = 0;

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0);

volatile int temperature = 0;
volatile unsigned long last_time;  // for debouncing
int prev_temperature;

unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 200;  // ms

ezButton button(SW_PIN);  // create ezButton object that attach to pin 4

static const unsigned char PROGMEM image_bluetooth_bits[] = {0x01,0x00,0x02,0x80,0x02,0x40,0x22,0x20,0x12,0x20,0x0a,0x40,0x06,0x80,0x03,0x00,0x06,0x80,0x0a,0x40,0x12,0x20,0x22,0x20,0x02,0x40,0x02,0x80,0x01,0x00,0x00,0x00};

static const unsigned char PROGMEM image_network_3_bars_bits[] = {0x00,0x0e,0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0xea,0x00,0xea,0x00,0xea,0x00,0xea,0x0e,0xea,0x0e,0xea,0x0e,0xea,0x0e,0xea,0xee,0xea,0xee,0xea,0xee,0xee,0x00,0x00};

static const unsigned char PROGMEM image_weather_temperature_bits[] = {0x1c,0x00,0x22,0x02,0x2b,0x05,0x2a,0x02,0x2b,0x38,0x2a,0x60,0x2b,0x40,0x2a,0x40,0x2a,0x60,0x49,0x38,0x9c,0x80,0xae,0x80,0xbe,0x80,0x9c,0x80,0x41,0x00,0x3e,0x00};

void setup(void) {
  Serial.begin(9600);
  u8g2.begin();
  sensors.begin();
  pinMode(SW_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  button.setDebounceTime(1);

  if (!ds3502.begin()) {
    Serial.println("Couldn't find DS3502 chip");
    while (1);
  }
  Serial.println("Found DS3502 chip");

  // use interrupt for CLK pin is enough
  // call ISR_encoderChange() when CLK pin changes from LOW to HIGH
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), ISR_encoderChange, RISING);
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
      if (wipervalue == 128) {
        wipervalue = 1;
      }
  
      ds3502.setWiper(wipervalue);
      Serial.print("Actual wiper Value: ");
      Serial.println(wipervalue);

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

    //u8g2.clearBuffer();
    //u8g2.setFont(u8g2_font_6x10_tr);
    //u8g2.drawStr(0, 10, displayStr);
    //u8g2.drawStr(0, 22, displayStrTarget);
    //u8g2.sendBuffer();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);

    u8g2.drawStr(1, 15, "Current:");
    u8g2.drawStr(60, 15, displayStr);

    u8g2.drawStr(1, 27, "Desired:");
    u8g2.drawStr(60, 27, displayStrTarget);

    u8g2.drawStr(1, 40, "Delta:");
    u8g2.drawStr(60, 40, displayStrDelta);

    u8g2.drawXBMP(95, 48, 15, 16, image_network_3_bars_bits);
    u8g2.drawXBMP(92, 16, 16, 16, image_weather_temperature_bits);
    u8g2.drawXBMP(113, 48, 14, 16, image_bluetooth_bits);

    u8g2.sendBuffer();
  }
}


void ISR_encoderChange() {
  if ((millis() - last_time) < 1)  // debounce time is 1ms
    return;

  if (digitalRead(DT_PIN) == HIGH) {
    temperature--;
  } else {
    temperature++;
  }

  last_time = millis();
}
