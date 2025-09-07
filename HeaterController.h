#ifndef HEATER_CONTROLLER_H
#define HEATER_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_DS3502.h>
#include "Config.h"

class HeaterController {
private:
  // Hardware
  Adafruit_DS3502* ds3502;
  int controlPin;
  
  // State management
  bool masterEnabled;
  HeatState currentState;
  uint8_t wiperValue;
  
  // Timing for anti-chatter logic
  unsigned long lastOnMs;
  unsigned long lastOffMs;
  unsigned long lastWiperStepMs;
  
  // Internal helper methods
  uint8_t clampWiper(uint8_t value);
  void setWiperSmooth(uint8_t targetValue);
  void setState(HeatState newState);
  
public:
  HeaterController(Adafruit_DS3502* ds3502Ptr, int heaterControlPin);
  
  // Initialization
  bool begin();
  void initializeTiming();  // Allow immediate startup
  
  // Master control (user toggle)
  void setMasterEnabled(bool enabled);
  bool isMasterEnabled() const { return masterEnabled; }
  
  // State and status
  HeatState getState() const { return currentState; }
  uint8_t getWiperValue() const { return wiperValue; }
  
  // Timing constraints
  bool canTurnOn() const;
  bool canTurnOff() const;
  unsigned long getTimeUntilCanTurnOn() const;
  unsigned long getTimeUntilCanTurnOff() const;
  
  // Main control logic
  void update(float cabinTemp, float targetTemp);
  
  // Debug information
  void printStatus() const;
};

#endif // HEATER_CONTROLLER_H