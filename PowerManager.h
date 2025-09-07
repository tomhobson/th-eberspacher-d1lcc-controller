#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "Config.h"

enum PowerState {
  POWER_ACTIVE,
  POWER_DISPLAY_OFF,
  POWER_LIGHT_SLEEP,
  POWER_DEEP_SLEEP
};

enum WakeupReason {
  WAKE_BUTTON,
  WAKE_ROTARY,
  WAKE_TIMER,
  WAKE_HEATER_CYCLE,
  WAKE_UNKNOWN
};

class PowerManager {
private:
  // State tracking
  PowerState currentState;
  unsigned long lastActivityTime;
  unsigned long lastWakeTime;
  WakeupReason lastWakeupReason;
  
  // Sleep configuration
  bool sleepEnabled;
  bool heaterRunning;
  unsigned long displayOffTimeout;
  unsigned long lightSleepTimeout;
  unsigned long deepSleepTimeout;
  
  // Wake-up tracking
  volatile bool buttonWakeFlag;
  volatile bool rotaryWakeFlag;
  volatile bool timerWakeFlag;
  
  // Power reduction methods
  void disableUnusedPeripherals();
  void enableRequiredPeripherals();
  void setupWatchdog(uint8_t prescaler);
  void disableWatchdog();
  
  // Sleep methods
  void enterLightSleep();
  void enterDeepSleep();
  void wakeUp();
  
  // ISR helper methods
  void handleButtonWake();
  void handleRotaryWake();
  void handleWatchdogWake();
  
public:
  PowerManager();
  
  // Initialization
  void begin();
  
  // Power state control
  void setSleepEnabled(bool enabled);
  bool isSleepEnabled() const { return sleepEnabled; }
  void setHeaterRunning(bool running);
  
  // Activity tracking
  void recordActivity();
  void recordButtonActivity();
  void recordRotaryActivity();
  unsigned long getLastActivityTime() const { return lastActivityTime; }
  unsigned long getTimeSinceActivity() const;
  
  // Power state management
  void update();
  PowerState getCurrentState() const { return currentState; }
  bool shouldDisplayBeOff() const;
  bool shouldEnterLightSleep() const;
  bool shouldEnterDeepSleep() const;
  
  // Manual power control
  void forceDisplayOff();
  void forceLightSleep();
  void forceDeepSleep();
  void forceWakeUp();
  
  // Wake-up information
  WakeupReason getLastWakeupReason() const { return lastWakeupReason; }
  unsigned long getTimeSinceWake() const;
  
  // Configuration
  void setDisplayOffTimeout(unsigned long timeout);
  void setLightSleepTimeout(unsigned long timeout);
  void setDeepSleepTimeout(unsigned long timeout);
  
  // ISR handlers (called from main sketch)
  void handleButtonInterrupt();
  void handleRotaryInterrupt();
  void handleWatchdogInterrupt();
  
  // Debug
  void printStatus() const;
  void printPowerStats() const;
};

// ISR function declarations
extern "C" {
  void powerButtonISR();
  void powerRotaryISR();
  void powerWatchdogISR();
}

#endif // POWER_MANAGER_H