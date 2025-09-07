#ifndef EBERSPACHER_CONTROLLER_H
#define EBERSPACHER_CONTROLLER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>
#include <Adafruit_DS3502.h>
#include <ezButton.h>

#include "Config.h"
#include "HeaterController.h"
#include "InputHandler.h"
#include "RTCManager.h"
#include "Display.h"
#include "MenuSystem.h"
#include "PowerManager.h"
#include "WakeupTimer.h"

enum SystemState {
  STATE_STARTUP,
  STATE_NORMAL,
  STATE_MENU,
  STATE_DEBUG,
  STATE_TIME_SET,
  STATE_ERROR
};

class EberspracherController {
private:
  // Hardware instances
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
  OneWire oneWire;
  DallasTemperature sensors;
  RTC_DS3231 rtc;
  Adafruit_DS3502 ds3502;
  ezButton button;
  
  // Controller instances
  HeaterController heaterController;
  InputHandler inputHandler;
  RTCManager rtcManager;
  Display display;
  MenuSystem menuSystem;
  PowerManager powerManager;
  WakeupTimer wakeupTimer;
  
  // System state
  SystemState currentState;
  float currentTemp;
  float targetTemp;
  bool systemEnabled;
  bool firstRun;
  
  // Timing
  unsigned long lastTempRead;
  unsigned long lastDisplayUpdate;
  unsigned long lastHeaterUpdate;
  unsigned long stateChangeTime;
  
  // Error tracking
  bool tempSensorError;
  bool rtcError;
  bool displayError;
  bool ds3502Error;
  
  // Internal methods
  void initializeHardware();
  bool setupComponents();
  void handleStartup();
  void handleNormalOperation();
  void handleMenuOperation();
  void handleDebugMode();
  void handleTimeSetMode();
  void handleErrorState();
  
  void updateTemperature();
  void updateHeater();
  void updateDisplay();
  void updateInputs();
  void updatePower();
  
  void processRotaryInterrupt();
  void changeState(SystemState newState);
  
  DisplayData buildDisplayData();
  void setupMenuCallbacks();
  
  // Static callback functions for menu system
  static bool getHeaterEnabled();
  static void setHeaterEnabled(bool enabled);
  static float getTargetTemp();
  static void setTargetTemp(float temp);
  static void enterTimeSetMode();
  static void enterDebugMode();
  static void enterPowerSaveMode();
  
  // Static wake-up timer callbacks
  static bool addWakeupTimerStatic(uint8_t hour, uint8_t minute, uint8_t temp, uint8_t dayMask, const char* name);
  static uint8_t getWakeupTimerCountStatic();
  static void* getWakeupTimerStatic(uint8_t index);
  static bool removeWakeupTimerStatic(uint8_t index);
  
  // Error handling
  void checkSystemHealth();
  void reportError(const char* component, const char* error);
  
public:
  EberspracherController();
  
  // Main system control
  bool begin();
  void loop();
  void shutdown();
  
  // System state
  SystemState getCurrentState() const { return currentState; }
  bool isSystemEnabled() const { return systemEnabled; }
  void setSystemEnabled(bool enabled);
  
  // Temperature control
  float getCurrentTemp() const { return currentTemp; }
  
  // Wake-up timer control
  WakeupTimer& getWakeupTimer() { return wakeupTimer; }
  bool addWakeupTimer(uint8_t hour, uint8_t minute, uint8_t targetTemp, uint8_t dayMask, const char* name = "");
  bool removeWakeupTimer(uint8_t index);
  
  // Diagnostics
  void printSystemStatus() const;
  void runDiagnostics();
  
  // ISR handlers (to be called from main sketch)
  void handleRotaryISR();
};

// Global instance pointer for ISR access
extern EberspracherController* g_controller;

#endif // EBERSPACHER_CONTROLLER_H